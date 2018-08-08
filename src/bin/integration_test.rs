#![cfg(test)]
use core::cmp;

use super::{Read, Result, Write};

pub struct UnlimitedBuffer {
    pub data: Vec<u8>,
    pub read_offset: usize,
}

impl UnlimitedBuffer {
    pub fn new(data: Option<&[u8]>) -> Self {
        let mut ret = UnlimitedBuffer {
            data: Vec::<u8>::new(),
            read_offset: 0,
        };
        if let Some(data) = data {
            ret.data.extend(data);
        }
        return ret;
    }
}

impl Read for UnlimitedBuffer {
    fn read(self: &mut Self, buf: &mut [u8]) -> Result<usize> {
        let bytes_to_read = cmp::min(buf.len(), self.data.len() - self.read_offset);
        if bytes_to_read > 0 {
            buf[0..bytes_to_read]
                .clone_from_slice(&self.data[self.read_offset..self.read_offset + bytes_to_read]);
        }
        self.read_offset += bytes_to_read;
        return Ok(bytes_to_read);
    }
}

impl Write for UnlimitedBuffer {
    fn write(self: &mut Self, buf: &[u8]) -> Result<usize> {
        self.data.extend(buf);
        return Ok(buf.len());
    }
    fn flush(self: &mut Self) -> Result<()> {
        return Ok(());
    }
}

fn round_trip_permissive(buffer_size: usize, data: &[u8]) {
    let mut input = UnlimitedBuffer::new(Some(data));
    let mut compressed = UnlimitedBuffer::new(None);
    let mut decompressed = UnlimitedBuffer::new(None);
    super::compress(&mut input, &mut compressed, buffer_size, true).unwrap();
    super::decompress(&mut compressed, &mut decompressed, buffer_size).unwrap();
    assert_eq!(decompressed.data, input.data);
}

#[test]
fn round_trip_empty() {
    round_trip_permissive(65536, &[]);
}

#[test]
fn round_trip_ones() {
    round_trip_permissive(65536, &[1; 256]);
}

#[test]
fn round_trip_empty_small_buffer() {
    round_trip_permissive(1, &[]);
}

#[test]
fn round_trip_ones_small_buffer() {
    round_trip_permissive(1, &[1; 256]);
}

fn round_trip_alice(buffer_size: usize) {
    round_trip_permissive(buffer_size, include_bytes!("../../testdata/alice29"));
}

#[test]
fn round_trip_alice_normal_buffer() {
    round_trip_alice(65536);
}

#[test]
fn round_trip_alice_small_buffer() {
    round_trip_alice(1);
}
