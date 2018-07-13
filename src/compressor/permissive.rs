use super::brotli_encoder::BrotliEncoder;
use byte_converter::{ByteConverter, LittleEndian};
use interface::{Compressor, LeptonFlushResult, LeptonOperationResult};
use primary_header::{serialize_header, HEADER_SIZE};
use secondary_header::{default_serialized_header, MARKER_SIZE, SECTION_HDR_SIZE};
use util::mem_copy;

static BASIC_CMP: [u8; 6] = [b'C', b'M', b'P', 0xff, 0xfe, 0xff];

pub struct LeptonPermissiveCompressor {
    encoder: BrotliEncoder,
    header: Option<[u8; HEADER_SIZE]>,
    header_written: usize,
    data: Vec<u8>,
    first_encode: bool,
    cmp_written: usize,
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
            cmp_written: 0,
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
        loop {
            match self.header {
                None => {
                    let pge_len = self.data.len() - SECTION_HDR_SIZE;
                    let pge_len_array = LittleEndian::u32_to_array(pge_len as u32);
                    self.data[MARKER_SIZE..SECTION_HDR_SIZE].clone_from_slice(&pge_len_array);
                    let mut input_offset = 0usize;
                    if let LeptonOperationResult::Failure(msg) = self.encoder.encode_all(
                        self.data.as_slice(),
                        &mut input_offset,
                        output,
                        output_offset,
                    ) {
                        return LeptonFlushResult::Failure(msg);
                    }
                    match self.encoder.finish_encode() {
                        Ok(size) => {
                            self.header =
                                Some(serialize_header(b'Y', 1u32, &[0u8; 12], pge_len, size));
                        }
                        Err(e) => return LeptonFlushResult::Failure(e),
                    }
                }
                Some(header) => {
                    if self.header_written < HEADER_SIZE {
                        mem_copy(output, output_offset, &header, &mut self.header_written);
                        if *output_offset == output.len() {
                            return LeptonFlushResult::NeedsMoreOutput;
                        }
                    } else {
                        match self.encoder.flush(output, output_offset) {
                            LeptonFlushResult::Success => {
                                if self.cmp_written < BASIC_CMP.len() {
                                    mem_copy(output, output_offset, &BASIC_CMP, &mut self.cmp_written);
                                    if self.cmp_written == BASIC_CMP.len() {
                                        return LeptonFlushResult::Success
                                    } else {
                                        return LeptonFlushResult::NeedsMoreOutput;
                                    }
                                } else {
                                    return LeptonFlushResult::Success;
                                }
                            }
                            other => return other,
                        }
                    }
                }
            }
        }
    }
}
