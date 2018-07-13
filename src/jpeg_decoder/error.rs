use std::io::Error as IoError;

use iostream::InputError;

pub type JpegResult<T> = ::std::result::Result<T, JpegError>;

/// An enumeration over JPEG features (currently) unsupported by this library.
///
/// Support for features listed here may be included in future versions of this library.
#[derive(Debug)]
pub enum UnsupportedFeature {
    /// Hierarchical JPEG.
    Hierarchical,
    /// Lossless JPEG.
    Lossless,
    /// JPEG using arithmetic entropy coding instead of Huffman coding.
    ArithmeticEntropyCoding,
    /// Sample precision in bits. 8 bit sample precision is what is currently supported.
    SamplePrecision(u8),
    /// Number of components in an image. 1, 3 and 4 components are currently supported.
    ComponentCount(u8),
    /// An image can specify a zero height in the frame header and use the DNL (Define Number of
    /// Lines) marker at the end of the first scan to define the number of lines in the frame.
    DNL,
    /// Subsampling ratio.
    SubsamplingRatio,
    /// A subsampling ratio not representable as an integer.
    NonIntegerSubsamplingRatio,
}

/// Errors that can occur while decoding a JPEG image.
#[derive(Debug)]
pub enum JpegError {
    /// The image is not formatted properly. The string contains detailed information about the
    /// error.
    Malformatted(String),
    /// The image makes use of a JPEG feature not (currently) supported by this library.
    Unsupported(UnsupportedFeature),
    /// An I/O error occurred while decoding the image.
    Io(IoError),
    /// EOF is encountered when trying to read data. This may or may not be an error.
    EOF,
}

impl From<InputError> for JpegError {
    fn from(_err: InputError) -> JpegError {
        JpegError::EOF
    }
}

impl From<IoError> for JpegError {
    fn from(err: IoError) -> JpegError {
        JpegError::Io(err)
    }
}

impl From<HuffmanError> for JpegError {
    fn from(err: HuffmanError) -> JpegError {
        use self::HuffmanError::*;
        match err {
            BadCode => JpegError::Malformatted("failed to decode huffman code".to_owned()),
            BadTable => JpegError::Malformatted("bad huffman code length".to_owned()),
            UnexpectedMarker(marker) => {
                JpegError::Malformatted(format!("unexpected marker {:02X?} in scan", marker))
            }
            EOF => JpegError::EOF,
        }
    }
}

#[derive(Debug)]
pub enum HuffmanError {
    BadCode,
    BadTable,
    UnexpectedMarker(u8),
    EOF,
}

impl From<InputError> for HuffmanError {
    fn from(_err: InputError) -> HuffmanError {
        HuffmanError::EOF
    }
}
