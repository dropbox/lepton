#![cfg(test)]
extern crate std;

use std::sync::{Arc, Condvar, Mutex};
use std::thread;
use super::byte_converter::{BigEndian, ByteConverter};
use super::iostream::{InputStream, iostream};

#[test]
fn ostream_test() {
    let (_, ostream) = iostream(30);
    assert!(ostream.is_empty());
    assert!(!ostream.eof_written());
    assert_eq!(ostream.len(), 0);

    // write some data
    let data1 = [0u8, 1u8, 2u8, 3u8];
    let result = ostream.write(&data1);
    assert!(result.is_ok() && result.unwrap() == data1.len());
    assert!(!ostream.is_empty());
    assert!(!ostream.eof_written());
    assert_eq!(ostream.len(), data1.len());

    // write data again
    let data2 = [4u8, 5u8, 6u8, 7u8, 8u8];
    let result = ostream.write(&data2);
    assert!(result.is_ok() && result.unwrap() == data2.len());
    assert!(!ostream.is_empty());
    assert!(!ostream.eof_written());
    assert_eq!(ostream.len(), data1.len() + data2.len());

    // write data again and mark EOF.
    let result = ostream.write_eof();
    assert!(result.is_ok());
    assert!(ostream.eof_written());
}

#[test]
fn iostream_peek_and_read_test() {
    let data = &[1u8, 2u8, 3u8, 4u8, 5u8];
    let (mut istream, ostream) = iostream(data.len());
    let _result = ostream.write(data);

    // test peek_byte() and peek()
    assert_eq!(istream.peek_byte().unwrap(), data[0]);
    let mut buffer = [0u8; 3];
    assert_eq!(istream.peek(&mut buffer).unwrap(), buffer.len());
    assert_eq!(buffer, data[0..buffer.len()]);

    // test read_byte() and read()
    for i in 0..data.len()-buffer.len() {
        assert_eq!(istream.read_byte(false).unwrap(), data[i]);
    }
    assert_eq!(istream.read(&mut buffer, true, true).unwrap(), buffer.len());
    assert_eq!(buffer, data[data.len()-buffer.len()..data.len()]);
    assert_eq!(istream.view_retained_data(), buffer);
}

#[test]
fn iostream_read16_and_read32_and_retrained_data_test() {
    let data = &[1u8, 2u8, 3u8, 4u8, 5u8, 6u8, 7u8, 8u8, 9u8, 10u8];
    let (mut istream, ostream) = iostream(data.len());
    let _result = ostream.write(data);
    assert_eq!(istream.read_u16::<BigEndian>(true).unwrap(), BigEndian::slice_to_u16(&data[0..2]));
    assert_eq!(istream.read_u16::<BigEndian>(false).unwrap(), BigEndian::slice_to_u16(&data[2..4]));
    assert_eq!(istream.read_u32::<BigEndian>(true).unwrap(), BigEndian::slice_to_u32(&data[4..8]));
    assert_eq!(istream.read_u16::<BigEndian>(false).unwrap(), BigEndian::slice_to_u16(&data[8..10]));

    let retained = istream.view_retained_data();
    assert_eq!(retained.len(), 6);
    assert_eq!(retained[0..2], data[0..2]);
    assert_eq!(retained[2..6], data[4..8]);
}

#[test]
fn istream_preload_test() {
    for i in 0..2  {
        let data: Vec<u8> = vec![0, 1, 2, 3, 4, 5, 6, 7];
        let mut istream = InputStream::preload(data.to_vec());
        let mut buffer = [0u8; 4];
        assert_eq!(istream.peek_byte().unwrap(), data[0]);
        assert_eq!(istream.consume(1, false).unwrap(), 1); // 0
        assert_eq!(istream.read_byte(false).unwrap(), data[1]); // 1
        assert_eq!(istream.read_byte(false).unwrap(), data[2]); // 2
        assert_eq!(istream.consume(2, false).unwrap(), 2); // 3, 4
        assert_eq!(istream.read_byte(false).unwrap(), data[5]); // 5

        match i {
            0 => {  // Test read(...) with some preloaded data left but not enough.
                assert_eq!(istream.read(&mut buffer, false, false).unwrap(), 2); // 6, 7
            },
            1 => {  // Test consume(...) with some preloaded data left but not enough.
                assert!(istream.consume(4, false).is_err()); // 6, 7
            },
            _ => {},
        }
    }
}

#[test]
fn blocking_read_test() {
    let (mut istream, ostream) = iostream(1024);
    let pair = Arc::new((Mutex::<bool>::new(false), Condvar::new()));
    let pair_child = pair.clone();

    const BUFFER_SIZE: usize = 10;
    let child = thread::spawn(move || {
        // Notify the main thread that this thread has started.
        let mut buffer = [0u8; BUFFER_SIZE];
        let &(ref lock, ref cv) = &*pair_child;
        {
            let mut started = lock.lock().unwrap();
            *started = true;
            cv.notify_one();
        }

        // Read from istream and check that we get 0, 1, 2, ...
        assert_eq!(istream.read(&mut buffer, true, true).unwrap(), BUFFER_SIZE);
        for i in 0..BUFFER_SIZE {
            assert_eq!(buffer[i], i as u8);
        }
    });

    // Wait for the thread to start up.
    let &(ref lock, ref cv) = &*pair;
    let mut started = lock.lock().unwrap();
    while !*started {
        started = cv.wait(started).unwrap();
    }

    // Write to ostream 0, 1, 2, ...
    let mut buffer = [0u8; BUFFER_SIZE];
    for i in 0..buffer.len() {
        buffer[i] = i as u8;
    }
    assert_eq!(ostream.write(&buffer).unwrap(), buffer.len());
    ostream.write_eof().unwrap();
    child.join().unwrap();
}

#[test]
fn intertwined_read_test() {
    let (mut istream1, ostream1) = iostream(1024);
    let (mut istream2, ostream2) = iostream(1024);
    assert_eq!(ostream1.write(&[0u8]).unwrap(), 1);

    const NUM_ITERATIONS: usize = 10;
    let thread1 = thread::spawn(move || {  // read from istream1 and write to ostream2
        let mut data_read_by_thread1: Vec<u8> = Vec::new();
        for _ in 0..NUM_ITERATIONS {
            let b_in = istream1.read_byte(false).unwrap();
            data_read_by_thread1.push(b_in);
            let b_out = [b_in + 1u8];
            assert_eq!(ostream2.write(&b_out).unwrap(), 1);
        }
        data_read_by_thread1
    });
    let thread2 = thread::spawn(move || {  // read from istream2 and write to ostream1
        let mut data_read_by_thread2: Vec<u8> = Vec::new();
        for _ in 0..NUM_ITERATIONS {
            let b_in = istream2.read_byte(false).unwrap();
            data_read_by_thread2.push(b_in);
            let b_out = [b_in + 2u8];
            assert_eq!(ostream1.write(&b_out).unwrap(), 1);
        }
        data_read_by_thread2
    });

    let data_read_by_thread1 = thread1.join().unwrap();
    let data_read_by_thread2 = thread2.join().unwrap();
    assert_eq!(data_read_by_thread1.len(), NUM_ITERATIONS);
    assert_eq!(data_read_by_thread2.len(), NUM_ITERATIONS);
    for i in 0..data_read_by_thread1.len() {
        assert_eq!(data_read_by_thread1[i], if i > 0 { data_read_by_thread2[i-1] + 2 } else { 0 });
        assert_eq!(data_read_by_thread2[i], data_read_by_thread1[i] + 1);
    }
}
