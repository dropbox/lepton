extern crate alloc_no_stdlib as alloc;
extern crate brotli;
extern crate core;
extern crate lepton;

use std::error::Error;
use std::fs::File;
use std::io::{self, Read, Result, Write};
use std::path::Path;

use lepton::{Compressor, Decompressor, ErrMsg, LeptonDecompressor, LeptonFlushResult,
             LeptonOperationResult, LeptonPermissiveCompressor};

mod integration_test;

#[derive(Copy, Clone, Debug)]
pub struct LeptonErrMsg(pub ErrMsg);

impl core::fmt::Display for LeptonErrMsg {
    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::result::Result<(), core::fmt::Error> {
        <ErrMsg as core::fmt::Debug>::fmt(&self.0, f)
    }
}

impl Error for LeptonErrMsg {
    fn description(&self) -> &str {
        "Lepton error"
    }
    fn cause(&self) -> Option<&Error> {
        None
    }
}

fn read_to_buffer<Reader: Read>(
    r: &mut Reader,
    buffer: &mut [u8],
    content_end: &mut usize,
    size_checker: &Fn(usize) -> Result<usize>,
) -> Result<usize> {
    loop {
        match r.read(&mut buffer[*content_end..]) {
            Ok(size) => {
                *content_end += size;
                return size_checker(size);
            }
            Err(e) => {
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
        }
    }
}

fn write_from_buffer<Writer: Write>(
    w: &mut Writer,
    buffer: &mut [u8],
    content_end: usize,
) -> Result<()> {
    let mut output_written = 0usize;
    while output_written < content_end {
        match w.write(&buffer[output_written..content_end]) {
            Ok(size) => {
                output_written += size;
            }
            Err(e) => {
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
        }
    }
    Ok(())
}

fn renew_buffer(buffer: &mut [u8], content_used: &mut usize, content_end: &mut usize) {
    if *content_used < *content_end {
        let content_left = *content_end - *content_used;
        let tmp = buffer[*content_used..*content_end].to_vec();
        buffer[..content_left].clone_from_slice(&tmp[..]);
        *content_end = content_left;
    } else {
        *content_end = 0;
    }
    *content_used = 0;
}

fn compress<Reader: Read, Writer: Write>(
    r: &mut Reader,
    w: &mut Writer,
    buffer_size: usize,
) -> Result<()> {
    let mut compressor = LeptonPermissiveCompressor::new();
    let ret = compress_internal(r, w, buffer_size, &mut compressor);
    // compressor.free();
    ret
}

fn compress_internal<Reader: Read, Writer: Write>(
    r: &mut Reader,
    w: &mut Writer,
    buffer_size: usize,
    compressor: &mut Compressor,
) -> Result<()> {
    let mut input_buffer = vec![0u8; buffer_size];
    let mut input_offset = 0usize;
    let mut input_end = 0usize;
    let mut output_buffer = vec![0u8; buffer_size];
    let mut output_offset = 0usize;
    let size_checker = |size: usize| Ok(size);
    let mut done = false;
    while !done {
        // FIXME: Maker sure input is exhausted before exiting
        match read_to_buffer(r, &mut input_buffer[..], &mut input_end, &size_checker) {
            Ok(size) => {
                if size == 0 {
                    done = true;
                }
            }
            Err(e) => return Err(e),
        }
        match compressor.encode(
            input_buffer[..].split_at(input_end).0,
            &mut input_offset,
            &mut output_buffer[..],
            &mut output_offset,
        ) {
            LeptonOperationResult::Failure(m) => {
                return Err(io::Error::new(io::ErrorKind::Other, LeptonErrMsg(m)))
            }
            _ => (),
        }
        // if done && 
        renew_buffer(&mut input_buffer, &mut input_offset, &mut input_end);
        if output_offset > 0 {
            match write_from_buffer(w, &mut output_buffer[..], output_offset) {
                Ok(()) => output_offset = 0,
                Err(e) => return Err(e),
            }
        }
    }
    done = false;
    while !done {
        match compressor.flush(&mut output_buffer, &mut output_offset) {
            LeptonFlushResult::Success => done = true,
            LeptonFlushResult::Failure(m) => {
                return Err(io::Error::new(io::ErrorKind::Other, LeptonErrMsg(m)))
            }
            LeptonFlushResult::NeedsMoreOutput => (),
        }
        match write_from_buffer(w, &mut output_buffer[..], output_offset) {
            Ok(()) => output_offset = 0,
            Err(e) => return Err(e),
        }
    }
    Ok(())
}

fn decompress<Reader: Read, Writer: Write>(
    r: &mut Reader,
    w: &mut Writer,
    buffer_size: usize,
) -> Result<()> {
    let mut decompressor = LeptonDecompressor::new();
    let ret = decompress_internal(r, w, buffer_size, &mut decompressor);
    // decompressor.free()
    ret
}

fn decompress_internal<Reader: Read, Writer: Write>(
    r: &mut Reader,
    w: &mut Writer,
    buffer_size: usize,
    decompressor: &mut Decompressor,
) -> Result<()> {
    let mut input_buffer = vec![0u8; buffer_size];
    let mut input_offset = 0usize;
    let mut input_end = 0usize;
    let mut output_buffer = vec![0u8; buffer_size];
    let mut output_offset = 0usize;
    let size_checker = |size: usize| {
        if size == 0 {
            Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                "Invalid Lepton file.",
            ))
        } else {
            Ok(size)
        }
    };
    match read_to_buffer(r, &mut input_buffer[..], &mut input_end, &size_checker) {
        Ok(_) => (),
        Err(e) => return Err(e),
    }
    loop {
        match decompressor.decode(
            input_buffer[..].split_at(input_end).0,
            &mut input_offset,
            &mut output_buffer[..],
            &mut output_offset,
        ) {
            LeptonOperationResult::Success => break,
            LeptonOperationResult::Failure(m) => {
                return Err(io::Error::new(io::ErrorKind::InvalidInput, LeptonErrMsg(m)));
            }
            LeptonOperationResult::NeedsMoreOutput => {
                match write_from_buffer(w, &mut output_buffer[..], output_offset) {
                    Ok(()) => output_offset = 0,
                    Err(e) => return Err(e),
                }
            }
            LeptonOperationResult::NeedsMoreInput => {
                renew_buffer(&mut input_buffer, &mut input_offset, &mut input_end);
                match read_to_buffer(r, &mut input_buffer[..], &mut input_end, &size_checker) {
                    Ok(_) => (),
                    Err(e) => return Err(e),
                }
            }
        }
    }
    write_from_buffer(w, &mut output_buffer[..], output_offset)
}

fn main() {
    let mut do_compress = true;
    let mut filenames = [std::string::String::new(), std::string::String::new()];
    let mut buffer_size: usize = 65_536;
    for argument in std::env::args().skip(1) {
        if argument == "-d" {
            do_compress = false;
            continue;
        }
        if argument.starts_with("-buffer_size") {
            buffer_size = argument
                .trim_left_matches("-buffer_size")
                .trim_matches('=')
                .parse::<usize>()
                .unwrap();
            continue;
        }
        if filenames[0] == "" {
            filenames[0] = argument.clone();
            continue;
        }
        if filenames[1] == "" {
            filenames[1] = argument.clone();
            continue;
        }
        panic!("Unknown Argument {:}", argument);
    }
    if filenames[0] != "" {
        let mut input = match File::open(&Path::new(&filenames[0])) {
            Err(e) => panic!("couldn't open {:}\n{:}", filenames[0], e),
            Ok(file) => file,
        };
        if filenames[1] != "" {
            let mut output = match File::create(&Path::new(&filenames[1])) {
                Err(e) => panic!("couldn't open file for writing: {:}\n{:}", filenames[1], e),
                Ok(file) => file,
            };
            if do_compress {
                match compress(&mut input, &mut output, buffer_size) {
                    Ok(_) => return,
                    Err(e) => panic!("Error {:?}", e),
                }
            } else {
                match decompress(&mut input, &mut output, buffer_size) {
                    Ok(_) => return,
                    Err(e) => panic!("Error {:?}", e),
                }
            }
        }
    }
}
