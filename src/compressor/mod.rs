mod brotli_encoder;
mod compressor;

pub use self::compressor::LeptonCompressor;

use alloc::Allocator;
use interface::LeptonFlushResult;
use resizable_buffer::ResizableByteBuffer;
use util::mem_copy;

fn flush_resizable_buffer<T: Clone + Default, AllocT: Allocator<T>>(
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
