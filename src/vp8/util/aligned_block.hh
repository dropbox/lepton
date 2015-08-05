#ifndef _ALIGNED_BLOCK_HH_
#define _ALIGNED_BLOCK_HH_
#include <assert.h>
#include "nd_array.hh"
#include "jpeg_meta.hh"
#include "../model/color_context.hh"


#define BLOCK_ENCODE_BACKWARDS
static constexpr Sirikata::Array1d< uint8_t, 64 > jpeg_zigzag = {{
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63 }};

struct ProbabilityTables;
class BoolEncoder;
class BoolDecoder;
struct BlockColorContext;


enum class ColorChannel { Y, Cb, Cr, NumBlockTypes };

class AlignedBlock
{
private:

  uint8_t num_nonzeros_ = 0;
  uint8_t num_nonzeros_7x7_ = 0;
  uint8_t num_nonzeros_x_ = 0;
  uint8_t num_nonzeros_y_ = 0;

public:
    Sirikata::Array2d<int16_t, 8, 8> coef = {{{}}};

  AlignedBlock() {}

  void recalculate_coded_length()
  {
    num_nonzeros_ = 0;
    num_nonzeros_7x7_ = 0;
    num_nonzeros_x_ = 0;
    num_nonzeros_y_ = 0;
    /* how many tokens are we going to encode? */
    for ( unsigned int index = 0; index < 64; index++ ) {
        unsigned int xy = jpeg_zigzag.at( index );
        unsigned int x = xy & 7;
        unsigned int y = xy >> 3;
        if ( coef.at( y, x ) ) {
            //coded_length_ = index + 1;
            ++num_nonzeros_;
            if (x == 0 && y) {
                ++num_nonzeros_y_;
            }
            if (y == 0 && x) {
                ++num_nonzeros_x_;
            }
            if (x > 0 && y > 0) {
                ++num_nonzeros_7x7_;
            }
        }
    }
  }

  Sirikata::Array2d<int16_t, 8, 8> & mutable_coefficients( void ) { return coef; }
  const Sirikata::Array2d<int16_t, 8, 8> & coefficients( void ) const { return coef; }

  //uint8_t coded_length() const { return coded_length_; }
  uint8_t num_nonzeros() const { return num_nonzeros_; }
  uint8_t num_nonzeros_7x7() const { return num_nonzeros_7x7_; }
  uint8_t num_nonzeros_x() const { return num_nonzeros_x_; }
  uint8_t num_nonzeros_y() const { return num_nonzeros_y_; }
};


#endif /* BLOCK_HH */
