use interface::{ErrMsg, LeptonOperationResult};
use primary_header::{deserialize_header, PrimaryHeader, HEADER_SIZE};
use util::mem_copy;

pub struct PrimaryHeaderParser {
    header: [u8; HEADER_SIZE],
    total_in: usize,
}

impl PrimaryHeaderParser {
    pub fn new() -> Self {
        PrimaryHeaderParser {
            header: [0u8; HEADER_SIZE],
            total_in: 0,
        }
    }

    pub fn parse(&mut self, input: &[u8], input_offset: &mut usize) -> LeptonOperationResult {
        mem_copy(&mut self.header, &mut self.total_in, input, input_offset);
        if self.total_in == HEADER_SIZE {
            LeptonOperationResult::Success
        } else {
            LeptonOperationResult::NeedsMoreInput
        }
    }

    pub fn build_header(&self) -> Result<PrimaryHeader, ErrMsg> {
        return deserialize_header(&self.header[..self.total_in]);
    }
}
