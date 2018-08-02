use brotli::BrotliResult;
use jpeg::JpegError;
use secondary_header::Marker;

pub type SimpleResult<T> = Result<(), T>;

#[derive(Clone, Debug, PartialEq)]
pub enum ErrMsg {
    BrotliCompressStreamFail,
    BrotliDecompressStreamFail,
    BrotliEncodeNeedsOutputWithoutFlush,
    BrotliFlushNeedsInput,
    CodecKilled,
    HDRMissing,
    IncompletePrimaryHeader,
    IncompleteSecondaryHeaderMarker,
    IncompleteSecondaryHeaderSection(u8, Marker),
    IncompleteThreadSegment,
    InternalDecompressorExhausted,
    InvalidSecondaryHeaderMarker(u8, u8, u8),
    JpegDecodeFail(JpegError),
    PADMIssing,
    PrematureDecodeCompletion,
    PrematureSecondaryHeaderCompletion,
    UnreachableRawSize,
    WrongMagicNumber,
}

#[derive(PartialEq)]
pub enum LeptonOperationResult {
    Failure(ErrMsg),
    Success,
    NeedsMoreInput,
    NeedsMoreOutput,
}

impl From<BrotliResult> for LeptonOperationResult {
    fn from(result: BrotliResult) -> Self {
        match result {
            BrotliResult::ResultSuccess => LeptonOperationResult::Success,
            BrotliResult::NeedsMoreInput => LeptonOperationResult::NeedsMoreInput,
            BrotliResult::NeedsMoreOutput => LeptonOperationResult::NeedsMoreOutput,
            BrotliResult::ResultFailure => {
                LeptonOperationResult::Failure(ErrMsg::BrotliDecompressStreamFail)
            }
        }
    }
}

pub enum LeptonFlushResult {
    Failure(ErrMsg),
    Success,
    NeedsMoreOutput,
}

#[derive(PartialEq)]
pub enum CumulativeOperationResult {
    Finish,
    NeedsMoreInput,
}

pub trait Compressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult;
    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult;
}

pub trait Decompressor {
    fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult;
}
