use super::constants::{MAX_COMPONENTS, UNZIGZAG};
use bit_writer::BitWriter;
use io::BufferedOutputStream;
use iostream::OutputResult;

pub struct JpegEncoder {
    pub bit_writer: BitWriter<BufferedOutputStream>,
    dc_predictors: [i16; MAX_COMPONENTS],
}

impl JpegEncoder {
    pub fn new(output: BufferedOutputStream) -> Self {
        Self {
            bit_writer: BitWriter::new(output, true),
            dc_predictors: [0; MAX_COMPONENTS],
        }
    }

    // FIXME: Handle progressive
    pub fn encode_block(
        &mut self,
        block: &[i16],
        component_index_in_scan: usize,
        dc_huffman_table: Option<&[(u16, u8); 256]>,
        ac_huffman_table: Option<&[(u16, u8); 256]>,
    ) -> OutputResult<()> {
        // Differential DC encoding
        let dc_value = block[0];
        let diff = dc_value.wrapping_sub(self.dc_predictors[component_index_in_scan]);
        self.dc_predictors[component_index_in_scan] = dc_value;
        let (size, value) = encode_coefficient(diff);
        self.huffman_encode(size, dc_huffman_table.clone().unwrap())?;
        self.bit_writer.write_bits(value, size)?;
        // Figure F.2
        let mut zero_run = 0;
        let mut index = 0usize;
        while index < 63 {
            index += 1;
            if block[UNZIGZAG[index] as usize] == 0 {
                if index == 63 {
                    self.huffman_encode(0x00, ac_huffman_table.clone().unwrap())?;
                    break;
                }
                zero_run += 1;
            } else {
                while zero_run > 15 {
                    self.huffman_encode(0xF0, ac_huffman_table.clone().unwrap())?;
                    zero_run -= 16;
                }
                let (size, value) = encode_coefficient(block[UNZIGZAG[index] as usize]);
                let symbol = (zero_run << 4) | size;
                self.huffman_encode(symbol, ac_huffman_table.clone().unwrap())?;
                self.bit_writer.write_bits(value, size)?;
                zero_run = 0;
            }
        }
        Ok(())
    }

    fn huffman_encode(&mut self, val: u8, table: &[(u16, u8)]) -> OutputResult<()> {
        let (code, size) = table[val as usize];
        if size > 16 {
            panic!("bad huffman value");
        }
        self.bit_writer.write_bits(code, size)
    }
}

fn encode_coefficient(coefficient: i16) -> (u8, u16) {
    let mut magnitude = coefficient.abs() as u16;
    let mut num_bits = 0u8;
    while magnitude > 0 {
        magnitude >>= 1;
        num_bits += 1;
    }
    let mask = (1 << num_bits as usize) - 1;
    let val = if coefficient < 0 {
        (coefficient - 1) & mask
    } else {
        coefficient & mask
    };
    (num_bits, val as u16)
}
