pub enum ErrMsg {
    BrotliCompressStreamFail(u8, u8),
    BrotliEncodeStreamNeedsOutputWithoutFlush,
    BrotliFlushStreamNeedsInput,
}

pub enum LeptonEncodeResult {
    Failure(ErrMsg),
    Success,
    NeedsMoreInput,
    NeedsMoreOutput,
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
