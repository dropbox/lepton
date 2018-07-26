pub trait ArithmeticCoder {
    fn parse_bit(
        &mut self,
        input: &mut i16,
        input_offset: &mut u8,
        output: &mut i16,
        output_offset: &mut u8,
        prior: &mut u8,
    );
}

pub struct ArithmeticDecoder {}

impl ArithmeticCoder for ArithmeticDecoder {
    fn parse_bit(
        &mut self,
        input: &mut i16,
        input_offset: &mut u8,
        output: &mut i16,
        output_offset: &mut u8,
        prior: &mut u8,
    ) {
    }
}
