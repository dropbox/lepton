use super::brotli_encoder::BrotliEncoder;
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult};
use primary_header::{serialize_header, HEADER_SIZE};

enum EncoderType {
    Brotli,
    Lepton,
}

pub struct LeptonCompressor {
    brotli_encoder: BrotliEncoder,
    active_encoder: EncoderType,
    total_in: usize,
    header: Option<[u8; HEADER_SIZE]>,
    header_written: usize,
}

impl LeptonCompressor {
    pub fn new() -> Self {
        LeptonCompressor {
            brotli_encoder: BrotliEncoder::new(),
            active_encoder: EncoderType::Brotli,
            total_in: 0,
            header: None,
            header_written: 0,
        }
    }
}

impl Compressor for LeptonCompressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        let old_input_offset = *input_offset;
        let result = match self.active_encoder {
            // TODO: check for start of JPEG data
            EncoderType::Brotli => {
                self.brotli_encoder
                    .encode(input, input_offset, output, output_offset)
            }
            EncoderType::Lepton => LeptonOperationResult::Success,
        };
        self.total_in += *input_offset - old_input_offset;
        result
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        // if let None = self.header {
        //     self.header = Some(serialize_header(
        //         &0,
        //         &1,
        //         &[0u8; 12],
        //         &self.total_in,
        //         &self.brotli_encoder.finish(),
        //     ));
        //     self.active_encoder = EncoderType::Brotli;
        // }
        // if self.header_written < HEADER_SIZE {
        //     // TODO: write out header
        //     LeptonFlushResult::NeedsMoreOutput
        // } else {
        //     match self.active_encoder {
        //         EncoderType::Brotli => match self.brotli_encoder.flush(output, output_offset) {
        //             LeptonFlushResult::Success => {
        //                 self.active_encoder = EncoderType::Lepton;
        //                 LeptonFlushResult::NeedsMoreOutput
        //             }
        //             other => other,
        //         },
        //         EncoderType::Lepton => LeptonFlushResult::Success,
        //     }
        // }
        match self.active_encoder {
            EncoderType::Brotli => match self.brotli_encoder.flush(output, output_offset) {
                LeptonFlushResult::Success => {
                    self.active_encoder = EncoderType::Lepton;
                    LeptonFlushResult::NeedsMoreOutput
                }
                other => other,
            },
            EncoderType::Lepton => LeptonFlushResult::Success,
        }
    }
}
