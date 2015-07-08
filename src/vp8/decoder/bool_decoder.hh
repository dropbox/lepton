#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH
#include <vector>

#include "../util/arithmetic_code.hh"
typedef uint8_t Probability;
#include "model.hh"
#include "../../io/Reader.hh"
#include "JpegArithmeticCoder.hh"
#include "slice.hh"
typedef int8_t TreeNode;

class Branch;
class ChunkReader : public Sirikata::DecoderReader {
    Chunk chunk_;
public:
    ChunkReader(const Chunk &s_chunk) : chunk_(s_chunk) {}
    std::pair<unsigned int, Sirikata::JpegError> Read(unsigned char *data,
                                                      unsigned int value) {
        std::pair<unsigned int, Sirikata::JpegError> retval(value, Sirikata::JpegError::nil());
        if (chunk_.size() < retval.first) {
            retval.first = chunk_.size();
        }
        if (retval.first) {
            memcpy(data, chunk_.buffer(), retval.first);
            chunk_ = chunk_(retval.first);
        } else {
            retval.second = Sirikata::JpegError::errEOF();
        }
        return retval;
    }
};
class JpegBoolDecoder : public ChunkReader {
    Sirikata::ArithmeticCoder jpeg_coder_;
public:
    JpegBoolDecoder(const Chunk& c) : ChunkReader(c), jpeg_coder_(false) {

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
  template <class BufferWrapper> VP8BoolDecoder( const BufferWrapper & s_chunk ) :
     buffer(s_chunk.buffer()),
     size(s_chunk.size()),
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
  if (retval) {
      branch.record_true_and_update();
  } else {
      branch.record_false_and_update();
  }
  return retval;
}
  
};

#ifdef JPEG_ENCODER
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public JpegBoolDecoder {public:
    BoolDecoder(const Chunk&c) : JpegBoolDecoder(c){}
};
#else
//easier than a typedef so that we can forward declare this class elsewhere
class BoolDecoder : public VP8BoolDecoder { public:
    BoolDecoder(const Chunk&c) : VP8BoolDecoder(c){}
};
#endif


#endif /* BOOL_DECODER_HH */
