use core::mem;
use std::collections::HashMap;

use alloc::Allocator;
use brotli::{BrotliDecompressStream, BrotliState, HuffmanCode};

use interface::{ErrMsg, LeptonOperationResult};
use resizable_buffer::ResizableByteBuffer;
use secondary_header::{SecondaryHeaderMarker as Marker, MARKER_SIZE, PAD_SECTION_SIZE,
                       SECTION_HDR_SIZE};
use util::{mem_copy, u8_array_to_u32};

pub struct SecondaryHeader {
    hdr: Vec<u8>,
    pge: Vec<u8>,
    optional: HashMap<Marker, Vec<u8>>,
}

pub struct SecondaryHeaderParser<
    AllocU8: Allocator<u8>,
    AllocU32: Allocator<u32>,
    AllocHC: Allocator<HuffmanCode>,
> {
    decoder: BrotliState<AllocU8, AllocU32, AllocHC>,
    total_out: usize,
    target_len: usize,
    total_in: usize,
    data: ResizableByteBuffer<u8, AllocU8>,
    header: Option<SecondaryHeader>,
    pge_written: usize,
    hdr_written: usize,
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
            decoder: BrotliState::new(alloc_u8, alloc_u32, alloc_huffman_code),
            total_out: 0,
            target_len,
            total_in: 0,
            data: ResizableByteBuffer::<u8, AllocU8>::new(),
            header: None,
            pge_written: 0,
            hdr_written: 0,
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
            // TODO: Error if parse return success but target_len not reached?
            let original_offset = *input_offset;
            let remaining_in = self.target_len - self.total_in;
            let truncated_input;
            if remaining_in < input.len() - *input_offset {
                truncated_input = &input[..(*input_offset + remaining_in)];
            } else {
                truncated_input = input;
            }
            let result = match self.parse(truncated_input, input_offset) {
                LeptonOperationResult::Success => LeptonOperationResult::NeedsMoreOutput,
                other => other,
            };
            self.total_in += *input_offset - original_offset;
            result
        } else {
            self.flush(output, output_offset)
        }
    }

    // This function will change the parser's own copy of header to `None`.
    pub fn extract_header(&mut self) -> Option<SecondaryHeader> {
        match self.header {
            Some(_) => mem::replace(&mut self.header, None),
            None => None,
        }
    }

    fn parse(&mut self, input: &[u8], input_offset: &mut usize) -> LeptonOperationResult {
        let mut available_out;
        let mut output_offset;
        let result;
        {
            let output = self.data
                .checkout_next_buffer(&mut self.decoder.alloc_u8, Some(256));
            output_offset = 0;
            let mut available_in = input.len() - *input_offset;
            available_out = output.len();
            result = LeptonOperationResult::from(BrotliDecompressStream(
                &mut available_in,
                input_offset,
                input,
                &mut available_out,
                &mut output_offset,
                output,
                &mut self.total_out,
                &mut self.decoder,
            ));
        }
        // TODO: May need to add additional checks here
        self.data.commit_next_buffer(output_offset);
        result
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonOperationResult {
        if let None = self.header {
            self.header = match self.build_header() {
                Some(header) => Some(header),
                None => return LeptonOperationResult::Failure(ErrMsg::BuildSecondaryHeaderFail),
            };
        }
        if let Some(ref header) = self.header {
            if self.pge_written < header.pge.len() {
                mem_copy(
                    output,
                    output_offset,
                    &header.pge[..],
                    &mut self.pge_written,
                )
            } else if self.hdr_written < header.hdr.len() {
                mem_copy(
                    output,
                    output_offset,
                    &header.hdr[..],
                    &mut self.hdr_written,
                )
            }
            if self.pge_written == header.pge.len() && self.hdr_written == header.hdr.len() {
                LeptonOperationResult::Success
            } else {
                LeptonOperationResult::NeedsMoreOutput
            }
        } else {
            LeptonOperationResult::Failure(ErrMsg::BuildSecondaryHeaderFail)
        }
    }

    fn build_header(&self) -> Option<SecondaryHeader> {
        let mut header = SecondaryHeader {
            hdr: Vec::new(),
            pge: Vec::new(),
            optional: HashMap::new(),
        };
        let data = self.data.slice();
        let mut ptr: usize = 0;
        if !data[..MARKER_SIZE].eq(Marker::HDR.value()) {
            return None;
        }
        match read_sized_section(data, &mut ptr) {
            Some((Marker::HDR, body)) => header.hdr.extend_from_slice(body),
            _ => return None,
        }
        match read_pad(data, &mut ptr) {
            Some((marker, pad)) => header.optional.insert(marker, pad.to_vec()),
            None => return None,
        };
        while ptr < data.len() {
            match read_sized_section(data, &mut ptr) {
                // FIXME: May need different treatment for PGE and PGR
                Some((Marker::PGE, body)) | Some((Marker::PGR, body)) => {
                    header.pge.extend_from_slice(body)
                }
                Some((marker, body)) => {
                    header.optional.insert(marker, body.to_vec());
                }
                None => return None,
            };
        }
        Some(header)
    }
}

fn read_pad<'a>(data: &'a [u8], offset: &mut usize) -> Option<(Marker, &'a [u8])> {
    if data.len() < *offset + PAD_SECTION_SIZE {
        return None;
    }
    let marker = match Marker::from(&data[*offset..]) {
        Some(Marker::P0D) => Marker::P0D,
        Some(Marker::PAD) => Marker::PAD,
        _ => return None,
    };
    let section_end = *offset + PAD_SECTION_SIZE;
    let body = &data[(*offset + MARKER_SIZE)..section_end];
    *offset = section_end;
    Some((marker, &body))
}

fn read_sized_section<'a>(data: &'a [u8], offset: &mut usize) -> Option<(Marker, &'a [u8])> {
    // if data.len() < *offset + MARKER_SIZE {
    //     return None;
    // }
    let marker = match Marker::from(&data[*offset..]) {
        Some(marker) => marker,
        None => return None,
    };
    let section_hdr_size: usize = match marker {
        Marker::HHX => MARKER_SIZE,
        _ => SECTION_HDR_SIZE,
    };
    if data.len() < *offset + section_hdr_size {
        return None;
    }
    let section_len = match marker {
        Marker::HHX => (data[*offset + 2] as usize) * 16, // BYTES_PER_HANDOFF = 16
        _ => u8_array_to_u32(data, &(*offset + MARKER_SIZE)) as usize,
    };
    let section_end = *offset + section_hdr_size + section_len;
    if data.len() < section_end {
        return None;
    }
    let body = &data[(*offset + section_hdr_size)..section_end];
    *offset = section_end;
    Some((marker, &body))
}
