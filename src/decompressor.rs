use alloc::Allocator;
use brotli::{BrotliDecompressStream, BrotliState, HuffmanCode};
use interface::{Decompressor, LeptonOperationResult};

pub enum LeptonDecompressor<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    SecondaryHeader(SecondaryHeaderParser<AllocU8, AllocU32, AllocHC>),
}

impl<AllocU8: Allocator<u8>, AllocU32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>>
    LeptonDecompressor<AllocU8, AllocU32, AllocHC>
{
    pub fn new(alloc_u8: AllocU8, alloc_u32: AllocU32, alloc_huffman_code: AllocHC) -> Self {
        LeptonDecompressor::SecondaryHeader(SecondaryHeaderParser::new(alloc_u8, alloc_u32, alloc_huffman_code))
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
        match *self {
            LeptonDecompressor::SecondaryHeader(ref mut parser) => {
                parser.decode(input, input_offset, output, output_offset)
            }
        }
    }
}

pub struct SecondaryHeaderParser<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    brotli_decoder: BrotliState<AllocU8, AllocU32, AllocHC>,
    total_out: usize,
}

impl<AllocU8: Allocator<u8>, AllocU32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>>
    SecondaryHeaderParser<AllocU8, AllocU32, AllocHC>
{
    fn new(alloc_u8: AllocU8, alloc_u32: AllocU32, alloc_huffman_code: AllocHC) -> Self {
        SecondaryHeaderParser {
            brotli_decoder: BrotliState::new(alloc_u8, alloc_u32, alloc_huffman_code),
            total_out: 0,
        }
    }

    fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        let mut available_in = input.len() - *input_offset;
        let mut available_out = output.len() - *output_offset;
        LeptonOperationResult::from(BrotliDecompressStream(
            &mut available_in,
            input_offset,
            input,
            &mut available_out,
            output_offset,
            output,
            &mut self.total_out,
            &mut self.brotli_decoder,
        ))
    }
}
