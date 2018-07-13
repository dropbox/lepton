use super::error::{JpegError, JpegResult};
use super::huffman::{HuffmanTable, HuffmanTableClass};
use super::jpeg::{CodingProcess, Component, Dimensions, EntropyCoding, FrameInfo, ScanInfo};
use super::marker::Marker;
use super::marker::Marker::*;
use byte_converter::BigEndian;
use iostream::InputStream;

fn read_length(input: &mut InputStream, marker: Marker) -> JpegResult<usize> {
    assert!(marker.has_length());
    // length is including itself.
    let length = input.read_u16::<BigEndian>(true)? as usize;
    if length <= 2 {
        return Err(JpegError::Malformatted(format!(
            "encountered {:?} with invalid length {}",
            marker, length
        )));
    }
    Ok(length - 2)
}

fn skip_bytes(input: &mut InputStream, length: usize) -> JpegResult<()> {
    input.consume(length, true)?;
    Ok(())
}

// Section B.2.2
pub fn parse_sof(input: &mut InputStream, n: u8) -> JpegResult<FrameInfo> {
    match n {
        0...3 | 5...7 | 9...11 | 13...15 => (),
        _ => panic!("Invalid SOF (n={})", n),
    }
    let length = read_length(input, SOF(n))?;

    if length <= 6 {
        return Err(JpegError::Malformatted("invalid length in SOF".to_owned()));
    }
    let is_baseline = n == 0;
    let is_differential = match n {
        0...3 | 9...11 => false,
        5...7 | 13...15 => true,
        _ => unreachable!(),
    };
    let coding_process = match n {
        0 | 1 | 5 | 9 | 13 => CodingProcess::DctSequential,
        2 | 6 | 10 | 14 => CodingProcess::DctProgressive,
        3 | 7 | 11 | 15 => CodingProcess::Lossless,
        _ => unreachable!(),
    };
    let entropy_coding = match n {
        0...3 | 5...7 => EntropyCoding::Huffman,
        9...11 | 13...15 => EntropyCoding::Arithmetic,
        _ => unreachable!(),
    };
    let precision = input.read_byte(true)?;
    match precision {
        8 => {}
        12 => {
            if is_baseline {
                return Err(JpegError::Malformatted(
                    "12 bit sample precision is not allowed in baseline".to_owned(),
                ));
            }
        }
        _ => {
            if coding_process != CodingProcess::Lossless {
                return Err(JpegError::Malformatted(format!(
                    "invalid precision {} in frame header",
                    precision
                )));
            }
        }
    }
    let height = input.read_u16::<BigEndian>(true)?;
    let width = input.read_u16::<BigEndian>(true)?;
    // height:
    // "Value 0 indicates that the number of lines shall be defined by the DNL marker and
    //     parameters at the end of the first scan (see B.2.5)."
    if width == 0 {
        return Err(JpegError::Malformatted(
            "zero width in frame header".to_owned(),
        ));
    }
    let component_count = input.read_byte(true)?;
    if component_count == 0 {
        return Err(JpegError::Malformatted(
            "zero component count in frame header".to_owned(),
        ));
    }
    if coding_process == CodingProcess::DctProgressive && component_count > 4 {
        return Err(JpegError::Malformatted(
            "progressive frame with more than 4 components".to_owned(),
        ));
    }
    if length != 6 + 3 * (component_count as usize) {
        return Err(JpegError::Malformatted("invalid length in SOF".to_owned()));
    }
    let mut components: Vec<Component> = Vec::with_capacity(component_count as usize);
    for _ in 0..component_count {
        let identifier = input.read_byte(true)?;
        // Each component's identifier must be unique.
        if components.iter().any(|c| c.identifier == identifier) {
            return Err(JpegError::Malformatted(format!(
                "duplicate frame component identifier {}",
                identifier
            )));
        }
        let byte = input.read_byte(true)?;
        let horizontal_sampling_factor = byte >> 4;
        let vertical_sampling_factor = byte & 0x0f;
        if horizontal_sampling_factor == 0 || horizontal_sampling_factor > 4 {
            return Err(JpegError::Malformatted(format!(
                "invalid horizontal sampling factor {}",
                horizontal_sampling_factor
            )));
        }
        if vertical_sampling_factor == 0 || vertical_sampling_factor > 4 {
            return Err(JpegError::Malformatted(format!(
                "invalid vertical sampling factor {}",
                vertical_sampling_factor
            )));
        }
        let quantization_table_index = input.read_byte(true)?;

        if quantization_table_index > 3
            || (coding_process == CodingProcess::Lossless && quantization_table_index != 0)
        {
            return Err(JpegError::Malformatted(format!(
                "invalid quantization table index {}",
                quantization_table_index
            )));
        }
        components.push(Component {
            identifier: identifier,
            horizontal_sampling_factor: horizontal_sampling_factor,
            vertical_sampling_factor: vertical_sampling_factor,
            quantization_table_index: quantization_table_index as usize,
            size: Dimensions::default(),
            size_in_block: Dimensions::default(),
        });
    }
    let h_samp_max = components
        .iter()
        .map(|c| c.horizontal_sampling_factor)
        .max()
        .unwrap();
    let v_samp_max = components
        .iter()
        .map(|c| c.vertical_sampling_factor)
        .max()
        .unwrap();
    let size_in_mcu = Dimensions {
        width: (width as f32 / (h_samp_max as f32 * 8.0)).ceil() as u16,
        height: (height as f32 / (v_samp_max as f32 * 8.0)).ceil() as u16,
    };
    for component in components.iter_mut() {
        component.size.width = (width as f32
            * (component.horizontal_sampling_factor as f32 / h_samp_max as f32))
            .ceil() as u16;
        component.size.height = (height as f32
            * (component.vertical_sampling_factor as f32 / v_samp_max as f32))
            .ceil() as u16;

        component.size_in_block.width =
            size_in_mcu.width * component.horizontal_sampling_factor as u16;
        component.size_in_block.height =
            size_in_mcu.height * component.vertical_sampling_factor as u16;
    }
    Ok(FrameInfo {
        is_baseline,
        is_differential,
        coding_process,
        entropy_coding,
        precision,
        size: Dimensions {
            width: width,
            height: height,
        },
        size_in_mcu,
        components,
    })
}

// Section B.2.3
pub fn parse_sos(input: &mut InputStream, frame: &FrameInfo) -> JpegResult<ScanInfo> {
    let length = read_length(input, SOS)?;
    let component_count = input.read_byte(true)? as usize;
    if component_count == 0 || component_count > 4 {
        return Err(JpegError::Malformatted(format!(
            "invalid component count {} in scan header",
            component_count
        )));
    }
    if length != 4 + 2 * component_count {
        return Err(JpegError::Malformatted("invalid length in SOS".to_owned()));
    }
    let mut component_indices = vec![0usize; component_count];
    let mut dc_table_indices = vec![0usize; component_count];
    let mut ac_table_indices = vec![0usize; component_count];
    for _ in 0..component_count {
        let identifier = input.read_byte(true)?;
        let component_index = match frame.components.iter().position(|c| c.identifier == identifier) {
            Some(value) => value,
            None => return Err(JpegError::Malformatted(format!("scan component identifier {} does not match any of the component identifiers defined in the frame", identifier))),
        };
        // Each of the scan's components must be unique.
        if component_indices.contains(&component_index) {
            return Err(JpegError::Malformatted(format!(
                "duplicate scan component identifier {}",
                identifier
            )));
        }
        // "... the ordering in the scan header shall follow the ordering in the frame header."
        if component_index < *component_indices.iter().max().unwrap_or(&0) {
            return Err(JpegError::Malformatted(
                "the scan component order does not follow the order in the frame header".to_owned(),
            ));
        }
        let byte = input.read_byte(true)?;
        let dc_table_index = byte >> 4;
        let ac_table_index = byte & 0x0f;
        if dc_table_index > 3 || (frame.is_baseline && dc_table_index > 1) {
            return Err(JpegError::Malformatted(format!(
                "invalid dc table index {}",
                dc_table_index
            )));
        }
        if ac_table_index > 3 || (frame.is_baseline && ac_table_index > 1) {
            return Err(JpegError::Malformatted(format!(
                "invalid ac table index {}",
                ac_table_index
            )));
        }
        component_indices.push(component_index);
        dc_table_indices.push(dc_table_index as usize);
        ac_table_indices.push(ac_table_index as usize);
    }
    let blocks_per_mcu = component_indices
        .iter()
        .map(|&i| {
            frame.components[i].horizontal_sampling_factor as u32
                * frame.components[i].vertical_sampling_factor as u32
        })
        .fold(0, ::std::ops::Add::add);
    if component_count > 1 && blocks_per_mcu > 10 {
        return Err(JpegError::Malformatted(
            "scan with more than one component and more than 10 blocks per MCU".to_owned(),
        ));
    }
    let spectral_selection_start = input.read_byte(true)?;
    let spectral_selection_end = input.read_byte(true)?;
    let byte = input.read_byte(true)?;
    let successive_approximation_high = byte >> 4;
    let successive_approximation_low = byte & 0x0f;
    if frame.coding_process == CodingProcess::DctProgressive {
        if spectral_selection_end > 63
            || spectral_selection_start > spectral_selection_end
            || (spectral_selection_start == 0 && spectral_selection_end != 0)
        {
            return Err(JpegError::Malformatted(format!(
                "invalid spectral selection parameters: ss={}, se={}",
                spectral_selection_start, spectral_selection_end
            )));
        }
        if spectral_selection_start != 0 && component_count != 1 {
            return Err(JpegError::Malformatted(
                "spectral selection scan with AC coefficients can't have more than one component"
                    .to_owned(),
            ));
        }
        if successive_approximation_high > 13 || successive_approximation_low > 13 {
            return Err(JpegError::Malformatted(format!(
                "invalid successive approximation parameters: ah={}, al={}",
                successive_approximation_high, successive_approximation_low
            )));
        }
        // Section G.1.1.1.2
        // "Each scan which follows the first scan for a given band progressively improves
        //     the precision of the coefficients by one bit, until full precision is reached."
        if successive_approximation_high != 0
            && successive_approximation_high != successive_approximation_low + 1
        {
            return Err(JpegError::Malformatted(
                "successive approximation scan with more than one bit of improvement".to_owned(),
            ));
        }
    } else {
        if spectral_selection_start != 0 || spectral_selection_end != 63 {
            return Err(JpegError::Malformatted(
                "spectral selection is not allowed in non-progressive scan".to_owned(),
            ));
        }
        if successive_approximation_high != 0 || successive_approximation_low != 0 {
            return Err(JpegError::Malformatted(
                "successive approximation is not allowed in non-progressive scan".to_owned(),
            ));
        }
    }
    Ok(ScanInfo {
        component_indices: component_indices,
        dc_table_indices: dc_table_indices,
        ac_table_indices: ac_table_indices,
        spectral_selection: spectral_selection_start..(spectral_selection_end + 1),
        successive_approximation_high: successive_approximation_high,
        successive_approximation_low: successive_approximation_low,
    })
}

// Section B.2.4.1
pub fn parse_dqt(input: &mut InputStream, tables: &mut [Option<[u16; 64]>; 4]) -> JpegResult<()> {
    let mut length = read_length(input, DQT)?;
    // Each DQT segment may contain multiple quantization tables.
    while length > 0 {
        let byte = input.read_byte(true)?;
        let precision = (byte >> 4) as usize;
        let index = (byte & 0x0f) as usize;

        // The combination of 8-bit sample precision and 16-bit quantization tables is explicitly
        // disallowed by the JPEG spec:
        //     "An 8-bit DCT-based process shall not use a 16-bit precision quantization table."
        //     "Pq: Quantization table element precision – Specifies the precision of the Qk
        //      values. Value 0 indicates 8-bit Qk values; value 1 indicates 16-bit Qk values. Pq
        //      shall be zero for 8 bit sample precision P (see B.2.2)."
        // libjpeg allows this behavior though, and there are images in the wild using it. So to
        // match libjpeg's behavior we are deviating from the JPEG spec here.
        if precision > 1 {
            return Err(JpegError::Malformatted(format!(
                "invalid precision {} in DQT",
                precision
            )));
        }
        if index > 3 {
            return Err(JpegError::Malformatted(format!(
                "invalid destination identifier {} in DQT",
                index
            )));
        }
        if length < 65 + 64 * precision {
            return Err(JpegError::Malformatted("invalid length in DQT".to_owned()));
        }
        let mut table = [0u16; 64];
        for i in 0..64 {
            table[i] = match precision {
                0 => input.read_byte(true)? as u16,
                1 => input.read_u16::<BigEndian>(true)?,
                _ => unreachable!(),
            };
        }
        if table.iter().any(|&val| val == 0) {
            return Err(JpegError::Malformatted(
                "quantization table contains element with a zero value".to_owned(),
            ));
        }
        tables[index] = Some(table);
        length -= 65 + 64 * precision;
    }
    Ok(())
}

// Section B.2.4.2
pub fn parse_dht(
    input: &mut InputStream,
    is_baseline: Option<bool>,
    dc_tables: &mut [Option<HuffmanTable>; 4],
    ac_tables: &mut [Option<HuffmanTable>; 4],
) -> JpegResult<()> {
    let mut length = read_length(input, DHT)?;
    let mut counts = [0u8; 16];
    let mut values = vec![];
    // Each DHT segment may contain multiple huffman tables.
    while length > 17 {
        let byte = input.read_byte(true)?;
        let class = byte >> 4;
        let index = (byte & 0x0f) as usize;
        if class != 0 && class != 1 {
            return Err(JpegError::Malformatted(format!(
                "invalid class {} in DHT",
                class
            )));
        }
        if is_baseline == Some(true) && index > 1 {
            return Err(JpegError::Malformatted(
                "a maximum of two huffman tables per class are allowed in baseline".to_owned(),
            ));
        }
        if index > 3 {
            return Err(JpegError::Malformatted(format!(
                "invalid destination identifier {} in DHT",
                index
            )));
        }
        input.read(&mut counts, true, true)?;
        let size = counts
            .iter()
            .map(|&val| val as usize)
            .fold(0, ::std::ops::Add::add);

        if size == 0 {
            return Err(JpegError::Malformatted(
                "encountered table with zero length in DHT".to_owned(),
            ));
        } else if size > 256 {
            return Err(JpegError::Malformatted(
                "encountered table with excessive length in DHT".to_owned(),
            ));
        } else if size > length - 17 {
            return Err(JpegError::Malformatted("invalid length in DHT".to_owned()));
        }
        values.resize(size, 0);
        input.read(&mut values, true, true)?;
        match class {
            0 => {
                dc_tables[index] = Some(HuffmanTable::new(&counts, &values, HuffmanTableClass::DC)?)
            }
            1 => {
                ac_tables[index] = Some(HuffmanTable::new(&counts, &values, HuffmanTableClass::AC)?)
            }
            _ => unreachable!(),
        }
        length -= 17 + size;
    }
    if length != 0 {
        return Err(JpegError::Malformatted("invalid length in DHT".to_owned()));
    }
    Ok(())
}

// Section B.2.4.4
pub fn parse_dri(input: &mut InputStream) -> JpegResult<u16> {
    let length = read_length(input, DRI)?;
    if length != 2 {
        return Err(JpegError::Malformatted(
            "DRI with invalid length".to_owned(),
        ));
    }
    Ok(input.read_u16::<BigEndian>(true)?)
}

// Section B.2.4.5
pub fn parse_com(input: &mut InputStream) -> JpegResult<Vec<u8>> {
    let length = read_length(input, COM)?;
    let mut buffer = vec![0u8; length];
    input.read(&mut buffer, true, true)?;
    Ok(buffer)
}

// Section B.2.4.6
pub fn parse_app(input: &mut InputStream, marker: Marker) -> JpegResult<bool> {
    let length = read_length(input, marker)?;
    let mut bytes_read = 0;
    let mut result = false;
    match marker {
        APP(0) => {
            if length >= 5 {
                let mut buffer = [0u8; 5];
                input.read(&mut buffer, true, true)?;
                bytes_read = buffer.len();
                // https://sno.phy.queensu.ca/~phil/exiftool/TagNames/JPEG.html#AVI1
                if &buffer[0..5] == &[b'A', b'V', b'I', b'1', b'\0'] {
                    result = true;
                }
            }
        }
        _ => (),
    }
    skip_bytes(input, length - bytes_read)?;
    Ok(result)
}
