use std::cell::RefCell;

use super::factory::StateFactory;
use super::specialization::CodecSpecialization;
use arithmetic_coder::ArithmeticCoder;
use constants::INPUT_STREAM_PRELOAD_LEN;
use interface::{ErrMsg, SimpleResult};
use io::BufferedOutputStream;
use iostream::{iostream, InputError, InputStream, OutputError, OutputStream};
use jpeg::{get_components, process_scan, split_scan, Component, Dimensions, Scan};
use std::thread;
use thread_handoff::ThreadHandoffExt;

const OUTPUT_BUFFER_SIZE: usize = 4096;

#[derive(Clone, Debug)]
pub enum CodecError {
    CodingFailure(ErrMsg),
    ReadAfterFinish,
    TooMuchInput,
}

pub fn create_codecs<
    Coder: ArithmeticCoder + 'static,
    Specialization: CodecSpecialization + 'static,
    Factory: StateFactory<Coder, Specialization>,
>(
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    mut scans: Vec<Scan>,
    thread_handoffs: &[ThreadHandoffExt],
    pad: u8,
) -> Vec<LeptonCodec> {
    let mut codecs = Vec::with_capacity(thread_handoffs.len());
    for (i, handoff) in thread_handoffs.iter().enumerate() {
        // FIXME: Minimize cloning scans and pass only necessary coefficients
        let mcu_y_end = if i == thread_handoffs.len() - 1
            || handoff.end_scan < thread_handoffs[i + 1].start_scan
        {
            None
        } else {
            Some(thread_handoffs[i + 1].mcu_y_start)
        };
        let codec_scans = scans[(handoff.start_scan as usize)..=(handoff.end_scan as usize)]
            .iter_mut()
            .enumerate()
            .map(|(j, scan)| {
                let mcu_y_end = if j != (handoff.end_scan - handoff.start_scan) as usize
                    || i == thread_handoffs.len() - 1
                    || handoff.end_scan < thread_handoffs[i + 1].start_scan
                {
                    None
                } else {
                    Some(thread_handoffs[i + 1].mcu_y_start)
                };
                split_scan(
                    scan,
                    &components,
                    if j == 0 { handoff.mcu_y_start } else { 0 },
                    if j == (handoff.end_scan - handoff.start_scan) as usize {
                        mcu_y_end
                    } else {
                        None
                    },
                )
            })
            .collect();
        codecs.push(LeptonCodec::new::<Coder, Specialization, Factory>(
            components.clone(),
            size_in_mcu.clone(),
            codec_scans,
            handoff,
            mcu_y_end,
            pad,
        ));
    }
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
        handoff: &ThreadHandoffExt,
        mcu_y_end: Option<u16>,
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
            mcu_y_end,
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
        if let Err(OutputError::ReaderAborted) = self.to_codec.write(data) {
            let err = match self.codec_handle.take().unwrap().join().unwrap() {
                Ok(()) => CodecError::TooMuchInput,
                Err(msg) => CodecError::CodingFailure(msg),
            };
            self.error = Some(err.clone());
            return Err(err);
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
            self.join_codec()?;
        }
        let len = match self.from_codec.read(buf, false, false) {
            Ok(len) => len,
            Err(InputError::UnexpectedEof) => {
                self.join_codec()?;
                self.finished = true;
                0
            }
            _ => unreachable!(),
        };
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

    fn join_codec(&mut self) -> SimpleResult<CodecError> {
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
        Ok(())
    }
}

struct InternalCodec<Coder: ArithmeticCoder, Specialization: CodecSpecialization> {
    coder: Coder,
    specialization: Specialization,
    input: InputStream,
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    mcu_y_start: u16,
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
        handoff: &ThreadHandoffExt,
        mcu_y_end: Option<u16>,
        pad: u8,
    ) -> Self {
        let (coder, specialization) = Factory::build(
            BufferedOutputStream::new(output, OUTPUT_BUFFER_SIZE),
            handoff,
            mcu_y_end,
            pad,
        );
        Self {
            coder,
            specialization,
            input,
            components,
            size_in_mcu,
            scans,
            mcu_y_start: handoff.mcu_y_start,
        }
    }

    pub fn start(mut self) -> SimpleResult<ErrMsg> {
        let mut result = Ok(());
        for i in 0..self.scans.len() {
            result = self.process_scan(i);
            if result.is_err() {
                break;
            }
        }
        self.specialization.flush()?;
        self.input.abort();
        self.specialization.write_eof();
        result
    }

    fn process_scan(&mut self, scan_index: usize) -> SimpleResult<ErrMsg> {
        let scan = &mut self.scans[scan_index];
        let mut components = get_components(&scan.info.component_indices, &mut self.components);
        let input = &mut self.input;
        let specialization = &mut self.specialization;
        specialization.prepare_scan(scan, scan_index)?;
        let specialization = RefCell::new(specialization);
        let mut mcu_callback = |mcu_y: usize, mcu_x: usize, restart: bool, expected_rst: u8| {
            specialization
                .borrow_mut()
                .prepare_mcu(mcu_y, mcu_x, restart, expected_rst)
        };
        let mut block_callback = |block_y: usize,
                                  block_x: usize,
                                  component_index_in_scan: usize,
                                  component: &mut Component,
                                  scan: &mut Scan| {
            specialization.borrow_mut().process_block(
                input,
                block_y,
                block_x,
                component_index_in_scan,
                component,
                scan,
            )
        };
        if process_scan(
            scan,
            &mut components,
            if scan_index == 0 {
                self.mcu_y_start as usize
            } else {
                0
            },
            &self.size_in_mcu,
            &mut mcu_callback,
            &mut block_callback,
        )? {
            specialization.borrow_mut().finish_scan()?;
        }
        scan.coefficients = None;
        Ok(())
    }
}
