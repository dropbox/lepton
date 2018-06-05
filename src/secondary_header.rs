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
    pub fn from(input: &[u8]) -> Option<Self> {
        if input[..2].eq(b"HH") {
            return Some(SecondaryHeaderMarker::HHX);
        }
        let marker = &input[..MARKER_SIZE];
        match marker {
            b"HDR" => Some(SecondaryHeaderMarker::HDR),
            b"P0D" => Some(SecondaryHeaderMarker::P0D),
            b"PAD" => Some(SecondaryHeaderMarker::PAD),
            b"CRS" => Some(SecondaryHeaderMarker::CRS),
            b"FRS" => Some(SecondaryHeaderMarker::FRS),
            b"PGE" => Some(SecondaryHeaderMarker::PGE),
            b"PGR" => Some(SecondaryHeaderMarker::PGR),
            b"GRB" => Some(SecondaryHeaderMarker::GRB),
            b"EEE" => Some(SecondaryHeaderMarker::EEE),
            b"SIZ" => Some(SecondaryHeaderMarker::SIZ),
            _ => None,
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
