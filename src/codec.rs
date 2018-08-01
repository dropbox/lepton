use std::thread;

use arithmetic_coder::ArithmeticCoder;
use constants::INPUT_STREAM_PRELOAD_LEN;
use interface::{ErrMsg, SimpleResult};
use io::{BufferedOutputStream, Write};
use iostream::{iostream, InputStream, OutputError, OutputStream};
use jpeg_decoder::{process_scan, Component, Dimensions, Scan};
use thread_handoff::ThreadHandoffExt;

const OUTPUT_BUFFER_SIZE: usize = 4096;

#[derive(Clone, Debug)]
pub enum CodecError {
    CodingFailure(ErrMsg),
    ReadAfterFinish,
    TooMuchInput,
    WriteAfterEOF,
}

pub fn create_codecs<ArithmeticEncoderOrDecoder: ArithmeticCoder + Send + 'static>(
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    mut thread_handoffs: Vec<ThreadHandoffExt>,
    pad: u8,
    coder_factory: &Fn() -> ArithmeticEncoderOrDecoder,
) -> Vec<LeptonCodec> {
    let mut codecs = Vec::with_capacity(thread_handoffs.len());
    for _ in 0..thread_handoffs.len() {
        let handoff = thread_handoffs.pop().unwrap();
        let codec_scans = scans[(handoff.start_scan as usize)..=(handoff.end_scan as usize)]
            .iter()
            .map(|scan| scan.clone())
            .collect();
        codecs.push(LeptonCodec::new(
            coder_factory(),
            components.clone(),
            size_in_mcu.clone(),
            codec_scans,
            handoff,
            pad,
        ));
    }
    codecs.reverse();
    codecs
}

pub struct LeptonCodec {
    codec_handle: Option<thread::JoinHandle<SimpleResult<ErrMsg>>>,
    to_codec: OutputStream,
    from_codec: InputStream,
    error: Option<CodecError>,
    consumed_all_input: bool,
    finished: bool,
}

impl LeptonCodec {
    pub fn new<ArithmeticEncoderOrDecoder: ArithmeticCoder + Send + 'static>(
        arithmetic_coder: ArithmeticEncoderOrDecoder,
        components: Vec<Component>,
        size_in_mcu: Dimensions,
        scans: Vec<Scan>,
        handoff: ThreadHandoffExt,
        pad: u8,
    ) -> Self {
        let (codec_input, to_codec) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let (from_codec, codec_output) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let codec_handle = Some(
            thread::Builder::new()
                .name("codec thread".to_owned())
                .spawn(move || {
                    InternalCodec::new(
                        arithmetic_coder,
                        codec_input,
                        codec_output,
                        components,
                        size_in_mcu,
                        scans,
                        handoff,
                        pad,
                    ).start()
                })
                .unwrap(),
        );
        Self {
            codec_handle,
            to_codec,
            from_codec,
            error: None,
            consumed_all_input: false,
            finished: false,
        }
    }

    /// Returns true when the codec has processed all its input until EOF and
    /// written out all its output.
    pub fn finished(&self) -> bool {
        self.finished
    }

    pub fn write(&mut self, data: &[u8]) -> SimpleResult<CodecError> {
        self.check_error()?;
        if self.consumed_all_input {
            return Err(CodecError::TooMuchInput);
        }
        if let Err(e) = self.to_codec.write(data) {
            use self::OutputError::*;
            match e {
                EofWritten => return Err(CodecError::WriteAfterEOF),
                ReaderAborted => {
                    let err = match self.codec_handle.take().unwrap().join().unwrap() {
                        Ok(()) => CodecError::TooMuchInput,
                        Err(msg) => CodecError::CodingFailure(msg),
                    };
                    self.error = Some(err.clone());
                    return Err(err);
                }
            }
        }
        Ok(())
    }

    pub fn write_eof(&mut self) {
        let _ = self.to_codec.write_eof();
    }

    pub fn read(&mut self, buf: &mut [u8]) -> Result<usize, CodecError> {
        self.check_error()?;
        if self.finished {
            return Err(CodecError::ReadAfterFinish);
        }
        if self.from_codec.eof_written() {
            if self.codec_handle.is_some() {
                match self.codec_handle.take().unwrap().join().unwrap() {
                    Ok(()) => self.consumed_all_input = true,
                    Err(msg) => {
                        let err = CodecError::CodingFailure(msg);
                        self.error = Some(err.clone());
                        return Err(err);
                    }
                }
            }
        }
        let len = self.from_codec.read(buf, false, false).unwrap();
        if self.from_codec.is_eof() {
            self.finished = true;
        }
        return Ok(len);
    }

    pub fn kill(self) {
        let _ = self.to_codec.write_eof();
        let _ = self.from_codec.abort();
    }

    fn check_error(&self) -> SimpleResult<CodecError> {
        match self.error {
            Some(ref err) => Err(err.clone()),
            None => Ok(()),
        }
    }
}

struct InternalCodec<ArithmeticEncoderOrDecoder: ArithmeticCoder> {
    arithmetic_coder: ArithmeticEncoderOrDecoder,
    input: InputStream,
    output: BufferedOutputStream,
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    handoff: ThreadHandoffExt,
    pad: u8,
}

impl<ArithmeticEncoderOrDecoder: ArithmeticCoder> InternalCodec<ArithmeticEncoderOrDecoder> {
    pub fn new(
        arithmetic_coder: ArithmeticEncoderOrDecoder,
        input: InputStream,
        output: OutputStream,
        components: Vec<Component>,
        size_in_mcu: Dimensions,
        scans: Vec<Scan>,
        handoff: ThreadHandoffExt,
        pad: u8,
    ) -> Self {
        InternalCodec {
            arithmetic_coder,
            input,
            output: BufferedOutputStream::new(output, OUTPUT_BUFFER_SIZE),
            components,
            size_in_mcu,
            scans,
            handoff,
            pad,
        }
    }

    pub fn start(mut self) -> SimpleResult<ErrMsg> {
        for i in 0..self.scans.len() {
            self.process_scan(i)?;
        }
        self.output.flush().unwrap(); // FIXME
        self.input.abort();
        self.output.ostream.write_eof().unwrap(); // FIXME
        Ok(())
    }

    fn process_scan(&mut self, scan_index: usize) -> SimpleResult<ErrMsg> {
        let scan = &mut self.scans[scan_index];
        let input = &mut self.input;
        let output = &mut self.output;
        let mut mcu_row_callback = |mcu_y: usize| Ok(());
        let mut mcu_callback = |mcu_y: usize, mcu_x: usize| Ok(());
        let mut block_callback = |block_y: usize,
                                  block_x: usize,
                                  component_index_in_scan: usize,
                                  component: &Component,
                                  scan: &mut Scan| {
            process_block(
                input,
                output,
                block_y,
                block_x,
                component_index_in_scan,
                component,
                scan,
            )
        };
        let mut rst_callback = |exptected_rst: u8| Ok(());
        process_scan(
            scan,
            &self.components,
            &self.size_in_mcu,
            &mut mcu_row_callback,
            &mut mcu_callback,
            &mut block_callback,
            &mut rst_callback,
        )?;
        scan.coefficients = None;
        Ok(())
    }
}

fn process_block(
    input: &mut InputStream,
    output: &mut BufferedOutputStream,
    y: usize,
    x: usize,
    component_index_in_scan: usize,
    component: &Component,
    scan: &mut Scan,
) -> SimpleResult<ErrMsg> {
    let mut block = [0u8; 128];
    // println!("process block {} {}", y, x);
    match scan.coefficients {
        Some(ref coefficients) => {
            let block_offset = (y * component.size_in_block.width as usize + x) * 64;
            for (i, &coefficient) in coefficients[component_index_in_scan]
                [block_offset..block_offset + 64]
                .iter()
                .enumerate()
            {
                block[2 * i] = (coefficient >> 8) as u8;
                block[2 * i + 1] = coefficient as u8;
            }
        }
        None => {
            // TODO: Maybe transform coefficients to i16 here
            input.read(&mut block, true, false).unwrap(); // FIXME
        }
    }
    output.write(&block).unwrap(); // FIXME
    Ok(())
}
