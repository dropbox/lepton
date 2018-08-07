use std::cmp::min;
use std::fmt;
use std::iter::repeat;

use super::error::{HuffmanError, JpegError, JpegResult};
use super::jpeg::ScanInfo;
use super::marker::Marker;
use super::util::build_from_size_and_value;
use iostream::{InputResult, InputStream};

pub type HuffmanResult<T> = Result<T, HuffmanError>;

const LUT_BITS: u8 = 8; // LUT = Lookup Table

#[derive(Debug)]
pub struct HuffmanDecoder {
    bits: u64,
    bit_start: u8,
    n_bit: u8,
    buffer: Vec<u8>,
    is_eof: bool,
    start_byte: usize,
    pge: Vec<u8>,
}

impl HuffmanDecoder {
    pub fn new(start_byte: usize) -> HuffmanDecoder {
        HuffmanDecoder {
            bits: 0,
            bit_start: 0,
            n_bit: 0,
            buffer: vec![],
            is_eof: false,
            start_byte,
            pge: vec![],
        }
    }

    // Section F.2.2.3
    // Figure F.16
    pub fn decode(
        &mut self,
        input: &mut InputStream,
        table: &HuffmanDecodeTable,
    ) -> HuffmanResult<u8> {
        let (value, size) = table.lut[self.peek_bits(input, LUT_BITS, true)? as usize];
        if size > 0 {
            self.consume_bits(size);
            Ok(value)
        } else {
            let bits = self.peek_bits(input, 16, true)?;
            for i in LUT_BITS..min(16, self.n_bit) {
                let code = (bits >> (15 - i)) as i32;
                if code <= table.maxcode[i as usize] {
                    self.consume_bits(i + 1);
                    let index = (code + table.delta[i as usize]) as usize;
                    return Ok(table.values[index]);
                }
            }
            if self.n_bit < 16 {
                Err(HuffmanError::EOF)
            } else {
                Err(HuffmanError::BadCode)
            }
        }
    }

    pub fn decode_fast_ac(
        &mut self,
        input: &mut InputStream,
        table: &HuffmanDecodeTable,
    ) -> HuffmanResult<Option<(i16, u8)>> {
        if let Some(ref ac_lut) = table.ac_lut {
            let (value, run_size) = ac_lut[self.peek_bits(input, LUT_BITS, true)? as usize];
            if run_size != 0 {
                let run = run_size >> 4;
                let size = run_size & 0x0f;
                self.consume_bits(size);
                return Ok(Some((value, run)));
            }
        }
        Ok(None)
    }

    #[inline]
    pub fn get_bits(&mut self, input: &mut InputStream, count: u8) -> HuffmanResult<u16> {
        let bits = self.peek_bits(input, count, false)?;
        self.consume_bits(count);
        Ok(bits)
    }

    #[inline]
    pub fn receive_extend(&mut self, input: &mut InputStream, count: u8) -> HuffmanResult<i16> {
        let value = self.get_bits(input, count)?;
        Ok(build_from_size_and_value(count, value))
    }

    pub fn read_rst(
        &mut self,
        input: &mut InputStream,
        expected_rst_num: u8,
    ) -> JpegResult<Marker> {
        if self.read_byte(input)? != 0xFF {
            return Err(JpegError::Malformatted(
                "no marker found where expected".to_owned(),
            ));
        }
        let mut byte = self.read_byte(input)?;
        // Section B.1.1.2
        // "Any marker may optionally be preceded by any number of fill bytes, which are bytes assigned code X’FF’."
        while byte == 0xFF {
            byte = self.read_byte(input)?;
        }
        match byte {
            0x00 => Err(JpegError::Malformatted(
                "0xFF00 found where RST marker was expected".to_owned(),
            )),
            _ => match Marker::from_u8(byte).unwrap() {
                Marker::RST(n) => {
                    if n != expected_rst_num {
                        Err(JpegError::Malformatted(format!(
                            "found RST{} where RST{} was expected",
                            n, expected_rst_num
                        )))
                    } else {
                        Ok(Marker::RST(n))
                    }
                }
                other => Err(JpegError::Malformatted(format!(
                    "found marker {:?} inside scan where RST{} was expected",
                    other, expected_rst_num
                ))),
            },
        }
    }

    pub fn n_available_bit(&self) -> u8 {
        self.n_bit
    }

    pub fn reset(&mut self) {
        self.bits = 0;
        self.bit_start = 0;
        self.n_bit = 0;
    }

    pub fn clear_buffer(&mut self) {
        self.buffer.clear()
    }

    pub fn view_buffer(&self) -> &[u8] {
        &self.buffer
    }

    /// Returns the overhanging byte and the bit position where the
    /// overhang ends.
    pub fn handover_byte(&self) -> (u8, u8) {
        ((self.bits >> 56) as u8, self.bit_start)
    }

    pub fn end_pge(&mut self) {
        self.start_byte = 0;
    }

    pub fn get_pge(&self) -> &[u8] {
        &self.pge
    }

    #[inline]
    fn peek_bits(
        &mut self,
        input: &mut InputStream,
        count: u8,
        zero_fill: bool,
    ) -> HuffmanResult<u16> {
        assert!(count <= 16);
        if self.n_bit < count {
            if let Err(e) = self.read_bits(input, count) {
                if !zero_fill || self.n_bit == 0 {
                    return Err(e);
                }
            }
        }
        Ok(((self.bits >> (64 - self.bit_start - count)) & ((1 << count) - 1)) as u16)
    }

    #[inline]
    fn consume_bits(&mut self, count: u8) {
        // FIXME: Return error instead of panicking
        assert!(count <= self.n_bit);
        self.bit_start += count;
        self.n_bit -= count;
        while self.bit_start >= 8 {
            self.bit_start -= 8;
            self.bits <<= 8;
        }
    }

    fn read_bits(&mut self, input: &mut InputStream, count: u8) -> HuffmanResult<()> {
        assert!(count <= 57);
        while self.n_bit < count {
            let byte = input.peek_byte()?;
            if byte == 0xFF {
                let mut buf = [0u8; 2];
                input.peek(&mut buf)?;
                if buf[1] != 0x00 {
                    return Err(HuffmanError::UnexpectedMarker(
                        Marker::from_u8(buf[1]).unwrap(),
                    ));
                }
                self.read_byte(input)?;
            }
            self.read_byte(input)?;
            self.bits |= (byte as u64) << (56 - self.bit_start - self.n_bit);
            self.n_bit += 8;
        }
        Ok(())
    }

    fn read_byte(&mut self, input: &mut InputStream) -> InputResult<u8> {
        match input.read_byte(false) {
            Ok(byte) => {
                if self.start_byte > 0 && input.processed_len() > self.start_byte {
                    self.pge.push(byte);
                }
                self.buffer.push(byte);
                Ok(byte)
            }
            Err(e) => Err(e),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum HuffmanTableClass {
    DC,
    AC,
}

#[derive(Clone)]
pub enum HuffmanTable {
    Empty,
    DecodeTable(HuffmanDecodeTable),
    EncodeTable([(u16, u8); 256]),
}

impl HuffmanTable {
    pub fn new(
        bits: &[u8; 16],
        values: &[u8],
        class: HuffmanTableClass,
        encode: bool,
    ) -> HuffmanResult<HuffmanTable> {
        Ok(if encode {
            HuffmanTable::EncodeTable(huffman_encode_table(bits, values)?)
        } else {
            HuffmanTable::DecodeTable(HuffmanDecodeTable::new(bits, values, class)?)
        })
    }

    pub fn is_empty(&self) -> bool {
        enum_match!(self, HuffmanTable::Empty)
    }

    // pub fn is_decode_table(&self) -> bool {
    //     enum_match!(self, HuffmanTable::DecodeTable(_))
    // }

    // pub fn is_encode_table(&self) -> bool {
    //     enum_match!(self, HuffmanTable::EncodeTable(_))
    // }

    pub fn decode_table(&self) -> &HuffmanDecodeTable {
        match self {
            HuffmanTable::DecodeTable(ref table) => table,
            _ => self.panic("decode_table"),
        }
    }

    // pub fn encode_table(&self) -> &[(u16, u8); 256] {
    //     match self {
    //         HuffmanTable::EncodeTable(ref table) => table,
    //         _ => self.panic("encode_table"),
    //     }
    // }

    pub fn clone_encode_table(&self) -> Option<[(u16, u8); 256]> {
        match self {
            HuffmanTable::EncodeTable(ref table) => Some(table.clone()),
            _ => None,
        }
    }

    fn panic<T>(&self, method_name: &str) -> T {
        use self::HuffmanTable::*;
        panic!(
            "called HuffmanTable::{}() on {} `{:?}` value",
            method_name,
            match self {
                Empty => "an",
                DecodeTable(_) => "a",
                EncodeTable(_) => "an",
            },
            self
        );
    }
}

impl fmt::Debug for HuffmanTable {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use self::HuffmanTable::*;
        write!(
            f,
            "{}",
            match self {
                Empty => "Empty",
                DecodeTable(_) => "DecodeTable",
                EncodeTable(_) => "EncodeTable",
            }
        )
    }
}

#[derive(Clone)]
pub struct HuffmanDecodeTable {
    values: Vec<u8>,
    delta: [i32; 16],
    maxcode: [i32; 16],
    lut: [(u8, u8); 1 << LUT_BITS],
    ac_lut: Option<[(i16, u8); 1 << LUT_BITS]>,
}

impl HuffmanDecodeTable {
    pub fn new(
        bits: &[u8; 16],
        values: &[u8],
        class: HuffmanTableClass,
    ) -> HuffmanResult<HuffmanDecodeTable> {
        let (huffcode, huffsize) = derive_huffman_codes(bits)?;
        // Section F.2.2.3
        // Figure F.15
        // delta[i] is set to VALPTR(I) - MINCODE(I)
        let mut delta = [0i32; 16];
        let mut maxcode = [-1i32; 16];
        let mut j = 0;
        for i in 0..16 {
            if bits[i] != 0 {
                delta[i] = j as i32 - huffcode[j] as i32;
                j += bits[i] as usize;
                maxcode[i] = huffcode[j - 1] as i32;
            }
        }
        // Build a lookup table for faster decoding.
        let mut lut = [(0u8, 0u8); 1 << LUT_BITS];
        for (i, &size) in huffsize
            .iter()
            .enumerate()
            .filter(|&(_, &size)| size <= LUT_BITS)
        {
            let bits_remaining = LUT_BITS - size;
            let start = (huffcode[i] << bits_remaining) as usize;
            for j in 0..1 << bits_remaining {
                lut[start + j] = (values[i], size);
            }
        }
        // Build a lookup table for small AC coefficients which both decodes the value and does the
        // equivalent of receive_extend.
        let ac_lut = match class {
            HuffmanTableClass::DC => None,
            HuffmanTableClass::AC => {
                let mut table = [(0i16, 0u8); 1 << LUT_BITS];
                for (i, &(value, size)) in lut.iter().enumerate() {
                    let run_length = value >> 4;
                    let magnitude_category = value & 0x0f;
                    if magnitude_category > 0 && size + magnitude_category <= LUT_BITS {
                        let unextended_ac_value = (((i << size) & ((1 << LUT_BITS) - 1))
                            >> (LUT_BITS - magnitude_category))
                            as u16;
                        let ac_value = build_from_size_and_value(magnitude_category, unextended_ac_value);
                        table[i] = (ac_value, (run_length << 4) | (size + magnitude_category));
                    }
                }
                Some(table)
            }
        };
        Ok(HuffmanDecodeTable {
            values: values.to_vec(),
            delta: delta,
            maxcode: maxcode,
            lut: lut,
            ac_lut: ac_lut,
        })
    }
}

fn huffman_encode_table(bits: &[u8; 16], values: &[u8]) -> HuffmanResult<[(u16, u8); 256]> {
    let mut table = [(0u16, 17u8); 256];
    let (huffcode, huffsize) = derive_huffman_codes(bits)?;
    for (i, &v) in values.iter().enumerate() {
        table[v as usize] = (huffcode[i], huffsize[i]);
    }
    Ok(table)
}

// Section C.2
fn derive_huffman_codes(bits: &[u8]) -> HuffmanResult<(Vec<u16>, Vec<u8>)> {
    // Figure C.1
    let huffsize = bits.iter()
        .enumerate()
        .fold(vec![], |mut acc, (i, &value)| {
            acc.extend(repeat((i + 1) as u8).take(value as usize));
            acc
        });
    // Figure C.2
    let mut huffcode = vec![0u16; huffsize.len()];
    let mut code_size = huffsize[0];
    let mut code = 0u32;
    for (i, &size) in huffsize.iter().enumerate() {
        while code_size < size {
            code <<= 1;
            code_size += 1;
        }
        if code >= (1u32 << size) {
            return Err(HuffmanError::BadTable);
        }
        huffcode[i] = code as u16;
        code += 1;
    }
    Ok((huffcode, huffsize))
}

// https://www.loc.gov/preservation/digital/formats/fdd/fdd000063.shtml
// "Avery Lee, writing in the rec.video.desktop newsgroup in 2001, commented that "MJPEG, or at
//  least the MJPEG in AVIs having the MJPG fourcc, is restricted JPEG with a fixed -- and
//  *omitted* -- Huffman table. The JPEG must be YCbCr colorspace, it must be 4:2:2, and it must
//  use basic Huffman encoding, not arithmetic or progressive.... You can indeed extract the
//  MJPEG frames and decode them with a regular JPEG decoder, but you have to prepend the DHT
//  segment to them, or else the decoder won't have any idea how to decompress the data.
//  The exact table necessary is given in the OpenDML spec.""
pub fn fill_default_mjpeg_tables(
    scan: &ScanInfo,
    dc_huffman_tables: &mut [HuffmanTable],
    ac_huffman_tables: &mut [HuffmanTable],
    encode: bool,
) {
    // Section K.3.3
    if dc_huffman_tables[0].is_empty() && scan.dc_table_indices.iter().any(|&i| i == 0) {
        // Table K.3
        dc_huffman_tables[0] = HuffmanTable::new(
            &[
                0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00,
            ],
            &[
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
            ],
            HuffmanTableClass::DC,
            encode,
        ).unwrap();
    }
    if dc_huffman_tables[1].is_empty() && scan.dc_table_indices.iter().any(|&i| i == 1) {
        // Table K.4
        dc_huffman_tables[1] = HuffmanTable::new(
            &[
                0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00,
                0x00, 0x00,
            ],
            &[
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
            ],
            HuffmanTableClass::DC,
            encode,
        ).unwrap();
    }
    if ac_huffman_tables[0].is_empty() && scan.ac_table_indices.iter().any(|&i| i == 0) {
        // Table K.5
        ac_huffman_tables[0] = HuffmanTable::new(
            &[
                0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00,
                0x01, 0x7D,
            ],
            &[
                0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51,
                0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1,
                0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18,
                0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57,
                0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
                0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92,
                0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
                0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
                0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
                0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2,
                0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
            ],
            HuffmanTableClass::AC,
            encode,
        ).unwrap();
    }
    if ac_huffman_tables[1].is_empty() && scan.ac_table_indices.iter().any(|&i| i == 1) {
        // Table K.6
        ac_huffman_tables[1] = HuffmanTable::new(
            &[
                0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01,
                0x02, 0x77,
            ],
            &[
                0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07,
                0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09,
                0x23, 0x33, 0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25,
                0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
                0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
                0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74,
                0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
                0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA,
                0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
                0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2,
                0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
            ],
            HuffmanTableClass::AC,
            encode,
        ).unwrap();
    }
}
