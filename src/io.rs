use iostream::{OutputResult, OutputStream};

pub trait Write {
    fn write(&mut self, data: &[u8]) -> OutputResult<usize>;
    fn flush(&mut self) -> OutputResult<usize>;
    fn written_len(&self) -> usize;
}

pub struct BufferedOutputStream {
    pub ostream: OutputStream,
    buffer: Vec<u8>,
    total_out: usize,
}

impl BufferedOutputStream {
    pub fn new(ostream: OutputStream, buffer_len: usize) -> Self {
        BufferedOutputStream {
            ostream,
            buffer: Vec::with_capacity(buffer_len),
            total_out: 0,
        }
    }
}

impl Write for BufferedOutputStream {
    fn write(&mut self, data: &[u8]) -> OutputResult<usize> {
        Ok(if self.buffer.len() + data.len() >= self.buffer.capacity()
            || data.len() >= self.buffer.capacity() / 2
        {
            let len = self.ostream.write_all(&[&self.buffer, data])?;
            self.total_out += data.len();
            self.buffer.clear();
            len
        } else {
            self.buffer.extend(data);
            self.total_out += data.len();
            data.len()
        })
    }

    fn flush(&mut self) -> OutputResult<usize> {
        let len = self.ostream.write(&self.buffer)?;
        self.buffer.clear();
        Ok(len)
    }

    fn written_len(&self) -> usize {
        self.total_out
    }
}