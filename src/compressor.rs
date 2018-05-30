use alloc::Allocator;
use brotli;
use brotli::enc::encode::{BrotliEncoderCompressStream, BrotliEncoderCreateInstance,
                          BrotliEncoderIsFinished, BrotliEncoderOperation,
                          BrotliEncoderStateStruct};
use core::cmp::min;
use interface::{Compressor, ErrMsg, LeptonEncodeResult, LeptonFlushResult};
use resizable_buffer::ResizableByteBuffer;

pub struct LeptonCompressor<
    AllocU8: Allocator<u8>,
    AllocU16: Allocator<u16>,
    AllocU32: Allocator<u32>,
    AllocI32: Allocator<i32>,
    AllocCommand: Allocator<brotli::enc::command::Command>,
    AllocU64: Allocator<u64>,
    AllocF64: Allocator<brotli::enc::util::floatX>,
    AllocFV: Allocator<brotli::enc::vectorization::Mem256f>,
    AllocHL: Allocator<brotli::enc::histogram::HistogramLiteral>,
    AllocHC: Allocator<brotli::enc::histogram::HistogramCommand>,
    AllocHD: Allocator<brotli::enc::histogram::HistogramDistance>,
    AllocHP: Allocator<brotli::enc::cluster::HistogramPair>,
    AllocCT: Allocator<brotli::enc::histogram::ContextType>,
    AllocHT: Allocator<brotli::enc::entropy_encode::HuffmanTree>,
    AllocZN: Allocator<brotli::enc::ZopfliNode>,
> {
    brotli_encoder: BrotliEncoderStateStruct<AllocU8, AllocU16, AllocU32, AllocI32, AllocCommand>,
    brotli_data: ResizableByteBuffer<u8, AllocU8>,
    encoded_byte_offset: usize,
    m64: AllocU64,
    mf64: AllocF64,
    mfv: AllocFV,
    mhl: AllocHL,
    mhc: AllocHC,
    mhd: AllocHD,
    mhp: AllocHP,
    mct: AllocCT,
    mht: AllocHT,
    mzn: AllocZN,
}

impl<
        AllocU8: Allocator<u8>,
        AllocU16: Allocator<u16>,
        AllocU32: Allocator<u32>,
        AllocI32: Allocator<i32>,
        AllocCommand: Allocator<brotli::enc::command::Command>,
        AllocU64: Allocator<u64>,
        AllocF64: Allocator<brotli::enc::util::floatX>,
        AllocFV: Allocator<brotli::enc::vectorization::Mem256f>,
        AllocHL: Allocator<brotli::enc::histogram::HistogramLiteral>,
        AllocHC: Allocator<brotli::enc::histogram::HistogramCommand>,
        AllocHD: Allocator<brotli::enc::histogram::HistogramDistance>,
        AllocHP: Allocator<brotli::enc::cluster::HistogramPair>,
        AllocCT: Allocator<brotli::enc::histogram::ContextType>,
        AllocHT: Allocator<brotli::enc::entropy_encode::HuffmanTree>,
        AllocZN: Allocator<brotli::enc::ZopfliNode>,
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
        m8: AllocU8,
        m16: AllocU16,
        m32: AllocU32,
        mi32: AllocI32,
        mc: AllocCommand,
        m64: AllocU64,
        mf64: AllocF64,
        mfv: AllocFV,
        mhl: AllocHL,
        mhc: AllocHC,
        mhd: AllocHD,
        mhp: AllocHP,
        mct: AllocCT,
        mht: AllocHT,
        mzn: AllocZN,
    ) -> Self {
        LeptonCompressor {
            brotli_encoder: BrotliEncoderCreateInstance(m8, m16, mi32, m32, mc),
            brotli_data: ResizableByteBuffer::<u8, AllocU8>::new(),
            encoded_byte_offset: 0,
            m64,
            mf64,
            mfv,
            mhl,
            mhc,
            mhd,
            mhp,
            mct,
            mht,
            mzn,
        }
    }
    fn internal_encode(
        &mut self,
        op: BrotliEncoderOperation,
        input: &[u8],
        input_offset: &mut usize,
        is_end: bool,
    ) -> LeptonEncodeResult {
        let mut nothing: Option<usize> = None;
        let mut available_in = input.len() - *input_offset;
        if available_in == 0 && BrotliEncoderIsFinished(&mut self.brotli_encoder) != 0 {
            return LeptonEncodeResult::Success;
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
                &mut self.m64,
                &mut self.mf64,
                &mut self.mfv,
                &mut self.mhl,
                &mut self.mhc,
                &mut self.mhd,
                &mut self.mhp,
                &mut self.mct,
                &mut self.mht,
                &mut self.mzn,
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
                return LeptonEncodeResult::Failure(ErrMsg::BrotliCompressStreamFail);
            }
        }
        self.brotli_data.commit_next_buffer(brotli_out_offset);
        // TODO: The following block may never get hit
        /****************/
        if available_out != 0 && available_in == 0
            && BrotliEncoderIsFinished(&mut self.brotli_encoder) == 0
        {
            return LeptonEncodeResult::NeedsMoreInput;
        }
        /****************/
        if is_end {
            if BrotliEncoderIsFinished(&mut self.brotli_encoder) == 0 {
                LeptonEncodeResult::NeedsMoreOutput
            } else {
                LeptonEncodeResult::Success
            }
        } else {
            LeptonEncodeResult::NeedsMoreInput
        }
    }
}

impl<
        AllocU8: Allocator<u8>,
        AllocU16: Allocator<u16>,
        AllocU32: Allocator<u32>,
        AllocI32: Allocator<i32>,
        AllocCommand: Allocator<brotli::enc::command::Command>,
        AllocU64: Allocator<u64>,
        AllocF64: Allocator<brotli::enc::util::floatX>,
        AllocFV: Allocator<brotli::enc::vectorization::Mem256f>,
        AllocHL: Allocator<brotli::enc::histogram::HistogramLiteral>,
        AllocHC: Allocator<brotli::enc::histogram::HistogramCommand>,
        AllocHD: Allocator<brotli::enc::histogram::HistogramDistance>,
        AllocHP: Allocator<brotli::enc::cluster::HistogramPair>,
        AllocCT: Allocator<brotli::enc::histogram::ContextType>,
        AllocHT: Allocator<brotli::enc::entropy_encode::HuffmanTree>,
        AllocZN: Allocator<brotli::enc::ZopfliNode>,
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
    ) -> LeptonEncodeResult {
        match self.internal_encode(
            BrotliEncoderOperation::BROTLI_OPERATION_PROCESS,
            input,
            input_offset,
            false,
        ) {
            LeptonEncodeResult::NeedsMoreOutput => {
                LeptonEncodeResult::Failure(ErrMsg::BrotliEncodeStreamNeedsOutputWithoutFlush)
            }
            LeptonEncodeResult::Failure(m) => LeptonEncodeResult::Failure(m),
            LeptonEncodeResult::Success | LeptonEncodeResult::NeedsMoreInput => {
                LeptonEncodeResult::NeedsMoreInput
            }
        }
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        let mut zero = 0usize;
        loop {
            match self.internal_encode(
                BrotliEncoderOperation::BROTLI_OPERATION_FINISH,
                &[],
                &mut zero,
                true,
            ) {
                LeptonEncodeResult::Failure(m) => return LeptonFlushResult::Failure(m),
                LeptonEncodeResult::Success => break,
                LeptonEncodeResult::NeedsMoreOutput => {}
                LeptonEncodeResult::NeedsMoreInput => {
                    return LeptonFlushResult::Failure(ErrMsg::BrotliFlushStreamNeedsInput)
                }
            }
        }
        flush_buffer(
            output,
            output_offset,
            &mut self.brotli_data,
            &mut self.encoded_byte_offset,
        )
    }
}

fn flush_buffer<T: Sized + Default + Clone, AllocT: Allocator<T>>(
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
