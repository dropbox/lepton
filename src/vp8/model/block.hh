#ifndef _BLOCK_HH
#define _BLOCK_HH
#include <assert.h>
#include "plane.hh"
#include "fixed_array.hh"
#include "jpeg_meta.hh"
#define BLOCK_ENCODE_BACKWARDS
static constexpr FixedArray< uint8_t, 64 > jpeg_zigzag = {{
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

enum BlockType { Y, Cb, Cr };

class Block
{
private:
  // Keeping coefficients at base address of block reduces pointer arithmetic in accesses
  FixedArray<int16_t, 64> coefficients_ {{}};

  typename Plane< Block >::LocalArea context_;

  BlockType type_;

  uint8_t coded_length_ = 255;

  uint8_t num_nonzeros_ = 0;
  uint8_t num_nonzeros_7x7_ = 0;
  uint8_t num_nonzeros_x_ = 0;
  uint8_t num_nonzeros_y_ = 0;
  
public:
  enum {
    BLOCK_SLICE = 16
  };


  Block( const typename Plane< Block >::LocalArea & context, const BlockType type )
    : context_( context ), type_( type )
  {}

  const typename Plane< Block >::LocalArea & context( void ) const { return context_; }

  BlockType type() const { return type_; }
  
  void parse_tokens( BoolDecoder & data, ProbabilityTables & probability_tables );

  void recalculate_coded_length()
  {
    coded_length_ = 0;
    num_nonzeros_ = 0;
    num_nonzeros_7x7_ = 0;
    num_nonzeros_x_ = 0;
    num_nonzeros_y_ = 0;
    /* how many tokens are we going to encode? */
    for ( unsigned int index = 0; index < 64; index++ ) {
        unsigned int xy = jpeg_zigzag.at( index );
        unsigned int x = xy & 7;
        unsigned int y = xy / 8;
        if ( coefficients_.at( xy ) ) {
            coded_length_ = index + 1;
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
#ifndef USE_ZEROS
//    num_nonzeros_ = coded_length_; FIXME
#endif
    //assert(coded_length_ + num_nonzeros_ * 65 < 260);
/*
    if (coded_length_ > 16) {
        assert(coded_length_ / 4 + 12 < 30);
        num_nonzeros_ = coded_length_ / 4 + 12 + 3-num_nonzeros_;
    } else {
        num_nonzeros_ = coded_length_;
    }
*/
    //num_nonzeros_=std::min(coded_length_, (unsigned char)31U);
  }
  
    std::pair<Optional<int16_t>,
              Optional<int16_t> > get_near_coefficients(uint8_t unzigzag_coord) const {
        std::pair<Optional<int16_t> , Optional<int16_t> > retval;
#ifdef BLOCK_ENCODE_BACKWARDS
        if (unzigzag_coord < 63) {
            if (unzigzag_coord % 8 == 7 ) {
                retval.second = coefficients().at(unzigzag_coord + 8);
            } else if (unzigzag_coord < 64 - 8) {
                retval.first = coefficients().at(unzigzag_coord + 1);
                retval.second = coefficients().at(unzigzag_coord + 8);
            } else {
                retval.first = coefficients().at(unzigzag_coord + 1);
            }
        }
#else
        if (index > 0) {
            if (unzigzag_coord % 8 == 0 ) {
                retval.second = coefficients().at(unzigzag_coord - 8);
            } else if (unzigzag_coord > 8) {
                retval.first = coefficients().at(unzigzag_coord - 1);
                retval.second = coefficients().at(unzigzag_coord - 8);
            } else {
                retval.first = coefficients().at(unzigzag_coord - 1);
            }
        }
#endif
        return retval;
  }
  void serialize_tokens( BoolEncoder & data,
			 ProbabilityTables & probability_tables ) const;

  std::array<int16_t, 64> & mutable_coefficients() { return coefficients_; }
  const std::array<int16_t, 64> & coefficients() const { return coefficients_; }

  uint8_t coded_length() const { return coded_length_; }
  uint8_t num_nonzeros() const { return num_nonzeros_; }
  uint8_t num_nonzeros_7x7() const { return num_nonzeros_7x7_; }
  uint8_t num_nonzeros_x() const { return num_nonzeros_x_; }
  uint8_t num_nonzeros_y() const { return num_nonzeros_y_; }
};

#endif
