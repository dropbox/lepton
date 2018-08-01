use super::factory::StateFactory;
use super::specialization::CodecSpecialization;
use arithmetic_coder::ArithmeticCoder;
use byte_converter::{BigEndian, ByteConverter};
use constants::INPUT_STREAM_PRELOAD_LEN;
use interface::{ErrMsg, SimpleResult};
use io::{BufferedOutputStream, Write};
use iostream::{iostream, InputStream, OutputError, OutputStream};
use jpeg::{process_scan, Component, Dimensions, Scan};
use std::thread;
use thread_handoff::ThreadHandoffExt;

const OUTPUT_BUFFER_SIZE: usize = 4096;

#[derive(Clone, Debug)]
pub enum CodecError {
    CodingFailure(ErrMsg),
    ReadAfterFinish,
    TooMuchInput,
    WriteAfterEOF,
}

pub fn create_codecs<
    Coder: ArithmeticCoder + 'static,
    Specialization: CodecSpecialization + 'static,
    Factory: StateFactory<Coder, Specialization>,
>(
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    mut thread_handoffs: Vec<ThreadHandoffExt>,
    pad: u8,
) -> Vec<LeptonCodec> {
    let mut codecs = Vec::with_capacity(thread_handoffs.len());
    for _ in 0..thread_handoffs.len() {
        let handoff = thread_handoffs.pop().unwrap();
        let codec_scans = scans[(handoff.start_scan as usize)..=(handoff.end_scan as usize)]
            .iter()
            .map(|scan| scan.clone())
            .collect();
        codecs.push(LeptonCodec::new::<Coder, Specialization, Factory>(
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
    pub fn new<
        Coder: ArithmeticCoder + 'static,
        Specialization: CodecSpecialization + 'static,
        Factory: StateFactory<Coder, Specialization>,
    >(
        components: Vec<Component>,
        size_in_mcu: Dimensions,
        scans: Vec<Scan>,
        handoff: ThreadHandoffExt,
        pad: u8,
    ) -> Self {
        let (codec_input, to_codec) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let (from_codec, codec_output) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let codec = InternalCodec::new::<Factory>(
            codec_input,
            codec_output,
            components,
            size_in_mcu,
            scans,
            handoff,
            pad,
        );
        let codec_handle = Some(
            thread::Builder::new()
                .name("codec thread".to_owned())
                .spawn(move || codec.start())
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

    pub fn kill(mut self) {
        self.write_eof();
        let _ = self.from_codec.abort();
    }

    fn check_error(&self) -> SimpleResult<CodecError> {
        match self.error {
            Some(ref err) => Err(err.clone()),
            None => Ok(()),
        }
    }
}

struct InternalCodec<Coder: ArithmeticCoder, Specialization: CodecSpecialization> {
    coder: Coder,
    specialization: Specialization,
    input: InputStream,
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    handoff: ThreadHandoffExt,
    pad: u8,
}

impl<Coder: ArithmeticCoder, Specialization: CodecSpecialization>
    InternalCodec<Coder, Specialization>
{
    pub fn new<Factory: StateFactory<Coder, Specialization>>(
        input: InputStream,
        output: OutputStream,
        components: Vec<Component>,
        size_in_mcu: Dimensions,
        scans: Vec<Scan>,
        handoff: ThreadHandoffExt,
        pad: u8,
    ) -> Self {
        let (coder, specialization) =
            Factory::build(BufferedOutputStream::new(output, OUTPUT_BUFFER_SIZE));
        Self {
            coder,
            specialization,
            input,
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
            // TODO: Flush arithmetic coder
        }
        self.specialization.flush()?;
        self.input.abort();
        self.specialization.write_eof();
        Ok(())
    }

    fn process_scan(&mut self, scan_index: usize) -> SimpleResult<ErrMsg> {
        let scan = &mut self.scans[scan_index];
        let input = &mut self.input;
        let specialization = &mut self.specialization;
        let mut mcu_row_callback = |mcu_y: usize| Ok(());
        let mut mcu_callback = |mcu_y: usize, mcu_x: usize| Ok(());
        let mut block_callback = |block_y: usize,
                                  block_x: usize,
                                  component_index_in_scan: usize,
                                  component: &Component,
                                  scan: &mut Scan| {
            specialization.process_block(
                input,
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
    let mut block_holder: [i16; 64];
    let block = match scan.coefficients {
        Some(ref coefficients) => {
            let block_offset = (y * component.size_in_block.width as usize + x) * 64;
            &coefficients[component_index_in_scan][block_offset..block_offset + 64]
        }
        None => {
            let mut block_u8 = [0; 128];
            block_holder = [0i16; 64];
            input.read(&mut block_u8, true, false).unwrap(); // FIXME
            for (i, coefficient) in block_holder.iter_mut().enumerate() {
                *coefficient = BigEndian::slice_to_u16(&block_u8[(2 * i)..]) as i16;
            }
            &block_holder
        }
    };
    // output.write(block).unwrap(); // FIXME
    Ok(())
}
