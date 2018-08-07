use std::cmp::max;
use std::ops::Range;

use super::constants::{MAX_COMPONENTS, UNZIGZAG};
use super::util::split_into_size_and_value;
use bit_writer::BitWriter;
use io::Write;
use iostream::OutputResult;
use jpeg::ScanInfo;

pub struct JpegEncoder<Writer: Write> {
    pub bit_writer: BitWriter<Writer>,
    dc_predictors: [i16; MAX_COMPONENTS],
    eob_run: u16,
}

impl<Writer: Write> JpegEncoder<Writer> {
    pub fn new(output: Writer, dc_predictors: &[i16; MAX_COMPONENTS]) -> Self {
        Self {
            bit_writer: BitWriter::new(output, true),
            dc_predictors: dc_predictors.clone(),
            eob_run: 0,
        }
    }

    pub fn encode_block(
        &mut self,
        coefficients: &[i16],
        scan: &ScanInfo,
        component_index_in_scan: usize,
        dc_huffman_table: Option<&[(u16, u8); 256]>,
        ac_huffman_table: Option<&[(u16, u8); 256]>,
    ) -> OutputResult<()> {
        encode_block(
            &mut self.bit_writer,
            coefficients,
            dc_huffman_table,
            ac_huffman_table,
            &scan.spectral_selection,
            &mut self.eob_run,
            &mut self.dc_predictors[component_index_in_scan],
        )
    }

    pub fn handle_rst(&mut self) {
        self.dc_predictors = [0; MAX_COMPONENTS];
        self.eob_run = 0;
    }
}

fn encode_block<Writer: Write>(
    bit_writer: &mut BitWriter<Writer>,
    coefficients: &[i16],
    dc_huffman_table: Option<&[(u16, u8); 256]>,
    ac_huffman_table: Option<&[(u16, u8); 256]>,
    spectral_selection: &Range<u8>,
    eob_run: &mut u16,
    dc_predictor: &mut i16,
) -> OutputResult<()> {
    if spectral_selection.start == 0 {
        // Section F.2.2.1
        // Figure F.12
        let dc_value = coefficients[0];
        let diff = dc_value.wrapping_sub(*dc_predictor);
        *dc_predictor = dc_value;
        let (size, value) = split_into_size_and_value(diff);
        huffman_encode(size, dc_huffman_table.unwrap(), bit_writer)?;
        bit_writer.write_bits(value, size)?;
    }
    if spectral_selection.end > 1 {
        if *eob_run > 0 {
            *eob_run -= 1;
            return Ok(());
        }
        // Section F.1.2.2.1
        let mut zero_run = 0;
        let mut index = max(spectral_selection.start, 1);
        let ac_huffman_table = ac_huffman_table.unwrap();
        while index < spectral_selection.end {
            if coefficients[UNZIGZAG[index as usize]] == 0 {
                // FIXME: Write EOB run
                if index == spectral_selection.end - 1 {
                    *eob_run = coefficients[coefficients.len() - 1] as u16;
                    assert!(*eob_run > 0);
                    if *eob_run == 1 {
                        huffman_encode(0x00, ac_huffman_table, bit_writer)?;
                    } else {
                        let r = 1u8 << (15 - eob_run.leading_zeros());
                        huffman_encode(r << 4, ac_huffman_table, bit_writer)?;
                        bit_writer.write_bits(*eob_run - 1 << r, r)?;
                    }
                    *eob_run -= 1;
                    break;
                }
                zero_run += 1;
            } else {
                while zero_run >= 16 {
                    huffman_encode(0xF0, ac_huffman_table, bit_writer)?;
                    zero_run -= 16;
                }
                let (size, value) = split_into_size_and_value(coefficients[UNZIGZAG[index as usize]]);
                let symbol = (zero_run << 4) | size;
                huffman_encode(symbol, ac_huffman_table, bit_writer)?;
                bit_writer.write_bits(value, size)?;
                zero_run = 0;
            }
            index += 1;
        }
    }
    Ok(())
}

fn huffman_encode<Writer: Write>(
    val: u8,
    table: &[(u16, u8)],
    bit_writer: &mut BitWriter<Writer>,
) -> OutputResult<()> {
    let (code, size) = table[val as usize];
    if size > 16 {
        panic!("bad huffman value");
    }
    bit_writer.write_bits(code, size)
}
