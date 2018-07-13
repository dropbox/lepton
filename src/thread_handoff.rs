use byte_converter::{ByteConverter, LittleEndian};

pub const MAX_N_CHANNEL: usize = 4;
pub const BYTES_PER_HANDOFF: usize = 16;

#[derive(Clone, Default)]
pub struct ThreadHandoff {
    luma_y_start: u16, // luma -> luminance
    segment_size: u32,
    overhang_byte: u8,
    n_overhang_bits: u8,
    last_dc: [u16; MAX_N_CHANNEL],
}

pub fn serialize(data: Vec<ThreadHandoff>) -> Vec<u8> {
    let mut result = Vec::<u8>::with_capacity(BYTES_PER_HANDOFF * data.len() + 1);
    result.push(data.len() as u8);
    for handoff in data.iter() {
        result.push(handoff.luma_y_start as u8);
        result.push((handoff.luma_y_start >> 8) as u8);
        result.extend(LittleEndian::u32_to_array(handoff.segment_size).iter());
        result.push(handoff.overhang_byte);
        result.push(handoff.n_overhang_bits);
        for last_dc in handoff.last_dc.iter() {
            result.push(*last_dc as u8);
            result.push((*last_dc >> 8) as u8);
        }
    }
    result
}
