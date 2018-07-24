use byte_converter::{ByteConverter, LittleEndian};
use interface::CumulativeOperationResult;
use jpeg_decoder::{JpegResult, JpegStreamDecoder, Scan};
use secondary_header::{Marker, MARKER_SIZE, PAD_SECTION_SIZE, SECTION_HDR_SIZE};
use thread_handoff::{ThreadHandoffExt, BYTES_PER_HANDOFF_EXT};

pub struct LeptonData {
    // TODO: Maybe add # RST markers and # blocks per channel
    pub secondary_header: Vec<u8>,
    pub cmp: Vec<u8>,
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
            Ok(jpeg) => {
                let mut format = jpeg.format.unwrap();
                let jpeg_header_len = jpeg.scans
                    .iter()
                    .fold(0, |accumulator: usize, element: &Scan| {
                        accumulator + element.raw_header.len()
                    });
                // FIXME: Select handoffs
                let mut thread_handoff = vec![];
                let mut secondary_header = Vec::with_capacity(
                    SECTION_HDR_SIZE * 3
                        + PAD_SECTION_SIZE
                        + MARKER_SIZE
                        + jpeg_header_len
                        + format.handoff.len() * BYTES_PER_HANDOFF_EXT
                        + self.pge.len()
                        + format.pge.len()
                        + format.grb.len(),
                );
                secondary_header.extend(Marker::HDR.value());
                secondary_header.extend(LittleEndian::u32_to_array(jpeg_header_len as u32).iter());
                let mut cmp = vec![b'C', b'M', b'P'];
                for mut scan in jpeg.scans {
                    secondary_header.append(&mut scan.raw_header);
                    for mut component in scan.coefficients.unwrap() {
                        cmp.extend(component.drain(..).map(|x| x as u8)); // FIXME: Use arithmetic encoding
                    }
                }
                secondary_header.extend(Marker::P0D.value());
                secondary_header.push(format.pad_byte);
                secondary_header.extend(Marker::THX.value());
                secondary_header.append(&mut ThreadHandoffExt::serialize(&thread_handoff));
                secondary_header.extend(Marker::PGE.value());
                secondary_header.extend(
                    LittleEndian::u32_to_array((self.pge.len() + format.pge.len()) as u32).iter(),
                );
                secondary_header.append(&mut self.pge);
                secondary_header.append(&mut format.pge);
                secondary_header.extend(Marker::GRB.value());
                secondary_header.extend(LittleEndian::u32_to_array(format.grb.len() as u32).iter());
                secondary_header.append(&mut format.grb);
                println!("{:02X?}", secondary_header);
                Ok(LeptonData {
                    secondary_header,
                    cmp,
                })
            }
            Err(e) => Err(e),
        });
    }
}
