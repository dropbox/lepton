use bit_writer::BitWriter;
use byte_converter::{BigEndian, ByteConverter};
use interface::{ErrMsg, SimpleResult};
use io::{BufferedOutputStream, Write};
use iostream::{InputError, InputStream, OutputError};
use jpeg::{Component, JpegEncoder, Scan};
use thread_handoff::ThreadHandoffExt;

pub trait CodecSpecialization: Send {
    fn prepare_scan(
        &mut self,
        scan: &mut Scan,
        scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg>;
    fn process_block(
        &mut self,
        input: &mut InputStream,
        y: usize,
        x: usize,
        component_index_in_scan: usize,
        component: &Component,
        scan: &mut Scan,
    ) -> SimpleResult<ErrMsg>;
    fn flush(&mut self) -> SimpleResult<ErrMsg>;
    fn write_eof(&mut self);
}

pub struct DecoderCodec {
    jpeg_encoder: JpegEncoder,
    pad: u8,
    first_scan: u16,
    mcu_y_start: u16,
}

impl DecoderCodec {
    pub fn new(output: BufferedOutputStream, thread_handoff: &ThreadHandoffExt, pad: u8) -> Self {
        let mut jpeg_encoder = JpegEncoder::new(output);
        jpeg_encoder
            .bit_writer
            .write_bits(
                thread_handoff.overhang_byte as u16,
                thread_handoff.n_overhang_bit,
            )
            .unwrap();
        Self {
            jpeg_encoder: jpeg_encoder,
            pad,
            first_scan: thread_handoff.start_scan,
            mcu_y_start: thread_handoff.mcu_y_start,
        }
    }
}

impl CodecSpecialization for DecoderCodec {
    fn prepare_scan(
        &mut self,
        scan: &mut Scan,
        scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg> {
        if scan_index_in_thread > 1 || (self.mcu_y_start == 0 && self.first_scan > 0) {
            self.jpeg_encoder.bit_writer.writer.write(&scan.raw_header)?;
            scan.raw_header.clear();
            scan.raw_header.shrink_to_fit();
        }
        Ok(())
    }

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
        match input.read(&mut block_u8, true, false) {
            Ok(_) => (),
            Err(InputError::UnexpectedEof) => return Err(ErrMsg::IncompleteThreadSegment),
            Err(InputError::UnexpectedSigAbort) => unreachable!(),
        }
        for (i, coefficient) in block.iter_mut().enumerate() {
            *coefficient = BigEndian::slice_to_u16(&block_u8[(2 * i)..]) as i16;
        }
        self.jpeg_encoder.encode_block(
            &block,
            component_index_in_scan,
            scan.dc_encode_table[scan.info.dc_table_indices[component_index_in_scan]].as_ref(),
            scan.ac_encode_table[scan.info.ac_table_indices[component_index_in_scan]].as_ref(),
        )?;
        Ok(())
    }

    fn flush(&mut self) -> SimpleResult<ErrMsg> {
        self.jpeg_encoder.bit_writer.pad_byte(self.pad)?;
        self.jpeg_encoder.bit_writer.writer.flush()?;
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
    fn prepare_scan(
        &mut self,
        _scan: &mut Scan,
        _scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg> {
        Ok(())
    }

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
            self.bit_writer.write_bits(coefficient as u16, 16)?;
        }
        Ok(())
    }

    fn flush(&mut self) -> SimpleResult<ErrMsg> {
        self.bit_writer.writer.flush()?;
        Ok(())
    }

    fn write_eof(&mut self) {
        let _ = self.bit_writer.writer.ostream.write_eof();
    }
}

impl From<OutputError> for ErrMsg {
    fn from(err: OutputError) -> ErrMsg {
        match err {
            OutputError::ReaderAborted => ErrMsg::CodecKilled,
        }
    }
}
