use super::error::JpegResult;
use super::immediate::ImmediateWorker;
use super::{RowData, Worker};
use std::sync::mpsc::{self, Sender};
use std::thread;

enum WorkerMsg {
    Start(RowData),
    AppendRow((usize, Vec<i16>)),
    GetResult((usize, Sender<Vec<u8>>)),
}

pub struct ThreadedWorker {
    sender: Sender<WorkerMsg>,
}

impl Worker for ThreadedWorker {
    fn new() -> JpegResult<Self> {
        let thread_builder = thread::Builder::new().name("worker thread".to_owned());
        let (tx, rx) = mpsc::channel();
        thread_builder
            .spawn(move || {
                let mut worker = ImmediateWorker::new_immediate();
                while let Ok(message) = rx.recv() {
                    match message {
                        WorkerMsg::Start(data) => {
                            worker.start_immediate(data);
                        }
                        WorkerMsg::AppendRow(row) => {
                            worker.append_row_immediate(row);
                        }
                        WorkerMsg::GetResult((index, chan)) => {
                            let _ = chan.send(worker.get_result_immediate(index));
                        }
                    }
                }
            })?;
        Ok(ThreadedWorker { sender: tx })
    }

    fn start(&mut self, row_data: RowData) -> JpegResult<()> {
        Ok(self.sender.send(WorkerMsg::Start(row_data))?)
    }

    fn append_row(&mut self, row: (usize, Vec<i16>)) -> JpegResult<()> {
        Ok(self.sender.send(WorkerMsg::AppendRow(row))?)
    }

    fn get_result(&mut self, index: usize) -> JpegResult<Vec<u8>> {
        let (tx, rx) = mpsc::channel();
        self.sender.send(WorkerMsg::GetResult((index, tx)))?;
        Ok(rx.recv()?)
    }
}
