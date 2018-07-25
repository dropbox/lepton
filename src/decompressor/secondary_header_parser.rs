use alloc::HeapAlloc;
use brotli::{BrotliDecompressStream, BrotliResult, BrotliState, HuffmanCode};

use interface::LeptonOperationResult;
use resizable_buffer::ResizableByteBuffer;
use secondary_header::{deserialize_header, SecondaryHeader};
use util::mem_copy;

pub struct SecondaryHeaderParser {
    decoder: BrotliState<HeapAlloc<u8>, HeapAlloc<u32>, HeapAlloc<HuffmanCode>>,
    target_len: usize,
    total_in: usize,
    total_out: usize,
    data: ResizableByteBuffer<u8, HeapAlloc<u8>>,
    header: Option<SecondaryHeader>,
    output_jpeg_hdr: bool,
    pge_written: usize,
    hdr_written: usize,
}

impl SecondaryHeaderParser {
    pub fn new(target_len: usize, output_jpeg_hdr: bool) -> Self {
        SecondaryHeaderParser {
            decoder: BrotliState::new(
                HeapAlloc::new(0),
                HeapAlloc::new(0),
                HeapAlloc::new(HuffmanCode::default()),
            ),
            target_len,
            total_in: 0,
            total_out: 0,
            data: ResizableByteBuffer::<u8, HeapAlloc<u8>>::new(),
            header: None,
            output_jpeg_hdr,
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
            // TODO: Error if parse return success but target_len not reached
            let old_input_offset = *input_offset;
            let remaining_in = self.target_len - self.total_in;
            let truncated_input = if remaining_in < input.len() - *input_offset {
                &input[..(*input_offset + remaining_in)]
            } else {
                input
            };
            let result = match self.parse(truncated_input, input_offset) {
                LeptonOperationResult::Success => LeptonOperationResult::NeedsMoreOutput,
                other => other,
            };
            self.total_in += *input_offset - old_input_offset;
            result
        } else {
            self.flush(output, output_offset)
        }
    }

    pub fn take_header(self) -> SecondaryHeader {
        match self.header {
            Some(header) => header,
            None => panic!("header has not been built"),
        }
    }

    fn parse(&mut self, input: &[u8], input_offset: &mut usize) -> LeptonOperationResult {
        let mut available_in = input.len() - *input_offset;
        let mut output_offset: usize;
        let mut result: BrotliResult;
        loop {
            {
                let output = self.data
                    .checkout_next_buffer(&mut self.decoder.alloc_u8, Some(256));
                let mut available_out = output.len();
                output_offset = 0;
                result = BrotliDecompressStream(
                    &mut available_in,
                    input_offset,
                    input,
                    &mut available_out,
                    &mut output_offset,
                    output,
                    &mut self.total_out,
                    &mut self.decoder,
                );
            }
            self.data.commit_next_buffer(output_offset);
            match result {
                BrotliResult::NeedsMoreOutput => (),
                other => return LeptonOperationResult::from(other),
            }
        }
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonOperationResult {
        loop {
            match self.header {
                Some(ref mut header) => {
                    if self.pge_written < header.pge.len() {
                        mem_copy(output, output_offset, &header.pge, &mut self.pge_written);
                        if self.pge_written == header.pge.len() {
                            header.pge.clear();
                        }
                    } else if self.output_jpeg_hdr && self.hdr_written < header.hdr.len() {
                        // FIXME: defer writing out hdr to lepton_decoder
                        mem_copy(output, output_offset, &header.hdr, &mut self.hdr_written);
                    }
                    if self.pge_written >= header.pge.len()
                        && (!self.output_jpeg_hdr || self.hdr_written == header.hdr.len())
                    {
                        return LeptonOperationResult::Success;
                    } else if *output_offset == output.len() {
                        return LeptonOperationResult::NeedsMoreOutput;
                    }
                }
                None => match deserialize_header(self.data.slice()) {
                    Ok(header) => self.header = Some(header),
                    Err(e) => return LeptonOperationResult::Failure(e),
                },
            }
        }
    }
}
