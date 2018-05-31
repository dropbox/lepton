extern crate alloc_no_stdlib as alloc;
extern crate brotli;
extern crate core;

pub mod compressor;
pub mod decompressor;
pub mod resizable_buffer;

mod interface;

pub use interface::{Compressor, Decompressor, ErrMsg, LeptonFlushResult, LeptonOperationResult};