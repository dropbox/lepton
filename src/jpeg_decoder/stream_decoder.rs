use std::thread;

use super::decoder::{DecodeResult, JpegDecoder};
use super::error::JpegResult;
use iostream::{iostream, OutputStream};

const PRE_LOAD_LEN: usize = 4 * 1024;

pub struct JpegStreamDecoder {
    decoder_handle: Option<thread::JoinHandle<DecodeResult>>,
    ostream: OutputStream,
    result: Option<DecodeResult>,
}

impl JpegStreamDecoder {
    pub fn new(start_byte: usize) -> JpegResult<Self> {
        let (istream, ostream) = iostream(PRE_LOAD_LEN);
        let decoder_handle = thread::Builder::new()
            .name("decoder thread".to_owned())
            .spawn(move || JpegDecoder::new(istream, start_byte, false).decode())?;
        Ok(JpegStreamDecoder {
            decoder_handle: Some(decoder_handle),
            ostream,
            result: None,
        })
    }

    pub fn decode(&mut self, input: &[u8], input_offset: &mut usize) -> JpegDecodeResult {
        match self.ostream.write(input) {
            Ok(_) => {
                *input_offset = input.len();
                JpegDecodeResult::NeedsMoreInput
            }
            Err(_) => self.finish(),
        }
    }

    pub fn flush(&mut self) -> JpegDecodeResult {
        let _ = self.ostream.write_eof();
        self.finish()
    }

    pub fn take_result(self) -> Result<DecodeResult, Self> {
        if let Some(result) = self.result {
            Ok(result)
        } else {
            Err(self)
        }
    }

    fn finish(&mut self) -> JpegDecodeResult {
        if self.decoder_handle.is_some() {
            self.result = Some(self.decoder_handle.take().unwrap().join().unwrap());
        }
        JpegDecodeResult::Finish
    }
}

pub enum JpegDecodeResult {
    Finish,
    NeedsMoreInput,
}
