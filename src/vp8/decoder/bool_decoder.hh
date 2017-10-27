#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH
#include <vector>

#include "model.hh"
#include "../../io/Reader.hh"
#include "JpegArithmeticCoder.hh"
#include "vpx_bool_reader.hh"
#ifdef ENABLE_ANS_EXPERIMENTAL
#include "ans_bool_reader.hh"
#endif
typedef int8_t TreeNode;

class Branch;
class SliceReader : public Sirikata::DecoderReader {
    const uint8_t *buffer_;
    size_t size_;
public:
    SliceReader(const uint8_t* sbuffer, size_t ssize) : buffer_(sbuffer), size_(ssize) {}
    std::pair<unsigned int, Sirikata::JpegError> Read(unsigned char *data,
                                                      unsigned int value) {
        std::pair<unsigned int, Sirikata::JpegError> retval(value, Sirikata::JpegError::nil());
        if (size_ < retval.first) {
            retval.first = size_;
        }
        if (retval.first) {
            memcpy(data, buffer_, retval.first);
            buffer_ += retval.first;
            size_ -= retval.first;
        } else {
            retval.second = Sirikata::JpegError::errEOF();
        }
        return retval;
    }
};
class JpegBoolDecoder : public SliceReader {
    Sirikata::ArithmeticCoder jpeg_coder_;
public:
    JpegBoolDecoder(const uint8_t *buffer, size_t size)
      : SliceReader(buffer, size),
        jpeg_coder_(false) {

    }
    bool get( Branch & branch ) {
        return jpeg_coder_.arith_decode(this, &branch.probability_);
    }
};
/*
#ifdef JPEG_ENCODER
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public JpegBoolDecoder {public:
    BoolDecoder(const uint8_t *data, size_t size) : JpegBoolDecoder(data, size){}
};
#else
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public VPXBoolReader { public:
    BoolDecoder(const uint8_t *data, size_t size) : VPXBoolReader(data, size){}
    BoolDecoder(PacketReader*pr) : VPXBoolReader(pr){}
    BoolDecoder() {}
};
#endif
*/

#endif /* BOOL_DECODER_HH */
