use iostream::{InputStream, IoStream};
use std::sync::{Arc};
use std::vec;
pub trait ArithmeticCoder: Send {
    fn warm(&mut self,
            _input_stream:&mut InputStream) {}
    fn parse_bit(
        &mut self,
        input_stream: &mut InputStream,
        prior: &mut u8,
        bit: &mut u8);
    fn flush(&mut self) -> &[u8];
}

pub struct ValidatingDecoder {
    input: u16,
    input_offset: u8,
}

impl Default for ValidatingDecoder {
    fn default() -> Self {
        ValidatingDecoder {
            input:0,
            input_offset: 16,
        }
    }
}

impl ArithmeticCoder for ValidatingDecoder {
    fn warm(
        &mut self,
        input_stream: &mut InputStream) {
        while self.input_offset >= 8 {
            self.input_offset -= 8;
            self.input >>= 8;
            if let Ok(byt) = input_stream.read_byte(false) {
                self.input |= u16::from(byt) << 8;
            }
        }
    }
        

    fn parse_bit(
        &mut self,
        input_stream: &mut InputStream,
        prior: &mut u8,
        bit: &mut u8,
    ) {
        assert!(self.input_offset <= 7);
        let desired_prior = (self.input >> (1 + self.input_offset)) & 0xFF;
        *bit = ((self.input >> (self.input_offset)) & 0x1) as u8;
        assert_eq!(desired_prior as u8, *prior);
        self.input_offset += 9;
        if self.input_offset >= 8 {
            self.input_offset -= 8;
            self.input >>= 8;
            if let Ok(byt) = input_stream.read_byte(false) {
                self.input |= u16::from(byt) << 8;
            }
        }
        if self.input_offset >= 8 {
            self.input_offset -= 8;
            self.input >>= 8;
            if let Ok(byt) = input_stream.read_byte(false) {
                self.input |= u16::from(byt) << 8;
            }
        }
    }
    fn flush(&mut self) -> &[u8] {
        &[]
    }
}
#[derive(Default)]
pub struct ValidatingEncoder {
    data: vec::Vec<u8>,
    cur_output:u16,
    output_offset:u8,
    last_flushed: usize,
}

impl ArithmeticCoder for ValidatingEncoder {
    fn parse_bit(
        &mut self,
        _input_stream: &mut InputStream,
        prior: &mut u8,
        bit: &mut u8,
    ) {
        assert!(self.output_offset <= 7);
        if *bit != 0 {
            self.cur_output |= 1 << self.output_offset;
        }
        self.cur_output |= u16::from(*prior) << (self.output_offset + 1);
        self.output_offset += 9;
        self.data.push(self.cur_output as u8 & 0xff);
        self.cur_output >>= 8;
        self.output_offset -= 8;
        if self.output_offset >= 8 {
            self.data.push(self.cur_output as u8 & 0xff);
            self.cur_output >>= 8;
            self.output_offset -= 8;
        }
    }
    fn flush(&mut self) -> &[u8] {
        while self.output_offset != 0 {
            self.data.push(self.cur_output as u8 & 0xff);
            self.cur_output >>= 8;
            if self.output_offset >= 8 {
                self.output_offset -= 8;
            } else {
                self.output_offset = 0;
            }
        }
        let lf = self.last_flushed;
        self.last_flushed = self.data.len();
        &self.data[lf..]
    }
}



mod test {
    use super::*;
    #[test]
    fn test_arithmetic_validator() {
        let mut e = ValidatingEncoder::default();
        let mut d = ValidatingDecoder::default();
        let buf = Arc::new(IoStream::default());
        let mut io = InputStream::new(buf.clone(), 1024);
        e.warm(&mut io);
        let mut prior = 128u8;
        let mut bit = 1;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        prior = 64u8;
        bit = 0;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        prior = 32u8;
        bit = 0;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        prior = 16u8;
        bit = 0;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        prior = 255u8;
        bit = 1;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        buf.write(e.flush());
        buf.write(&[0,0,0,0]);
        d.warm(&mut io);
        prior = 128u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
        assert_eq!(bit, 1);
        prior = 64u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
        assert_eq!(bit, 0);        
        prior = 32u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
        assert_eq!(bit, 0);
        prior = 16u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
        assert_eq!(bit, 0);
        prior = 255u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
        assert_eq!(bit, 1);
    }
    #[test]
    #[should_panic]
    fn test_arithmetic_validator_wrong_prob() {
        let mut e = ValidatingEncoder::default();
        let mut d = ValidatingDecoder::default();
        let buf = Arc::new(IoStream::default());
        let mut io = InputStream::new(buf.clone(), 1024);
        e.warm(&mut io);
        let mut prior = 128u8;
        let mut bit = 1;
        e.parse_bit(&mut io, &mut prior, &mut bit);
        buf.write(e.flush());
        d.warm(&mut io);
        prior = 127u8;
        bit = 2;
        d.parse_bit(&mut io, &mut prior, &mut bit);
    }
}
