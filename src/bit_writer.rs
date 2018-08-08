use io::Write;
use iostream::OutputResult;
use core::marker::PhantomData;

pub trait BoolValue {
    #[inline(always)]
    fn value() -> bool;
}

pub struct TrueValue {}
pub struct FalseValue {}
pub type ShouldEscape = TrueValue;
pub type NoEscaping = FalseValue;
impl BoolValue for TrueValue {
    #[inline(always)]
    fn value() -> bool {
        true
    }
}
impl BoolValue for FalseValue {
    #[inline(always)]
    fn value() -> bool {
        false
    }
}
pub struct BitWriter<Writer, Escape: BoolValue> {
    pub writer: Writer,
    bits: u32,
    n_bit: u8,
    escape: PhantomData<Escape>,
}



impl<Writer: Write, Escape: BoolValue> BitWriter<Writer, Escape> {
    pub fn new(writer: Writer) -> Self {
        BitWriter {
            writer,
            bits: 0,
            n_bit: 0,
            escape: PhantomData::default(),
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
            let byte = (self.bits >> 24) as u8;
            self.writer.write(&[byte])?;
            if byte == 0xFF && Escape::value() {
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
}
