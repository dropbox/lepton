use std::cmp::min;
use std::collections::VecDeque;
use std::sync::{Arc, Condvar, Mutex, MutexGuard};

use byte_converter::ByteConverter;

pub type InputResult<T> = Result<T, InputError>;
pub type OutputResult<T> = Result<T, OutputError>;

const CONVERTER_BUF_LEN: usize = 8;

#[derive(Debug, PartialEq)]
pub enum InputError {
    UnexpectedSigAbort,
    UnexpectedEof,
}

#[derive(Debug)]
pub enum OutputError {
    ReaderAborted,
}

pub fn iostream(preload_len: usize) -> (InputStream, OutputStream) {
    let iostream = Arc::new(IoStream::default());
    (
        InputStream::new(iostream.clone(), preload_len),
        OutputStream { ostream: iostream },
    )
}

// TODO: Maybe make InputStream a trait to allow different impls, e.g. dummy impl
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

    pub fn preload(data: Vec<u8>) -> Self {
        let istream = IoStream::default();
        istream.write_eof().unwrap();
        InputStream {
            istream: Arc::new(istream),
            preload_buffer: Buffer::with_content(data),
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

    /// Reads the given number of bytes and returns the number of bytes read.
    ///
    /// # Arguments
    ///
    /// * `buf` - The destination buffer for storing the bytes read.
    /// * `fill` - if true, will block for additional bytes beyond the preloaded data if needed.
    /// * `keep` - if true, bytes read are retained and can be accessed by `view_retained_data`.
    pub fn read(&mut self, buf: &mut [u8], fill: bool, keep: bool) -> InputResult<usize> {
        let len = buf.len();
        self.read_internal(Some(buf), len, fill, keep)
    }

    /// Reads the given number of bytes and discards them.
    /// Returns the number of bytes actually read.
    pub fn consume(&mut self, len: usize, keep: bool) -> InputResult<usize> {
        if keep {
            let mut dummy = vec![0u8; len];
            self.read_internal(Some(&mut dummy), len, true, keep)
        } else {
            self.read_internal(None, len, true, keep)
        }
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

    /// NOTE: If `buf` is none, will read bytes and discard them.
    fn read_internal(
        &mut self,
        mut buf: Option<&mut [u8]>,
        len: usize,
        fill: bool,
        keep: bool,
    ) -> InputResult<usize> {
        match buf {
            Some(ref b) => assert_eq!(
                b.len(),
                len,
                "buffer, if given, must match the given length"
            ),
            None => assert!(!keep, "buffer is necessary unless keep is set to false"),
        };
        if len == 0 {
            return Ok(0);
        }
        let mut read_len = 0; // bytes read so far.

        // Read from preloaded data if possible.
        let mut preload_used: usize = 0;
        let preload_available = self.preload_buffer.data_len();
        if preload_available > 0 {
            read_len = min(len, preload_available);
            if let Some(ref mut buf) = buf {
                buf[..read_len].copy_from_slice(self.preload_buffer.data_slice(Some(read_len)));
            }
            preload_used = read_len;
        }

        // Read additional data if necessary.
        assert!(read_len <= len);
        let bytes_to_read = if fill { len - read_len } else { 0 };
        assert!(bytes_to_read <= len - read_len);
        if bytes_to_read >= self.preload_buffer.capacity() / 2 {
            // Skip preload_buffer and directly populate into the buffer.
            read_len += if let Some(ref mut buf) = buf {
                self.istream
                    .read(&mut buf[preload_available..], bytes_to_read)?
            } else {
                self.istream.consume(len)?
            };
        } else if bytes_to_read > 0 || read_len == 0 {
            // Populate the preload_buffer and read from it.
            // Q: if this encounters an error, does preload_buffer get corrupted?
            let _len = self.istream
                .read(self.preload_buffer.slice_mut(), bytes_to_read)?;
            self.preload_buffer.write_offset = _len;
            self.preload_buffer.read_offset = 0;
            let size_to_fill = min(len - read_len, _len);
            if let Some(ref mut buf) = buf {
                buf[read_len..(read_len + size_to_fill)]
                    .copy_from_slice(&self.preload_buffer.slice()[..size_to_fill]);
            }
            preload_used = size_to_fill;
            read_len += size_to_fill;
        }

        // Mark (some) preloaded data as having been consumed.
        if preload_used > 0 {
            self.preload_buffer.consume(preload_used);
        }

        // Retain the bytes read if requested.
        if keep {
            self.retained_buffer.extend(&buf.unwrap()[..read_len]);
        }

        self.processed_len += read_len;
        Ok(read_len)
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
    pub fn write_all(&self, bufs: &[&[u8]]) -> OutputResult<usize> {
        self.ostream.write_all(bufs)
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
        stream_buf = Self::wait_for_read(stream_buf, len, &self.cv)?;
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
        if stream_buf.data.len() < min_len {
            stream_buf = Self::wait_for_read(stream_buf, min_len, &self.cv)?;
        }
        let read_len = stream_buf.read(buf);
        if consume {
            stream_buf.consume(read_len);
        }
        Ok(read_len)
    }

    fn lock_for_read(&self) -> InputResult<MutexGuard<StreamBuffer>> {
        let stream_buf = self.data.lock().unwrap();
        stream_buf.validate_for_read(1)?;
        Ok(stream_buf)
    }

    fn wait_for_read<'a>(
        mut stream_buf: MutexGuard<'a, StreamBuffer>,
        min_len: usize,
        cv: &Condvar,
    ) -> InputResult<MutexGuard<'a, StreamBuffer>> {
        while stream_buf.data.len() < min_len {
            stream_buf.validate_for_read(min_len)?;
            stream_buf.target_len = min_len;
            stream_buf = cv.wait(stream_buf).unwrap();
            stream_buf.target_len = 0;
        }
        Ok(stream_buf)
    }
}

// Methods for OutputStream
impl IoStream {
    pub fn write(&self, buf: &[u8]) -> OutputResult<usize> {
        self.write_all(&[buf])
    }

    pub fn write_all(&self, bufs: &[&[u8]]) -> OutputResult<usize> {
        let mut stream_buf = self.lock_for_write()?;
        let mut total_len = 0;
        for buf in bufs.iter() {
            stream_buf.data.extend(buf.iter());
            total_len += buf.len()
        }
        if stream_buf.data.len() >= stream_buf.target_len {
            self.cv.notify_one();
        }
        Ok(total_len)
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

    fn is_eof(&self) -> bool {
        self.data.is_empty() && self.eof_written
    }

    fn validate_for_read(&self, min_len: usize) -> InputResult<()> {
        use self::InputError::*;
        if self.data.len() < min_len {
            if self.aborted {
                return Err(UnexpectedSigAbort);
            } else if self.eof_written {
                return Err(UnexpectedEof);
            }
        }
        Ok(())
    }

    fn validate_for_write(&self) -> OutputResult<()> {
        if self.eof_written {
            panic!("attempt to write after writing EOF");
        } else if self.aborted {
            Err(OutputError::ReaderAborted)
        } else {
            Ok(())
        }
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

    fn with_content(data: Vec<u8>) -> Self {
        Buffer {
            write_offset: data.len(),
            read_offset: 0,
            data,
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
