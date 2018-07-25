use std::cmp::min;

use interface::LeptonOperationResult;
use secondary_header::SecondaryHeader;
use util::mem_copy;

pub struct LeptonDecoder {
    header: SecondaryHeader,
    target_len: usize,
    total_out: usize,
    current_scan: usize,
    scan_header_written: usize,
}

impl LeptonDecoder {
    pub fn new(header: SecondaryHeader, target_len: usize) -> Self {
        LeptonDecoder {
            header,
            target_len,
            total_out: 0,
            current_scan: 0,
            scan_header_written: 0,
        }
    }

    pub fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        let old_output_offset = *output_offset;
        let cmp_needed = self.target_len - self.header.grb.len() - self.total_out;
        if cmp_needed > 0 {
            let input_end = min(*input_offset + cmp_needed, input.len());
            mem_copy(output, output_offset, &input[..input_end], input_offset);
        } else {
            if !self.header.grb.is_empty() {
                let mut grb_written = self.total_out - self.target_len + self.header.grb.len();
                mem_copy(output, output_offset, &self.header.grb, &mut grb_written);
            }
        };
        self.total_out += *output_offset - old_output_offset;
        if self.total_out < self.target_len - self.header.grb.len() {
            LeptonOperationResult::NeedsMoreInput
        } else if self.total_out < self.target_len {
            LeptonOperationResult::NeedsMoreOutput
        } else {
            LeptonOperationResult::Success
        }
    }
}
