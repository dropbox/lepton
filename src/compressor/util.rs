use alloc::Allocator;
use mux::{Mux, StreamMuxer};

use compressor::brotli_encoder::BrotliEncoder;
use interface::{Compressor, LeptonFlushResult};
use primary_header::HEADER_SIZE as PRIMARY_HEADER_SIZE;
use util::mem_copy;

const CMP_HEADER: [u8; 3] = [b'C', b'M', b'P'];

pub fn flush_lepton_data<AllocU8: Allocator<u8>>(
    output: &mut [u8],
    output_offset: &mut usize,
    primary_header: &[u8],
    brotli_encoder: &mut BrotliEncoder,
    cmp: &mut Mux<AllocU8>,
    primary_header_written: &mut usize,
    brotli_done: &mut bool,
    cmp_header_written: &mut usize,
) -> Option<LeptonFlushResult> {
    if *primary_header_written < PRIMARY_HEADER_SIZE {
        mem_copy(
            output,
            output_offset,
            primary_header,
            primary_header_written,
        );
    } else if *brotli_done {
        if *cmp_header_written < CMP_HEADER.len() {
            mem_copy(output, output_offset, &CMP_HEADER, cmp_header_written);
        }
        if !cmp.wrote_eof() {
            *output_offset += cmp.flush(&mut output[*output_offset..]);
            if cmp.wrote_eof() {
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
