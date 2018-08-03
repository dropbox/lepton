use io::Write;
use iostream::OutputResult;

pub struct BitWriter<Writer> {
    pub writer: Writer,
    bits: u32,
    n_bit: u8,
    escape: bool,
}

impl<Writer: Write> BitWriter<Writer> {
    pub fn new(writer: Writer, escape: bool) -> Self {
        BitWriter {
            writer,
            bits: 0,
            n_bit: 0,
            escape,
        }
    }

    pub fn write_bits(&mut self, mut bits: u16, size: u8) -> OutputResult<()> {
        if size == 0 {
            return Ok(());
        }
        assert!(size <= 16);
        if size < 16 {
            bits &= (1 << size) - 1;
        }
        self.bits |= u32::from(bits) << (32 - (self.n_bit + size)) as usize;
        self.n_bit += size;
        while self.n_bit >= 8 {
            let byte = (self.bits & (0u32.wrapping_sub(1) << 24)) >> 24;
            self.writer.write(&[byte as u8])?;
            if byte == 0xFF && self.escape {
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

    pub fn n_buffered_bit(&self) -> usize {
        self.n_bit as usize
    }
}
