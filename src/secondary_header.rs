use std::collections::HashMap;

use byte_converter::{ByteConverter, LittleEndian};
use interface::ErrMsg;
use thread_handoff::ThreadHandoff;

pub const MARKER_SIZE: usize = 3;
pub const SECTION_HDR_SIZE: usize = 7;
pub const PAD_SECTION_SIZE: usize = 4;
static BASIC_HEADER: [u8; 156] = [
    0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c,
    0x00, 0x00, 0xff, 0xdb, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02,
    0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06, 0x06, 0x05,
    0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a, 0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e,
    0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d, 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13,
    0x12, 0x10, 0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xff, 0xc4, 0x00, 0x14, 0x10, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xda, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0x54, 0xdd,
];

#[derive(Eq, Hash, PartialEq)]
pub enum Marker {
    HDR,
    P0D,
    PAD,
    HHX,
    CRS,
    FRS,
    PGE,
    PGR,
    GRB,
    EEE,
    SIZ,
}

impl Marker {
    pub fn from(input: &[u8]) -> Result<Self, ErrMsg> {
        if input.len() < 3 {
            return Err(ErrMsg::IncompleteSecondaryHeaderMarker);
        }
        if input[..2].eq(b"HH") {
            return Ok(Marker::HHX);
        }
        let marker = &input[..MARKER_SIZE];
        match marker {
            b"HDR" => Ok(Marker::HDR),
            b"P0D" => Ok(Marker::P0D),
            b"PAD" => Ok(Marker::PAD),
            b"CRS" => Ok(Marker::CRS),
            b"FRS" => Ok(Marker::FRS),
            b"PGE" => Ok(Marker::PGE),
            b"PGR" => Ok(Marker::PGR),
            b"GRB" => Ok(Marker::GRB),
            b"EEE" => Ok(Marker::EEE),
            b"SIZ" => Ok(Marker::SIZ),
            _ => Err(ErrMsg::InvalidSecondaryHeaderMarker(
                input[0], input[1], input[2],
            )),
        }
    }

    pub fn value(&self) -> &[u8; 3] {
        match *self {
            Marker::HDR => b"HDR",
            Marker::P0D => b"P0D",
            Marker::PAD => b"PAD",
            // During compression only the first 2 bytes of HHX should be used
            Marker::HHX => b"HHX",
            Marker::CRS => b"CRS",
            Marker::FRS => b"FRS",
            Marker::PGE => b"PGE",
            Marker::PGR => b"PGR",
            Marker::GRB => b"GRB",
            Marker::EEE => b"EEE",
            Marker::SIZ => b"SIZ",
        }
    }
}

pub struct SecondaryHeader {
    // TODO: May need to differentiate PGE and PGR
    // TODO: Extract GRB?
    pub hdr: Vec<u8>,
    pub pge: Vec<u8>,
    pub optional: HashMap<Marker, Vec<u8>>,
}

pub fn default_serialized_header() -> Vec<u8> {
    let mut result = Vec::with_capacity(256); // Returned len is 167
    result.extend(Marker::HDR.value());
    result.extend(LittleEndian::u32_to_array(BASIC_HEADER.len() as u32).iter());
    result.extend(BASIC_HEADER.iter());
    result.extend(Marker::P0D.value());
    result.push(1);
    result.extend(Marker::HHX.value()[..2].iter());
    result.extend(ThreadHandoff::serialize(vec![ThreadHandoff::default(); 1]));
    result.extend(Marker::GRB.value());
    result.extend([0, 0, 0, 0].iter());
    result
}

pub fn deserialize_header(data: &[u8]) -> Result<SecondaryHeader, ErrMsg> {
    let mut header = SecondaryHeader {
        hdr: Vec::new(),
        pge: Vec::new(),
        optional: HashMap::new(),
    };
    let mut ptr = 0usize;
    match read_sized_section(data, &mut ptr) {
        Ok((Marker::HDR, body)) => header.hdr.extend(body),
        Ok(_) => return Err(ErrMsg::HDRMissing),
        Err(e) => return Err(e),
    }
    match read_pad(data, &mut ptr) {
        Ok((marker, pad)) => header.optional.insert(marker, pad.to_vec()),
        Err(e) => return Err(e),
    };
    while ptr < data.len() {
        match read_sized_section(data, &mut ptr) {
            Ok((Marker::PGE, body)) | Ok((Marker::PGR, body)) => header.pge.extend(body),
            Ok((marker, body)) => {
                header.optional.insert(marker, body.to_vec());
            }
            Err(e) => return Err(e),
        };
    }
    Ok(header)
}

fn read_pad<'a>(data: &'a [u8], offset: &mut usize) -> Result<(Marker, &'a [u8]), ErrMsg> {
    if data.len() < *offset + PAD_SECTION_SIZE {
        return Err(incomplete_secondary_header_section(Marker::P0D, 0));
    }
    let marker = match Marker::from(&data[*offset..]) {
        Ok(Marker::P0D) => Marker::P0D,
        Ok(Marker::PAD) => Marker::PAD,
        Ok(_) => return Err(ErrMsg::PADMIssing),
        Err(e) => return Err(e),
    };
    let section_end = *offset + PAD_SECTION_SIZE;
    let body = &data[(*offset + MARKER_SIZE)..section_end];
    *offset = section_end;
    Ok((marker, &body))
}

fn read_sized_section<'a>(
    data: &'a [u8],
    offset: &mut usize,
) -> Result<(Marker, &'a [u8]), ErrMsg> {
    let marker = match Marker::from(&data[*offset..]) {
        Ok(marker) => marker,
        Err(e) => return Err(e),
    };
    let section_hdr_size: usize = match marker {
        Marker::HHX => MARKER_SIZE,
        _ => SECTION_HDR_SIZE,
    };
    if data.len() < *offset + section_hdr_size {
        return Err(incomplete_secondary_header_section(marker, 1));
    }
    let section_len = match marker {
        Marker::HHX => (data[*offset + 2] as usize) * 16, // BYTES_PER_HANDOFF = 16
        _ => LittleEndian::slice_to_u32(&data[(*offset + MARKER_SIZE)..]) as usize,
    };
    let section_end = *offset + section_hdr_size + section_len;
    if data.len() < section_end {
        return Err(incomplete_secondary_header_section(marker, 2));
    }
    let body = &data[(*offset + section_hdr_size)..section_end];
    *offset = section_end;
    Ok((marker, &body))
}

fn incomplete_secondary_header_section(marker: Marker, code: u8) -> ErrMsg {
    ErrMsg::IncompleteSecondaryHeaderSection(code, *marker.value())
}
