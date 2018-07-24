use std::cmp::min;
use std::collections::VecDeque;
use std::sync::{Arc, Condvar, Mutex, MutexGuard};

use byte_converter::ByteConverter;

pub type InputResult<T> = Result<T, InputError>;
pub type OutputResult<T> = Result<T, OutputError>;

const CONVERTER_BUF_LEN: usize = 8;

#[derive(Debug)]
pub enum InputError {
    UnexpectedSigAbort(usize),
    UnexpectedEof(usize),
}

#[derive(Debug)]
pub enum OutputError {
    ReaderAborted,
    EofWritten,
}

pub fn iostream(preload_len: usize) -> (InputStream, OutputStream) {
    let iostream = Arc::new(IoStream::default());
    (
        InputStream::new(iostream.clone(), preload_len),
        OutputStream { ostream: iostream },
    )
}

pub struct InputStream {
    istream: Arc<IoStream>,
    preload_buffer: Buffer,
    retained_buffer: Vec<u8>,
    processed_len: usize,
}

impl InputStream {
    pub fn new(istream: Arc<IoStream>, preload_len: usize) -> Self {
        InputStream {
            istream,
            preload_buffer: Buffer::new(preload_len),
            retained_buffer: vec![],
            processed_len: 0,
        }
    }

    #[inline(always)]
    pub fn peek_byte(&mut self) -> InputResult<u8> {
        let mut byte = [0u8];
        self.peek(&mut byte)?;
        Ok(byte[0])
    }

    #[inline(always)]
    pub fn read_byte(&mut self, keep: bool) -> InputResult<u8> {
        let mut byte = [0u8];
        self.read(&mut byte, true, keep)?;
        Ok(byte[0])
    }

    #[inline(always)]
    pub fn read_u16<Converter: ByteConverter>(&mut self, keep: bool) -> InputResult<u16> {
        self.read_as_type(2, &Converter::slice_to_u16, keep)
    }

    #[inline(always)]
    pub fn read_u32<Converter: ByteConverter>(&mut self, keep: bool) -> InputResult<u32> {
        self.read_as_type(4, &Converter::slice_to_u32, keep)
    }

    pub fn peek(&mut self, buf: &mut [u8]) -> InputResult<usize> {
        if buf.len() == 0 {
            return Ok(0);
        }
        let preload_available = self.preload_buffer.data_len();
        let buf_len = buf.len();
        if buf_len <= preload_available {
            buf.copy_from_slice(self.preload_buffer.data_slice(Some(buf_len)));
        } else {
            let preload_buf_cap = self.preload_buffer.capacity();
            if buf_len > preload_buf_cap {
                self.istream.peek(&mut buf[preload_available..])?;
                self.istream
                    .consume(preload_buf_cap - preload_available)
                    .unwrap();
                buf[..preload_available].copy_from_slice(self.preload_buffer.data_slice(None));
                self.preload_buffer
                    .slice_mut()
                    .copy_from_slice(&buf[..preload_buf_cap]);
                self.preload_buffer.write_offset = preload_buf_cap;
                self.preload_buffer.read_offset = 0;
            } else {
                self.preload_buffer.move_data_to_front();
                let len = self.istream.read(
                    &mut self.preload_buffer.slice_mut()[preload_available..],
                    buf_len - preload_available,
                )?;
                self.preload_buffer.commit_write(len);
                buf.copy_from_slice(self.preload_buffer.data_slice(Some(buf_len)));
            }
        }
        Ok(buf_len)
    }

    pub fn read(&mut self, buf: &mut [u8], fill: bool, keep: bool) -> InputResult<usize> {
        if buf.len() == 0 {
            return Ok(0);
        }
        let preload_available = self.preload_buffer.data_len();
        let (old_preload_start, old_preload_end) = (
            self.preload_buffer.read_offset,
            self.preload_buffer.write_offset,
        );
        let mut read_len = 0;
        if preload_available > 0 {
            read_len = min(buf.len(), preload_available);
            buf[..read_len].copy_from_slice(self.preload_buffer.data_slice(Some(read_len)));
            self.preload_buffer.consume(read_len);
        }
        if preload_available == 0 || (fill && preload_available < buf.len()) {
            let buf_available = buf.len() - preload_available;
            match if buf_available >= self.preload_buffer.capacity() / 2 {
                self.istream.read(
                    &mut buf[preload_available..],
                    if fill { buf_available } else { 1 },
                )
            } else {
                match self.istream.read(
                    self.preload_buffer.slice_mut(),
                    if fill { buf_available } else { 1 },
                ) {
                    Ok(len) => {
                        self.preload_buffer.write_offset = len;
                        self.preload_buffer.read_offset = 0;
                        let size_to_fill = min(buf_available, len);
                        buf[preload_available..(preload_available + size_to_fill)]
                            .copy_from_slice(&self.preload_buffer.slice()[..size_to_fill]);
                        self.preload_buffer.consume(size_to_fill);
                        Ok(size_to_fill)
                    }
                    Err(e) => Err(e),
                }
            } {
                Ok(len) => read_len += len,
                Err(e) => {
                    self.preload_buffer.write_offset = old_preload_end;
                    self.preload_buffer.read_offset = old_preload_start;
                    return Err(e);
                }
            }
        }
        self.processed_len += read_len;
        if keep {
            self.retained_buffer.extend(&buf[..read_len]);
        }
        Ok(read_len)
    }

    pub fn consume(&mut self, len: usize, keep: bool) -> InputResult<usize> {
        if len == 0 {
            return Ok(0);
        }
        let preload_available = self.preload_buffer.data_len();
        let preloaded_len = min(preload_available, len);
        let (old_preload_start, old_preload_end) = (
            self.preload_buffer.read_offset,
            self.preload_buffer.write_offset,
        );
        let old_retained_len = self.retained_buffer.len();
        if keep {
            if preload_available > 0 {
                self.retained_buffer
                    .extend(self.preload_buffer.data_slice(
                        if preloaded_len == preload_available {
                            None
                        } else {
                            Some(preloaded_len)
                        },
                    ));
            }
        }
        self.preload_buffer.consume(preloaded_len);
        if len > preload_available {
            let size_to_consume = len - preload_available;
            if let Err(e) = if size_to_consume >= self.preload_buffer.capacity() / 2 {
                self.direct_consume(size_to_consume, keep)
            } else {
                match self.istream
                    .read(self.preload_buffer.slice_mut(), size_to_consume)
                {
                    Ok(len) => {
                        self.preload_buffer.write_offset = len;
                        self.preload_buffer.read_offset = 0;
                        if keep {
                            self.retained_buffer
                                .extend(&self.preload_buffer.slice()[..size_to_consume]);
                        }
                        self.preload_buffer.consume(size_to_consume);
                        Ok(size_to_consume)
                    }
                    Err(e) => Err(e),
                }
            } {
                if keep {
                    self.preload_buffer.write_offset = old_preload_end;
                    self.preload_buffer.read_offset = old_preload_start;
                    self.retained_buffer.truncate(old_retained_len);
                }
                return Err(e);
            }
        }
        self.processed_len += len;
        Ok(len)
    }

    #[inline(always)]
    pub fn processed_len(&self) -> usize {
        self.processed_len
    }

    #[inline(always)]
    pub fn reset_processed_len(&mut self) {
        self.processed_len = 0;
    }

    #[inline(always)]
    pub fn view_retained_data(&self) -> &[u8] {
        &self.retained_buffer
    }

    #[inline(always)]
    pub fn add_retained_data(&mut self, data: &[u8]) {
        self.retained_buffer.extend(data);
    }

    #[inline(always)]
    pub fn clear_retained_data(&mut self) {
        self.retained_buffer.clear();
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.istream.is_empty()
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.istream.len()
    }

    #[inline(always)]
    pub fn eof_written(&self) -> bool {
        self.istream.eof_written()
    }

    #[inline(always)]
    pub fn is_eof(&self) -> bool {
        self.preload_buffer.data_len() == 0 && self.istream.is_eof()
    }

    #[inline(always)]
    pub fn is_aborted(&self) -> bool {
        self.istream.is_aborted()
    }

    #[inline(always)]
    pub fn abort(&self) {
        self.istream.abort()
    }

    #[inline(always)]
    fn read_as_type<T>(
        &mut self,
        len: usize,
        converter: &Fn(&[u8]) -> T,
        keep: bool,
    ) -> InputResult<T> {
        assert!(len <= CONVERTER_BUF_LEN);
        let mut buf = [0u8; CONVERTER_BUF_LEN];
        self.read(&mut buf[..len], true, keep)?;
        Ok(converter(&buf[..len]))
    }

    fn direct_consume(&mut self, len: usize, keep: bool) -> InputResult<usize> {
        if keep {
            let old_retained_len = self.retained_buffer.len();
            self.retained_buffer.resize(old_retained_len + len, 0);
            let result = self.istream
                .read(&mut self.retained_buffer[old_retained_len..], len);
            match result {
                Ok(len) => self.processed_len += len,
                Err(_) => self.retained_buffer.truncate(old_retained_len),
            }
            result
        } else {
            let len = self.istream.consume(len)?;
            self.processed_len += len;
            Ok(len)
        }
    }
}

#[derive(Clone)]
pub struct OutputStream {
    ostream: Arc<IoStream>,
}

impl OutputStream {
    pub fn new(ostream: Arc<IoStream>) -> Self {
        OutputStream { ostream }
    }

    #[inline(always)]
    pub fn write(&self, buf: &[u8]) -> OutputResult<usize> {
        self.ostream.write(buf)
    }

    #[inline(always)]
    pub fn write_eof(&self) -> OutputResult<()> {
        self.ostream.write_eof()
    }

    #[inline(always)]
    pub fn is_empty(&self) -> bool {
        self.ostream.is_empty()
    }

    #[inline(always)]
    pub fn len(&self) -> usize {
        self.ostream.len()
    }

    #[inline(always)]
    pub fn eof_written(&self) -> bool {
        self.ostream.eof_written()
    }

    #[inline(always)]
    pub fn is_aborted(&self) -> bool {
        self.ostream.is_aborted()
    }
}

#[derive(Default)]
pub struct IoStream {
    data: Mutex<StreamBuffer>,
    cv: Condvar,
}

// Methods shared by InputStream and OutputStream
impl IoStream {
    pub fn is_empty(&self) -> bool {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.data.is_empty()
    }

    pub fn len(&self) -> usize {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.data.len()
    }

    pub fn eof_written(&self) -> bool {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.eof_written
    }

    pub fn is_aborted(&self) -> bool {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.aborted
    }
}

// Methods for InputStream
impl IoStream {
    pub fn peek(&self, buf: &mut [u8]) -> InputResult<usize> {
        let buf_len = buf.len();
        self.read_internal(buf, buf_len, false)
    }

    pub fn read(&self, buf: &mut [u8], min_len: usize) -> InputResult<usize> {
        self.read_internal(buf, min_len, true)
    }

    pub fn consume(&self, len: usize) -> InputResult<usize> {
        let mut stream_buf = self.lock_for_read()?;
        stream_buf = Self::wait_for_read(stream_buf, len, len, &self.cv)?;
        Ok(stream_buf.consume(len))
    }

    pub fn is_eof(&self) -> bool {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.is_eof()
    }

    pub fn abort(&self) {
        let mut stream_buf = self.data.lock().unwrap();
        stream_buf.aborted = true;
    }

    fn read_internal(&self, buf: &mut [u8], min_len: usize, consume: bool) -> InputResult<usize> {
        if min_len > buf.len() {
            panic!(
                "required minimum length {} is greater than buffer size {}",
                min_len,
                buf.len()
            );
        }
        let mut stream_buf = self.lock_for_read()?;
        stream_buf = Self::wait_for_read(stream_buf, min_len, buf.len(), &self.cv)?;
        let read_len = stream_buf.read(buf);
        if consume {
            stream_buf.consume(read_len);
        }
        Ok(read_len)
    }

    fn lock_for_read(&self) -> InputResult<MutexGuard<StreamBuffer>> {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.validate_for_read(false)?;
        Ok(stream_buf)
    }

    fn wait_for_read<'a>(
        mut stream_buf: MutexGuard<'a, StreamBuffer>,
        min_len: usize,
        target_len: usize,
        cv: &Condvar,
    ) -> InputResult<MutexGuard<'a, StreamBuffer>> {
        if stream_buf.data.len() < target_len {
            loop {
                stream_buf.validate_for_read(true)?;
                stream_buf.target_len = target_len;
                stream_buf = cv.wait(stream_buf).unwrap();
                stream_buf.target_len = 0;
                if stream_buf.data.len() >= min_len {
                    break;
                }
            }
        }
        Ok(stream_buf)
    }
}

// Methods for OutputStream
impl IoStream {
    pub fn write(&self, buf: &[u8]) -> OutputResult<usize> {
        let mut stream_buf = self.lock_for_write()?;
        stream_buf.data.extend(buf.iter());
        if stream_buf.data.len() >= stream_buf.target_len {
            self.cv.notify_one();
        }
        Ok(buf.len())
    }

    pub fn write_eof(&self) -> OutputResult<()> {
        let mut stream_buf = self.lock_for_write()?;
        stream_buf.eof_written = true;
        self.cv.notify_one();
        Ok(())
    }

    fn lock_for_write(&self) -> OutputResult<MutexGuard<StreamBuffer>> {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.validate_for_write()?;
        Ok(stream_buf)
    }
}

#[derive(Default)]
struct StreamBuffer {
    data: VecDeque<u8>,
    target_len: usize,
    eof_written: bool,
    aborted: bool,
}

impl StreamBuffer {
    fn read(&self, buf: &mut [u8]) -> usize {
        let read_len = min(buf.len(), self.data.len());
        let data_slices = self.data.as_slices();
        let first_slice_len = data_slices.0.len();
        if read_len <= first_slice_len {
            buf[..read_len].clone_from_slice(&data_slices.0[..read_len]);
        } else {
            buf[..first_slice_len].clone_from_slice(data_slices.0);
            buf[first_slice_len..read_len]
                .clone_from_slice(&data_slices.1[..(read_len - first_slice_len)]);
        }
        read_len
    }

    fn consume(&mut self, len: usize) -> usize {
        if len < self.data.len() {
            for _ in 0..len {
                self.data.pop_front();
            }
            len
        } else {
            let ret = self.data.len();
            self.data.clear();
            ret
        }
    }

    fn validate_for_read(&self, require_active: bool) -> InputResult<()> {
        use self::InputError::*;
        if self.data.is_empty() || require_active {
            if self.aborted {
                return Err(UnexpectedSigAbort(self.data.len()));
            } else if self.eof_written {
                return Err(UnexpectedEof(self.data.len()));
            }
        }
        Ok(())
    }

    fn validate_for_write(&self) -> OutputResult<()> {
        use self::OutputError::*;
        if self.aborted {
            Err(ReaderAborted)
        } else if self.eof_written {
            Err(EofWritten)
        } else {
            Ok(())
        }
    }

    fn is_eof(&self) -> bool {
        self.data.is_empty() && self.eof_written
    }
}

struct Buffer {
    data: Vec<u8>,
    write_offset: usize,
    read_offset: usize,
}

impl Buffer {
    fn new(size: usize) -> Self {
        Buffer {
            data: vec![0u8; size],
            write_offset: 0,
            read_offset: 0,
        }
    }

    fn data_len(&self) -> usize {
        self.write_offset - self.read_offset
    }

    fn capacity(&self) -> usize {
        self.data.len()
    }

    // fn clear(&mut self) {
    //     self.write_offset = 0;
    //     self.read_offset = 0;
    // }

    fn data_slice(&self, len: Option<usize>) -> &[u8] {
        match len {
            Some(len) => {
                let data_available = self.write_offset - self.read_offset;
                if len > data_available {
                    panic!(
                        "queried length {} greater than data available {}",
                        len, data_available
                    );
                }
                &self.data[self.read_offset..(self.read_offset + len)]
            }
            None => &self.data[self.read_offset..self.write_offset],
        }
    }

    fn slice(&self) -> &[u8] {
        &self.data
    }

    fn slice_mut(&mut self) -> &mut [u8] {
        &mut self.data
    }

    fn commit_write(&mut self, len: usize) {
        let space_available = self.data.len() - self.write_offset;
        if len > space_available {
            panic!(
                "commit write length {} greater than space available {}",
                len, space_available
            );
        }
        self.write_offset += len;
    }

    fn consume(&mut self, len: usize) {
        let data_available = self.write_offset - self.read_offset;
        if len > data_available {
            panic!(
                "consume length {} greater than data available {}",
                len, data_available
            );
        }
        if len == data_available {
            self.write_offset = 0;
            self.read_offset = 0;
        } else {
            self.read_offset += len;
        }
    }

    fn move_data_to_front(&mut self) {
        let data_len = self.data_len();
        for i in 0..data_len {
            self.data[0] = self.data[self.read_offset + i];
        }
        self.write_offset = data_len;
        self.read_offset = 0;
    }
}
