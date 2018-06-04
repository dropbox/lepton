use core::mem;

use alloc::Allocator;
use brotli::HuffmanCode;

use super::primary_header::{PrimaryHeader, PrimaryHeaderParser};
use super::secondary_header::{SecondaryHeader, SecondaryHeaderParser};
use interface::{Decompressor, ErrMsg, LeptonOperationResult};
use resizable_buffer::ResizableByteBuffer;

pub struct LeptonDecompressor<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    decompressor: InternalDecompressor<AllocU8, AllocU32, AllocHC>,
    header: Option<PrimaryHeader>,
    suffix: ResizableByteBuffer<u8, AllocU8>,
    alloc_u8: Option<AllocU8>,
    alloc_u32: Option<AllocU32>,
    alloc_huffman_code: Option<AllocHC>,
    total_out: usize,
}

impl<AllocU8: Allocator<u8>, AllocU32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>>
    LeptonDecompressor<AllocU8, AllocU32, AllocHC>
{
    pub fn new(alloc_u8: AllocU8, alloc_u32: AllocU32, alloc_huffman_code: AllocHC) -> Self {
        LeptonDecompressor {
            decompressor: InternalDecompressor::PrimaryHeader(PrimaryHeaderParser::new()),
            header: None,
            suffix: ResizableByteBuffer::<u8, AllocU8>::new(),
            alloc_u8: Some(alloc_u8),
            alloc_u32: Some(alloc_u32),
            alloc_huffman_code: Some(alloc_huffman_code),
            total_out: 0,
        }
    }

    fn next_internal_decompressor(
        &mut self,
    ) -> Option<InternalDecompressor<AllocU8, AllocU32, AllocHC>> {
        match self.decompressor {
            InternalDecompressor::PrimaryHeader(_) => {
                let alloc_u8 = match mem::replace(&mut self.alloc_u8, None) {
                    Some(alloc_u8) => alloc_u8,
                    None => return None,
                };
                let alloc_u32 = match mem::replace(&mut self.alloc_u32, None) {
                    Some(alloc_u32) => alloc_u32,
                    None => return None,
                };
                let alloc_huffman_code = match mem::replace(&mut self.alloc_huffman_code, None) {
                    Some(alloc_huffman_code) => alloc_huffman_code,
                    None => return None,
                };
                let primary_header = match self.header {
                    Some(header) => header,
                    None => return None,
                };
                Some(InternalDecompressor::SecondaryHeader(
                    SecondaryHeaderParser::new(
                        primary_header.secondary_hdr_size,
                        alloc_u8,
                        alloc_u32,
                        alloc_huffman_code,
                    ),
                ))
            }
            InternalDecompressor::SecondaryHeader(_) => Some(InternalDecompressor::JPEG),
            InternalDecompressor::JPEG => None,
        }
    }
}

impl<AllocU8: Allocator<u8>, AllocU32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>> Decompressor
    for LeptonDecompressor<AllocU8, AllocU32, AllocHC>
{
    fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        let mut result = LeptonOperationResult::Success;
        while *input_offset < input.len() && *output_offset < output.len() {
            if let Some(header) = self.header {
                if self.total_out >= header.raw_size {
                    // TODO: remove extra output
                    return LeptonOperationResult::Success;
                }
            }
            // TODO: Update total_out
            match self.decompressor {
                InternalDecompressor::PrimaryHeader(ref mut parser) => {
                    match parser.parse(input, input_offset) {
                        LeptonOperationResult::Success => {
                            self.header = match parser.build_header() {
                                Ok(header) => Some(header),
                                Err(msg) => return LeptonOperationResult::Failure(msg),
                            }
                        }
                        other => return other,
                    }
                }
                InternalDecompressor::SecondaryHeader(ref mut parser) => {
                    match parser.decode(input, input_offset, output, output_offset) {
                        LeptonOperationResult::Success => {
                            // TODO: get out suffix data
                        }
                        other => return other,
                    }
                }
                InternalDecompressor::JPEG => return LeptonOperationResult::Success,
            }
            self.decompressor = match self.next_internal_decompressor() {
                Some(internal_decompressor) => internal_decompressor,
                None => {
                    return LeptonOperationResult::Failure(ErrMsg::InternalDecompressorSwitchFail)
                }
            };
        }
        if *output_offset < output.len() && *input_offset == input.len() {
            LeptonOperationResult::NeedsMoreInput
        } else {
            LeptonOperationResult::NeedsMoreOutput
        }
    }
}

enum InternalDecompressor<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    PrimaryHeader(PrimaryHeaderParser),
    SecondaryHeader(SecondaryHeaderParser<AllocU8, AllocU32, AllocHC>),
    JPEG,
}
