use std::collections::HashMap;

use alloc::HeapAlloc;
use mux::{Mux, StreamDemuxer};

use arithmetic_coder::ArithmeticDecoder;
use codec::{create_codecs, CodecError, LeptonCodec};
use interface::{ErrMsg, LeptonOperationResult, SimpleResult};
use secondary_header::{Marker, SecondaryHeader};
use util::mem_copy;

pub struct LeptonDecoder {
    target_len: usize,
    total_out: usize,
    cmp_header_left: usize,
    mux: Mux<HeapAlloc<u8>>,
    alloc_u8: HeapAlloc<u8>,
    codecs: Vec<LeptonCodec>,
    current_stream: u8,
    error: Option<ErrMsg>,
    grb: Vec<u8>,
    _optional_sections: HashMap<Marker, Vec<u8>>,
}

impl LeptonDecoder {
    pub fn new(header: SecondaryHeader, target_len: usize) -> Self {
        let codecs = create_codecs(
            header.hdr.frame.components,
            header.hdr.frame.size_in_mcu,
            header.hdr.scans,
            header.thx,
            header.pad,
            &|| ArithmeticDecoder {},
        );
        Self {
            target_len,
            total_out: 0,
            cmp_header_left: 3,
            mux: Mux::new(codecs.len()),
            alloc_u8: HeapAlloc::new(0),
            codecs,
            current_stream: 0,
            error: None,
            grb: header.grb,
            _optional_sections: header.optional,
        }
    }

    pub fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        match self.error {
            Some(ref msg) => LeptonOperationResult::Failure(msg.clone()),
            None => {
                // Skip CMP header
                if self.cmp_header_left != 0 {
                    let available_input = input.len() - *input_offset;
                    if available_input < self.cmp_header_left {
                        self.cmp_header_left -= available_input;
                        *input_offset += available_input;
                        return LeptonOperationResult::NeedsMoreInput;
                    } else {
                        *input_offset += self.cmp_header_left;
                        self.cmp_header_left = 0;
                    }
                }

                // Demux CMP
                if !self.mux.encountered_eof() {
                    *input_offset += self.mux
                        .deserialize(&input[*input_offset..], &mut self.alloc_u8);
                }

                let old_output_offset = *output_offset;
                let output_needed = self.target_len - self.grb.len() - self.total_out;
                let is_eof = self.mux.encountered_eof();
                if output_needed > 0 {
                    if self.current_stream >= self.codecs.len() as u8 {
                        let msg = ErrMsg::InsufficientCMP;
                        self.error = Some(msg.clone());
                        return LeptonOperationResult::Failure(msg);
                    }
                    // Decode CMP
                    for stream in self.current_stream..self.codecs.len() as u8 {
                        if self.mux.data_len(stream) > 0 || is_eof {
                            if let Err(msg) =
                                self.codec_decode(stream, output, output_offset, is_eof)
                            {
                                self.error = Some(msg.clone());
                                for codec in self.codecs.drain(self.current_stream as usize..) {
                                    codec.kill();
                                }
                                return LeptonOperationResult::Failure(msg);
                            }
                        }
                    }
                } else {
                    // Output GRB
                    if !self.grb.is_empty() {
                        let mut grb_written = self.total_out - self.target_len + self.grb.len();
                        mem_copy(output, output_offset, &self.grb, &mut grb_written);
                    }
                };
                self.total_out += *output_offset - old_output_offset;
                if *output_offset == output.len() {
                    LeptonOperationResult::NeedsMoreOutput
                } else {
                    if !is_eof {
                        LeptonOperationResult::NeedsMoreInput
                    } else if self.total_out < self.target_len {
                        LeptonOperationResult::NeedsMoreOutput
                    } else {
                        LeptonOperationResult::Success
                    }
                }
            }
        }
    }

    fn codec_decode(
        &mut self,
        stream_index: u8,
        output: &mut [u8],
        output_offset: &mut usize,
        is_eof: bool,
    ) -> SimpleResult<ErrMsg> {
        let codec = &mut self.codecs[stream_index as usize];
        if self.mux.data_len(stream_index) > 0 {
            if codec.finished() {
                return Err(ErrMsg::PrematureDecodeCompletion);
            } else {
                let input = self.mux.read_buffer(stream_index);
                codec.write(&input.data[*input.read_offset..])?;
                *input.read_offset = input.data.len();
                if is_eof {
                    codec.write_eof();
                }
            }
        }
        if stream_index == self.current_stream {
            *output_offset += codec.read(&mut output[*output_offset..])?;
            if codec.finished() {
                self.current_stream += 1;
            }
        }
        Ok(())
    }
}

impl From<CodecError> for ErrMsg {
    fn from(err: CodecError) -> Self {
        use self::CodecError::*;
        match err {
            CodingFailure(msg) => return msg,
            ReadAfterFinish => panic!("read from codec after it has finished"),
            WriteAfterEOF => panic!("write to codec after EOF"),
            TooMuchInput => return ErrMsg::PrematureDecodeCompletion,
        }
    }
}
