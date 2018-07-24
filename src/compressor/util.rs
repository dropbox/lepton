use compressor::brotli_encoder::BrotliEncoder;
use interface::{Compressor, LeptonFlushResult};
use primary_header::HEADER_SIZE as PRIMARY_HEADER_SIZE;
use util::mem_copy;

pub fn flush_lepton_data(
    output: &mut [u8],
    output_offset: &mut usize,
    primary_header: &[u8],
    brotli_encoder: &mut BrotliEncoder,
    cmp: &[u8],
    primary_header_written: &mut usize,
    brotli_done: &mut bool,
    cmp_written: &mut usize,
) -> Option<LeptonFlushResult> {
    if *primary_header_written < PRIMARY_HEADER_SIZE {
        mem_copy(
            output,
            output_offset,
            primary_header,
            primary_header_written,
        );
    } else if *brotli_done {
        if *cmp_written < cmp.len() {
            mem_copy(output, output_offset, cmp, cmp_written);
            if *cmp_written == cmp.len() {
                return Some(LeptonFlushResult::Success);
            } else {
                return Some(LeptonFlushResult::NeedsMoreOutput);
            }
        } else {
            return Some(LeptonFlushResult::Success);
        }
    } else {
        match brotli_encoder.flush(output, output_offset) {
            LeptonFlushResult::Success => *brotli_done = true,
            other => return Some(other),
        }
    }
    None
}
