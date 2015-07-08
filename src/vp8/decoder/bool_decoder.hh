#ifndef BOOL_DECODER_HH
#define BOOL_DECODER_HH
#include <vector>

#include "../util/arithmetic_code.hh"
typedef uint8_t Probability;
#include "model.hh"

class Branch;

class BoolDecoder
{
private:
  const uint8_t *buffer;
  uint64_t size;
    arithmetic_code<uint64_t, uint8_t>::decoder<const uint8_t *,
                             uint8_t> inner;
public:
  template <class BufferWrapper> BoolDecoder( const BufferWrapper & s_chunk ) :
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

#endif /* BOOL_DECODER_HH */
