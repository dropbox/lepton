// Table B.1
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Marker {
    /// Start Of Frame markers
    ///
    /// - SOF(0):  Baseline DCT (Huffman coding)
    /// - SOF(1):  Extended sequential DCT (Huffman coding)
    /// - SOF(2):  Progressive DCT (Huffman coding)
    /// - SOF(3):  Lossless (sequential) (Huffman coding)
    /// - SOF(5):  Differential sequential DCT (Huffman coding)
    /// - SOF(6):  Differential progressive DCT (Huffman coding)
    /// - SOF(7):  Differential lossless (sequential) (Huffman coding)
    /// - SOF(9):  Extended sequential DCT (arithmetic coding)
    /// - SOF(10): Progressive DCT (arithmetic coding)
    /// - SOF(11): Lossless (sequential) (arithmetic coding)
    /// - SOF(13): Differential sequential DCT (arithmetic coding)
    /// - SOF(14): Differential progressive DCT (arithmetic coding)
    /// - SOF(15): Differential lossless (sequential) (arithmetic coding)
    SOF(u8),
    /// Reserved for JPEG extensions
    JPG,
    /// Define Huffman table(s)
    DHT,
    /// Define arithmetic coding conditioning(s)
    DAC,
    /// Restart with modulo 8 count `m`
    RST(u8),
    /// Start of image
    SOI,
    /// End of image
    EOI,
    /// Start of scan
    SOS,
    /// Define quantization table(s)
    DQT,
    /// Define number of lines
    DNL,
    /// Define restart interval
    DRI,
    /// Define hierarchical progression
    DHP,
    /// Expand reference component(s)
    EXP,
    /// Reserved for application segments
    APP(u8),
    /// Reserved for JPEG extensions
    JPGn(u8),
    /// Comment
    COM,
    /// For temporary private use in arithmetic coding
    TEM,
    /// Reserved
    RES,
}

impl Marker {
    pub fn has_length(self) -> bool {
        use self::Marker::*;
        match self {
            RST(..) | SOI | EOI | TEM => false,
            _ => true,
        }
    }

    pub fn from_u8(n: u8) -> Result<Marker, InvalidMarker> {
        use self::Marker::*;
        match n {
            0x00 => Err(InvalidMarker(n)), // Byte stuffing
            0x01 => Ok(TEM),
            0x02 ... 0xBF => Ok(RES),
            0xC0 => Ok(SOF(0)),
            0xC1 => Ok(SOF(1)),
            0xC2 => Ok(SOF(2)),
            0xC3 => Ok(SOF(3)),
            0xC4 => Ok(DHT),
            0xC5 => Ok(SOF(5)),
            0xC6 => Ok(SOF(6)),
            0xC7 => Ok(SOF(7)),
            0xC8 => Ok(JPG),
            0xC9 => Ok(SOF(9)),
            0xCA => Ok(SOF(10)),
            0xCB => Ok(SOF(11)),
            0xCC => Ok(DAC),
            0xCD => Ok(SOF(13)),
            0xCE => Ok(SOF(14)),
            0xCF => Ok(SOF(15)),
            0xD0 => Ok(RST(0)),
            0xD1 => Ok(RST(1)),
            0xD2 => Ok(RST(2)),
            0xD3 => Ok(RST(3)),
            0xD4 => Ok(RST(4)),
            0xD5 => Ok(RST(5)),
            0xD6 => Ok(RST(6)),
            0xD7 => Ok(RST(7)),
            0xD8 => Ok(SOI),
            0xD9 => Ok(EOI),
            0xDA => Ok(SOS),
            0xDB => Ok(DQT),
            0xDC => Ok(DNL),
            0xDD => Ok(DRI),
            0xDE => Ok(DHP),
            0xDF => Ok(EXP),
            0xE0 => Ok(APP(0)),
            0xE1 => Ok(APP(1)),
            0xE2 => Ok(APP(2)),
            0xE3 => Ok(APP(3)),
            0xE4 => Ok(APP(4)),
            0xE5 => Ok(APP(5)),
            0xE6 => Ok(APP(6)),
            0xE7 => Ok(APP(7)),
            0xE8 => Ok(APP(8)),
            0xE9 => Ok(APP(9)),
            0xEA => Ok(APP(10)),
            0xEB => Ok(APP(11)),
            0xEC => Ok(APP(12)),
            0xED => Ok(APP(13)),
            0xEE => Ok(APP(14)),
            0xEF => Ok(APP(15)),
            0xF0 => Ok(JPGn(0)),
            0xF1 => Ok(JPGn(1)),
            0xF2 => Ok(JPGn(2)),
            0xF3 => Ok(JPGn(3)),
            0xF4 => Ok(JPGn(4)),
            0xF5 => Ok(JPGn(5)),
            0xF6 => Ok(JPGn(6)),
            0xF7 => Ok(JPGn(7)),
            0xF8 => Ok(JPGn(8)),
            0xF9 => Ok(JPGn(9)),
            0xFA => Ok(JPGn(10)),
            0xFB => Ok(JPGn(11)),
            0xFC => Ok(JPGn(12)),
            0xFD => Ok(JPGn(13)),
            0xFE => Ok(COM),
            0xFF => Err(InvalidMarker(n)), // Fill byte
            _ => unreachable!(),
        }
    }
}

#[derive(Debug, PartialEq)]
pub struct InvalidMarker(u8);
