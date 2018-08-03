use byte_converter::{ByteConverter, LittleEndian};
use jpeg::MAX_COMPONENTS;

pub trait ThreadHandoff: Sized {
    const BYTES_PER_HANDOFF: usize;
    const NAME: &'static str;
    fn serialize(data: &[Self]) -> Vec<u8>;
    fn deserialize_unchecked(data: &[u8]) -> Self;
    fn deserialize(data: &[u8]) -> Self {
        check_len(data.len(), Self::BYTES_PER_HANDOFF, Self::NAME);
        Self::deserialize_unchecked(data)
    }
}

#[derive(Clone, Default)]
pub struct ThreadHandoffOld {
    luma_y_start: u16, // luma -> luminance
    segment_size: u32,
    overhang_byte: u8,
    n_overhang_bit: u8,
    last_dc: [i16; MAX_COMPONENTS],
}

impl ThreadHandoff for ThreadHandoffOld {
    const BYTES_PER_HANDOFF: usize = 16;
    const NAME: &'static str = "ThreadHandoffOld";

    fn serialize(data: &[Self]) -> Vec<u8> {
        let mut result = Vec::<u8>::with_capacity(Self::BYTES_PER_HANDOFF * data.len() + 1);
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

    fn deserialize_unchecked(data: &[u8]) -> Self {
        let mut last_dc = [0i16; MAX_COMPONENTS];
        let mut last_dc_start = 8;
        for i in 0..MAX_COMPONENTS {
            last_dc[i] =
                LittleEndian::slice_to_u16(&data[last_dc_start..(last_dc_start + 2)]) as i16;
            last_dc_start += 2;
        }
        ThreadHandoffOld {
            luma_y_start: LittleEndian::slice_to_u16(&data[..2]),
            segment_size: LittleEndian::slice_to_u32(&data[2..6]),
            overhang_byte: data[6],
            n_overhang_bit: data[7],
            last_dc,
        }
    }
}

#[derive(Clone, Default)]
pub struct ThreadHandoffExt {
    pub start_scan: u16,
    pub end_scan: u16,
    pub mcu_y_start: u16,
    pub segment_size: u32,
    pub overhang_byte: u8, // No guarantee on value when n_overhang_bit = 0
    pub n_overhang_bit: u8,
    pub last_dc: [i16; MAX_COMPONENTS],
}

impl ThreadHandoff for ThreadHandoffExt {
    const BYTES_PER_HANDOFF: usize = 20;
    const NAME: &'static str = "ThreadHandoffExt";

    fn serialize(data: &[Self]) -> Vec<u8> {
        let mut result = Vec::<u8>::with_capacity(Self::BYTES_PER_HANDOFF * data.len() + 1);
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

    fn deserialize_unchecked(data: &[u8]) -> Self {
        Self::from_thread_handoff(
            ThreadHandoffOld::deserialize(&data[4..]),
            LittleEndian::slice_to_u16(&data[..2]),
            LittleEndian::slice_to_u16(&data[2..4]),
        )
    }
}

impl From<ThreadHandoffOld> for ThreadHandoffExt {
    fn from(handoff: ThreadHandoffOld) -> Self {
        Self::from_thread_handoff(handoff, 0, 0)
    }
}

impl ThreadHandoffExt {
    fn from_thread_handoff(handoff: ThreadHandoffOld, start_scan: u16, end_scan: u16) -> Self {
        ThreadHandoffExt {
            start_scan,
            end_scan,
            mcu_y_start: handoff.luma_y_start,
            segment_size: handoff.segment_size,
            overhang_byte: handoff.overhang_byte,
            n_overhang_bit: handoff.n_overhang_bit,
            last_dc: handoff.last_dc,
        }
    }
}

pub fn deserialize_all<T: ThreadHandoff>(data: &[u8]) -> Vec<T> {
    let n = data.len() / T::BYTES_PER_HANDOFF;
    check_len(
        data.len(),
        T::BYTES_PER_HANDOFF * n,
        &format!("{} {}", n, T::NAME),
    );
    let mut start = 0;
    let mut result = Vec::with_capacity(n);
    for _ in 0..n {
        result.push(T::deserialize_unchecked(
            &data[start..(start + T::BYTES_PER_HANDOFF)],
        ));
        start += T::BYTES_PER_HANDOFF;
    }
    result
}

fn check_len(actual_len: usize, target_len: usize, name: &str) {
    if actual_len != target_len {
        panic!(
            "Wrong number of bytes for {}: {} needed, {} provided",
            name, target_len, actual_len
        );
    }
}
