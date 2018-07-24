use alloc::HeapAlloc;
use brotli::{BrotliDecompressStream, BrotliResult, BrotliState, HuffmanCode};

use interface::LeptonOperationResult;
use resizable_buffer::ResizableByteBuffer;
use secondary_header::{deserialize_header, SecondaryHeader};
use util::mem_copy;

pub struct SecondaryHeaderParser {
    decoder: BrotliState<HeapAlloc<u8>, HeapAlloc<u32>, HeapAlloc<HuffmanCode>>,
    total_out: usize,
    target_len: usize,
    total_in: usize,
    data: ResizableByteBuffer<u8, HeapAlloc<u8>>,
    header: Option<SecondaryHeader>,
    pge_written: usize,
    hdr_written: usize,
}

impl SecondaryHeaderParser {
    // FIXME: Need to consider in skip_hdr
    pub fn new(target_len: usize) -> Self {
        SecondaryHeaderParser {
            decoder: BrotliState::new(
                HeapAlloc::new(0),
                HeapAlloc::new(0),
                HeapAlloc::new(HuffmanCode::default()),
            ),
            total_out: 0,
            target_len,
            total_in: 0,
            data: ResizableByteBuffer::<u8, HeapAlloc<u8>>::new(),
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
            let old_input_offset = *input_offset;
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
                Some(ref header) => {
                    if self.pge_written < header.pge.len() {
                        mem_copy(
                            output,
                            output_offset,
                            header.pge.as_slice(),
                            &mut self.pge_written,
                        )
                    } else if self.hdr_written < header.hdr.len() {
                        mem_copy(
                            output,
                            output_offset,
                            header.hdr.as_slice(),
                            &mut self.hdr_written,
                        )
                    }
                    if self.pge_written == header.pge.len() && self.hdr_written == header.hdr.len()
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
