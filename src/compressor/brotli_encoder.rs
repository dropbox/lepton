use super::flush_resizable_buffer;
use alloc::Allocator;
use brotli;
use brotli::enc::cluster::HistogramPair;
use brotli::enc::command::Command;
use brotli::enc::encode::{BrotliEncoderCompressStream, BrotliEncoderCreateInstance,
                          BrotliEncoderIsFinished, BrotliEncoderOperation,
                          BrotliEncoderStateStruct};
use brotli::enc::entropy_encode::HuffmanTree;
use brotli::enc::histogram::{ContextType, HistogramCommand, HistogramDistance, HistogramLiteral};
use brotli::enc::util::floatX;
use brotli::enc::vectorization::Mem256f;
use brotli::enc::ZopfliNode;
use interface::{Compressor, ErrMsg, LeptonFlushResult, LeptonOperationResult};
use resizable_buffer::ResizableByteBuffer;

pub struct BrotliEncoder<
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
    encoder: BrotliEncoderStateStruct<AllocU8, AllocU16, AllocU32, AllocI32, AllocCommand>,
    data: ResizableByteBuffer<u8, AllocU8>,
    written_end: usize,
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
    BrotliEncoder<
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
        BrotliEncoder {
            encoder: BrotliEncoderCreateInstance(
                alloc_u8, alloc_u16, alloc_i32, alloc_u32, alloc_cmd,
            ),
            data: ResizableByteBuffer::<u8, AllocU8>::new(),
            written_end: 0,
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
        }
    }

    pub fn size(&self) -> usize {
        self.data.len()
    }

    fn encode_internal(
        &mut self,
        op: BrotliEncoderOperation,
        input: &[u8],
        input_offset: &mut usize,
        is_end: bool,
    ) -> LeptonOperationResult {
        let mut nothing: Option<usize> = None;
        let mut available_in = input.len() - *input_offset;
        if available_in == 0 && BrotliEncoderIsFinished(&mut self.encoder) != 0 {
            return LeptonOperationResult::Success;
        }
        let mut available_out;
        let mut brotli_out_offset = 0usize;
        {
            let brotli_buffer = self.data
                .checkout_next_buffer(&mut self.encoder.m8, Some(256));
            available_out = brotli_buffer.len();
            let mut nop_callback =
                |_data: &[brotli::interface::Command<brotli::InputReference>]| ();
            if BrotliEncoderCompressStream(
                &mut self.encoder,
                &mut self.alloc_u64,
                &mut self.alloc_f64,
                &mut self.alloc_float_vec,
                &mut self.alloc_hist_literal,
                &mut self.alloc_hist_cmd,
                &mut self.alloc_hist_dist,
                &mut self.alloc_hist_pair,
                &mut self.alloc_context_type,
                &mut self.alloc_huffman_tree,
                &mut self.alloc_zopfli_node,
                op,
                &mut available_in,
                input,
                input_offset,
                &mut available_out,
                brotli_buffer,
                &mut brotli_out_offset,
                &mut nothing,
                &mut nop_callback,
            ) <= 0
            {
                return LeptonOperationResult::Failure(ErrMsg::BrotliCompressStreamFail);
            }
        }
        self.data.commit_next_buffer(brotli_out_offset);
        if is_end {
            if BrotliEncoderIsFinished(&mut self.encoder) == 0 {
                if available_out != 0 && available_in == 0 {
                    LeptonOperationResult::NeedsMoreInput
                } else {
                    LeptonOperationResult::NeedsMoreOutput
                }
            } else {
                LeptonOperationResult::Success
            }
        } else {
            LeptonOperationResult::NeedsMoreInput
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
    for BrotliEncoder<
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
        _output: &mut [u8],
        _output_offset: &mut usize,
    ) -> LeptonOperationResult {
        match self.encode_internal(
            BrotliEncoderOperation::BROTLI_OPERATION_PROCESS,
            input,
            input_offset,
            false,
        ) {
            LeptonOperationResult::NeedsMoreOutput => {
                LeptonOperationResult::Failure(ErrMsg::BrotliEncodeNeedsOutputWithoutFlush)
            }
            LeptonOperationResult::Failure(m) => LeptonOperationResult::Failure(m),
            LeptonOperationResult::Success | LeptonOperationResult::NeedsMoreInput => {
                LeptonOperationResult::NeedsMoreInput
            }
        }
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        loop {
            match self.encode_internal(
                BrotliEncoderOperation::BROTLI_OPERATION_FINISH,
                &[],
                &mut 0usize,
                true,
            ) {
                LeptonOperationResult::Failure(m) => return LeptonFlushResult::Failure(m),
                LeptonOperationResult::Success => break,
                LeptonOperationResult::NeedsMoreOutput => (),
                LeptonOperationResult::NeedsMoreInput => {
                    return LeptonFlushResult::Failure(ErrMsg::BrotliFlushNeedsInput)
                }
            }
        }
        flush_resizable_buffer(output, output_offset, &mut self.data, &mut self.written_end)
    }
}
