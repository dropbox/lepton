use std::cmp;
use std::mem;
use std::ops::Range;
use std::sync::Arc;

use super::error::{JpegError, JpegResult, UnsupportedFeature};
use super::huffman::{fill_default_mjpeg_tables, HuffmanDecoder, HuffmanTable};
use super::jpeg::FrequencyImage;
use super::marker::Marker;
use super::parser::{
    parse_app, parse_com, parse_dht, parse_dqt, parse_dri, parse_sof, parse_sos,
    AdobeColorTransform, AppData, CodingProcess, Component, Dimensions, EntropyCoding, FrameInfo,
    ScanInfo,
};
use super::upsampler::Upsampler;
use super::worker::{RowData, ThreadedWorker, Worker};
use iostream::InputStream;

pub const MAX_COMPONENTS: usize = 4;

static UNZIGZAG: [u8; 64] = [
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20,
    13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59,
    52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
];

/// An enumeration over combinations of color spaces and bit depths a pixel can have.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum PixelFormat {
    /// Luminance (grayscale), 8 bits
    L8,
    /// RGB, 8 bits per channel
    RGB24,
    /// CMYK, 8 bits per channel
    CMYK32,
}

/// Represents metadata of an image.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ImageInfo {
    /// The width of the image, in pixels.
    pub width: u16,
    /// The height of the image, in pixels.
    pub height: u16,
    /// The pixel format of the image.
    pub pixel_format: PixelFormat,
}

/// JPEG decoder
pub struct JpegDecoder {
    input: InputStream,

    frame: Option<FrameInfo>, // TODO: Make this a variable in the function?
    dc_huffman_tables: [Option<HuffmanTable>; 4],
    ac_huffman_tables: [Option<HuffmanTable>; 4],
    quantization_tables: [Option<[u16; 64]>; 4],

    restart_interval: u16,
    color_transform: Option<AdobeColorTransform>,
    is_jfif: bool,
    is_mjpeg: bool,

    // Used for progressive JPEGs.
    coefficients: Vec<Vec<i16>>,
    // Bitmask of which coefficients has been completely decoded.
    coefficients_finished: [u64; MAX_COMPONENTS],
}

impl JpegDecoder {
    /// Creates a new `Decoder` using the reader `reader`.
    pub fn new(input: InputStream) -> Self {
        JpegDecoder {
            input: input,
            frame: None,
            dc_huffman_tables: [None, None, None, None],
            ac_huffman_tables: [None, None, None, None],
            quantization_tables: [None, None, None, None],
            restart_interval: 0,
            color_transform: None,
            is_jfif: false,
            is_mjpeg: false,
            coefficients: Vec::new(),
            coefficients_finished: [0; MAX_COMPONENTS],
        }
    }

    /// Returns metadata about the image.
    ///
    /// The returned value will be `None` until a call to either `read_info` or `decode` has
    /// returned `Ok`.
    pub fn info(&self) -> Option<ImageInfo> {
        match self.frame {
            Some(ref frame) => {
                let pixel_format = match frame.components.len() {
                    1 => PixelFormat::L8,
                    3 => PixelFormat::RGB24,
                    4 => PixelFormat::CMYK32,
                    _ => panic!(),
                };

                Some(ImageInfo {
                    width: frame.size.width,
                    height: frame.size.height,
                    pixel_format: pixel_format,
                })
            }
            None => None,
        }
    }

    /// Decodes the image and returns the decoded pixels if successful.
    pub fn decode(&mut self) -> JpegResult<FrequencyImage> {
        if self.input.read_byte_keep()? != 0xFF
            || Marker::from_u8(try!(self.input.read_byte_keep())) != Ok(Marker::SOI)
        {
            return Err(JpegError::Malformatted(
                "first two bytes is not a SOI marker",
            ));
        }
        let mut previous_marker = Marker::SOI;
        let mut pending_marker = None;
        let mut worker = None;
        let mut scans_processed = 0;
        let mut planes;
        loop {
            let marker = match pending_marker.take() {
                Some(m) => m,
                None => self.read_marker()?,
            };
            match marker {
                // Frame header
                Marker::SOF(n) => {
                    // Section 4.10
                    // "An image contains only one frame in the cases of sequential and
                    //  progressive coding processes; an image contains multiple frames for the
                    //  hierarchical mode."
                    if self.frame.is_some() {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Hierarchical));
                    }
                    let frame = parse_sof(&mut self.input, n)?;
                    let component_count = frame.components.len();

                    if frame.is_differential {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Hierarchical));
                    }
                    if frame.coding_process == CodingProcess::Lossless {
                        return Err(JpegError::Unsupported(UnsupportedFeature::Lossless));
                    }
                    if frame.entropy_coding == EntropyCoding::Arithmetic {
                        return Err(JpegError::Unsupported(
                            UnsupportedFeature::ArithmeticEntropyCoding,
                        ));
                    }
                    // TODO: Do we support higher precision?
                    if frame.precision != 8 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::SamplePrecision(
                            frame.precision,
                        )));
                    }
                    if frame.size.height == 0 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::DNL));
                    }
                    if component_count != 1 && component_count != 3 && component_count != 4 {
                        return Err(JpegError::Unsupported(UnsupportedFeature::ComponentCount(
                            component_count as u8,
                        )));
                    }
                    // Make sure we support the subsampling ratios used.
                    // TODO: Do we need this?
                    let _ = Upsampler::new(&frame.components, frame.size.width, frame.size.height)?;
                    self.frame = Some(frame);
                    planes = vec![Vec::new(); component_count];
                }
                // Scan header
                Marker::SOS => {
                    if self.frame.is_none() {
                        return Err(JpegError::Malformatted("scan encountered before frame"));
                    }
                    let frame = self.frame.as_ref().unwrap();
                    let scan = parse_sos(&mut self.input, frame)?;
                    if frame.coding_process == CodingProcess::DctProgressive
                        && self.coefficients.is_empty()
                    {
                        self.coefficients = frame
                            .components
                            .iter()
                            .map(|c| {
                                let block_count = c.size_in_block.width as usize
                                    * c.size_in_block.height as usize;
                                vec![0; block_count * 64]
                            })
                            .collect();
                    }
                    if scan.successive_approximation_low == 0 {
                        for &i in scan.component_indices.iter() {
                            for j in scan.spectral_selection.clone() {
                                self.coefficients_finished[i] |= 1 << j;
                            }
                        }
                    }
                    let is_final_scan = scan.component_indices
                        .iter()
                        .all(|&i| self.coefficients_finished[i] == !0);
                    if worker.is_none() {
                        worker = Some(ThreadedWorker::new()?);
                    }
                    // TODO: Output data for each scan instead of only for the last scan
                    let (marker, data) =
                        self.decode_scan(frame, &scan, worker.as_mut().unwrap(), is_final_scan)?;
                    if let Some(data) = data {
                        for (i, plane) in data.into_iter()
                            .enumerate()
                            .filter(|&(_, ref plane)| !plane.is_empty())
                        {
                            planes[i] = plane;
                        }
                    }
                    pending_marker = marker;
                    scans_processed += 1;
                }
                // Table-specification and miscellaneous markers
                // Quantization table-specification
                Marker::DQT => {
                    let tables = parse_dqt(&mut self.input, &mut self.quantization_tables)?;
                    // TODO: Do we need to unsigzag?
                    // for (i, &table) in tables.into_iter().enumerate() {
                    //     if let Some(table) = table {
                    //         let mut unzigzagged_table = [0u16; 64];
                    //         for j in 0..64 {
                    //             unzigzagged_table[UNZIGZAG[j] as usize] = table[j];
                    //         }
                    //         self.quantization_tables[i] = Some(Arc::new(unzigzagged_table));
                    //     }
                    // }
                }
                // Huffman table-specification
                Marker::DHT => {
                    let is_baseline = self.frame.as_ref().map(|frame| frame.is_baseline);
                    /* let (dc_tables, ac_tables) = */
                    parse_dht(
                        &mut self.input,
                        is_baseline,
                        &mut self.dc_huffman_tables,
                        &mut self.ac_huffman_tables,
                    )?;
                    // let current_dc_tables = mem::replace(&mut self.dc_huffman_tables, vec![]);
                    // self.dc_huffman_tables = dc_tables
                    //     .into_iter()
                    //     .zip(current_dc_tables.into_iter())
                    //     .map(|(a, b)| a.or(b))
                    //     .collect();
                    // let current_ac_tables = mem::replace(&mut self.ac_huffman_tables, vec![]);
                    // self.ac_huffman_tables = ac_tables
                    //     .into_iter()
                    //     .zip(current_ac_tables.into_iter())
                    //     .map(|(a, b)| a.or(b))
                    //     .collect();
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
                Marker::APP(..) => {
                    if let Some(data) = parse_app(&mut self.input, marker)? {
                        match data {
                            AppData::Adobe(color_transform) => {
                                self.color_transform = Some(color_transform)
                            }
                            AppData::Jfif => {
                                // From the JFIF spec:
                                // "The APP0 marker is used to identify a JPEG FIF file.
                                //     The JPEG FIF APP0 marker is mandatory right after the SOI marker."
                                // Some JPEGs in the wild does not follow this though, so we allow
                                // JFIF headers anywhere APP0 markers are allowed.
                                /*
                                if previous_marker != Marker::SOI {
                                    return Err(JpegError::Format("the JFIF APP0 marker must come right after the SOI marker"));
                                }
                                */
                                self.is_jfif = true;
                            }
                            AppData::Avi1 => self.is_mjpeg = true,
                        }
                    }
                }
                // Restart
                Marker::RST(..) => {
                    // Some encoders emit a final RST marker after entropy-coded data, which
                    // decode_scan does not take care of. So if we encounter one, we ignore it.
                    if previous_marker != Marker::SOS {
                        return Err(JpegError::Malformatted(
                            "RST found outside of entropy-coded data",
                        ));
                    }
                }
                // Define number of lines
                Marker::DNL => {
                    // Section B.2.1
                    // "If a DNL segment (see B.2.5) is present, it shall immediately follow the first scan."
                    if previous_marker != Marker::SOS || scans_processed != 1 {
                        return Err(JpegError::Malformatted(
                            "DNL is only allowed immediately after the first scan",
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
                    return Err(JpegError::Malformatted(&format!(
                        "{:?} marker found where not allowed",
                        marker
                    )))
                }
            }
            previous_marker = marker;
        }
        if planes.is_empty() || planes.iter().any(|plane| plane.is_empty()) {
            return Err(JpegError::Malformatted("no data found"));
        }
        let frame = self.frame.as_ref().unwrap();
        compute_image(
            &frame.components,
            &planes,
            frame.size,
            self.is_jfif,
            self.color_transform,
        )
    }

    fn read_marker(&mut self) -> JpegResult<Marker> {
        // This should be an error as the JPEG spec doesn't allow extraneous data between marker segments.
        // libjpeg allows this though and there are images in the wild utilising it, so we are
        // forced to support this behavior.
        // Sony Ericsson P990i is an example of a device which produce this sort of JPEGs.
        while self.input.read_byte_keep()? != 0xFF {}

        let mut byte = self.input.read_byte_keep()?;

        // Section B.1.1.2
        // "Any marker may optionally be preceded by any number of fill bytes, which are bytes assigned code X’FF’."
        while byte == 0xFF {
            byte = self.input.read_byte_keep()?;
        }

        match byte {
            0x00 => Err(JpegError::Malformatted(
                "0xFF00 found where marker was expected",
            )),
            _ => Ok(Marker::from_u8(byte).unwrap()),
        }
    }

    fn decode_scan(
        &mut self,
        frame: &FrameInfo,
        scan: &ScanInfo,
        worker: &mut ThreadedWorker,
        produce_data: bool,
    ) -> JpegResult<(Option<Marker>, Option<Vec<Vec<u8>>>)> {
        assert!(scan.component_indices.len() <= MAX_COMPONENTS);
        let components: Vec<Component> = scan.component_indices
            .iter()
            .map(|&i| frame.components[i].clone())
            .collect();
        // Verify that all required quantization tables has been set.
        if components
            .iter()
            .any(|component| self.quantization_tables[component.quantization_table_index].is_none())
        {
            return Err(JpegError::Malformatted("use of unset quantization table"));
        }
        if self.is_mjpeg {
            fill_default_mjpeg_tables(
                scan,
                &mut self.dc_huffman_tables,
                &mut self.ac_huffman_tables,
            );
        }
        // Verify that all required huffman tables has been set.
        if scan.spectral_selection.start == 0
            && scan.dc_table_indices
                .iter()
                .any(|&i| self.dc_huffman_tables[i].is_none())
        {
            return Err(JpegError::Malformatted(
                "scan makes use of unset dc huffman table",
            ));
        }
        if scan.spectral_selection.end > 1
            && scan.ac_table_indices
                .iter()
                .any(|&i| self.ac_huffman_tables[i].is_none())
        {
            return Err(JpegError::Malformatted(
                "scan makes use of unset ac huffman table",
            ));
        }
        if produce_data {
            // Prepare the worker thread for the work to come.
            for (i, component) in components.iter().enumerate() {
                let row_data = RowData {
                    index: i,
                    component: component.clone(),
                    quantization_table: Arc::new(
                        self.quantization_tables[component.quantization_table_index].unwrap(),
                    ),
                };
                worker.start(row_data)?;
            }
        }
        let blocks_per_mcu: Vec<u16> = components
            .iter()
            .map(|c| c.horizontal_sampling_factor as u16 * c.vertical_sampling_factor as u16)
            .collect();
        let is_progressive = frame.coding_process == CodingProcess::DctProgressive;
        let is_interleaved = components.len() > 1;
        let mut dummy_block = [0i16; 64];
        let mut huffman = HuffmanDecoder::new();
        let mut dc_predictors = [0i16; MAX_COMPONENTS];
        let mut mcus_left_until_restart = self.restart_interval;
        let mut expected_rst_num = 0;
        let mut eob_run = 0;
        let mut mcu_row_coefficients = Vec::with_capacity(components.len());
        if produce_data && !is_progressive {
            for component in components.iter() {
                let coefficients_per_mcu_row = component.size_in_block.width as usize
                    * component.vertical_sampling_factor as usize
                    * 64;
                mcu_row_coefficients.push(vec![0i16; coefficients_per_mcu_row]);
            }
        }
        for mcu_y in 0..frame.size_in_mcu.height {
            for mcu_x in 0..frame.size_in_mcu.width {
                for (i, component) in components.iter().enumerate() {
                    for j in 0..blocks_per_mcu[i] {
                        let (block_x, block_y) = if is_interleaved {
                            // Section A.2.3
                            (
                                mcu_x * component.horizontal_sampling_factor as u16
                                    + j % component.horizontal_sampling_factor as u16,
                                mcu_y * component.vertical_sampling_factor as u16
                                    + j / component.horizontal_sampling_factor as u16,
                            )
                        } else {
                            // Section A.2.2

                            let blocks_per_row = component.size_in_block.width as usize;
                            let block_num = (mcu_y as usize * frame.size_in_mcu.width as usize
                                + mcu_x as usize)
                                * blocks_per_mcu[i] as usize
                                + j as usize;

                            let x = (block_num % blocks_per_row) as u16;
                            let y = (block_num / blocks_per_row) as u16;

                            if x * 8 >= component.size.width || y * 8 >= component.size.height {
                                continue;
                            }

                            (x, y)
                        };

                        let block_offset = (block_y as usize
                            * component.size_in_block.width as usize
                            + block_x as usize) * 64;
                        let mcu_row_offset = mcu_y as usize
                            * component.size_in_block.width as usize
                            * component.vertical_sampling_factor as usize
                            * 64;
                        let coefficients = if is_progressive {
                            &mut self.coefficients[scan.component_indices[i]]
                                [block_offset..block_offset + 64]
                        } else if produce_data {
                            &mut mcu_row_coefficients[i]
                                [block_offset - mcu_row_offset..block_offset - mcu_row_offset + 64]
                        } else {
                            &mut dummy_block[..]
                        };

                        if scan.successive_approximation_high == 0 {
                            decode_block(
                                &mut self.input,
                                coefficients,
                                &mut huffman,
                                self.dc_huffman_tables[scan.dc_table_indices[i]].as_ref(),
                                self.ac_huffman_tables[scan.ac_table_indices[i]].as_ref(),
                                scan.spectral_selection.clone(),
                                scan.successive_approximation_low,
                                &mut eob_run,
                                &mut dc_predictors[i],
                            )?;
                        } else {
                            decode_block_successive_approximation(
                                &mut self.input,
                                coefficients,
                                &mut huffman,
                                self.ac_huffman_tables[scan.ac_table_indices[i]].as_ref(),
                                scan.spectral_selection.clone(),
                                scan.successive_approximation_low,
                                &mut eob_run,
                            )?;
                        }
                    }
                }

                if self.restart_interval > 0 {
                    let is_last_mcu = mcu_x == frame.size_in_mcu.width - 1
                        && mcu_y == frame.size_in_mcu.height - 1;
                    mcus_left_until_restart -= 1;

                    if mcus_left_until_restart == 0 && !is_last_mcu {
                        match huffman.take_marker(&mut self.input)? {
                            Some(Marker::RST(n)) => {
                                if n != expected_rst_num {
                                    return Err(JpegError::Malformatted(&format!(
                                        "found RST{} where RST{} was expected",
                                        n, expected_rst_num
                                    )));
                                }

                                huffman.reset();
                                // Section F.2.1.3.1
                                dc_predictors = [0i16; MAX_COMPONENTS];
                                // Section G.1.2.2
                                eob_run = 0;

                                expected_rst_num = (expected_rst_num + 1) % 8;
                                mcus_left_until_restart = self.restart_interval;
                            }
                            Some(marker) => {
                                return Err(JpegError::Malformatted(&format!(
                                    "found marker {:?} inside scan where RST{} was expected",
                                    marker, expected_rst_num
                                )))
                            }
                            None => {
                                return Err(JpegError::Malformatted(&format!(
                                    "no marker found where RST{} was expected",
                                    expected_rst_num
                                )))
                            }
                        }
                    }
                }
            }

            if produce_data {
                // Send the coefficients from this MCU row to the worker thread for dequantization and idct.
                for (i, component) in components.iter().enumerate() {
                    let coefficients_per_mcu_row = component.size_in_block.width as usize
                        * component.vertical_sampling_factor as usize
                        * 64;

                    let row_coefficients = if is_progressive {
                        let offset = mcu_y as usize * coefficients_per_mcu_row;
                        self.coefficients[scan.component_indices[i]]
                            [offset..offset + coefficients_per_mcu_row]
                            .to_vec()
                    } else {
                        mem::replace(
                            &mut mcu_row_coefficients[i],
                            vec![0i16; coefficients_per_mcu_row],
                        )
                    };

                    worker.append_row((i, row_coefficients))?;
                }
            }
        }

        let marker = huffman.take_marker(&mut self.input)?;

        if produce_data {
            // Retrieve all the data from the worker thread.
            let mut data = vec![Vec::new(); frame.components.len()];

            for (i, &component_index) in scan.component_indices.iter().enumerate() {
                data[component_index] = worker.get_result(i)?;
            }

            Ok((marker, Some(data)))
        } else {
            Ok((marker, None))
        }
    }
}

fn decode_block(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    dc_table: Option<&HuffmanTable>,
    ac_table: Option<&HuffmanTable>,
    spectral_selection: Range<u8>,
    successive_approximation_low: u8,
    eob_run: &mut u16,
    dc_predictor: &mut i16,
) -> JpegResult<()> {
    debug_assert_eq!(coefficients.len(), 64);

    if spectral_selection.start == 0 {
        // Section F.2.2.1
        // Figure F.12
        let value = huffman.decode(input, dc_table.unwrap())?;
        let diff = match value {
            0 => 0,
            _ => {
                // Section F.1.2.1.1
                // Table F.1
                if value > 11 {
                    return Err(JpegError::Malformatted(
                        "invalid DC difference magnitude category",
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

    let mut index = cmp::max(spectral_selection.start, 1);

    if index < spectral_selection.end && *eob_run > 0 {
        *eob_run -= 1;
        return Ok(());
    }

    // Section F.1.2.2.1
    while index < spectral_selection.end {
        if let Some((value, run)) = huffman.decode_fast_ac(input, ac_table.unwrap())? {
            index += run;

            if index >= spectral_selection.end {
                break;
            }

            coefficients[UNZIGZAG[index as usize] as usize] = value << successive_approximation_low;
            index += 1;
        } else {
            let byte = huffman.decode(input, ac_table.unwrap())?;
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

                coefficients[UNZIGZAG[index as usize] as usize] =
                    huffman.receive_extend(input, s)? << successive_approximation_low;
                index += 1;
            }
        }
    }

    Ok(())
}

fn decode_block_successive_approximation(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    ac_table: Option<&HuffmanTable>,
    spectral_selection: Range<u8>,
    successive_approximation_low: u8,
    eob_run: &mut u16,
) -> JpegResult<()> {
    debug_assert_eq!(coefficients.len(), 64);

    let bit = 1 << successive_approximation_low;

    if spectral_selection.start == 0 {
        // Section G.1.2.1

        if huffman.get_bits(input, 1)? == 1 {
            coefficients[0] |= bit;
        }
    } else {
        // Section G.1.2.3

        if *eob_run > 0 {
            *eob_run -= 1;
            refine_non_zeroes(input, coefficients, huffman, spectral_selection, 64, bit)?;
            return Ok(());
        }

        let mut index = spectral_selection.start;

        while index < spectral_selection.end {
            let byte = huffman.decode(input, ac_table.unwrap())?;
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
                1 => {
                    if huffman.get_bits(input, 1)? == 1 {
                        value = bit;
                    } else {
                        value = -bit;
                    }
                }
                _ => return Err(JpegError::Malformatted("unexpected huffman code")),
            }

            let range = Range {
                start: index,
                end: spectral_selection.end,
            };
            index = refine_non_zeroes(input, coefficients, huffman, range, zero_run_length, bit)?;

            if value != 0 {
                coefficients[UNZIGZAG[index as usize] as usize] = value;
            }

            index += 1;
        }
    }

    Ok(())
}

fn refine_non_zeroes(
    input: &mut InputStream,
    coefficients: &mut [i16],
    huffman: &mut HuffmanDecoder,
    range: Range<u8>,
    zrl: u8,
    bit: i16,
) -> JpegResult<u8> {
    debug_assert_eq!(coefficients.len(), 64);

    let last = range.end - 1;
    let mut zero_run_length = zrl;

    for i in range {
        let index = UNZIGZAG[i as usize] as usize;

        if coefficients[index] == 0 {
            if zero_run_length == 0 {
                return Ok(i);
            }

            zero_run_length -= 1;
        } else if huffman.get_bits(input, 1)? == 1 && coefficients[index] & bit == 0 {
            if coefficients[index] > 0 {
                coefficients[index] += bit;
            } else {
                coefficients[index] -= bit;
            }
        }
    }

    Ok(last)
}

fn compute_image(
    components: &[Component],
    data: &[Vec<u8>],
    output_size: Dimensions,
    is_jfif: bool,
    color_transform: Option<AdobeColorTransform>,
) -> JpegResult<Vec<u8>> {
    if data.iter().any(|data| data.is_empty()) {
        return Err(JpegError::Malformatted("not all components has data"));
    }

    if components.len() == 1 {
        let component = &components[0];

        if component.size.width % 8 == 0 && component.size.height % 8 == 0 {
            return Ok(data[0].clone());
        }

        let mut buffer = vec![0u8; component.size.width as usize * component.size.height as usize];
        let line_stride = component.size_in_block.width as usize * 8;

        for y in 0..component.size.height as usize {
            for x in 0..component.size.width as usize {
                buffer[y * component.size.width as usize + x] = data[0][y * line_stride + x];
            }
        }

        Ok(buffer)
    } else {
        compute_image_parallel(components, data, output_size, is_jfif, color_transform)
    }
}

fn compute_image_parallel(
    components: &[Component],
    data: &[Vec<u8>],
    output_size: Dimensions,
    is_jfif: bool,
    color_transform: Option<AdobeColorTransform>,
) -> JpegResult<Vec<u8>> {
    let color_convert_func = choose_color_convert_func(components.len(), is_jfif, color_transform)?;
    let upsampler = Upsampler::new(components, output_size.width, output_size.height)?;
    let line_size = output_size.width as usize * components.len();
    let mut image = vec![0u8; line_size * output_size.height as usize];

    for (row, line) in image.chunks_mut(line_size).enumerate() {
        upsampler.upsample_and_interleave_row(data, row, output_size.width as usize, line);
        color_convert_func(line, output_size.width as usize);
    }

    Ok(image)
}

fn choose_color_convert_func(
    component_count: usize,
    _is_jfif: bool,
    color_transform: Option<AdobeColorTransform>,
) -> JpegResult<fn(&mut [u8], usize)> {
    match component_count {
        3 => {
            // http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/JPEG.html#Adobe
            // Unknown means the data is RGB, so we don't need to perform any color conversion on it.
            if color_transform == Some(AdobeColorTransform::Unknown) {
                Ok(color_convert_line_null)
            } else {
                Ok(color_convert_line_ycbcr)
            }
        }
        4 => {
            // http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/JPEG.html#Adobe
            match color_transform {
                Some(AdobeColorTransform::Unknown) => Ok(color_convert_line_cmyk),
                Some(_) => Ok(color_convert_line_ycck),
                None => Err(JpegError::Malformatted(
                    "4 components without Adobe APP14 metadata to tell color space",
                )),
            }
        }
        _ => panic!(),
    }
}

fn color_convert_line_null(_data: &mut [u8], _width: usize) {}

fn color_convert_line_ycbcr(data: &mut [u8], width: usize) {
    for i in 0..width {
        let (r, g, b) = ycbcr_to_rgb(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]);

        data[i * 3] = r;
        data[i * 3 + 1] = g;
        data[i * 3 + 2] = b;
    }
}

fn color_convert_line_ycck(data: &mut [u8], width: usize) {
    for i in 0..width {
        let (r, g, b) = ycbcr_to_rgb(data[i * 4], data[i * 4 + 1], data[i * 4 + 2]);
        let k = data[i * 4 + 3];

        data[i * 4] = r;
        data[i * 4 + 1] = g;
        data[i * 4 + 2] = b;
        data[i * 4 + 3] = 255 - k;
    }
}

fn color_convert_line_cmyk(data: &mut [u8], width: usize) {
    for i in 0..width {
        data[i * 4] = 255 - data[i * 4];
        data[i * 4 + 1] = 255 - data[i * 4 + 1];
        data[i * 4 + 2] = 255 - data[i * 4 + 2];
        data[i * 4 + 3] = 255 - data[i * 4 + 3];
    }
}

// ITU-R BT.601
fn ycbcr_to_rgb(y: u8, cb: u8, cr: u8) -> (u8, u8, u8) {
    let y = y as f32;
    let cb = cb as f32 - 128.0;
    let cr = cr as f32 - 128.0;

    let r = y + 1.40200 * cr;
    let g = y - 0.34414 * cb - 0.71414 * cr;
    let b = y + 1.77200 * cb;

    (
        clamp((r + 0.5) as i32, 0, 255) as u8,
        clamp((g + 0.5) as i32, 0, 255) as u8,
        clamp((b + 0.5) as i32, 0, 255) as u8,
    )
}

fn clamp<T: PartialOrd>(value: T, min: T, max: T) -> T {
    if value < min {
        return min;
    }
    if value > max {
        return max;
    }
    value
}
