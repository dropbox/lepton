use alloc::HeapAlloc;
use mux::Mux;

use super::brotli_encoder::BrotliEncoder;
use super::util::flush_lepton_data;
use byte_converter::{ByteConverter, LittleEndian};
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult};
use primary_header::{serialize_header, HEADER_SIZE};
use secondary_header::{default_serialized_header, MARKER_SIZE, SECTION_HDR_SIZE};

pub struct LeptonPermissiveCompressor {
    encoder: BrotliEncoder,
    header: Option<[u8; HEADER_SIZE]>,
    header_written: usize,
    data: Vec<u8>,
    first_encode: bool,
    brotli_done: bool,
    cmp: Mux<HeapAlloc<u8>>,
    cmp_header_written: usize,
}

impl LeptonPermissiveCompressor {
    pub fn new() -> Self {
        let mut data = Vec::with_capacity(1024); // FIXME: Better initial capacity?
        data.extend([b'P', b'G', b'E', 0, 0, 0, 0].iter());
        LeptonPermissiveCompressor {
            encoder: BrotliEncoder::new(),
            header: None,
            header_written: 0,
            data,
            first_encode: true,
            brotli_done: false,
            cmp: Mux::new(1),
            cmp_header_written: 0,
        }
    }
}

impl Compressor for LeptonPermissiveCompressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        if self.first_encode {
            let mut basic_header_offset = 0usize;
            if let LeptonOperationResult::Failure(msg) = self.encoder.encode_all(
                &default_serialized_header(),
                &mut basic_header_offset,
                output,
                output_offset,
            ) {
                return LeptonOperationResult::Failure(msg);
            }
            self.first_encode = false;
        }
        self.data.extend(input[*input_offset..].iter());
        *input_offset = input.len();
        LeptonOperationResult::NeedsMoreInput
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        while *output_offset < output.len() {
            match self.header {
                None => {
                    let pge_len = self.data.len() - SECTION_HDR_SIZE;
                    let pge_len_array = LittleEndian::u32_to_array(pge_len as u32);
                    self.data[MARKER_SIZE..SECTION_HDR_SIZE].clone_from_slice(&pge_len_array);
                    let mut input_offset = 0usize;
                    if let LeptonOperationResult::Failure(msg) = self.encoder.encode_all(
                        &self.data,
                        &mut input_offset,
                        output,
                        output_offset,
                    ) {
                        return LeptonFlushResult::Failure(msg);
                    }
                    match self.encoder.finish_encode() {
                        Ok(size) => {
                            self.header = Some(serialize_header(b'Y', 1, &[0; 12], pge_len, size));
                        }
                        Err(e) => return LeptonFlushResult::Failure(e),
                    }
                }
                Some(header) => {
                    if let Some(result) = flush_lepton_data(
                        output,
                        output_offset,
                        &header,
                        &mut self.encoder,
                        &mut self.cmp,
                        &mut self.header_written,
                        &mut self.brotli_done,
                        &mut self.cmp_header_written,
                    ) {
                        return result;
                    }
                }
            }
        }
        LeptonFlushResult::NeedsMoreOutput
    }
}
