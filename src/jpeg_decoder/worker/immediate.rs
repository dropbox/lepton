use super::decoder::MAX_COMPONENTS;
use super::error::JpegResult;
use super::idct::dequantize_and_idct_block;
use super::parser::Component;
use super::{RowData, Worker};
use std::mem;
use std::sync::Arc;

pub struct ImmediateWorker {
    offsets: [usize; MAX_COMPONENTS],
    results: Vec<Vec<u8>>,
    components: Vec<Option<Component>>,
    quantization_tables: Vec<Option<Arc<[u16; 64]>>>,
}

impl ImmediateWorker {
    pub fn new_immediate() -> ImmediateWorker {
        ImmediateWorker {
            offsets: [0; MAX_COMPONENTS],
            results: vec![Vec::new(); MAX_COMPONENTS],
            components: vec![None; MAX_COMPONENTS],
            quantization_tables: vec![None; MAX_COMPONENTS],
        }
    }
    pub fn start_immediate(&mut self, data: RowData) {
        assert!(self.results[data.index].is_empty());

        self.offsets[data.index] = 0;
        self.results[data.index].resize(
            data.component.size_in_block.width as usize * data.component.size_in_block.height as usize
                * 64,
            0u8,
        );
        self.components[data.index] = Some(data.component);
        self.quantization_tables[data.index] = Some(data.quantization_table);
    }
    pub fn append_row_immediate(&mut self, (index, data): (usize, Vec<i16>)) {
        // Convert coefficients from a MCU row to samples.

        let component = self.components[index].as_ref().unwrap();
        let quantization_table = self.quantization_tables[index].as_ref().unwrap();
        let block_count =
            component.size_in_block.width as usize * component.vertical_sampling_factor as usize;
        let line_stride = component.size_in_block.width as usize * 8;

        assert_eq!(data.len(), block_count * 64);

        for i in 0..block_count {
            let x = (i % component.size_in_block.width as usize) * 8;
            let y = (i / component.size_in_block.width as usize) * 8;
            dequantize_and_idct_block(
                &data[i * 64..(i + 1) * 64],
                quantization_table,
                line_stride,
                &mut self.results[index][self.offsets[index] + y * line_stride + x..],
            );
        }

        self.offsets[index] += data.len();
    }
    pub fn get_result_immediate(&mut self, index: usize) -> Vec<u8> {
        mem::replace(&mut self.results[index], Vec::new())
    }
}

impl Worker for ImmediateWorker {
    fn new() -> JpegResult<Self> {
        Ok(ImmediateWorker::new_immediate())
    }
    fn start(&mut self, data: RowData) -> JpegResult<()> {
        self.start_immediate(data);
        Ok(())
    }
    fn append_row(&mut self, row: (usize, Vec<i16>)) -> JpegResult<()> {
        self.append_row_immediate(row);
        Ok(())
    }
    fn get_result(&mut self, index: usize) -> JpegResult<Vec<u8>> {
        Ok(self.get_result_immediate(index))
    }
}
