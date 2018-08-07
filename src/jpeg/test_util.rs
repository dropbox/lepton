#![cfg(test)]

use super::util::{build_from_size_and_value, split_into_size_and_value};

#[test]
fn split_into_size_and_value_test() {
    for c in (i16::min_value() >> 1)..(i16::max_value() >> 1) {
        let (size, val) = split_into_size_and_value(c);
        assert_eq!(build_from_size_and_value(size, val), c);
    }
}
