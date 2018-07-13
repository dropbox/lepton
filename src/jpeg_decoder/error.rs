use std::any::Any;
use std::error::Error as StdError;
use std::fmt;
use std::io::Error as IoError;
use std::sync::mpsc::{RecvError, SendError};

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
    Malformatted(&'static str),
    /// The image makes use of a JPEG feature not (currently) supported by this library.
    Unsupported(UnsupportedFeature),
    /// An I/O error occurred while decoding the image.
    Io(IoError),
    // /// An internal error occurred while decoding the image.
    // Internal(Box<StdError>),
}

impl From<InputError> for JpegError {
    fn from(err: InputError) -> JpegError {
        JpegError::Malformatted("unexpected EOF")
    }
}

// impl fmt::Display for JpegError {
//     fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
//         match *self {
//             JpegError::Format(ref description)      => write!(f, "invalid JPEG format: {}", description),
//             JpegError::Unsupported(ref feature) => write!(f, "unsupported JPEG feature: {:?}", feature),
//             JpegError::Io(ref err)           => err.fmt(f),
//             JpegError::Internal(ref err)     => err.fmt(f),
//         }
//     }
// }

// impl StdError for JpegError {
//     fn description(&self) -> &str {
//         match *self {
//             JpegError::Format(_)         => "invalid JPEG format",
//             JpegError::Unsupported(_)    => "unsupported JPEG feature",
//             JpegError::Io(ref err)       => err.description(),
//             JpegError::Internal(ref err) => err.description(),
//         }
//     }

//     fn cause(&self) -> Option<&StdError> {
//         match *self {
//             JpegError::Io(ref err) => Some(err),
//             JpegError::Internal(ref err) => Some(&**err),
//             _ => None,
//         }
//     }
// }

impl From<IoError> for JpegError {
    fn from(err: IoError) -> JpegError {
        JpegError::Io(err)
    }
}

// impl From<RecvError> for JpegError {
//     fn from(err: RecvError) -> JpegError {
//         JpegError::Internal(Box::new(err))
//     }
// }

// impl<T: Any + Send> From<SendError<T>> for JpegError {
//     fn from(err: SendError<T>) -> JpegError {
//         JpegError::Internal(Box::new(err))
//     }
// }
