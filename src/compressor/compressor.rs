use std::cmp::{max, min};

use super::brotli_encoder::BrotliEncoder;
use super::lepton_encoder::{LeptonData, LeptonEncoder};
use super::util::flush_lepton_data;
use interface::{
    Compressor, CumulativeOperationResult, ErrMsg, LeptonFlushResult, LeptonOperationResult,
};
use primary_header::{serialize_header, HEADER_SIZE as PRIMARY_HEADER_SIZE};

pub struct LeptonCompressor {
    brotli_encoder: BrotliEncoder,
    lepton_encoder: Option<LeptonEncoder>,
    result: Option<Result<LeptonData, ErrMsg>>,
    start_byte: usize,
    embedding: usize,
    total_in: usize,
    pge: Vec<u8>,
    primary_header: Option<[u8; PRIMARY_HEADER_SIZE]>,
    primary_header_written: usize,
    brotli_done: bool,
    cmp_written: usize,
}

impl LeptonCompressor {
    pub fn new(start_byte: usize, embedding: usize) -> Self {
        LeptonCompressor {
            brotli_encoder: BrotliEncoder::new(),
            lepton_encoder: Some(LeptonEncoder::new(max(start_byte - embedding, 0))),
            result: None,
            start_byte,
            embedding,
            total_in: 0,
            pge: vec![],
            primary_header: None,
            primary_header_written: 0,
            brotli_done: false,
            cmp_written: 0,
        }
    }

    fn finish_lepton_encode(&mut self) -> Result<(), ErrMsg> {
        let result;
        self.result = Some(match self.lepton_encoder.take().unwrap().take_result() {
            Ok(data) => {
                let mut offset = 0;
                match self.brotli_encoder.encode_all(
                    &data.secondary_header,
                    &mut offset,
                    &mut [],
                    &mut 0,
                ) {
                    LeptonOperationResult::Failure(msg) => {
                        result = Err(msg.clone());
                        Err(msg)
                    }
                    _ => {
                        result = Ok(());
                        Ok(data)
                    }
                }
            }
            Err(e) => {
                result = Err(ErrMsg::JpegDecodeFail(e.clone()));
                Err(ErrMsg::JpegDecodeFail(e))
            }
        });
        result
    }
}

impl Compressor for LeptonCompressor {
    fn encode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        _output: &mut [u8],
        _output_offset: &mut usize,
    ) -> LeptonOperationResult {
        if let Some(ref result) = self.result {
            return match result {
                Ok(_) => {
                    while *input_offset < input.len() {
                        self.brotli_encoder
                            .encode(input, input_offset, &mut [], &mut 0);
                    }
                    LeptonOperationResult::NeedsMoreInput
                }
                Err(msg) => LeptonOperationResult::Failure(msg.clone()),
            };
        }
        let mut result = LeptonOperationResult::NeedsMoreInput;
        while *input_offset < input.len() {
            let old_input_offset = *input_offset;
            if self.total_in < self.embedding {
                let dist_to_embedding = self.embedding - self.total_in;
                let consume_end = min(old_input_offset + dist_to_embedding, input.len());
                if self.start_byte < self.embedding
                    && self.start_byte < self.total_in + input.len() - old_input_offset
                {
                    let dist_to_start = self.start_byte - self.total_in;
                    let pge_start = old_input_offset + max(dist_to_start, 0);
                    self.pge.extend(&input[pge_start..consume_end]);
                }
                *input_offset = consume_end;
            } else {
                if !self.pge.is_empty() {
                    self.lepton_encoder.as_mut().unwrap().add_pge(&self.pge);
                    self.pge.clear();
                }
                if self.lepton_encoder
                    .as_mut()
                    .unwrap()
                    .encode(input, input_offset)
                    == CumulativeOperationResult::Finish
                {
                    if let Err(msg) = self.finish_lepton_encode() {
                        result = LeptonOperationResult::Failure(msg);
                    }
                }
            };
            self.total_in += *input_offset - old_input_offset;
        }
        result
    }

    fn flush(&mut self, output: &mut [u8], output_offset: &mut usize) -> LeptonFlushResult {
        match self.result {
            Some(ref result) => if let Err(ref msg) = result {
                return LeptonFlushResult::Failure(msg.clone());
            },
            None => {
                self.lepton_encoder.as_mut().unwrap().flush();
                if let Err(msg) = self.finish_lepton_encode() {
                    return LeptonFlushResult::Failure(msg);
                }
            }
        }
        while *output_offset < output.len() {
            match self.primary_header {
                None => match self.brotli_encoder.finish_encode() {
                    Ok(size) => {
                        self.primary_header = Some(serialize_header(
                            if self.start_byte <= self.embedding {
                                b'N'
                            } else {
                                b'Y'
                            },
                            1, // TODO: Mutlithread
                            &[0; 12],
                            self.total_in,
                            size,
                        ));
                    }
                    Err(e) => return LeptonFlushResult::Failure(e),
                },
                Some(ref header) => {
                    if let Some(result) = flush_lepton_data(
                        output,
                        output_offset,
                        header,
                        &mut self.brotli_encoder,
                        &self.result.as_ref().unwrap().as_ref().unwrap().cmp,
                        &mut self.primary_header_written,
                        &mut self.brotli_done,
                        &mut self.cmp_written,
                    ) {
                        return result;
                    }
                }
            }
        }
        LeptonFlushResult::NeedsMoreOutput
    }
}
