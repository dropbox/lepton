use alloc::HeapAlloc;
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
use util::flush_resizable_buffer;

pub struct BrotliEncoder {
    encoder: BrotliEncoderStateStruct<
        HeapAlloc<u8>,
        HeapAlloc<u16>,
        HeapAlloc<u32>,
        HeapAlloc<i32>,
        HeapAlloc<Command>,
    >,
    data: ResizableByteBuffer<u8, HeapAlloc<u8>>,
    written_end: usize,
    alloc_u64: HeapAlloc<u64>,
    alloc_f64: HeapAlloc<floatX>,
    alloc_float_vec: HeapAlloc<Mem256f>,
    alloc_hist_cmd: HeapAlloc<HistogramCommand>,
    alloc_hist_dist: HeapAlloc<HistogramDistance>,
    alloc_hist_literal: HeapAlloc<HistogramLiteral>,
    alloc_hist_pair: HeapAlloc<HistogramPair>,
    alloc_context_type: HeapAlloc<ContextType>,
    alloc_huffman_tree: HeapAlloc<HuffmanTree>,
    alloc_zopfli_node: HeapAlloc<ZopfliNode>,
}

impl BrotliEncoder {
    pub fn new() -> Self {
        BrotliEncoder {
            encoder: BrotliEncoderCreateInstance(
                HeapAlloc::new(0),
                HeapAlloc::new(0),
                HeapAlloc::new(0),
                HeapAlloc::new(0),
                HeapAlloc::new(Command::default()),
            ),
            data: ResizableByteBuffer::<u8, HeapAlloc<u8>>::new(),
            written_end: 0,
            alloc_u64: HeapAlloc::new(0),
            alloc_f64: HeapAlloc::new(floatX::default()),
            alloc_float_vec: HeapAlloc::new(Mem256f::default()),
            alloc_hist_cmd: HeapAlloc::new(HistogramCommand::default()),
            alloc_hist_dist: HeapAlloc::new(HistogramDistance::default()),
            alloc_hist_literal: HeapAlloc::new(HistogramLiteral::default()),
            alloc_hist_pair: HeapAlloc::new(HistogramPair::default()),
            alloc_context_type: HeapAlloc::new(ContextType::default()),
            alloc_huffman_tree: HeapAlloc::new(HuffmanTree::default()),
            alloc_zopfli_node: HeapAlloc::new(ZopfliNode::default()),
        }
    }

    pub fn finish(&mut self) -> Result<usize, ErrMsg> {
        loop {
            match self.encode_internal(
                BrotliEncoderOperation::BROTLI_OPERATION_FINISH,
                &[],
                &mut 0usize,
                true,
            ) {
                LeptonOperationResult::Failure(msg) => return Err(msg),
                LeptonOperationResult::Success => return Ok(self.data.len()),
                LeptonOperationResult::NeedsMoreOutput => (),
                LeptonOperationResult::NeedsMoreInput => return Err(ErrMsg::BrotliFlushNeedsInput),
            }
        }
    }

    pub fn encode_all(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        while *input_offset < input.len() {
            match self.encode(input, input_offset, output, output_offset) {
                LeptonOperationResult::Failure(msg) => return LeptonOperationResult::Failure(msg),
                _ => (),
            }
        }
        LeptonOperationResult::NeedsMoreInput
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

impl Compressor for BrotliEncoder {
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
        flush_resizable_buffer(output, output_offset, &mut self.data, &mut self.written_end)
    }
}
