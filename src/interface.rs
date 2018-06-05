use brotli::BrotliResult;

pub const LEPTON_VERSION: u8 = 3;
pub const HEADER_SIZE: usize = 28;
pub const MAGIC_NUMBER: [u8; 2] = [0xcf, 0x84];

#[derive(Copy, Clone, Debug)]
pub enum ErrMsg {
    BrotliCompressStreamFail,
    BrotliDecompressStreamFail,
    BrotliEncodeNeedsOutputWithoutFlush,
    BrotliFlushNeedsInput,
    HDRMissing,
    IncompletePrimaryHeader,
    IncompleteSecondaryHeaderMarker,
    IncompleteSecondaryHeaderSection(u8),
    InternalDecompressorExhausted,
    InvalidSecondaryHeaderMarker(u8, u8, u8),
    PADMIssing,
    PrimaryHeaderNotBuilt,
    SecondaryHeaderNotBuilt,
    WrongMagicNumber,
}

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
