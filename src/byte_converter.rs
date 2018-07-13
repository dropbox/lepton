pub trait ByteConverter {
    fn slice_to_u16(slice: &[u8]) -> u16;
    fn slice_to_u32(slice: &[u8]) -> u32;
    fn u16_to_array(n: u16) -> [u8; 2];
    fn u32_to_array(n: u32) -> [u8; 4];
}

pub struct BigEndian {}

impl ByteConverter for BigEndian {
    fn slice_to_u16(slice: &[u8]) -> u16 {
        ((slice[0] as u16) << 8) + (slice[1] as u16)
    }

    fn slice_to_u32(slice: &[u8]) -> u32 {
        ((slice[0] as u32) << 24) + ((slice[1] as u32) << 16) + ((slice[2] as u32) << 8)
            + (slice[3] as u32)
    }

    fn u16_to_array(n: u16) -> [u8; 2] {
        [(n >> 8) as u8, n as u8]
    }

    fn u32_to_array(n: u32) -> [u8; 4] {
        [(n >> 24) as u8, (n >> 16) as u8, (n >> 8) as u8, n as u8]
    }
}

pub struct LittleEndian {}

impl ByteConverter for LittleEndian {
    fn slice_to_u16(slice: &[u8]) -> u16 {
        ((slice[1] as u16) << 8) + (slice[0] as u16)
    }

    fn slice_to_u32(slice: &[u8]) -> u32 {
        ((slice[3] as u32) << 24) + ((slice[2] as u32) << 16) + ((slice[1] as u32) << 8)
            + (slice[0] as u32)
    }

    fn u16_to_array(n: u16) -> [u8; 2] {
        [n as u8, (n >> 8) as u8]
    }

    fn u32_to_array(n: u32) -> [u8; 4] {
        [n as u8, (n >> 8) as u8, (n >> 16) as u8, (n >> 24) as u8]
    }
}
