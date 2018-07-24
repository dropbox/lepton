use std::ops::Range;

use thread_handoff::ThreadHandoffExt;

pub struct Jpeg {
    pub frame: FrameInfo,
    pub scans: Vec<Scan>,
    pub restart_interval: u16, // 0 indicates restart is not enabled
    pub format: Option<FormatInfo>,
}

#[derive(Clone)]
pub struct FrameInfo {
    pub is_baseline: bool,
    pub is_differential: bool,
    pub coding_process: CodingProcess,
    pub entropy_coding: EntropyCoding,
    pub precision: u8,
    pub size: Dimensions,
    pub size_in_mcu: Dimensions,
    pub components: Vec<Component>,
}

pub struct Scan {
    pub raw_header: Vec<u8>, // Raw header bytes after previous SOS up to and including this SOS
    pub info: ScanInfo,
    pub coefficients: Option<Vec<Vec<i16>>>,
    pub truncation: Option<ScanTruncation>,
}

impl Scan {
    pub fn new(raw_header: &[u8], info: ScanInfo) -> Self {
        Scan {
            raw_header: raw_header.to_vec(),
            info,
            coefficients: None,
            truncation: None,
        }
    }

    pub fn is_empty(&self) -> bool {
        match self.truncation {
            Some(ref trunc) => trunc.component_index_in_scan == 0 && trunc.block_y == 0 && trunc.block_x == 0,
            None => false,
        }
    }
}

#[derive(Debug)]
pub struct ScanInfo {
    pub component_indices: Vec<usize>,
    pub dc_table_indices: Vec<usize>,
    pub ac_table_indices: Vec<usize>,
    pub spectral_selection: Range<u8>,
    pub successive_approximation_high: u8,
    pub successive_approximation_low: u8,
}

#[derive(Clone, Debug)]
pub struct Component {
    pub identifier: u8,
    pub horizontal_sampling_factor: u8,
    pub vertical_sampling_factor: u8,
    pub quantization_table_index: usize,
    pub size: Dimensions,
    pub size_in_block: Dimensions,
}

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Dimensions {
    pub width: u16,
    pub height: u16,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CodingProcess {
    DctSequential,
    DctProgressive,
    Lossless,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum EntropyCoding {
    Huffman,
    Arithmetic,
}

pub struct FormatInfo {
    pub len: usize, // This is a rough estimate of the end of entropy-encoded data
    pub pad_byte: u8,
    pub pad_start_bit: u8,
    pub handoff: Vec<ThreadHandoffExt>,
    pub pge: Vec<u8>,
    pub grb: Vec<u8>,
}

impl Default for FormatInfo {
    fn default() -> Self {
        FormatInfo {
            len: 0,
            pad_byte: 0,
            pad_start_bit: 8,
            handoff: vec![],
            pge: vec![],
            grb: vec![],
        }
    }
}

pub struct ScanTruncation {
    pub component_index_in_scan: usize,
    pub block_y: usize,
    pub block_x: usize,
}

impl ScanTruncation {
    pub fn new(component_index_in_scan: usize, block_y: usize, block_x: usize) -> Self {
        ScanTruncation {
            component_index_in_scan,
            block_y,
            block_x,
        }
    }
}
