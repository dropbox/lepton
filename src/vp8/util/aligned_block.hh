#ifndef ALIGNED_BLOCK_HH_
#define ALIGNED_BLOCK_HH_
#include <assert.h>
#include "nd_array.hh"
#include "jpeg_meta.hh"
#include "../model/color_context.hh"

#define OPTIMIZED_7x7
static constexpr Sirikata::Array1d< uint8_t, 64 > jpeg_zigzag_to_raster = {{
    0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
}};

static constexpr Sirikata::Array1d< uint8_t, 64 > raster_to_jpeg_zigzag = {{
    0,  1,  5,  6, 14, 15, 27, 28,
    2,  4,  7, 13, 16, 26, 29, 42,
    3,  8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
}};

#ifdef OPTIMIZED_7x7
static constexpr Sirikata::Array1d< uint8_t, 64 > aligned_to_raster = {{
    9, 10,
    17, 25, 18, 11,
    12, 19, 26, 33, 41, 34,
    27, 20, 13, 14, 21, 28,
    35, 42, 49, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
    0, // DC
    1, 2, 3, 4, 5, 6, 7, // 1x8
    8, 16, 24, 32, 40, 48, 56, // 8x1
}};

static constexpr Sirikata::Array1d< uint8_t, 64 > raster_to_aligned = {{
        49, 50, 51, 52, 53, 54, 55, 56, 
        57, 0, 1, 5, 6, 14, 15, 27, 
        58, 2, 4, 7, 13, 16, 26, 28, 
        59, 3, 8, 12, 17, 25, 29, 38, 
        60, 9, 11, 18, 24, 30, 37, 39, 
        61, 10, 19, 23, 31, 36, 40, 45, 
        62, 20, 22, 32, 35, 41, 44, 46, 
        63, 21, 33, 34, 42, 43, 47, 48
}};
static constexpr Sirikata::Array1d< uint8_t, 64 > zigzag_to_aligned = {{
        49, 50, 57, 58, 0, 51, 52, 1, 
        2, 59, 60, 3, 4, 5, 53, 54, 
        6, 7, 8, 9, 61, 62, 10, 11, 
        12, 13, 14, 55, 56, 15, 16, 17, 
        18, 19, 20, 63, 21, 22, 23, 24, 
        25, 26, 27, 28, 29, 30, 31, 32, 
        33, 34, 35, 36, 37, 38, 39, 40, 
        41, 42, 43, 44, 45, 46, 47, 48
}};

static constexpr Sirikata::Array1d< uint8_t, 64 > aligned_to_zigzag = {{
        4, 7, 8, 11, 12, 13, 16, 17, 
        18, 19, 22, 23, 24, 25, 26, 29, 
        30, 31, 32, 33, 34, 36, 37, 38, 
        39, 40, 41, 42, 43, 44, 45, 46, 
        47, 48, 49, 50, 51, 52, 53, 54, 
        55, 56, 57, 58, 59, 60, 61, 62, 
        63, 0, 1, 5, 6, 14, 15, 27, 
        28, 2, 3, 9, 10, 20, 21, 35
}};
#else
#define aligned_to_zigzag raster_to_jpeg_zigzag
#define zigzag_to_aligned jpeg_zigzag_to_raster
struct IdentityArray1d {
    static uint8_t at(uint8_t a) {return a;}
    template <int a> static constexpr uint8_t kat() {return a;}
};
static IdentityArray1d raster_to_aligned;
static IdentityArray1d aligned_to_raster;
#endif


struct BlockColorContext;


enum class ColorChannel { Y, Cb, Cr,
#ifdef ALLOW_FOUR_COLORS
    Ck,
#endif
    NumBlockTypes };

class AlignedBlock
{
#ifdef OPTIMIZED_7x7
public:
#endif
  Sirikata::Array1d<int16_t, 64> coef = {{{}}};
  enum Index : uint8_t{
#ifdef OPTIMIZED_7x7
      AC_7x7_INDEX = 0,
      AC_7x7_END = 49,
      DC_INDEX = 49,
      ROW_X_INDEX = 50,
      ROW_X_END = 57,
      ROW_Y_INDEX = 57,
      ROW_Y_END = 64
#else
      //AC_7x7_INDEX = 9,
      //AC_7x7_END = 63,
      DC_INDEX = 0,
      //ROW_X_INDEX = 1,
      //ROW_X_END = 7,
      //ROW_Y_INDEX = 57,
      //ROW_Y_END = 64
#endif
  };
public:
  AlignedBlock() {
  }
    int16_t*raw_data() {
        return &coef.at(0);
    }
    const int16_t*raw_data() const {
        return &coef.at(0);
    }
  uint8_t recalculate_coded_length() const
  {
    uint8_t num_nonzeros_7x7 = 0;
    /* how many tokens are we going to encode? */
    for ( unsigned int index = 0; index < 64; index++ ) {
        unsigned int xy = jpeg_zigzag_to_raster.at( index );
        unsigned int x = xy & 7;
        unsigned int y = xy >> 3;
        if (coef.at(raster_to_aligned.at(xy))) {
            //coded_length_ = index + 1;
            if (x > 0 && y > 0) {
                ++num_nonzeros_7x7;
            }
        }
    }
      return num_nonzeros_7x7;
  }
  void bzero() {
    coef.memset(0);
  }
  int16_t & dc() {return coef.at(DC_INDEX); }
  int16_t dc()const {return coef.at(DC_INDEX); }

  int16_t & mutable_coefficients_raster(uint8_t index) {return coef.at(raster_to_aligned.at(index)); }
  int16_t coefficients_raster(uint8_t index) const { return coef.at(raster_to_aligned.at(index)); }

  int16_t & mutable_coefficients_zigzag(uint8_t index) {return coef.at(zigzag_to_aligned.at(index)); }
  int16_t coefficients_zigzag(uint8_t index) const { return coef.at(zigzag_to_aligned.at(index)); }

};


#endif /* BLOCK_HH */
