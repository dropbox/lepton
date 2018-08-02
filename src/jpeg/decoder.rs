use std::cell::RefCell;
use std::cmp::max;
use std::mem::replace;
use std::ops::Range;

use bit_vec::BitVec;

use super::constants::{MAX_COMPONENTS, UNZIGZAG};
use super::error::{JpegError, JpegResult, UnsupportedFeature};
use super::huffman::{fill_default_mjpeg_tables, HuffmanDecoder, HuffmanTable};
use super::jpeg::{
    CodingProcess, Component, EntropyCoding, FormatInfo, FrameInfo, Jpeg, Scan, ScanTruncation,
};
use super::marker::Marker;
use super::parser::{parse_app, parse_com, parse_dht, parse_dqt, parse_dri, parse_sof, parse_sos};
use super::util::process_scan;
use iostream::InputStream;
use thread_handoff::ThreadHandoffExt;

pub type DecodeResult = JpegResult<Jpeg>;

/// JPEG decoder
pub struct JpegDecoder {
    input: InputStream,
    dc_huffman_tables: [HuffmanTable; 4],
    ac_huffman_tables: [HuffmanTable; 4],
    quantization_tables: [Option<[u16; 64]>; 4],
    non_zero_coefficients: Vec<BitVec>, // This is really non-zero AC coefficients
    restart_interval: u16,
    is_mjpeg: bool,
    n_scan_processed: usize,
    header_only: bool,
    start_byte: usize,
}

impl JpegDecoder {
    pub fn new(input: InputStream, start_byte: usize, header_only: bool) -> Self {
        use self::HuffmanTable::Empty;
        JpegDecoder {
            input: input,
            dc_huffman_tables: [Empty, Empty, Empty, Empty],
            ac_huffman_tables: [Empty, Empty, Empty, Empty],
            quantization_tables: [None, None, None, None],
            non_zero_coefficients: vec![],
            restart_interval: 0,
            is_mjpeg: false,
            n_scan_processed: 0,
            header_only: header_only,
            start_byte: if header_only { 0 } else { start_byte },
        }
    }

    pub fn decode(mut self) -> DecodeResult {
        let mut frame = None;
        let mut scans = vec![];
        let mut format = if self.header_only {
            None
        } else {
            Some(FormatInfo::default())
        };
        match self.decode_internal(&mut frame, &mut scans, &mut format) {
            Ok(_) => (),
            Err(JpegError::EOF) => match format {
                Some(ref format) => {
                    if self.input.processed_len() < self.start_byte {
                        return Err(JpegError::Malformatted(
                            "EOF encountered before start_byte".to_owned(),
                        ));
                    }
                    if format.handoff.is_empty() {
                        return Err(JpegError::Malformatted(
                            "no/insufficient entropy encoded data".to_owned(),
                        ));
                    }
                    if scans.last().unwrap().is_empty() {
                        scans.pop();
                    }
                }
                None => return Err(JpegError::Malformatted("incomplete header".to_owned())),
            },
            Err(e) => return Err(e),
        }
        self.input.abort();
        if let Some(ref mut format) = format {
            format.grb.extend(self.input.view_retained_data());
            let old_grb_len = format.grb.len();
            format.len = self.input.processed_len() - old_grb_len;
            format.grb.resize(old_grb_len + self.input.len(), 0);
            self.input
                .read(&mut format.grb[old_grb_len..], true, false)
                .unwrap();
        }
        Ok(Jpeg {
            frame: frame.unwrap(),
            scans,
            format,
        })
    }

    fn decode_internal(
        &mut self,
        frame: &mut Option<FrameInfo>,
        scans: &mut Vec<Scan>,
        format: &mut Option<FormatInfo>,
    ) -> JpegResult<()> {
        if self.input.read_byte(true)? != 0xFF
            || Marker::from_u8(try!(self.input.read_byte(true))) != Ok(Marker::SOI)
        {
            return Err(JpegError::Malformatted(
                "first two bytes is not a SOI marker".to_owned(),
            ));
        }
        let mut previous_marker = Marker::SOI;
        loop {
            let marker = self.read_marker()?;
            match marker {
                // Frame header
                Marker::SOF(n) => {
                    // Section 4.10
                    // "An image contains only one frame in the cases of sequential and
                    //  progressive coding processes; an image contains multiple frames for the
                    //  hierarchical mode."
                    if frame.is_some() {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Hierarchical));
                    }
                    let frame_info = parse_sof(&mut self.input, n)?;
                    let component_count = frame_info.components.len();

                    if frame_info.is_differential {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Hierarchical));
                    }
                    if frame_info.coding_process == CodingProcess::Lossless {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Lossless));
                    }
                    if frame_info.entropy_coding == EntropyCoding::Arithmetic {
                        return Err(JpegError::Unsupported(
                            UnsupportedFeature::ArithmeticEntropyCoding,
                        ));
                    }
                    // FIXME: Probably need to modify huffman and decode_block to support
                    // higher precision
                    if frame_info.precision != 8 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::SamplePrecision(
                            frame_info.precision,
                        )));
                    }
                    if frame_info.size.height == 0 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::DNL));
                    }
                    if component_count != 1 && component_count != 3 && component_count != 4 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::ComponentCount(
                            component_count as u8,
                        )));
                    }
                    replace(frame, Some(frame_info));
                }
                // Scan header
                Marker::SOS => {
                    if frame.is_none() {
                        return Err(JpegError::Malformatted(
                            "scan encountered before frame".to_owned(),
                        ));
                    }
                    let frame_info = frame.as_ref().unwrap();
                    let scan_info = parse_sos(&mut self.input, frame_info)?;
                    if self.is_mjpeg {
                        fill_default_mjpeg_tables(
                            &scan_info,
                            &mut self.dc_huffman_tables,
                            &mut self.ac_huffman_tables,
                            self.header_only,
                        );
                    }
                    scans.push(Scan::new(
                        self.input.view_retained_data(),
                        scan_info,
                        self.restart_interval,
                    ));
                    if self.in_pge() {
                        format.as_mut().unwrap().pge.extend(
                            &self.input.view_retained_data()[max(
                                self.input.view_retained_data().len() - self.input.processed_len()
                                    + self.start_byte,
                                0,
                            )..],
                        );
                    }
                    self.input.clear_retained_data();
                    let current_scan = scans.last_mut().unwrap();
                    self.verify_huffman_tables(current_scan)?;
                    if let Some(ref mut format) = format {
                        // FIXME: We don't need non_zero_coefficient in sequential mode
                        // if frame_info.coding_process == CodingProcess::DctProgressive
                        //     && self.non_zero_coefficients.is_empty()
                        if self.non_zero_coefficients.is_empty() {
                            self.non_zero_coefficients = frame_info
                                .components
                                .iter()
                                .map(|c| {
                                    let block_count = c.size_in_block.width as usize
                                        * c.size_in_block.height as usize;
                                    BitVec::from_elem(block_count * 64, false)
                                })
                                .collect();
                        }
                        self.decode_scan(frame_info, current_scan, format)?;
                        if self.start_byte > 0 {
                            current_scan.coefficients = None;
                        }
                    } else {
                        for (i, (dc_table, ac_table)) in self.dc_huffman_tables
                            .iter()
                            .zip(self.ac_huffman_tables.iter())
                            .enumerate()
                        {
                            // FIXME: Only clone the tables that we need
                            current_scan.dc_encode_table[i] = dc_table.clone_encode_table();
                            current_scan.ac_encode_table[i] = ac_table.clone_encode_table();
                        }
                        if self.input.is_eof() {
                            return Ok(());
                        }
                    }
                    self.n_scan_processed += 1;
                }
                // Table-specification and miscellaneous markers
                // Quantization table-specification
                Marker::DQT => {
                    parse_dqt(&mut self.input, &mut self.quantization_tables)?;
                }
                // Huffman table-specification
                Marker::DHT => {
                    let is_baseline = frame.as_ref().map(|frame| frame.is_baseline);
                    parse_dht(
                        &mut self.input,
                        is_baseline,
                        &mut self.dc_huffman_tables,
                        &mut self.ac_huffman_tables,
                        self.header_only,
                    )?;
                }
                // Arithmetic conditioning table-specification
                Marker::DAC => {
                    return Err(JpegError::Unsupported(
                        UnsupportedFeature::ArithmeticEntropyCoding,
                    ))
                }
                // Restart interval definition
                Marker::DRI => self.restart_interval = parse_dri(&mut self.input)?,
                // Comment
                Marker::COM => {
                    let _comment = parse_com(&mut self.input)?;
                }
                // Application data
                Marker::APP(_) => {
                    self.is_mjpeg = parse_app(&mut self.input, marker)?;
                }
                // Restart
                Marker::RST(_) => {
                    // Some encoders emit a final RST marker after entropy-coded data, which
                    // decode_scan does not take care of. So if we encounter one, we ignore it.
                    if previous_marker != Marker::SOS {
                        return Err(JpegError::Malformatted(
                            "RST found outside of entropy-coded data".to_owned(),
                        ));
                    }
                }
                // Define number of lines
                Marker::DNL => {
                    // Section B.2.1
                    // "If a DNL segment (see B.2.5) is present, it shall immediately follow the first scan."
                    if previous_marker != Marker::SOS || self.n_scan_processed != 1 {
                        return Err(JpegError::Malformatted(
                            "DNL is only allowed immediately after the first scan".to_owned(),
                        ));
                    }
                    return Err(JpegError::Unsupported(UnsupportedFeature::DNL));
                }
                // Hierarchical mode markers
                Marker::DHP | Marker::EXP => {
                    return Err(JpegError::Unsupported(UnsupportedFeature::Hierarchical))
                }
                // End of image
                Marker::EOI => break,
                _ => {
                    return Err(JpegError::Malformatted(format!(
                        "{:?} marker found where not allowed",
                        marker
                    )))
                }
            }
            previous_marker = marker;
        }
        Ok(())
    }

    fn read_marker(&mut self) -> JpegResult<Marker> {
        // This should be an error as the JPEG spec doesn't allow extraneous data between marker segments.
        // libjpeg allows this though and there are images in the wild utilising it, so we are
        // forced to support this behavior.
        // Sony Ericsson P990i is an example of a device which produce this sort of JPEGs.
        while self.input.read_byte(true)? != 0xFF {}
        let mut byte = self.input.read_byte(true)?;
        // Section B.1.1.2
        // "Any marker may optionally be preceded by any number of fill bytes, which are bytes assigned code X’FF’."
        while byte == 0xFF {
            byte = self.input.read_byte(true)?;
        }
        match byte {
            0x00 => Err(JpegError::Malformatted(
                "0xFF00 found where marker was expected (read_marker)".to_owned(),
            )),
            _ => Ok(Marker::from_u8(byte).unwrap()),
        }
    }

    fn decode_scan(
        &mut self,
        frame: &FrameInfo,
        scan: &mut Scan,
        format: &mut FormatInfo,
    ) -> JpegResult<()> {
        let components: Vec<Component>;
        let subsequent_successive_approximation: bool;
        {
            let scan_info = &scan.info;
            assert!(scan_info.component_indices.len() <= MAX_COMPONENTS);
            scan.coefficients = Some(
                scan.info
                    .component_indices
                    .iter()
                    .map(|&i| {
                        let component = &frame.components[i];
                        let block_count = component.size_in_block.width as usize
                            * component.size_in_block.height as usize;
                        vec![0; block_count * 64]
                    })
                    .collect(),
            );
            components = scan_info
                .component_indices
                .iter()
                .map(|&i| frame.components[i].clone())
                .collect();
            // Verify that all required quantization tables has been set.
            if components.iter().any(|component| {
                self.quantization_tables[component.quantization_table_index].is_none()
            }) {
                return Err(JpegError::Malformatted(
                    "use of unset quantization table".to_owned(),
                ));
            }
            subsequent_successive_approximation = scan_info.successive_approximation_high > 0;
        }
        let huffman = RefCell::new(HuffmanDecoder::new(self.start_byte));
        let self_cell = RefCell::new(self);
        let format = RefCell::new(format);
        let dc_predictors = RefCell::new([0i16; MAX_COMPONENTS]);
        let eob_run = RefCell::new(0);
        let reset_dc = RefCell::new(false);
        let result = {
            let mut mcu_row_callback = |mcu_y: usize| {
                let mut slf = self_cell.borrow_mut();
                let mut format = format.borrow_mut();
                let huffman = &mut huffman.borrow_mut();
                if !subsequent_successive_approximation || mcu_y == 0 {
                    // Finish PGE
                    if slf.in_pge() {
                        slf.start_byte = 0;
                        huffman.end_pge();
                        format.pge.extend(huffman.get_pge());
                    }
                    // Add thread handoff for each MCU row
                    if slf.start_byte == 0 {
                        let (overhang_byte, n_overhang_bit) = huffman.handover_byte();
                        format.handoff.push(ThreadHandoffExt {
                            start_scan: slf.n_scan_processed as u16,
                            end_scan: slf.n_scan_processed as u16,
                            mcu_y_start: mcu_y as u16,
                            segment_size: slf.input.processed_len() as u32,
                            overhang_byte,
                            n_overhang_bit,
                            last_dc: dc_predictors.borrow().clone(),
                        })
                    }
                }
                Ok(())
            };
            let mut mcu_callback = |_mcu_y: usize, _mcu_x: usize| {
                if reset_dc.replace(false) {
                    dc_predictors.replace([0i16; MAX_COMPONENTS]);
                }
                Ok(())
            };
            let mut block_callback = |block_y: usize,
                                      block_x: usize,
                                      component_index_in_scan: usize,
                                      component: &Component,
                                      scan: &mut Scan| {
                self_cell.borrow_mut().decode_block(
                    block_y,
                    block_x,
                    component_index_in_scan,
                    component,
                    scan,
                    &mut huffman.borrow_mut(),
                    &mut eob_run.borrow_mut(),
                    &mut dc_predictors.borrow_mut()[component_index_in_scan],
                    &mut format.borrow_mut(),
                )
            };
            let mut rst_callback = |expected_rst: u8| {
                let mut format = format.borrow_mut();
                let huffman = &mut huffman.borrow_mut();
                update_padding(&mut format, huffman)?;
                if !subsequent_successive_approximation {
                    huffman.clear_buffer();
                }
                match huffman.read_rst(&mut self_cell.borrow_mut().input, expected_rst) {
                    Ok(_) => {
                        huffman.reset();
                        // Section F.2.1.3.1
                        reset_dc.replace(true);
                        // Section G.1.2.2
                        eob_run.replace(0);
                        Ok(())
                    }
                    Err(JpegError::EOF) => {
                        format.grb.extend(huffman.view_buffer());
                        Err(JpegError::EOF)
                    }
                    Err(e) => Err(e),
                }
            };
            process_scan(
                scan,
                &components,
                &frame.size_in_mcu,
                &mut mcu_row_callback,
                &mut mcu_callback,
                &mut block_callback,
                &mut rst_callback,
            )
        };
        let slf = self_cell.into_inner();
        let format = format.into_inner();
        let huffman = huffman.into_inner();
        if result.is_ok() {
            if slf.in_pge() {
                format.pge.extend(huffman.get_pge());
            }
            update_padding(format, &huffman)?;
            // In case there are extraneous data between the end of the scan and next marker
            let huffman_buffer = huffman.view_buffer();
            let n_leftover_byte = huffman.n_available_bit() / 8;
            slf.input.add_retained_data(
                &huffman_buffer[(huffman_buffer.len() - n_leftover_byte as usize)..],
            );
        }
        result
    }

    fn decode_block(
        &mut self,
        y: usize,
        x: usize,
        scan_component_index: usize,
        component: &Component,
        scan: &mut Scan,
        huffman: &mut HuffmanDecoder,
        eob_run: &mut u16,
        dc_predictor: &mut i16,
        format: &mut FormatInfo,
    ) -> JpegResult<()> {
        let scan_info = &scan.info;
        let block_offset = (y * component.size_in_block.width as usize + x) * 64;
        let coefficients = &mut scan.coefficients.as_mut().unwrap()[scan_component_index]
            [block_offset..block_offset + 64];
        let non_zero_coefficients =
            &mut self.non_zero_coefficients[scan.info.component_indices[scan_component_index]];
        match if scan_info.successive_approximation_high == 0 {
            huffman.clear_buffer();
            decode_block(
                &mut self.input,
                coefficients,
                huffman,
                &self.dc_huffman_tables[scan_info.dc_table_indices[scan_component_index]],
                &self.ac_huffman_tables[scan_info.ac_table_indices[scan_component_index]],
                scan_info.spectral_selection.clone(),
                scan_info.successive_approximation_low,
                eob_run,
                dc_predictor,
                non_zero_coefficients,
                block_offset,
            )
        } else {
            decode_block_successive_approximation(
                &mut self.input,
                coefficients,
                huffman,
                &self.ac_huffman_tables[scan_info.ac_table_indices[scan_component_index]],
                scan_info.spectral_selection.clone(),
                eob_run,
                non_zero_coefficients,
                block_offset,
            )
        } {
            Ok(()) => Ok(()),
            Err(JpegError::EOF) => {
                if self.input.processed_len() < self.start_byte {
                    return Err(JpegError::Malformatted(
                        "EOF encountered before start_byte".to_owned(),
                    ));
                }
                if scan_component_index == 0 && x == 0 {
                    // Leftmost column of the first index of the scan
                    if y == 0 {
                        // First block of the scan
                        pop_handoff_and_verify_non_empty(format)?;
                        format.grb.extend(&scan.raw_header);
                    } else if scan_info.component_indices.len() == 1
                        || y % component.vertical_sampling_factor as usize == 0
                    {
                        // First block in the MCU row
                        pop_handoff_and_verify_non_empty(format)?;
                        format.grb.extend(huffman.view_buffer());
                    }
                }
                format.grb.extend(huffman.view_buffer());
                scan.truncation = Some(ScanTruncation::new(scan_component_index, y, x));
                Err(JpegError::EOF)
            }
            Err(e) => Err(e),
        }
    }

    fn verify_huffman_tables(&self, scan: &Scan) -> JpegResult<()> {
        let scan_info = &scan.info;
        if scan_info.spectral_selection.start == 0
            && scan_info
                .dc_table_indices
                .iter()
                .any(|&i| self.dc_huffman_tables[i].is_empty())
        {
            return Err(JpegError::Malformatted(
                "scan makes use of unset dc huffman table".to_owned(),
            ));
        }
        if scan_info.spectral_selection.end > 1
            && scan_info
                .ac_table_indices
                .iter()
                .any(|&i| self.ac_huffman_tables[i].is_empty())
        {
            Err(JpegError::Malformatted(
                "scan makes use of unset ac huffman table".to_owned(),
            ))
        } else {
            Ok(())
        }
    }

    fn in_pge(&self) -> bool {
        self.start_byte > 0 && self.input.processed_len() > self.start_byte
    }
}

fn pop_handoff_and_verify_non_empty(format: &mut FormatInfo) -> JpegResult<()> {
    format.handoff.pop();
    if format.handoff.is_empty() {
        Err(JpegError::EOF)
    } else {
        Ok(())
    }
}

fn update_padding(format: &mut FormatInfo, huffman: &HuffmanDecoder) -> JpegResult<()> {
    let (pad_byte, pad_start_bit) = huffman.handover_byte();
    if pad_start_bit == 0 {
        return Ok(());
    }
    if (((format.pad_byte ^ pad_byte) as u16) << max(format.pad_start_bit, pad_start_bit)) & 0xFF
        != 0
    {
        return Err(JpegError::Malformatted("inconsistent padding".to_owned()));
    }
    if pad_start_bit < format.pad_start_bit {
        format.pad_byte |= (pad_byte << pad_start_bit) >> pad_start_bit;
        format.pad_start_bit = pad_start_bit;
    }
    Ok(())
}

fn decode_block(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    dc_table: &HuffmanTable,
    ac_table: &HuffmanTable,
    spectral_selection: Range<u8>,
    successive_approximation_low: u8,
    eob_run: &mut u16,
    dc_predictor: &mut i16,
    non_zero_coefficients: &mut BitVec,
    block_offset: usize,
) -> JpegResult<()> {
    assert_eq!(coefficients.len(), 64);
    if spectral_selection.start == 0 {
        // Section F.2.2.1
        // Figure F.12
        let value = huffman.decode(input, dc_table.decode_table())?;
        let diff = match value {
            0 => 0,
            _ => {
                // Section F.1.2.1.1
                // Table F.1
                if value > 11 {
                    return Err(JpegError::Malformatted(
                        "invalid DC difference magnitude category".to_owned(),
                    ));
                }
                huffman.receive_extend(input, value)?
            }
        };
        // Malicious JPEG files can cause this add to overflow, therefore we use wrapping_add.
        // One example of such a file is tests/crashtest/images/dc-predictor-overflow.jpg
        *dc_predictor = dc_predictor.wrapping_add(diff);
        coefficients[0] = *dc_predictor << successive_approximation_low;
    }
    if spectral_selection.end > 1 {
        if *eob_run > 0 {
            *eob_run -= 1;
            return Ok(());
        }
        // Section F.1.2.2.1
        let mut index = max(spectral_selection.start, 1);
        let ac_table = ac_table.decode_table();
        while index < spectral_selection.end {
            match huffman.decode_fast_ac(input, ac_table)? {
                Some((value, run)) => {
                    index += run;
                    if index >= spectral_selection.end {
                        break;
                    }
                    update_coefficient(
                        value,
                        coefficients,
                        &mut index,
                        non_zero_coefficients,
                        block_offset,
                    );
                }
                None => {
                    let byte = huffman.decode(input, ac_table)?;
                    let r = byte >> 4;
                    let s = byte & 0x0f;
                    if s == 0 {
                        match r {
                            15 => index += 16, // Run length of 16 zero coefficients.
                            _ => {
                                *eob_run = (1 << r) - 1;
                                if r > 0 {
                                    *eob_run += huffman.get_bits(input, r)?;
                                }
                                break;
                            }
                        }
                    } else {
                        index += r;
                        if index >= spectral_selection.end {
                            break;
                        }
                        let value = huffman.receive_extend(input, s)?;
                        update_coefficient(
                            value,
                            coefficients,
                            &mut index,
                            non_zero_coefficients,
                            block_offset,
                        );
                    }
                }
            }
        }
    }
    Ok(())
}

/// DC coefficient is coded in 1 bit.
/// Each AC coefficient is coded in 3 bits. The first bit indicates whether the coefficient
/// has become non-zero in previous approximations. The second bit indicates whether the
/// coefficient needs an update in this approximation. If the coefficient does not have a
/// non-zero history and needs and update, the third bit indicates the sign of the coefficient,
/// where 1 means positive.
fn decode_block_successive_approximation(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    ac_table: &HuffmanTable,
    spectral_selection: Range<u8>,
    eob_run: &mut u16,
    non_zero_coefficients: &mut BitVec,
    block_offset: usize,
) -> JpegResult<()> {
    assert_eq!(coefficients.len(), 64);
    if spectral_selection.start == 0 {
        // Section G.1.2.1
        coefficients[0] = huffman.get_bits(input, 1)? as i16;
    }
    if spectral_selection.end > 1 {
        // Section G.1.2.3
        if *eob_run > 0 {
            *eob_run -= 1;
            decode_zero_run(
                input,
                coefficients,
                huffman,
                spectral_selection,
                64,
                non_zero_coefficients,
                block_offset,
            )?;
            return Ok(());
        }
        let mut index = max(spectral_selection.start, 1);
        while index < spectral_selection.end {
            let byte = huffman.decode(input, ac_table.decode_table())?;
            let r = byte >> 4;
            let s = byte & 0x0f;
            let mut zero_run_length = r;
            let mut value = 0;
            match s {
                0 => {
                    match r {
                        15 => {
                            // Run length of 16 zero coefficients.
                            // We don't need to do anything special here, zero_run_length is 15
                            // and then value (which is zero) gets written, resulting in 16
                            // zero coefficients.
                        }
                        _ => {
                            *eob_run = (1 << r) - 1;
                            if r > 0 {
                                *eob_run += huffman.get_bits(input, r)?;
                            }
                            // Force end of block.
                            zero_run_length = 64;
                        }
                    }
                }
                1 => value = 1 << 1 | huffman.get_bits(input, 1)? as i16,
                _ => {
                    return Err(JpegError::Malformatted(
                        "unexpected huffman code".to_owned(),
                    ))
                }
            }
            let range = index..spectral_selection.end;
            index = decode_zero_run(
                input,
                coefficients,
                huffman,
                range,
                zero_run_length,
                non_zero_coefficients,
                block_offset,
            )?;
            update_coefficient(
                value,
                coefficients,
                &mut index,
                non_zero_coefficients,
                block_offset,
            );
        }
    }
    Ok(())
}

fn decode_zero_run(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    range: Range<u8>,
    mut zero_run_length: u8,
    non_zero_coefficients: &mut BitVec,
    block_offset: usize,
) -> JpegResult<u8> {
    assert_eq!(coefficients.len(), 64);
    let last = range.end - 1;
    for i in range {
        let index = UNZIGZAG[i as usize];
        if !non_zero_coefficients[block_offset + index] {
            if zero_run_length == 0 {
                return Ok(i);
            }
            zero_run_length -= 1;
        } else {
            coefficients[index] = 1 << 2 | (huffman.get_bits(input, 1)? << 1) as i16;
        }
    }
    Ok(last)
}

fn update_coefficient(
    value: i16,
    block_coefficients: &mut [i16],
    zigzag_index: &mut u8,
    non_zero_coefficients: &mut BitVec,
    block_offset: usize,
) {
    if value != 0 {
        let raster_index = UNZIGZAG[*zigzag_index as usize];
        block_coefficients[raster_index] = value;
        non_zero_coefficients.set(block_offset + raster_index, true);
    }
    *zigzag_index += 1;
}
