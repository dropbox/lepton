use std::cmp::max;

use alloc::HeapAlloc;
use mux::{Mux, StreamMuxer};

use arithmetic_coder::ArithmeticDecoder;
use byte_converter::{ByteConverter, LittleEndian};
use codec::{create_codecs, EncoderCodec, EncoderStateFactory};
use interface::CumulativeOperationResult;
use jpeg::{FormatInfo, JpegResult, JpegStreamDecoder, Scan};
use secondary_header::{Marker, MARKER_SIZE, PAD_SECTION_SIZE, SECTION_HDR_SIZE};
use thread_handoff::{ThreadHandoff, ThreadHandoffExt};

pub struct LeptonData {
    // TODO: Maybe add # RST markers and # blocks per channel
    pub n_thread: u8,
    pub secondary_header: Vec<u8>,
    pub cmp: Mux<HeapAlloc<u8>>,
}

pub struct LeptonEncoder {
    jpeg_decoder: Option<JpegStreamDecoder>,
    result: Option<JpegResult<LeptonData>>,
    pge: Vec<u8>,
}

impl LeptonEncoder {
    pub fn new(start_byte: usize) -> Self {
        LeptonEncoder {
            jpeg_decoder: Some(JpegStreamDecoder::new(start_byte)),
            result: None,
            pge: vec![],
        }
    }

    pub fn encode(&mut self, input: &[u8], input_offset: &mut usize) -> CumulativeOperationResult {
        match self.jpeg_decoder {
            Some(ref mut decoder) => {
                if decoder.decode(input, input_offset) == CumulativeOperationResult::NeedsMoreInput
                {
                    return CumulativeOperationResult::NeedsMoreInput;
                }
            }
            None => return CumulativeOperationResult::Finish,
        };
        // JPEG decoder has finished
        self.finish();
        CumulativeOperationResult::Finish
    }

    pub fn flush(&mut self) {
        match self.jpeg_decoder {
            Some(ref mut decoder) => decoder.flush(),
            None => return,
        };
        self.finish();
    }

    pub fn add_pge(&mut self, pge: &[u8]) {
        self.pge.extend(pge);
    }

    pub fn take_result(self) -> JpegResult<LeptonData> {
        match self.result {
            Some(data) => data,
            None => panic!("encode has not finished"),
        }
    }

    fn finish(&mut self) {
        self.result = Some(match self.jpeg_decoder.take().unwrap().take_result() {
            Ok(mut jpeg) => {
                let mut format = jpeg.format.unwrap();
                let jpeg_header_len = jpeg.scans
                    .iter()
                    .fold(0, |accumulator: usize, element: &Scan| {
                        accumulator + element.raw_header.len()
                    });
                let thread_handoffs = select_handoffs(&format);
                println!("n thread: {}", thread_handoffs.len());
                let mut secondary_header = Vec::with_capacity(
                    SECTION_HDR_SIZE * 3
                        + PAD_SECTION_SIZE
                        + MARKER_SIZE
                        + jpeg_header_len
                        + thread_handoffs.len() * ThreadHandoffExt::BYTES_PER_HANDOFF
                        + self.pge.len()
                        + format.pge.len()
                        + format.grb.len(),
                );
                secondary_header.extend(Marker::HDR.value());
                secondary_header.extend(LittleEndian::u32_to_array(jpeg_header_len as u32).iter());
                for mut scan in jpeg.scans.iter_mut() {
                    secondary_header.append(&mut scan.raw_header);
                }
                secondary_header.extend(Marker::P0D.value());
                secondary_header.push(format.pad_byte);
                secondary_header.extend(Marker::THX.value());
                secondary_header.append(&mut ThreadHandoffExt::serialize(&thread_handoffs));
                secondary_header.extend(Marker::PGE.value());
                secondary_header.extend(
                    LittleEndian::u32_to_array((self.pge.len() + format.pge.len()) as u32).iter(),
                );
                secondary_header.append(&mut self.pge);
                secondary_header.append(&mut format.pge);
                secondary_header.extend(Marker::GRB.value());
                secondary_header.extend(LittleEndian::u32_to_array(format.grb.len() as u32).iter());
                secondary_header.append(&mut format.grb);
                let mut mux = Mux::<HeapAlloc<u8>>::new(thread_handoffs.len());
                let mut alloc_u8 = HeapAlloc::new(0);
                let mut codecs =
                    create_codecs::<ArithmeticDecoder, EncoderCodec, EncoderStateFactory>(
                        jpeg.frame.components,
                        jpeg.frame.size_in_mcu,
                        jpeg.scans,
                        &thread_handoffs,
                        format.pad_byte,
                    );
                loop {
                    let mut unfinished = codecs.len();
                    for (i, codec) in codecs.iter_mut().enumerate() {
                        if codec.finished() {
                            unfinished -= 1;
                        } else {
                            let write_buf = mux.write_buffer(i as u8, &mut alloc_u8);
                            let len = codec
                                .read(&mut write_buf.data[*write_buf.write_offset..])
                                .unwrap();
                            *write_buf.write_offset += len;
                        }
                    }
                    if unfinished == 0 {
                        break;
                    }
                    // FIXME: Do we want to wait here?
                }
                Ok(LeptonData {
                    n_thread: codecs.len() as u8,
                    secondary_header,
                    cmp: mux,
                })
            }
            Err(e) => Err(e),
        });
    }
}

fn select_handoffs(format: &FormatInfo) -> Vec<ThreadHandoffExt> {
    // let handoffs = &format.handoff;
    // let entropy_data_start = handoffs[0].segment_size as usize;
    // let entropy_data_len = format.len - entropy_data_start;
    // let mut n_thread = if entropy_data_len < 125000 {
    //     1
    // } else if entropy_data_len < 250000 {
    //     2
    // } else if entropy_data_len < 500000 {
    //     4
    // } else {
    //     8
    // };
    // let n_candidate = handoffs.len();
    // if n_candidate / 2 < n_thread {
    //     n_thread = max(n_candidate / 2, 1);
    // }
    // let mut selected_indices = vec![0; n_thread];
    // for i in 1..n_thread {
    //     let desired_segment_start = entropy_data_start + entropy_data_len * i / n_thread;
    //     selected_indices[i] = select_handoff(
    //         &handoffs,
    //         selected_indices[i - 1] + 1,
    //         desired_segment_start,
    //     );
    // }
    // selected_indices
    //     .into_iter()
    //     .take_while(|i: &usize| *i < handoffs.len())
    //     .map(|i: usize| handoffs[i].clone())
    //     .collect()
    format.handoff[..1].to_vec()
}

fn select_handoff(handoffs: &[ThreadHandoffExt], cursor: usize, desired_start: usize) -> usize {
    for i in cursor..handoffs.len() {
        if handoffs[i].segment_size > desired_start as u32 {
            return if i == cursor { i } else { i - 1 };
        }
    }
    handoffs.len()
}
