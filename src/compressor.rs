use alloc::Allocator;
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
use brotli;
use core::cmp::min;
use interface::{Compressor, ErrMsg, LeptonOperationResult, LeptonFlushResult};
use resizable_buffer::ResizableByteBuffer;

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
    brotli_encoder: BrotliEncoderStateStruct<AllocU8, AllocU16, AllocU32, AllocI32, AllocCommand>,
    brotli_data: ResizableByteBuffer<u8, AllocU8>,
    encoded_byte_offset: usize,
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
            brotli_encoder: BrotliEncoderCreateInstance(alloc_u8, alloc_u16, alloc_i32, alloc_u32, alloc_cmd),
            brotli_data: ResizableByteBuffer::<u8, AllocU8>::new(),
            encoded_byte_offset: 0,
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
    fn encode_internal(
        &mut self,
        op: BrotliEncoderOperation,
        input: &[u8],
        input_offset: &mut usize,
        is_end: bool,
    ) -> LeptonOperationResult {
        let mut nothing: Option<usize> = None;
        let mut available_in = input.len() - *input_offset;
        if available_in == 0 && BrotliEncoderIsFinished(&mut self.brotli_encoder) != 0 {
            return LeptonOperationResult::Success;
        }
        let mut available_out;
        let mut brotli_out_offset = 0usize;
        {
            let brotli_buffer = self.brotli_data
                .checkout_next_buffer(&mut self.brotli_encoder.m8, Some(256));
            available_out = brotli_buffer.len();
            let mut nop_callback =
                |_data: &[brotli::interface::Command<brotli::InputReference>]| ();
            if BrotliEncoderCompressStream(
                &mut self.brotli_encoder,
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
        self.brotli_data.commit_next_buffer(brotli_out_offset);
        // TODO: The following block may never get hit
        /****************/
        if available_out != 0 && available_in == 0
            && BrotliEncoderIsFinished(&mut self.brotli_encoder) == 0
        {
            return LeptonOperationResult::NeedsMoreInput;
        }
        /****************/
        if is_end {
            if BrotliEncoderIsFinished(&mut self.brotli_encoder) == 0 {
                LeptonOperationResult::NeedsMoreOutput
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
                LeptonOperationResult::Failure(ErrMsg::BrotliEncodeStreamNeedsOutputWithoutFlush)
            }
            LeptonOperationResult::Failure(m) => LeptonOperationResult::Failure(m),
            LeptonOperationResult::Success | LeptonOperationResult::NeedsMoreInput => {
                LeptonOperationResult::NeedsMoreInput
            }
        }
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        let mut zero = 0usize;
        loop {
            match self.encode_internal(
                BrotliEncoderOperation::BROTLI_OPERATION_FINISH,
                &[],
                &mut zero,
                true,
            ) {
                LeptonOperationResult::Failure(m) => return LeptonFlushResult::Failure(m),
                LeptonOperationResult::Success => break,
                LeptonOperationResult::NeedsMoreOutput => (),
                LeptonOperationResult::NeedsMoreInput => {
                    return LeptonFlushResult::Failure(ErrMsg::BrotliFlushStreamNeedsInput)
                }
            }
        }
        flush_resizable_buffer(
            output,
            output_offset,
            &mut self.brotli_data,
            &mut self.encoded_byte_offset,
        )
    }
}

fn flush_resizable_buffer<T: Sized + Default + Clone, AllocT: Allocator<T>>(
    output: &mut [T],
    output_offset: &mut usize,
    src_buffer: &ResizableByteBuffer<T, AllocT>,
    buffer_offset: &mut usize,
) -> LeptonFlushResult {
    let destination = output.split_at_mut(*output_offset).1;
    let src = src_buffer.slice().split_at(*buffer_offset).1;
    let copy_len = min(src.len(), destination.len());
    destination
        .split_at_mut(copy_len)
        .0
        .clone_from_slice(src.split_at(copy_len).0);
    *output_offset += copy_len;
    *buffer_offset += copy_len;
    if *buffer_offset == src_buffer.len() {
        return LeptonFlushResult::Success;
    }
    LeptonFlushResult::NeedsMoreOutput
}
