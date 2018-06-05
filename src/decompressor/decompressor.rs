use super::primary_header::{PrimaryHeader, PrimaryHeaderParser};
use super::secondary_header::{SecondaryHeader, SecondaryHeaderParser};
use interface::{Decompressor, ErrMsg, LeptonOperationResult};

pub struct LeptonDecompressor {
    decompressor: InternalDecompressor,
    primary_header: Option<PrimaryHeader>,
    secondary_header: Option<SecondaryHeader>,
    total_out: usize,
}

impl LeptonDecompressor {
    pub fn new() -> Self {
        LeptonDecompressor {
            decompressor: InternalDecompressor::PrimaryHeader(PrimaryHeaderParser::new()),
            primary_header: None,
            secondary_header: None,
            total_out: 0,
        }
    }

    fn next_internal_decompressor(&mut self) -> Result<(), ErrMsg> {
        self.decompressor = match self.decompressor {
            InternalDecompressor::PrimaryHeader(_) => {
                let primary_header = match self.primary_header {
                    Some(header) => header,
                    None => return Err(ErrMsg::PrimaryHeaderNotBuilt),
                };
                InternalDecompressor::SecondaryHeader(SecondaryHeaderParser::new(
                    primary_header.secondary_hdr_size,
                ))
            }
            InternalDecompressor::SecondaryHeader(_) => InternalDecompressor::JPEG,
            InternalDecompressor::JPEG => return Err(ErrMsg::InternalDecompressorExhausted),
        };
        Ok(())
    }
}

impl Decompressor for LeptonDecompressor {
    fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        while *input_offset < input.len() && *output_offset < output.len() {
            if let Some(primary_header) = self.primary_header {
                if self.total_out >= primary_header.raw_size {
                    *output_offset -= self.total_out - primary_header.raw_size;
                    return LeptonOperationResult::Success;
                }
            }
            self.total_out -= *output_offset;
            // self.total_out += *output_offset; must be called exactly once
            // before returning or comparing to raw_size
            match self.decompressor {
                InternalDecompressor::PrimaryHeader(ref mut parser) => {
                    match parser.parse(input, input_offset) {
                        LeptonOperationResult::Success => {
                            self.primary_header = match parser.build_header() {
                                Ok(header) => Some(header),
                                Err(e) => return LeptonOperationResult::Failure(e),
                            }
                        }
                        other => {
                            self.total_out += *output_offset;
                            return other;
                        }
                    }
                }
                InternalDecompressor::SecondaryHeader(ref mut parser) => {
                    match parser.decode(input, input_offset, output, output_offset) {
                        LeptonOperationResult::Success => {
                            self.secondary_header = match parser.extract_header() {
                                Ok(secondary_header) => Some(secondary_header),
                                Err(e) => return LeptonOperationResult::Failure(e),
                            };
                            // TODO: parse the simantics of secondary header
                            // May not need to keep the whole header
                        }
                        other => {
                            self.total_out += *output_offset;
                            return other;
                        }
                    }
                }
                // TODO: Connect JPEG decompressor
                InternalDecompressor::JPEG => {
                    self.total_out += *output_offset;
                    return LeptonOperationResult::Success;
                }
            }
            match self.next_internal_decompressor() {
                Ok(()) => (),
                Err(e) => {
                    self.total_out += *output_offset;
                    return LeptonOperationResult::Failure(e);
                }
            };
            self.total_out += *output_offset;
        }
        if let Some(primary_header) = self.primary_header {
            if self.total_out >= primary_header.raw_size {
                *output_offset -= self.total_out - primary_header.raw_size;
                return LeptonOperationResult::Success;
            }
        }
        if *output_offset < output.len() && *input_offset == input.len() {
            LeptonOperationResult::NeedsMoreInput
        } else {
            LeptonOperationResult::NeedsMoreOutput
        }
    }
}

enum InternalDecompressor {
    PrimaryHeader(PrimaryHeaderParser),
    SecondaryHeader(SecondaryHeaderParser),
    JPEG,
}
