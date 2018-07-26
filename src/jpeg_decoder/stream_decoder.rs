use std::thread;

use super::decoder::{DecodeResult, JpegDecoder};
use constants::INPUT_STREAM_PRELOAD_LEN;
use interface::CumulativeOperationResult;
use iostream::{iostream, OutputStream};

pub struct JpegStreamDecoder {
    decoder_handle: Option<thread::JoinHandle<DecodeResult>>,
    ostream: OutputStream,
    result: Option<DecodeResult>,
}

impl JpegStreamDecoder {
    pub fn new(start_byte: usize) -> Self {
        let (istream, ostream) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let decoder_handle = Some(
            thread::Builder::new()
                .name("decoder thread".to_owned())
                .spawn(move || JpegDecoder::new(istream, start_byte, false).decode())
                .unwrap(),
        );
        JpegStreamDecoder {
            decoder_handle,
            ostream,
            result: None,
        }
    }

    pub fn decode(&mut self, input: &[u8], input_offset: &mut usize) -> CumulativeOperationResult {
        use CumulativeOperationResult::*;
        match self.ostream.write(input) {
            Ok(_) => {
                *input_offset = input.len();
                NeedsMoreInput
            }
            Err(_) => {
                self.finish();
                Finish
            }
        }
    }

    pub fn flush(&mut self) {
        let _ = self.ostream.write_eof();
        self.finish();
    }

    pub fn take_result(self) -> DecodeResult {
        if let Some(result) = self.result {
            result
        } else {
            panic!("decoder has not finished");
        }
    }

    fn finish(&mut self) {
        if self.decoder_handle.is_some() {
            self.result = Some(self.decoder_handle.take().unwrap().join().unwrap());
        };
    }
}
