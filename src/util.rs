use core::cmp::min;

pub fn mem_copy<T: Clone>(
    dest: &mut [T],
    dest_offset: &mut usize,
    src: &[T],
    src_offset: &mut usize,
) {
    let dest_slice = dest.split_at_mut(*dest_offset).1;
    let src_slice = src.split_at(*src_offset).1;
    let copy_len = min(src_slice.len(), dest_slice.len());
    dest_slice
        .split_at_mut(copy_len)
        .0
        .clone_from_slice(src_slice.split_at(copy_len).0);
    *dest_offset += copy_len;
    *src_offset += copy_len;
}

pub fn u32_to_u8_array(num: &u32) -> [u8; 4] {
    let tmp = *num;
    [
        (tmp >> 24) as u8,
        (tmp >> 16) as u8,
        (tmp >> 8) as u8,
        tmp as u8,
    ]
}

pub fn u8_array_to_u32(slice: &[u8], index: &usize) -> u32 {
    let index = *index;
    ((slice[index] as u32) << 24) + ((slice[index + 1] as u32) << 16)
        + ((slice[index + 2] as u32) << 8) + (slice[index + 3] as u32)
}
