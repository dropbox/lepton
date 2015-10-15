#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH
#include <vector>

#include "../util/arithmetic_code.hh"
typedef uint8_t Probability;
#include "model.hh"
#include "../../io/Reader.hh"
#include "JpegArithmeticCoder.hh"
#include "vpx_bool_reader.hh"
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

class VP8BoolDecoder
{
private:
  const uint8_t *buffer;
  uint64_t size;
    arithmetic_code<uint64_t, uint8_t>::decoder<const uint8_t *,
                             uint8_t> inner;
public:
  template <class BufferWrapper> VP8BoolDecoder( const BufferWrapper & s_slice ) :
     buffer(s_slice.buffer()),
     size(s_slice.size()),
     inner(buffer, buffer + size) {
  }

  bool get( const Probability probability = 128 )
  {
      bool ret = inner.get([=](uint64_t range){if (range > 512) range /= 512; else range = 1; range *= ((255 - probability) * 2 + 1); assert(range > 0);return range;});
    #if 0
    static int counter = 0;
    fprintf(stderr, "%d) get %d with prob %d\n", counter++, (int)ret, (int) probability);
    #endif
    return ret;
  }

  bool get( Branch & branch ) {
  bool retval = get( branch.prob() );
  branch.record_obs_and_update(retval);
  return retval;
}
  
};

#ifdef JPEG_ENCODER
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public JpegBoolDecoder {public:
    BoolDecoder(const uint8_t *data, size_t size) : JpegBoolDecoder(data, size){}
};
#else
#if VP8_ENCODER
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public VP8BoolDecoder { public:
    BoolDecoder(const uint8_t *data, size_t size) : VP8BoolDecoder(data, size){}
};
#else
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public VPXBoolReader { public:
    BoolDecoder(const uint8_t *data, size_t size) : VPXBoolReader(data, size){}
    BoolDecoder() {}
};
#endif
#endif


#endif /* BOOL_DECODER_HH */
