use super::specialization::{CodecSpecialization, DecoderCodec, EncoderCodec};
use arithmetic_coder::{ArithmeticCoder, ArithmeticDecoder};
use io::BufferedOutputStream;
use thread_handoff::ThreadHandoffExt;

pub trait StateFactory<Coder: ArithmeticCoder, Specialization: CodecSpecialization>: Send {
    fn build(
        output: BufferedOutputStream,
        thread_handoff: &ThreadHandoffExt,
        mcu_y_end: Option<u16>,
        pad: u8,
    ) -> (Coder, Specialization);
}

pub struct EncoderStateFactory {}

impl StateFactory<ArithmeticDecoder, EncoderCodec> for EncoderStateFactory {
    fn build(
        output: BufferedOutputStream,
        thread_handoff: &ThreadHandoffExt,
        mcu_y_end: Option<u16>,
        _pad: u8,
    ) -> (ArithmeticDecoder, EncoderCodec) {
        (
            ArithmeticDecoder {},
            EncoderCodec::new(output, thread_handoff, mcu_y_end),
        )
    }
}

pub struct DecoderStateFactory {}

impl StateFactory<ArithmeticDecoder, DecoderCodec> for DecoderStateFactory {
    fn build(
        output: BufferedOutputStream,
        thread_handoff: &ThreadHandoffExt,
        _mcu_y_end: Option<u16>,
        pad: u8,
    ) -> (ArithmeticDecoder, DecoderCodec) {
        (
            ArithmeticDecoder {},
            DecoderCodec::new(output, thread_handoff, pad),
        )
    }
}
