use super::lepton_decoder::LeptonDecoder;
use super::primary_header_parser::PrimaryHeaderParser;
use super::secondary_header_parser::SecondaryHeaderParser;
use interface::{Decompressor, ErrMsg, LeptonOperationResult, SimpleResult};
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

    fn next_internal_decompressor(&mut self) -> SimpleResult<ErrMsg> {
        self.decompressor = match self.decompressor {
            InternalDecompressor::PrimaryHeader(_) => {
                let primary_header = self.primary_header.as_ref().unwrap();
                InternalDecompressor::SecondaryHeader(Some(SecondaryHeaderParser::new(
                    primary_header.secondary_hdr_size,
                    !primary_header.skip_hdr,
                )))
            }
            InternalDecompressor::SecondaryHeader(_) => {
                InternalDecompressor::CMP(LeptonDecoder::new(
                    self.secondary_header.take().unwrap(),
                    self.primary_header.as_ref().unwrap().raw_size - self.total_out,
                ))
            }
            InternalDecompressor::CMP(_) => return Err(ErrMsg::InternalDecompressorExhausted),
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
        loop {
            let old_output_offset = *output_offset;
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
                    let result =
                        parser
                            .as_mut()
                            .unwrap()
                            .decode(input, input_offset, output, output_offset);
                    if result == LeptonOperationResult::Success {
                        self.secondary_header = Some(parser.take().unwrap().take_header());
                    }
                    result
                }
                InternalDecompressor::CMP(ref mut decoder) => {
                    decoder.decode(input, input_offset, output, output_offset)
                }
            };
            self.total_out += *output_offset - old_output_offset;
            if let Some(primary_header) = self.primary_header {
                if self.total_out >= primary_header.raw_size {
                    *output_offset -= self.total_out - primary_header.raw_size;
                    self.total_out = primary_header.raw_size;
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
    CMP(LeptonDecoder),
}
