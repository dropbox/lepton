use interface::SimpleResult;
use jpeg::{Component, Dimensions, Scan};

pub fn process_scan<T>(
    scan: &mut Scan,
    components: &Vec<Component>, // Components in the scan
    size_in_mcu: &Dimensions,
    mcu_row_callback: &mut FnMut(usize) -> SimpleResult<T>, // Args: (mcu_y)
    mcu_callback: &mut FnMut(usize, usize) -> SimpleResult<T>, // Args: (mcu_y, mcu_x)
    block_callback: &mut FnMut(usize, usize, usize, &Component, &mut Scan) -> SimpleResult<T>, // Args: (block_y, block_x, component_index_in_scan, component, Scan)
    rst_callback: &mut FnMut(u8) -> SimpleResult<T>, // Args: (expected_rst)
) -> SimpleResult<T> {
    let is_interleaved = components.len() > 1;
    let &size_in_mcu = if is_interleaved {
        &size_in_mcu
    } else {
        &components[0].size_in_block
    };
    let mut n_mcu_left_until_restart = scan.restart_interval;
    let mut expected_rst: u8 = 0;
    for mcu_y in 0..size_in_mcu.height as usize {
        mcu_row_callback(mcu_y)?;
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
