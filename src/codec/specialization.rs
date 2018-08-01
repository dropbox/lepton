use super::codec::CodecError;
use bit_writer::BitWriter;
use byte_converter::{BigEndian, ByteConverter};
use interface::{ErrMsg, SimpleResult};
use io::{BufferedOutputStream, Write};
use iostream::InputStream;
use jpeg::{Component, JpegEncoder, Scan};

pub trait CodecSpecialization: Send {
    fn process_block(
        &mut self,
        input: &mut InputStream,
        y: usize,
        x: usize,
        component_index_in_scan: usize,
        component: &Component,
        scan: &mut Scan,
    ) -> SimpleResult<ErrMsg>;
    fn flush(&mut self) -> SimpleResult<CodecError>;
    fn write_eof(&mut self);
}

pub struct DecoderCodec {
    jpeg_encoder: JpegEncoder,
}

impl DecoderCodec {
    pub fn new(output: BufferedOutputStream) -> Self {
        Self {
            jpeg_encoder: JpegEncoder::new(output),
        }
    }
}

impl CodecSpecialization for DecoderCodec {
    fn process_block(
        &mut self,
        input: &mut InputStream,
        _y: usize,
        _x: usize,
        component_index_in_scan: usize,
        _component: &Component,
        scan: &mut Scan,
    ) -> SimpleResult<ErrMsg> {
        let mut block_u8 = [0; 128];
        let mut block = [0i16; 64];
        input.read(&mut block_u8, true, false).unwrap(); // FIXME
        for (i, coefficient) in block.iter_mut().enumerate() {
            *coefficient = BigEndian::slice_to_u16(&block_u8[(2 * i)..]) as i16;
        }
        self.jpeg_encoder
            .encode_block(
                &block,
                component_index_in_scan,
                scan.dc_encode_table[scan.info.dc_table_indices[component_index_in_scan]].as_ref(),
                scan.ac_encode_table[scan.info.ac_table_indices[component_index_in_scan]].as_ref(),
            )
            .unwrap(); // FIXME: Handle error
        Ok(())
    }

    fn flush(&mut self) -> SimpleResult<CodecError> {
        self.jpeg_encoder.bit_writer.writer.flush().unwrap();
        Ok(())
    }

    fn write_eof(&mut self) {
        let _ = self.jpeg_encoder.bit_writer.writer.ostream.write_eof();
    }
}

pub struct EncoderCodec {
    bit_writer: BitWriter<BufferedOutputStream>,
}

impl EncoderCodec {
    pub fn new(output: BufferedOutputStream) -> Self {
        Self {
            bit_writer: BitWriter::new(output, false),
        }
    }
}

impl CodecSpecialization for EncoderCodec {
    fn process_block(
        &mut self,
        _input: &mut InputStream,
        y: usize,
        x: usize,
        component_index_in_scan: usize,
        component: &Component,
        scan: &mut Scan,
    ) -> SimpleResult<ErrMsg> {
        let block_offset = (y * component.size_in_block.width as usize + x) * 64;
        let block = &scan.coefficients.as_ref().unwrap()[component_index_in_scan]
            [block_offset..block_offset + 64];
        for &coefficient in block.iter() {
            self.bit_writer.write_bits(coefficient as u16, 16).unwrap(); // FIXME: Handle error
        }
        Ok(())
    }

    fn flush(&mut self) -> SimpleResult<CodecError> {
        self.bit_writer.writer.flush().unwrap();
        Ok(())
    }

    fn write_eof(&mut self) {
        let _ = self.bit_writer.writer.ostream.write_eof();
    }
}
