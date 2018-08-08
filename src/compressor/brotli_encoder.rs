use alloc::HeapAlloc;
use brotli;
use brotli::enc::cluster::HistogramPair;
use brotli::enc::command::Command;
use brotli::enc::encode::{
    BrotliEncoderCompressStream, BrotliEncoderCreateInstance, BrotliEncoderIsFinished,
    BrotliEncoderOperation, BrotliEncoderStateStruct,
};
use brotli::enc::entropy_encode::HuffmanTree;
use brotli::enc::histogram::{ContextType, HistogramCommand, HistogramDistance, HistogramLiteral};
use brotli::enc::pdf::PDF;
use brotli::enc::util::floatX;
use brotli::enc::vectorization::Mem256f;
use brotli::enc::StaticCommand;
use brotli::enc::ZopfliNode;
use brotli::interface::PredictionModeContextMap;
use brotli::{InputReferenceMut, SliceOffset};
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
    encoded_data: ResizableByteBuffer<u8, HeapAlloc<u8>>,
    encoded_data_written: usize,
    done: bool,
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
            encoded_data: ResizableByteBuffer::<u8, HeapAlloc<u8>>::new(),
            encoded_data_written: 0,
            done: false,
        }
    }

    pub fn finish_encode(&mut self) -> Result<usize, ErrMsg> {
        if self.done {
            return Ok(self.encoded_data.len());
        }
        loop {
            match self.encode_internal(
                BrotliEncoderOperation::BROTLI_OPERATION_FINISH,
                &[],
                &mut 0usize,
                true,
            ) {
                LeptonOperationResult::Failure(msg) => return Err(msg),
                LeptonOperationResult::Success => {
                    self.done = true;
                    return Ok(self.encoded_data.len());
                }
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
        let mut available_in = input.len() - *input_offset;
        if available_in == 0 && BrotliEncoderIsFinished(&mut self.encoder) != 0 {
            return LeptonOperationResult::Success;
        }
        let mut available_out;
        let mut brotli_out_offset = 0usize;
        {
            let brotli_buffer = self
                .encoded_data
                .checkout_next_buffer(&mut self.encoder.m8, Some(256));
            available_out = brotli_buffer.len();
            let mut nop_callback =
                |_pm: &mut PredictionModeContextMap<InputReferenceMut>,
                 _command: &mut [brotli::interface::Command<SliceOffset>],
                 _input_pair: brotli::InputPair,
                 _alloc_float_vec: &mut HeapAlloc<Mem256f>,
                 _alloc_pdf: &mut HeapAlloc<PDF>,
                 _alloc_static_command: &mut HeapAlloc<StaticCommand>| ();
            if BrotliEncoderCompressStream(
                &mut self.encoder,
                &mut HeapAlloc::new(0),
                &mut HeapAlloc::new(floatX::default()),
                &mut HeapAlloc::new(Mem256f::default()),
                &mut HeapAlloc::new(PDF::default()),
                &mut HeapAlloc::new(StaticCommand::default()),
                &mut HeapAlloc::new(HistogramLiteral::default()),
                &mut HeapAlloc::new(HistogramCommand::default()),
                &mut HeapAlloc::new(HistogramDistance::default()),
                &mut HeapAlloc::new(HistogramPair::default()),
                &mut HeapAlloc::new(ContextType::default()),
                &mut HeapAlloc::new(HuffmanTree::default()),
                &mut HeapAlloc::new(ZopfliNode::default()),
                op,
                &mut available_in,
                input,
                input_offset,
                &mut available_out,
                brotli_buffer,
                &mut brotli_out_offset,
                &mut None,
                &mut nop_callback,
            ) <= 0
            {
                return LeptonOperationResult::Failure(ErrMsg::BrotliCompressStreamFail);
            }
        }
        self.encoded_data.commit_next_buffer(brotli_out_offset);
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
        match self.finish_encode() {
            Ok(_) => (),
            Err(e) => return LeptonFlushResult::Failure(e),
        }
        if self.encoded_data_written == self.encoded_data.len() {
            return LeptonFlushResult::Success;
        }
        flush_resizable_buffer(
            output,
            output_offset,
            &mut self.encoded_data,
            &mut self.encoded_data_written,
        )
    }
}
