use alloc::Allocator;
use brotli::{BrotliDecompressStream, BrotliState, HuffmanCode};

use interface::{ErrMsg, LeptonOperationResult};

pub struct SecondaryHeader {}

pub struct SecondaryHeaderParser<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    brotli_decoder: BrotliState<AllocU8, AllocU32, AllocHC>,
    total_out: usize,
    target_len: usize,
    total_in: usize,
    header: Option<SecondaryHeader>,
}

impl<AllocU8: Allocator<u8>, AllocU32: Allocator<u32>, AllocHC: Allocator<HuffmanCode>>
    SecondaryHeaderParser<AllocU8, AllocU32, AllocHC>
{
    pub fn new(
        target_len: usize,
        alloc_u8: AllocU8,
        alloc_u32: AllocU32,
        alloc_huffman_code: AllocHC,
    ) -> Self {
        SecondaryHeaderParser {
            brotli_decoder: BrotliState::new(alloc_u8, alloc_u32, alloc_huffman_code),
            total_out: 0,
            target_len,
            total_in: 0,
            header: None,
        }
    }

    pub fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        if self.total_in < self.target_len {
            return self.parse(input, input_offset);
        } else {
            return self.flush(output, output_offset);
        }
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

    pub fn get_header(&self) -> Option<&SecondaryHeader> {
        match self.header {
            Some(ref header) => Some(header),
            None => None,
        }
    }

    fn parse(&mut self, input: &[u8], input_offset: &mut usize) -> LeptonOperationResult {
        // TODO: Decompress and buffer
        LeptonOperationResult::Success
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonOperationResult {
        if let None = self.header {
            self.header = match self.build_header() {
                Some(header) => Some(header),
                None => return LeptonOperationResult::Failure(ErrMsg::IncompleteHeader),
            };
        }
        if let Some(ref header) = self.header {
            // TODO: write out PGE and HDR
        }
        LeptonOperationResult::Success
    }

    fn build_header(&self) -> Option<SecondaryHeader> {
        Some(SecondaryHeader {})
    }
}
