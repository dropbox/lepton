use std::cmp::min;
use std::collections::HashMap;
use std::thread;

use arithmetic_coder::ArithmeticDecoder;
use codec::LeptonCodec;
use constants::INPUT_STREAM_PRELOAD_LEN;
use interface::{ErrMsg, LeptonOperationResult, SimpleResult};
use iostream::{iostream, InputStream, OutputStream};
use secondary_header::{Marker, SecondaryHeader};
use util::mem_copy;

pub struct LeptonDecoder {
    target_len: usize,
    total_out: usize,
    codec_handle: Option<thread::JoinHandle<SimpleResult<ErrMsg>>>, // FIXME: collection of handles
    to_codec: OutputStream,
    from_codec: InputStream,
    grb: Vec<u8>,
    pad: u8,
    optional_sections: HashMap<Marker, Vec<u8>>,
}

impl LeptonDecoder {
    pub fn new(header: SecondaryHeader, target_len: usize) -> Self {
        let (codec_input, to_codec) = iostream(INPUT_STREAM_PRELOAD_LEN);
        let (from_codec, codec_output) = iostream(INPUT_STREAM_PRELOAD_LEN);
        // FIXME: collection of handles
        let components = header.hdr.frame.components;
        let size_in_mcu = header.hdr.frame.size_in_mcu;
        let scans = header.hdr.scans;
        let mut handoffs = header.thx;
        let codec_handle = Some(
            thread::Builder::new()
                .name("codec thread".to_owned())
                .spawn(move || {
                    LeptonCodec::new(
                        ArithmeticDecoder {},
                        codec_input,
                        codec_output,
                        components,
                        size_in_mcu,
                        scans,
                        handoffs.remove(0), // FIXME
                    ).start()
                })
                .unwrap(),
        );
        // ----------------------------
        LeptonDecoder {
            target_len,
            total_out: 0,
            codec_handle,
            to_codec,
            from_codec,
            grb: header.grb,
            pad: header.pad,
            optional_sections: header.optional,
        }
    }

    pub fn decode(
        &mut self,
        input: &[u8],
        input_offset: &mut usize,
        output: &mut [u8],
        output_offset: &mut usize,
    ) -> LeptonOperationResult {
        let old_output_offset = *output_offset;
        let output_needed = self.target_len - self.grb.len() - self.total_out;
        if output_needed > 0 {
            let input_end = min(*input_offset + output_needed, input.len());
            mem_copy(output, output_offset, &input[..input_end], input_offset);
        } else {
            if !self.grb.is_empty() {
                let mut grb_written = self.total_out - self.target_len + self.grb.len();
                mem_copy(output, output_offset, &self.grb, &mut grb_written);
            }
        };
        self.total_out += *output_offset - old_output_offset;
        if self.total_out < self.target_len - self.grb.len() {
            LeptonOperationResult::NeedsMoreInput
        } else if self.total_out < self.target_len {
            LeptonOperationResult::NeedsMoreOutput
        } else {
            LeptonOperationResult::Success
        }
    }
}
