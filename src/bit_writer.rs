use io::Write;
use iostream::OutputResult;
use jpeg_decoder::UNZIGZAG;

pub struct BitWriter<'a, Writer: 'a> {
    writer: &'a mut Writer,
    bits: u32,
    n_bit: u8,
}

impl<'a, Writer: Write + 'a> BitWriter<'a, Writer> {
    pub fn new(writer: &'a mut Writer) -> Self {
        BitWriter {
            writer,
            bits: 0,
            n_bit: 0,
        }
    }

    pub fn write_bits(&mut self, bits: u16, size: u8) -> OutputResult<()> {
        if size == 0 {
            return Ok(());
        }
        assert!(size <= 16);
        self.bits |= u32::from(bits) << (32 - (self.n_bit + size)) as usize;
        self.n_bit += size;
        while self.n_bit >= 8 {
            let byte = (self.bits & (0u32.wrapping_sub(1) << 24)) >> 24;
            self.writer.write(&[byte as u8])?;
            if byte == 0xFF {
                self.writer.write(&[0x00])?;
            }
            self.n_bit -= 8;
            self.bits <<= 8;
        }
        Ok(())
    }

    pub fn pad_byte(&mut self, pad_byte: u8) -> OutputResult<()> {
        let n_pad_bit = 8 - self.n_bit % 8;
        if n_pad_bit < 8 {
            self.write_bits(pad_byte as u16, n_pad_bit)
        } else {
            Ok(())
        }
    }

    pub fn huffman_encode(&mut self, val: u8, table: &[(u8, u16)]) -> OutputResult<()> {
        let (size, code) = table[val as usize];
        if size > 16 {
            panic!("bad huffman value");
        }
        self.write_bits(code, size)
    }

    pub fn write_block(
        &mut self,
        block: &[i16],
        prevdc: i16,
        dc_table: &[(u8, u16)],
        ac_table: &[(u8, u16)],
    ) -> OutputResult<i16> {
        // Differential DC encoding
        let dcval = block[0];
        let diff = dcval - prevdc;
        let (size, value) = encode_coefficient(diff);
        self.huffman_encode(size, dc_table)?;
        self.write_bits(value, size)?;
        // Figure F.2
        let mut zero_run = 0;
        let mut k = 0usize;
        loop {
            k += 1;
            if block[UNZIGZAG[k] as usize] == 0 {
                if k == 63 {
                    self.huffman_encode(0x00, ac_table)?;
                    break;
                }
                zero_run += 1;
            } else {
                while zero_run > 15 {
                    self.huffman_encode(0xF0, ac_table)?;
                    zero_run -= 16;
                }
                let (size, value) = encode_coefficient(block[UNZIGZAG[k] as usize]);
                let symbol = (zero_run << 4) | size;
                self.huffman_encode(symbol, ac_table)?;
                self.write_bits(value, size)?;
                zero_run = 0;
                if k == 63 {
                    break;
                }
            }
        }
        Ok(dcval)
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
