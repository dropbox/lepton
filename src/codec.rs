use arithmetic_coder::ArithmeticCoder;
use interface::ErrMsg;
use iostream::{InputStream, OutputStream};
use jpeg_decoder::{process_scan, Component, Dimensions, JpegError, Scan};
use thread_handoff::ThreadHandoffExt;

pub struct LeptonCodec<ArithmeticEncoderOrDecoder: ArithmeticCoder> {
    arithmetic_coder: ArithmeticEncoderOrDecoder,
    input: InputStream,
    output: OutputStream,
    components: Vec<Component>,
    size_in_mcu: Dimensions,
    scans: Vec<Scan>,
    handoff: ThreadHandoffExt,
}

impl<ArithmeticEncoderOrDecoder: ArithmeticCoder> LeptonCodec<ArithmeticEncoderOrDecoder> {
    pub fn new(
        arithmetic_coder: ArithmeticEncoderOrDecoder,
        input: InputStream,
        output: OutputStream,
        components: Vec<Component>,
        size_in_mcu: Dimensions,
        scans: Vec<Scan>,
        handoff: ThreadHandoffExt,
    ) -> Self {
        LeptonCodec {
            arithmetic_coder,
            input,
            output,
            components,
            size_in_mcu,
            scans,
            handoff,
        }
    }

    pub fn start(mut self) -> Result<(), ErrMsg> {
        for i in 0..self.scans.len() {
            self.process_scan(i)?;
        }
        Ok(())
    }

    fn process_scan(&mut self, scan_index: usize) -> Result<(), ErrMsg> {
        let scan = &mut self.scans[scan_index];
        let mut mcu_row_callback = |mcu_y: usize| Err(ErrMsg::JpegDecodeFail(JpegError::EOF));
        let mut mcu_callback = |mcu_y: usize, mcu_x: usize| Err(ErrMsg::JpegDecodeFail(JpegError::EOF));
        let mut block_callback = |block_y: usize, block_x: usize, component_index_in_scan: usize, component: &Component, scan: &mut Scan| Err(ErrMsg::JpegDecodeFail(JpegError::EOF));
        let mut rst_callback = |exptected_rst: u8| Err(ErrMsg::JpegDecodeFail(JpegError::EOF));
        process_scan(scan, &self.components, &self.size_in_mcu, &mut mcu_row_callback, &mut mcu_callback, &mut block_callback, &mut rst_callback)?;
        scan.coefficients = None;
        Ok(())
    }
}
