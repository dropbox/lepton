use interface::ErrMsg;

pub const MARKER_SIZE: usize = 3;
pub const SECTION_HDR_SIZE: usize = 7;
pub const PAD_SECTION_SIZE: usize = 4;

#[derive(Eq, Hash, PartialEq)]
pub enum SecondaryHeaderMarker {
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

impl SecondaryHeaderMarker {
    pub fn from(input: &[u8]) -> Result<Self, ErrMsg> {
        if input.len() < 3 {
            return Err(ErrMsg::IncompleteSecondaryHeaderMarker);
        }
        if input[..2].eq(b"HH") {
            return Ok(SecondaryHeaderMarker::HHX);
        }
        let marker = &input[..MARKER_SIZE];
        match marker {
            b"HDR" => Ok(SecondaryHeaderMarker::HDR),
            b"P0D" => Ok(SecondaryHeaderMarker::P0D),
            b"PAD" => Ok(SecondaryHeaderMarker::PAD),
            b"CRS" => Ok(SecondaryHeaderMarker::CRS),
            b"FRS" => Ok(SecondaryHeaderMarker::FRS),
            b"PGE" => Ok(SecondaryHeaderMarker::PGE),
            b"PGR" => Ok(SecondaryHeaderMarker::PGR),
            b"GRB" => Ok(SecondaryHeaderMarker::GRB),
            b"EEE" => Ok(SecondaryHeaderMarker::EEE),
            b"SIZ" => Ok(SecondaryHeaderMarker::SIZ),
            _ => Err(ErrMsg::InvalidSecondaryHeaderMarker(input[0], input[1], input[2])),
        }
    }

    pub fn value(&self) -> &[u8] {
        match *self {
            SecondaryHeaderMarker::HDR => b"HDR",
            SecondaryHeaderMarker::P0D => b"P0D",
            SecondaryHeaderMarker::PAD => b"PAD",
            SecondaryHeaderMarker::HHX => b"HH",
            SecondaryHeaderMarker::CRS => b"CRS",
            SecondaryHeaderMarker::FRS => b"FRS",
            SecondaryHeaderMarker::PGE => b"PGE",
            SecondaryHeaderMarker::PGR => b"PGR",
            SecondaryHeaderMarker::GRB => b"GRB",
            SecondaryHeaderMarker::EEE => b"EEE",
            SecondaryHeaderMarker::SIZ => b"SIZ",
        }
    }
}
