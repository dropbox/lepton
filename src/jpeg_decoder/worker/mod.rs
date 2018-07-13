mod immediate;
mod threaded;

pub use self::threaded::ThreadedWorker;

use super::decoder;
use super::error;
use super::error::JpegResult;
use super::idct;
use super::parser::{self, Component};
use std::sync::Arc;

pub struct RowData {
    pub index: usize,
    pub component: Component,
    pub quantization_table: Arc<[u16; 64]>,
}

pub trait Worker: Sized {
    fn new() -> JpegResult<Self>;
    fn start(&mut self, row_data: RowData) -> JpegResult<()>;
    fn append_row(&mut self, row: (usize, Vec<i16>)) -> JpegResult<()>;
    fn get_result(&mut self, index: usize) -> JpegResult<Vec<u8>>;
}
