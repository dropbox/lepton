#![cfg(test)]
extern crate std;

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
