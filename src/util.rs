use alloc::Allocator;
use core::cmp::min;

use interface::LeptonFlushResult;
use resizable_buffer::ResizableByteBuffer;

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

pub fn flush_resizable_buffer<T: Clone + Default, AllocT: Allocator<T>>(
    dest: &mut [T],
    dest_offset: &mut usize,
    src: &ResizableByteBuffer<T, AllocT>,
    src_offset: &mut usize,
) -> LeptonFlushResult {
    mem_copy(dest, dest_offset, src.slice(), src_offset);
    if *src_offset == src.len() {
        return LeptonFlushResult::Success;
    }
    LeptonFlushResult::NeedsMoreOutput
}
