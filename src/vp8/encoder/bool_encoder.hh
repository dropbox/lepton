#ifndef BOOL_ENCODER_HH
#define BOOL_ENCODER_HH

#include <vector>
#include <iostream>

#include "../util/arithmetic_code.hh"
#include <assert.h>
#include "branch.hh"
#include "../../io/MemReadWriter.hh"
#include "JpegArithmeticCoder.hh"
#include "vpx_bool_writer.hh"
/* Routines taken from ISO/IEC 10918-1 : 1993(E) */

class JpegBoolEncoder : public Sirikata::MemReadWriter {
    Sirikata::ArithmeticCoder jpeg_coder_;
public:
    JpegBoolEncoder(const Sirikata::JpegAllocator<unsigned char>&alloc=Sirikata::JpegAllocator<unsigned char>())
        : MemReadWriter(alloc), jpeg_coder_(true) {
    }
    void put( const bool value, Branch & branch ) {
        jpeg_coder_.arith_encode(this, &branch.probability_, value);
    }
    std::vector< uint8_t, Sirikata::JpegAllocator<unsigned char> > finish( void ) {
        jpeg_coder_.finish_encode(this);
        return buffer();
    }
};

typedef uint8_t Probability;
class Branch;

class VP8BoolEncoder
{
private:
  std::vector< uint8_t > output_;
  arithmetic_code<uint64_t,  uint8_t>::encoder<std::back_insert_iterator<std::vector<uint8_t> > , uint8_t> inner;


public:
  VP8BoolEncoder()
    : inner(std::back_insert_iterator<std::vector<uint8_t> >(output_)) {
  }

  void put( const bool value, Branch & branch );
  
  void put( const bool value, const Probability probability = 128 )
  {
      #if 0
      static int counter =0;
      fprintf(stderr, "%d) put %d with prob %d\n",counter++, (int)value, (int) probability);
      #endif
      inner.put(value ? 1 : 0, [=](uint64_t range){if (range > 512) range /= 512; else range = 1; range *= ((255 - probability) * 2 + 1); assert(range > 0); return range;});
  }

  std::vector< uint8_t > finish( void )
  {
    inner.finish();
    std::vector< uint8_t > ret( move( output_ ) );
    *this = VP8BoolEncoder();
    return ret;
  }
};
#ifdef JPEG_ENCODER
class BoolEncoder : public JpegBoolEncoder{};
#else
#ifdef VP8_ENCODER
class BoolEncoder : public VP8BoolEncoder{};
#else
class BoolEncoder : public VPXBoolWriter{};
#endif
#endif
#endif /* BOOL_ENCODER_HH */
