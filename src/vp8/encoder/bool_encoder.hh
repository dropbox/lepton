#ifndef BOOL_ENCODER_HH
#define BOOL_ENCODER_HH

#include <vector>
#include <iostream>

#include "../util/arithmetic_code.hh"
#include <assert.h>
#include "branch.hh"

typedef uint8_t Probability;
class Branch;

class BoolEncoder
{
private:
  std::vector< uint8_t > output_;
  arithmetic_code<uint64_t,  uint8_t>::encoder<std::back_insert_iterator<std::vector<uint8_t> > , uint8_t> inner;


public:
  BoolEncoder()
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
    *this = BoolEncoder();
    return ret;
  }
};

#endif /* BOOL_ENCODER_HH */
