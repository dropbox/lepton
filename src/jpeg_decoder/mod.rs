mod decoder;
mod error;
mod huffman;
mod jpeg;
mod marker;
mod parser;
mod stream_decoder;

pub use self::decoder::JpegDecoder;
pub use self::error::{JpegError, JpegResult, UnsupportedFeature};
pub use self::jpeg::*;
pub use self::stream_decoder::*;

pub const MAX_COMPONENTS: usize = 4;
