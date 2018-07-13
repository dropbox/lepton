use byte_converter::{ByteConverter, LittleEndian};
use interface::ErrMsg;

pub const LEPTON_VERSION: u8 = 2;
pub const HEADER_SIZE: usize = 28;
pub static MAGIC_NUMBER: [u8; 2] = [0xcf, 0x84];

#[derive(Clone, Copy)]
pub struct PrimaryHeader {
    pub version: u8,
    pub skip_hdr: bool,
    pub n_threads: u32,
    pub git_hash: [u8; 12],
    pub raw_size: usize,
    pub secondary_hdr_size: usize,
}

pub fn serialize_header(
    skip_serial_hdr: u8,
    n_threads: u32,
    git_hash: &[u8; 12],
    raw_size: usize,
    secondary_hdr_size: usize,
) -> [u8; HEADER_SIZE] {
    let mut header = [0u8; HEADER_SIZE];
    header[..MAGIC_NUMBER.len()].clone_from_slice(&MAGIC_NUMBER);
    header[2] = LEPTON_VERSION;
    header[3] = skip_serial_hdr;
    header[4..8].clone_from_slice(&LittleEndian::u32_to_array(n_threads));
    header[8..20].clone_from_slice(git_hash);
    header[20..24].clone_from_slice(&LittleEndian::u32_to_array(raw_size as u32));
    header[24..].clone_from_slice(&LittleEndian::u32_to_array(secondary_hdr_size as u32));
    header
}

pub fn deserialize_header(data: &[u8]) -> Result<PrimaryHeader, ErrMsg> {
    if data.len() < HEADER_SIZE {
        Err(ErrMsg::IncompletePrimaryHeader)
    } else if !data[..2].eq(&MAGIC_NUMBER) {
        Err(ErrMsg::WrongMagicNumber)
    } else {
        let mut header = PrimaryHeader {
            version: data[2],
            skip_hdr: if data[3] == 0 { false } else { true },
            n_threads: LittleEndian::slice_to_u32(&data[4..]),
            git_hash: [0u8; 12],
            raw_size: LittleEndian::slice_to_u32(&data[20..]) as usize,
            secondary_hdr_size: LittleEndian::slice_to_u32(&data[24..]) as usize,
        };
        header.git_hash.clone_from_slice(&data[8..20]);
        Ok(header)
    }
}
