use bit_writer::{BitWriter, NoEscaping};
use byte_converter::{BigEndian, ByteConverter};
use interface::{ErrMsg, SimpleResult};
use io::{BufferedOutputStream, Write};
use iostream::{InputError, InputStream, OutputError};
use jpeg::{mcu_row_offset, n_coefficient_per_block, Component, JpegEncoder, Scan};
use thread_handoff::ThreadHandoffExt;
use arithmetic_coder::ArithmeticCoder;
pub trait CodecSpecialization: Send {
    fn prepare_scan(
        &mut self,
        scan: &mut Scan,
        scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg>;
    fn finish_scan(&mut self) -> SimpleResult<ErrMsg> {
        Ok(())
    }
    fn prepare_mcu(
        &mut self,
        mcu_y: usize,
        mcu_x: usize,
        restart: bool,
        expected_rst: u8,
    ) -> Result<bool, ErrMsg>;
    fn process_block<Coder:ArithmeticCoder>(
        &mut self,
        input: &mut InputStream,
        coder: &mut Coder,
        y: usize,
        x: usize,
        component_index_in_scan: usize,
        component: &Component,
        scan: &mut Scan,
    ) -> Result<bool, ErrMsg>;
    fn flush<Coder:ArithmeticCoder>(&mut self, coder: &mut Coder) -> SimpleResult<ErrMsg>;
    fn write_eof(&mut self);
}

pub struct DecoderCodec {
    jpeg_encoder: JpegEncoder<BufferedOutputStream>,
    pad: u8,
    start_scan: u16,
    mcu_y_start: u16,
    first_mcu: bool,
}

impl DecoderCodec {
    pub fn new(output: BufferedOutputStream, thread_handoff: &ThreadHandoffExt, pad: u8) -> Self {
        let mut jpeg_encoder = JpegEncoder::new(output, &thread_handoff.last_dc);
        if thread_handoff.n_overhang_bit > 0 {
            jpeg_encoder
                .bit_writer
                .write_bits(
                    (thread_handoff.overhang_byte >> (8 - thread_handoff.n_overhang_bit)) as u16,
                    thread_handoff.n_overhang_bit,
                )
                .unwrap();
        }
        Self {
            jpeg_encoder: jpeg_encoder,
            pad,
            start_scan: thread_handoff.start_scan,
            mcu_y_start: thread_handoff.mcu_y_start,
            first_mcu: true,
        }
    }
}

impl CodecSpecialization for DecoderCodec {
    fn prepare_scan(
        &mut self,
        scan: &mut Scan,
        scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg> {
        let first_scan = scan_index_in_thread == 0;
        if !first_scan {
            self.jpeg_encoder.reset();
        }
        if !first_scan || (self.mcu_y_start == 0 && self.start_scan > 0) {
            self.jpeg_encoder.bit_writer.pad_byte(self.pad)?;
            self.jpeg_encoder.bit_writer.writer.write(&scan.raw_header)?;
            scan.raw_header.clear();
            scan.raw_header.shrink_to_fit();
        }
        Ok(())
    }

    fn finish_scan(&mut self) -> SimpleResult<ErrMsg> {
        self.jpeg_encoder.bit_writer.pad_byte(self.pad)?;
        Ok(())
    }

    fn prepare_mcu(
        &mut self,
        _mcu_y: usize,
        _mcu_x: usize,
        restart: bool,
        expected_rst: u8,
    ) -> Result<bool, ErrMsg> {
        if restart {
            if !self.first_mcu {
                let bit_writer = &mut self.jpeg_encoder.bit_writer;
                bit_writer.pad_byte(self.pad)?;
                bit_writer.writer.write(&[0xFF, 0xD0 + expected_rst])?;
            }
            self.jpeg_encoder.reset();
        }
        self.first_mcu = false;
        Ok(false)
    }

    fn process_block<Coder:ArithmeticCoder>(
        &mut self,
        input: &mut InputStream,
        coder: &mut Coder,
        _y: usize,
        _x: usize,
        component_index_in_scan: usize,
        _component: &Component,
        scan: &mut Scan,
    ) -> Result<bool, ErrMsg> {
        let n_coefficient_per_block = n_coefficient_per_block(&scan.info);
        let mut block = vec![0i16; n_coefficient_per_block];
        for coef in &mut block[..].iter_mut() {
            for bit_id in 0..16 {
                let mut bit = 0u8;
                let mut default_prior = 0x7f;
                coder.parse_bit(input, &mut default_prior, &mut bit);
                *coef |= i16::from(bit) << bit_id;
            }
        }
        self.jpeg_encoder.encode_block(
            &block,
            &scan.info,
            component_index_in_scan,
            scan.dc_encode_table[scan.info.dc_table_indices[component_index_in_scan]].as_ref(),
            scan.ac_encode_table[scan.info.ac_table_indices[component_index_in_scan]].as_ref(),
        )?;
        Ok(false)
    }

    fn flush<Coder:ArithmeticCoder>(&mut self, coder: &mut Coder) -> SimpleResult<ErrMsg> {
        self.jpeg_encoder.bit_writer.writer.flush()?;
        Ok(())
    }

    fn write_eof(&mut self) {
        let _ = self.jpeg_encoder.bit_writer.writer.ostream.write_eof();
    }
}

pub struct EncoderCodec {
    bit_writer: BitWriter<BufferedOutputStream, NoEscaping>,
    last_scan: u16,
    mcu_y_start: u16,
    mcu_y_end: Option<u16>,
    scan_index_in_thread: u16,
}

impl EncoderCodec {
    pub fn new(
        output: BufferedOutputStream,
        thread_handoff: &ThreadHandoffExt,
        mcu_y_end: Option<u16>,
    ) -> Self {
        Self {
            bit_writer: BitWriter::new(output),
            last_scan: thread_handoff.end_scan - thread_handoff.start_scan,
            mcu_y_start: thread_handoff.mcu_y_start,
            mcu_y_end,
            scan_index_in_thread: 0,
        }
    }
}

impl CodecSpecialization for EncoderCodec {
    fn prepare_scan(
        &mut self,
        _scan: &mut Scan,
        scan_index_in_thread: usize,
    ) -> SimpleResult<ErrMsg> {
        self.scan_index_in_thread = scan_index_in_thread as u16;
        Ok(())
    }

    fn prepare_mcu(
        &mut self,
        mcu_y: usize,
        mcu_x: usize,
        _restart: bool,
        _expected_rst: u8,
    ) -> Result<bool, ErrMsg> {
        if mcu_x == 0 {
            Ok(match self.mcu_y_end {
                Some(mcu_y_end) => {
                    self.scan_index_in_thread > self.last_scan
                        || self.scan_index_in_thread == self.last_scan && mcu_y as u16 >= mcu_y_end
                }
                None => false,
            })
        } else {
            Ok(false)
        }
    }

    fn process_block<Coder:ArithmeticCoder>(
        &mut self,
        input: &mut InputStream,
        coder: &mut Coder,
        y: usize,
        x: usize,
        component_index_in_scan: usize,
        component: &Component,
        scan: &mut Scan,
    ) -> Result<bool, ErrMsg> {
        if let Some(ref truncation) = scan.truncation {
            if truncation.is_end(component_index_in_scan, y, x) {
                return Ok(true);
            }
        }
        let n_coefficient_per_block = n_coefficient_per_block(&scan.info);
        let mut block_offset =
            (y * component.size_in_block.width as usize + x) * n_coefficient_per_block;
        if self.scan_index_in_thread == 0 {
            block_offset -= mcu_row_offset(&scan.info, component, self.mcu_y_start);
        }
        let block = &scan.coefficients.as_ref().unwrap()[component_index_in_scan]
            [block_offset..(block_offset + n_coefficient_per_block)];
        for &coefficient in block.iter() {
            for bit_id in 0..16 {
                let mut bit = (coefficient >> bit_id) as u8 & 0x1;
                let mut default_prior = 0x7f;
                coder.parse_bit(input, &mut default_prior, &mut bit);
            }
        }
        Ok(false)
    }

    fn flush<Coder:ArithmeticCoder>(&mut self, coder:&mut Coder) -> SimpleResult<ErrMsg> {
        self.bit_writer.writer.write(coder.flush());
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
