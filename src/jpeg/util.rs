use std::fmt::Debug;
use std::mem;

use interface::SimpleResult;
use jpeg::{Component, Dimensions, Scan};

pub fn process_scan<T: Debug>(
    scan: &mut Scan,
    components: &Vec<Component>, // Components in the scan
    mcu_y_start: usize,
    size_in_mcu: &Dimensions,
    mcu_row_callback: &mut FnMut(usize) -> Result<bool, T>, // Args: (mcu_y)
    mcu_callback: &mut FnMut(usize, usize) -> SimpleResult<T>, // Args: (mcu_y, mcu_x)
    block_callback: &mut FnMut(usize, usize, usize, &Component, &mut Scan) -> Result<bool, T>, // Args: (block_y, block_x, component_index_in_scan, component, Scan)
    rst_callback: &mut FnMut(u8) -> SimpleResult<T>, // Args: (expected_rst)
) -> SimpleResult<T> {
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
            let rst_skipped = mcu_skipped / restart_interval;
            (
                (restart_interval - mcu_skipped % restart_interval) as u16,
                (rst_skipped % 8) as u8,
            )
        } else {
            (scan.restart_interval, 0)
        };
    for mcu_y in mcu_y_start..size_in_mcu.height as usize {
        if mcu_row_callback(mcu_y)? {
            break;
        }
        for mcu_x in 0..size_in_mcu.width as usize {
            mcu_callback(mcu_y, mcu_x)?;
            if is_interleaved {
                for (i, component) in components.iter().enumerate() {
                    for block_y_offset in 0..component.vertical_sampling_factor as usize {
                        for block_x_offset in 0..component.horizontal_sampling_factor as usize {
                            let block_y = mcu_y * component.vertical_sampling_factor as usize
                                + block_y_offset;
                            let block_x = mcu_x * component.horizontal_sampling_factor as usize
                                + block_x_offset;
                            block_callback(block_y, block_x, i, component, scan)?;
                        }
                    }
                }
            } else {
                block_callback(mcu_y, mcu_x, 0, &components[0], scan)?;
            }
            if scan.restart_interval > 0 {
                n_mcu_left_until_restart -= 1;
                let is_last_mcu =
                    mcu_x as u16 == size_in_mcu.width - 1 && mcu_y as u16 == size_in_mcu.height - 1;
                if n_mcu_left_until_restart == 0 && !is_last_mcu {
                    rst_callback(expected_rst)?;
                    expected_rst = (expected_rst + 1) % 8;
                    n_mcu_left_until_restart = scan.restart_interval;
                }
            }
        }
    }
    Ok(())
}

pub fn split_scan(
    scan: &mut Scan,
    components: &[Component],
    mcu_y_start: u16,
    mcu_y_end: Option<u16>,
) -> Scan {
    let component_indices = &scan.info.component_indices;
    let splits = scan.coefficients.as_mut().map(|coefficients| {
        component_indices.iter().enumerate().fold(
            Vec::with_capacity(component_indices.len()),
            |mut acc, (i, &component_index)| {
                let component = &components[component_index];
                let coefficients = &mut coefficients[i];
                let split = coefficients
                    .iter()
                    .take(mcu_y_end.map_or(coefficients.len(), |mcu_row| {
                        mcu_row_offset(coefficients.len(), component, mcu_row)
                    }))
                    .skip(mcu_row_offset(coefficients.len(), component, mcu_y_start))
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

pub fn mcu_row_offset(n_component: usize, component: &Component, mcu_row: u16) -> usize {
    let vertical_sampling_factor = if n_component == 1 {
        1
    } else {
        component.vertical_sampling_factor as usize
    };
    mcu_row as usize * vertical_sampling_factor * component.size_in_block.width as usize * 64
}
