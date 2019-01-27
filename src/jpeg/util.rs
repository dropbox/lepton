use std::fmt::Debug;
use std::mem;

use jpeg::{Component, Dimensions, Scan, ScanInfo};

pub fn process_scan<T: Debug>(
    scan: &mut Scan,
    components: &mut Vec<Component>, // Components in the scan
    mcu_y_start: usize,
    size_in_mcu: &Dimensions,
    mcu_callback: &mut FnMut(usize, usize, bool, u8) -> Result<bool, T>, // Args: (mcu_y, mcu_x, restart, expected rst)
    block_callback: &mut FnMut(usize, usize, usize, &mut Component, &mut Scan) -> Result<bool, T>, // Args: (block_y, block_x, component_index_in_scan, component, Scan)
) -> Result<bool, T> {
    let is_interleaved = components.len() > 1;
    let &size_in_mcu = if is_interleaved {
        &size_in_mcu
    } else {
        &components[0].size_in_block
    };
    let (mut n_mcu_left_until_restart, mut expected_rst) =
        if mcu_y_start > 0 && scan.restart_interval > 0 {
            let mcu_skipped = mcu_y_start * size_in_mcu.width as usize;
            let restart_interval = scan.restart_interval as usize;
            let mut rst_skipped = mcu_skipped / restart_interval;
            let mut mcu_skipped_in_interval = mcu_skipped % restart_interval;
            if mcu_skipped_in_interval == 0 {
                mcu_skipped_in_interval = restart_interval;
                rst_skipped -= 1;
            }
            (
                (restart_interval - mcu_skipped_in_interval) as u16,
                (rst_skipped % 8) as u8,
            )
        } else {
            (scan.restart_interval, 0)
        };
    for mcu_y in mcu_y_start..size_in_mcu.height as usize {
        for mcu_x in 0..size_in_mcu.width as usize {
            let restart = scan.restart_interval > 0 && n_mcu_left_until_restart == 0;
            if mcu_callback(mcu_y, mcu_x, restart, expected_rst)? {
                return Ok(false);
            }
            if restart {
                expected_rst = (expected_rst + 1) % 8;
                n_mcu_left_until_restart = scan.restart_interval;
            }
            if is_interleaved {
                for (i, component) in components.iter_mut().enumerate() {
                    for block_y_offset in 0..component.vertical_sampling_factor as usize {
                        for block_x_offset in 0..component.horizontal_sampling_factor as usize {
                            let block_y = mcu_y * component.vertical_sampling_factor as usize
                                + block_y_offset;
                            let block_x = mcu_x * component.horizontal_sampling_factor as usize
                                + block_x_offset;
                            if block_callback(block_y, block_x, i, component, scan)? {
                                return Ok(false);
                            }
                        }
                    }
                }
            } else {
                if block_callback(mcu_y, mcu_x, 0, &mut components[0], scan)? {
                    return Ok(false);
                }
            }
            if scan.restart_interval > 0 {
                n_mcu_left_until_restart -= 1;
            }
        }
    }
    Ok(true)
}

pub fn get_components(component_indices: &[usize], all_components: &mut [Component]) -> Vec<Component> {
    component_indices
        .iter()
        .map(|&i| all_components[i].clone())
        .collect()
}

pub fn split_scan(
    scan: &mut Scan,
    components: &[Component],
    mcu_y_start: u16,
    mcu_y_end: Option<u16>,
) -> Scan {
    // FIXME: Can further reduce copying when the who scan is taken
    let component_indices = &scan.info.component_indices;
    let scan_info = &scan.info;
    let splits = scan.coefficients.as_mut().map(|coefficients| {
        component_indices.iter().enumerate().fold(
            Vec::with_capacity(component_indices.len()),
            |mut acc, (i, &component_index)| {
                let component = &components[component_index];
                let coefficients = &mut coefficients[i];
                let split = coefficients
                    .iter()
                    .take(mcu_y_end.map_or(coefficients.len(), |mcu_row| {
                        mcu_row_offset(scan_info, component, mcu_row)
                    }))
                    .skip(mcu_row_offset(scan_info, component, mcu_y_start))
                    .cloned()
                    .collect();
                acc.push(split);
                acc
            },
        )
    });
    let new_scan = Scan {
        raw_header: mem::replace(&mut scan.raw_header, vec![]),
        info: scan.info.clone(),
        restart_interval: scan.restart_interval,
        coefficients: splits,
        truncation: scan.truncation.clone(),
        dc_encode_table: scan.dc_encode_table.clone(),
        ac_encode_table: scan.ac_encode_table.clone(),
    };
    new_scan
}

pub fn mcu_row_offset(scan: &ScanInfo, component: &Component, mcu_row: u16) -> usize {
    let vertical_sampling_factor = if scan.component_indices.len() == 1 {
        1
    } else {
        component.vertical_sampling_factor as usize
    };
    mcu_row as usize
        * vertical_sampling_factor
        * component.size_in_block.width as usize
        * n_coefficient_per_block(scan)
}

pub fn n_coefficient_per_block(_scan: &ScanInfo) -> usize {
    // if scan.successive_approximation_high > 0 {
    //     17
    // } else {
    //     65
    // }
    65
}

pub fn split_into_size_and_value(coefficient: i16) -> (u8, u16) {
    let size = (16 - coefficient.abs().leading_zeros()) as u8;
    let mask = (1i16 << size as usize).wrapping_sub(1);
    let val = if coefficient < 0 {
        coefficient.wrapping_sub(1) & mask
    } else {
        coefficient & mask
    };
    (size, val as u16)
}

// Section F.2.2.1
// Figure F.12
pub fn build_from_size_and_value(count: u8, value: u16) -> i16 {
    assert!((1 << count) > value);
    if count > 0 {
        let vt = 1 << (count as u16 - 1);
        if value < vt {
            value as i16 + (-1 << count as i16) + 1
        } else {
            value as i16
        }
    } else {
        0i16
    }
}
