use brotli::BrotliResult;

pub enum ErrMsg {
    BrotliCompressStreamFail,
    BrotliDecompressFail,
    BrotliEncodeStreamNeedsOutputWithoutFlush,
    BrotliFlushStreamNeedsInput,
}

pub enum LeptonEncodeResult {
    Failure(ErrMsg),
    Success,
    NeedsMoreInput,
    NeedsMoreOutput,
}

impl From<BrotliResult> for LeptonEncodeResult {
    fn from(result: BrotliResult) -> Self {
        match result {
            BrotliResult::ResultSuccess => LeptonEncodeResult::Success,
            BrotliResult::NeedsMoreInput => LeptonEncodeResult::NeedsMoreInput,
            BrotliResult::NeedsMoreOutput => LeptonEncodeResult::NeedsMoreOutput,
            BrotliResult::ResultFailure => LeptonEncodeResult::Failure(ErrMsg::BrotliDecompressFail),
        }
    }
}

pub enum LeptonFlushResult {
    Failure(ErrMsg),
    Success,
    NeedsMoreOutput,
}

pub trait Compressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonEncodeResult;
    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult;
}

pub trait Decompressor {
    fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonEncodeResult;
}
