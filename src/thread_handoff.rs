use byte_converter::{ByteConverter, LittleEndian};
use jpeg_decoder::MAX_COMPONENTS;

pub const BYTES_PER_HANDOFF: usize = 16;
pub const BYTES_PER_HANDOFF_EXT: usize = 20;

#[derive(Clone, Default)]
pub struct ThreadHandoff {
    luma_y_start: u16, // luma -> luminance
    segment_size: u32,
    overhang_byte: u8,
    n_overhang_bit: u8,
    last_dc: [i16; MAX_COMPONENTS],
}

impl ThreadHandoff {
    pub fn serialize(data: &[ThreadHandoff]) -> Vec<u8> {
        let mut result = Vec::<u8>::with_capacity(BYTES_PER_HANDOFF * data.len() + 1);
        result.push(data.len() as u8);
        for handoff in data {
            result.extend(LittleEndian::u16_to_array(handoff.luma_y_start).iter());
            result.extend(LittleEndian::u32_to_array(handoff.segment_size).iter());
            result.push(handoff.overhang_byte);
            result.push(handoff.n_overhang_bit);
            for last_dc in handoff.last_dc.iter() {
                result.extend(LittleEndian::u16_to_array(*last_dc as u16).iter());
            }
        }
        result
    }
}

#[derive(Clone, Default)]
pub struct ThreadHandoffExt {
    pub start_scan: u16,
    pub end_scan: u16,
    pub mcu_y_start: u16,
    pub segment_size: u32, // Size of segment in end_scan
    pub overhang_byte: u8, // No guarantee on value when n_overhang_bit = 0
    pub n_overhang_bit: u8,
    pub last_dc: [i16; 4],
}

impl ThreadHandoffExt {
    pub fn serialize(data: &[ThreadHandoffExt]) -> Vec<u8> {
        let mut result = Vec::<u8>::with_capacity(BYTES_PER_HANDOFF_EXT * data.len() + 1);
        result.push(data.len() as u8);
        for handoff in data {
            result.extend(LittleEndian::u16_to_array(handoff.start_scan).iter());
            result.extend(LittleEndian::u16_to_array(handoff.end_scan).iter());
            result.extend(LittleEndian::u16_to_array(handoff.mcu_y_start).iter());
            result.extend(LittleEndian::u32_to_array(handoff.segment_size).iter());
            result.push(handoff.overhang_byte);
            result.push(handoff.n_overhang_bit);
            for last_dc in handoff.last_dc.iter() {
                result.extend(LittleEndian::u16_to_array(*last_dc as u16).iter());
            }
        }
        result
    }
}
