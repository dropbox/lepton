use super::brotli_encoder::BrotliEncoder;
use alloc::Allocator;
use brotli::enc::cluster::HistogramPair;
use brotli::enc::command::Command;
use brotli::enc::entropy_encode::HuffmanTree;
use brotli::enc::histogram::{ContextType, HistogramCommand, HistogramDistance, HistogramLiteral};
use brotli::enc::util::floatX;
use brotli::enc::vectorization::Mem256f;
use brotli::enc::ZopfliNode;
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult, HEADER_SIZE, LEPTON_VERSION,
                MAGIC_NUMBER};
use util::u32_to_u8_array;

enum EncoderType {
    Brotli,
    Lepton,
}

pub struct LeptonCompressor<
    AllocU8: Allocator<u8>,
    AllocU16: Allocator<u16>,
    AllocU32: Allocator<u32>,
    AllocI32: Allocator<i32>,
    AllocCommand: Allocator<Command>,
    AllocU64: Allocator<u64>,
    AllocF64: Allocator<floatX>,
    AllocFV: Allocator<Mem256f>,
    AllocHL: Allocator<HistogramLiteral>,
    AllocHC: Allocator<HistogramCommand>,
    AllocHD: Allocator<HistogramDistance>,
    AllocHP: Allocator<HistogramPair>,
    AllocCT: Allocator<ContextType>,
    AllocHT: Allocator<HuffmanTree>,
    AllocZN: Allocator<ZopfliNode>,
> {
    brotli_encoder: BrotliEncoder<
        AllocU8,
        AllocU16,
        AllocU32,
        AllocI32,
        AllocCommand,
        AllocU64,
        AllocF64,
        AllocFV,
        AllocHL,
        AllocHC,
        AllocHD,
        AllocHP,
        AllocCT,
        AllocHT,
        AllocZN,
    >,
    active_encoder: EncoderType,
    total_in: usize,
    header: Option<[u8; HEADER_SIZE]>,
    header_written: usize,
}

impl<
        AllocU8: Allocator<u8>,
        AllocU16: Allocator<u16>,
        AllocU32: Allocator<u32>,
        AllocI32: Allocator<i32>,
        AllocCommand: Allocator<Command>,
        AllocU64: Allocator<u64>,
        AllocF64: Allocator<floatX>,
        AllocFV: Allocator<Mem256f>,
        AllocHL: Allocator<HistogramLiteral>,
        AllocHC: Allocator<HistogramCommand>,
        AllocHD: Allocator<HistogramDistance>,
        AllocHP: Allocator<HistogramPair>,
        AllocCT: Allocator<ContextType>,
        AllocHT: Allocator<HuffmanTree>,
        AllocZN: Allocator<ZopfliNode>,
    >
    LeptonCompressor<
        AllocU8,
        AllocU16,
        AllocU32,
        AllocI32,
        AllocCommand,
        AllocU64,
        AllocF64,
        AllocFV,
        AllocHL,
        AllocHC,
        AllocHD,
        AllocHP,
        AllocCT,
        AllocHT,
        AllocZN,
    >
{
    pub fn new(
        alloc_u8: AllocU8,
        alloc_u16: AllocU16,
        alloc_u32: AllocU32,
        alloc_i32: AllocI32,
        alloc_cmd: AllocCommand,
        alloc_u64: AllocU64,
        alloc_f64: AllocF64,
        alloc_float_vec: AllocFV,
        alloc_hist_literal: AllocHL,
        alloc_hist_cmd: AllocHC,
        alloc_hist_dist: AllocHD,
        alloc_hist_pair: AllocHP,
        alloc_context_type: AllocCT,
        alloc_huffman_tree: AllocHT,
        alloc_zopfli_node: AllocZN,
    ) -> Self {
        LeptonCompressor {
            brotli_encoder: BrotliEncoder::new(
                alloc_u8,
                alloc_u16,
                alloc_u32,
                alloc_i32,
                alloc_cmd,
                alloc_u64,
                alloc_f64,
                alloc_float_vec,
                alloc_hist_literal,
                alloc_hist_cmd,
                alloc_hist_dist,
                alloc_hist_pair,
                alloc_context_type,
                alloc_huffman_tree,
                alloc_zopfli_node,
            ),
            active_encoder: EncoderType::Brotli,
            total_in: 0,
            header: None,
            header_written: 0,
        }
    }
}

impl<
        AllocU8: Allocator<u8>,
        AllocU16: Allocator<u16>,
        AllocU32: Allocator<u32>,
        AllocI32: Allocator<i32>,
        AllocCommand: Allocator<Command>,
        AllocU64: Allocator<u64>,
        AllocF64: Allocator<floatX>,
        AllocFV: Allocator<Mem256f>,
        AllocHL: Allocator<HistogramLiteral>,
        AllocHC: Allocator<HistogramCommand>,
        AllocHD: Allocator<HistogramDistance>,
        AllocHP: Allocator<HistogramPair>,
        AllocCT: Allocator<ContextType>,
        AllocHT: Allocator<HuffmanTree>,
        AllocZN: Allocator<ZopfliNode>,
    > Compressor
    for LeptonCompressor<
        AllocU8,
        AllocU16,
        AllocU32,
        AllocI32,
        AllocCommand,
        AllocU64,
        AllocF64,
        AllocFV,
        AllocHL,
        AllocHC,
        AllocHD,
        AllocHP,
        AllocCT,
        AllocHT,
        AllocZN,
    >
{
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
    header[4..8].clone_from_slice(&u32_to_u8_array(n_threads));
    header[8..20].clone_from_slice(git_hash);
    header[20..24].clone_from_slice(&u32_to_u8_array(&(*raw_size as u32)));
    header[24..].clone_from_slice(&u32_to_u8_array(&(*secondary_hdr_size as u32)));
    header
}
