use super::specialization::{CodecSpecialization, DecoderCodec, EncoderCodec};
use arithmetic_coder::{ArithmeticCoder, ArithmeticDecoder};
use io::BufferedOutputStream;

pub trait StateFactory<Coder: ArithmeticCoder, Specialization: CodecSpecialization>:
    Clone + Send
{
    fn build(output: BufferedOutputStream) -> (Coder, Specialization);
}

#[derive(Clone)]
pub struct EncoderStateFactory {}

impl StateFactory<ArithmeticDecoder, EncoderCodec> for EncoderStateFactory {
    fn build(output: BufferedOutputStream) -> (ArithmeticDecoder, EncoderCodec) {
        (ArithmeticDecoder {}, EncoderCodec::new(output))
    }
}

#[derive(Clone)]
pub struct DecoderStateFactory {}

impl StateFactory<ArithmeticDecoder, DecoderCodec> for DecoderStateFactory {
    fn build(output: BufferedOutputStream) -> (ArithmeticDecoder, DecoderCodec) {
        (ArithmeticDecoder {}, DecoderCodec::new(output))
    }
}
