pub trait ArithmeticCoder: Send {
    fn parse_bit(
        &mut self,
        input: u16,
        input_offset: &mut u8,
        output: &mut u16,
        output_offset: &mut u8,
        prior: &mut u8,
    );
}

pub struct ArithmeticDecoder {}

impl ArithmeticCoder for ArithmeticDecoder {
    fn parse_bit(
        &mut self,
        input: u16,
        input_offset: &mut u8,
        output: &mut u16,
        output_offset: &mut u8,
        _prior: &mut u8,
    ) {
        if *input_offset > 0 || *output_offset > 8 {
            return;
        }
        let byte = (input << *input_offset) & 0xFF00;
        *output |= byte >> *output_offset;
        *input_offset += 8;
        *output_offset += 8;
    }
}
