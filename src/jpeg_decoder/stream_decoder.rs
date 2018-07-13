use std::thread;

use super::decoder::JpegDecoder;
use super::error::JpegResult;
use super::jpeg::FrequencyImage;
use iostream::{iostream, OutputStream};

pub struct JpegStreamDecoder {
    decoder_handle: thread::JoinHandle<JpegResult<FrequencyImage>>,
    ostream: OutputStream,
}

impl JpegStreamDecoder {
    pub fn new() -> JpegResult<Self> {
        let (istream, ostream) = iostream();
        let decoder_handle = thread::Builder::new()
            .name("decoder thread".to_owned())
            .spawn(move || JpegDecoder::new(istream).decode())?;
        Ok(JpegStreamDecoder {
            decoder_handle,
            ostream,
        })
    }

    pub fn decode(&self, input: &[u8], input_offset: &mut usize) -> JpegResult<()> {
        match self.ostream.write(&input[*input_offset..]) {
            Ok(_) => {
                *input_offset = input.len();
                Ok(())
            }
            Err(_) => self.finish(),
        }
    }

    pub fn flush(&self) -> JpegResult<()> {
        self.ostream.write_eof();
        self.finish()
    }

    pub fn get_result() -> Option<()> {
        // TODO: Return FrequencyImage
        // TODO: Parse unread_data
        Some(())
    }

    fn finish(&self) -> JpegResult<()> {
        match self.decoder_handle.join().unwrap() {
            Ok(_) => Ok(()), // TODO: Save FrequencyImage
            Err(e) => Err(e),
        }
    }
}
