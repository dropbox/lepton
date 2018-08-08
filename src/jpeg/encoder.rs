use std::cmp::max;
use std::ops::Range;

use super::constants::{MAX_COMPONENTS, UNZIGZAG};
use super::util::split_into_size_and_value;
use bit_writer::{BitWriter, ShouldEscape};
use io::Write;
use iostream::OutputResult;
use jpeg::ScanInfo;

pub struct JpegEncoder<Writer: Write> {
    pub bit_writer: BitWriter<Writer, ShouldEscape>,
    dc_predictors: [i16; MAX_COMPONENTS],
    eob_run: u16,
}

impl<Writer: Write> JpegEncoder<Writer> {
    pub fn new(output: Writer, dc_predictors: &[i16; MAX_COMPONENTS]) -> Self {
        Self {
            bit_writer: BitWriter::new(output),
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
        if scan.successive_approximation_high == 0 {
            encode_block(
                &mut self.bit_writer,
                coefficients,
                dc_huffman_table,
                ac_huffman_table,
                &scan.spectral_selection,
                &mut self.eob_run,
                &mut self.dc_predictors[component_index_in_scan],
            )
        } else {
            encode_block_successive_approximation(
                &mut self.bit_writer,
                coefficients,
                ac_huffman_table,
                &scan.spectral_selection,
                &mut self.eob_run,
            )
        }
    }

    pub fn reset(&mut self) {
        self.dc_predictors = [0; MAX_COMPONENTS];
        self.eob_run = 0;
    }
}

// TODO: Refactor encode_block and encode_block_successive_approximation
// to reduce duplicated code
fn encode_block<Writer: Write>(
    bit_writer: &mut BitWriter<Writer, ShouldEscape>,
    coefficients: &[i16],
    dc_huffman_table: Option<&[(u16, u8); 256]>,
    ac_huffman_table: Option<&[(u16, u8); 256]>,
    spectral_selection: &Range<u8>,
    eob_run: &mut u16,
    dc_predictor: &mut i16,
) -> OutputResult<()> {
    if spectral_selection.start == 0 {
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
        let mut zero_run = 0;
        let mut index = max(spectral_selection.start, 1);
        let ac_huffman_table = ac_huffman_table.unwrap();
        while index < spectral_selection.end {
            if coefficients[UNZIGZAG[index as usize]] == 0 {
                if index == spectral_selection.end - 1 {
                    *eob_run = coefficients[coefficients.len() - 1] as u16;
                    assert!(*eob_run > 0);
                    if *eob_run == 1 {
                        huffman_encode(0x00, ac_huffman_table, bit_writer)?;
                    } else {
                        let r = 15 - eob_run.leading_zeros() as u8;
                        huffman_encode(r << 4, ac_huffman_table, bit_writer)?;
                        bit_writer.write_bits(*eob_run - (1 << r), r)?;
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
                let (size, value) =
                    split_into_size_and_value(coefficients[UNZIGZAG[index as usize]]);
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

fn encode_block_successive_approximation<Writer: Write>(
    bit_writer: &mut BitWriter<Writer, ShouldEscape>,
    coefficients: &[i16],
    ac_huffman_table: Option<&[(u16, u8); 256]>,
    spectral_selection: &Range<u8>,
    eob_run: &mut u16,
) -> OutputResult<()> {
    if spectral_selection.start == 0 {
        bit_writer.write_bits(coefficients[0] as u16, 1)?;
    }
    if spectral_selection.end > 1 {
        let mut index = max(spectral_selection.start, 1);
        if *eob_run > 0 {
            encode_zero_run(coefficients, index..spectral_selection.end, 64, bit_writer)?;
            *eob_run -= 1;
            return Ok(());
        }
        let mut zero_run = 0;
        let mut zero_run_start = index;
        let ac_huffman_table = ac_huffman_table.unwrap();
        while index < spectral_selection.end {
            let coefficient = coefficients[UNZIGZAG[index as usize]];
            if coefficient & 0x6 == 0x2 {
                while zero_run >= 16 {
                    huffman_encode(0xF0, ac_huffman_table, bit_writer)?;
                    zero_run_start =
                        encode_zero_run(coefficients, zero_run_start..index, 16, bit_writer)?;
                    zero_run -= 16;
                }
                huffman_encode((zero_run << 4) | 0x1, ac_huffman_table, bit_writer)?;
                bit_writer.write_bits(coefficient as u16, 1)?;
                encode_zero_run(coefficients, zero_run_start..index, 64, bit_writer)?;
                zero_run = 0;
                zero_run_start = index + 1;
            } else {
                if coefficient == 0 {
                    zero_run += 1;
                }
                if index == spectral_selection.end - 1 {
                    *eob_run = coefficients[coefficients.len() - 1] as u16;
                    assert!(*eob_run > 0);
                    if *eob_run == 1 {
                        huffman_encode(0x00, ac_huffman_table, bit_writer)?;
                    } else {
                        let r = 15 - eob_run.leading_zeros() as u8;
                        huffman_encode(r << 4, ac_huffman_table, bit_writer)?;
                        bit_writer.write_bits(*eob_run - (1 << r), r)?;
                    }
                    *eob_run -= 1;
                    encode_zero_run(
                        coefficients,
                        zero_run_start..spectral_selection.end,
                        64,
                        bit_writer,
                    )?;
                    break;
                }
            }
            index += 1;
        }
    }
    Ok(())
}

fn huffman_encode<Writer: Write>(
    value: u8,
    table: &[(u16, u8)],
    bit_writer: &mut BitWriter<Writer, ShouldEscape>,
) -> OutputResult<()> {
    let (code, size) = table[value as usize];
    if size > 16 {
        panic!("bad huffman value {:02X}", value);
    }
    bit_writer.write_bits(code, size)
}

fn encode_zero_run<Writer: Write>(
    coefficients: &[i16],
    range: Range<u8>,
    mut zero_run_length: u8,
    bit_writer: &mut BitWriter<Writer, ShouldEscape>,
) -> OutputResult<u8> {
    let end = range.end;
    for i in range {
        let coefficient = coefficients[UNZIGZAG[i as usize]];
        if coefficient == 0 {
            zero_run_length -= 1;
            if zero_run_length == 0 {
                return Ok(i + 1);
            }
        } else {
            bit_writer.write_bits(coefficient as u16 >> 1, 1)?;
        }
    }
    Ok(end)
}
