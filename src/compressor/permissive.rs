use super::brotli_encoder::BrotliEncoder;
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult};
use primary_header::{serialize_header, HEADER_SIZE};
use secondary_header::{default_serialized_header, MARKER_SIZE, SECTION_HDR_SIZE};
use util::{mem_copy, u32_to_le_u8_array};

pub struct LeptonPermissiveCompressor {
    encoder: BrotliEncoder,
    header: Option<[u8; HEADER_SIZE]>,
    header_written: usize,
    data: Vec<u8>,
    first_call: bool,
}

impl LeptonPermissiveCompressor {
    pub fn new() -> Self {
        let mut data = Vec::with_capacity(1024); // FIXME: Better initial capacity?
        data.extend_from_slice(&[b'P', b'G', b'E', 0, 0, 0, 0]);
        LeptonPermissiveCompressor {
            encoder: BrotliEncoder::new(),
            header: None,
            header_written: 0,
            data,
            first_call: true,
        }
    }
}

impl Compressor for LeptonPermissiveCompressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        if self.first_call {
            let mut basic_header_offset = 0usize;
            if let LeptonOperationResult::Failure(msg) = self.encoder.encode_all(
                &default_serialized_header(),
                &mut basic_header_offset,
                output,
                output_offset,
            ) {
                return LeptonOperationResult::Failure(msg);
            }
            self.first_call = false;
        }
        self.data.extend_from_slice(&input[*input_offset..]);
        *input_offset = input.len();
        LeptonOperationResult::NeedsMoreInput
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        match self.header {
            None => {
                let pge_len = self.data.len() - SECTION_HDR_SIZE;
                let pge_len_array = u32_to_le_u8_array(&(pge_len as u32));
                self.data[MARKER_SIZE..SECTION_HDR_SIZE].clone_from_slice(&pge_len_array);
                let mut input_offset = 0;
                if let LeptonOperationResult::Failure(msg) = self.encoder.encode_all(
                    self.data.as_slice(),
                    &mut input_offset,
                    output,
                    output_offset,
                ) {
                    return LeptonFlushResult::Failure(msg);
                }
                match self.encoder.finish() {
                    Ok(size) => {
                        self.header =
                            Some(serialize_header(&b'Y', &1u32, &[0u8; 12], &pge_len, &size));
                        LeptonFlushResult::NeedsMoreOutput
                    }
                    Err(e) => LeptonFlushResult::Failure(e),
                }
            }
            Some(header) => {
                if self.header_written < HEADER_SIZE {
                    mem_copy(output, output_offset, &header, &mut self.header_written);
                }
                if self.header_written == HEADER_SIZE {
                    self.encoder.flush(output, output_offset)
                // TODO: Write CMP after encoder is done flushing
                } else {
                    LeptonFlushResult::NeedsMoreOutput
                }
            }
        }
    }
}
