use super::primary_header_parser::PrimaryHeaderParser;
use super::secondary_header_parser::SecondaryHeaderParser;
use interface::{Decompressor, ErrMsg, LeptonOperationResult};
use primary_header::PrimaryHeader;
use secondary_header::SecondaryHeader;

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
                InternalDecompressor::SecondaryHeader(Some(SecondaryHeaderParser::new(
                    primary_header.secondary_hdr_size,
                )))
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
        let old_output_offset = *output_offset;
        loop {
            let result = match self.decompressor {
                InternalDecompressor::PrimaryHeader(ref mut parser) => {
                    match parser.parse(input, input_offset) {
                        LeptonOperationResult::Success => match parser.build_header() {
                            Ok(header) => {
                                self.primary_header = Some(header);
                                LeptonOperationResult::Success
                            }
                            Err(e) => LeptonOperationResult::Failure(e),
                        },
                        other => other,
                    }
                }
                InternalDecompressor::SecondaryHeader(ref mut parser) => {
                    let result = parser.as_mut().unwrap().decode(input, input_offset, output, output_offset);
                    if result == LeptonOperationResult::Success {
                        self.secondary_header = Some(parser.take().unwrap().take_header());
                    }
                    result
                }
                // TODO: Connect JPEG decompressor
                InternalDecompressor::JPEG => LeptonOperationResult::Success,
            };
            self.total_out += *output_offset - old_output_offset;
            if let Some(primary_header) = self.primary_header {
                if self.total_out >= primary_header.raw_size {
                    *output_offset -= self.total_out - primary_header.raw_size;
                    return LeptonOperationResult::Success;
                }
            }
            match result {
                LeptonOperationResult::Success => match self.next_internal_decompressor() {
                    Ok(()) => (),
                    Err(e) => return LeptonOperationResult::Failure(e),
                },
                other => return other,
            }
        }
    }
}

enum InternalDecompressor {
    PrimaryHeader(PrimaryHeaderParser),
    SecondaryHeader(Option<SecondaryHeaderParser>),
    JPEG,
}
