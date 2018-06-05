use super::brotli_encoder::BrotliEncoder;
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult, HEADER_SIZE, LEPTON_VERSION,
                MAGIC_NUMBER};
use util::u32_to_le_u8_array;

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
        self.total_in -= *input_offset;
        let result = match self.active_encoder {
            // TODO: check for start of JPEG data
            EncoderType::Brotli => {
                self.brotli_encoder
                    .encode(input, input_offset, output, output_offset)
            }
            EncoderType::Lepton => LeptonOperationResult::Success,
        };
        self.total_in += *input_offset;
        result
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        if let None = self.header {
            self.header = Some(make_header(
                &LEPTON_VERSION,
                &0,
                &1,
                &[0u8; 12],
                &self.total_in,
                &self.brotli_encoder.size(),
            ));
            self.active_encoder = EncoderType::Brotli;
        }
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

fn make_header(
    version: &u8,
    skip_serial_hdr: &u8,
    n_threads: &u32,
    git_hash: &[u8; 12],
    raw_size: &usize,
    secondary_hdr_size: &usize,
) -> [u8; HEADER_SIZE] {
    let mut header = [0u8; HEADER_SIZE];
    header[..MAGIC_NUMBER.len()].clone_from_slice(&MAGIC_NUMBER);
    header[2] = *version;
    header[3] = *skip_serial_hdr;
    header[4..8].clone_from_slice(&u32_to_le_u8_array(n_threads));
    header[8..20].clone_from_slice(git_hash);
    header[20..24].clone_from_slice(&u32_to_le_u8_array(&(*raw_size as u32)));
    header[24..].clone_from_slice(&u32_to_le_u8_array(&(*secondary_hdr_size as u32)));
    header
}
