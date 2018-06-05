use interface::{ErrMsg, LeptonOperationResult, HEADER_SIZE, MAGIC_NUMBER};
use util::{mem_copy, le_u8_array_to_u32};

#[derive(Clone, Copy)]
pub struct PrimaryHeader {
    pub version: u8,
    pub skip_hdr: bool,
    pub n_threads: u32,
    pub git_hash: [u8; 12],
    pub raw_size: usize,
    pub secondary_hdr_size: usize,
}

impl PrimaryHeader {
    fn from_array(array: &[u8; HEADER_SIZE]) -> Self {
        let mut header = PrimaryHeader {
            version: array[2],
            skip_hdr: if array[3] == 0 { false } else { true },
            n_threads: le_u8_array_to_u32(array, &4),
            git_hash: [0u8; 12],
            raw_size: le_u8_array_to_u32(array, &20) as usize,
            secondary_hdr_size: le_u8_array_to_u32(array, &24) as usize,
        };
        header.git_hash.clone_from_slice(&array[8..20]);
        header
    }
}

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
        if self.total_in < HEADER_SIZE {
            Err(ErrMsg::IncompletePrimaryHeader)
        } else if !self.header[..2].eq(&MAGIC_NUMBER) {
            Err(ErrMsg::WrongMagicNumber)
        } else {
            Ok(PrimaryHeader::from_array(&self.header))
        }
    }
}
