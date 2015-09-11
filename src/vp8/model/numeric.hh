#ifndef _VP8_MODEL_NUMERIC_HH_
#define _VP8_MODEL_NUMERIC_HH_
//#define DEBUGDECODE
//for uint16_t
#include <cstdint>
//for pair
#include <utility>
// for std::min
#include <algorithm>
#include <assert.h>
class BoolEncoder;
class BoolDecoder;

enum class TokenNode : uint8_t {
    ZERO,
    LENGTH0,//[0,0] = 1      [0,1,0,1] = 5 [1,1,0,1] = 9
    LENGTH1,//[1,0,0] = 2    [0,1,1,1] = 6 [1,1,1,1] = 10
    LENGTH2,//[0,1,0,0] = 3  [1,1,0,0] = 7
    LENGTH3,//[0,1,1,0] = 4  [1,1,1,0] = 8
    VAL0,
    VAL1,
    VAL2,
    VAL3,
    VAL4,
    VAL5,
    VAL6,
    VAL7,
    VAL8,
    NEGATIVE,
    EOB,
    BaseOffset // do not use
};

static constexpr uint8_t NUMERIC_LENGTH_MAX = 12;
static constexpr uint8_t NUMERIC_LENGTH_BITS = 4;

template<typename intt> intt log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    constexpr intt b[] = {0x2,
        0xC,
        0xF0,
        (intt)0xFF00,
        (intt)0xFFFF0000U,
        (intt)0xFFFFFFFF00000000ULL};
    constexpr intt S[] = {1, 2, 4, 8, 16, 32};
    intt r = 0; // result of log2(v) will go here
    for (signed int i = loop_max; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i])
        {
            v >>= S[i];
            r |= S[i];
        }
    }
    return r;
}
static constexpr uint8_t LogTable16[16] = {
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
};
inline constexpr uint8_t uint16log2(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
        ? 12 + LogTable16[v >> 12]
            : 8 + LogTable16[v>>8]
                : 4 + LogTable16[v>>4]
                    : LogTable16[v];
}
static constexpr uint8_t LenTable16[16] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};
inline constexpr uint8_t uint16bit_length(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
        ? 12 + LenTable16[v >> 12]
            : 8 + LenTable16[v>>8]
                : 4 + LenTable16[v>>4]
                    : LenTable16[v];
}

template <typename intt> intt bit_length(intt v) {
    return v == 0 ? 0 : log2(v) + 1;
}
template<class EncoderT> uint8_t put_ceil_log_coefficient( EncoderT &e,
                                                           const uint16_t token_value ) {
    if (token_value == 1) {
        e.encode_one(false, TokenNode::LENGTH0);
        e.encode_one(false, TokenNode::LENGTH1);
        return 1;
    }
    if (token_value < 4) {
        e.encode_one(true, TokenNode::LENGTH0);
        e.encode_one(false, TokenNode::LENGTH1);
        e.encode_one(false, TokenNode::LENGTH2);
        return 2;
    }
    uint8_t length = log2(token_value);
    ++length;
    uint8_t prefix_coded_length =
        2 //offset
        + (((length - 3)&4)>>2) // 3 + length
        + 4*((length - 3) & 3); //  3-6 or 7-10
     e.encode_one((prefix_coded_length&1) ? true : false, TokenNode::LENGTH0);
     e.encode_one((prefix_coded_length&2) ? true : false, TokenNode::LENGTH1);
     e.encode_one((prefix_coded_length&4) ? true : false, TokenNode::LENGTH2);
     e.encode_one((prefix_coded_length&8) ? true : false, TokenNode::LENGTH3);
    return length;
}

template<class EncoderT> void put_one_natural_significand_coefficient( EncoderT &e,
                                                                       uint8_t exp,
                                                                       const uint16_t token_value )
{
    assert(token_value < 1024);// don't support higher than this yet
    for (uint8_t i = 0; i + 1 < exp; ++i) {
        e.encode_one((token_value&(1 << i)), (TokenNode)((int)TokenNode::VAL0 + i));
    }
    return;
}

template<class EncoderT> void put_one_natural_coefficient( EncoderT &e,
                                                           const uint16_t token_value )
{
    assert(token_value < 1024);// don't support higher than this yet
    uint8_t length = put_ceil_log_coefficient(e, token_value);
    for (uint8_t i = 0; i + 1 < length; ++i) {
        e.encode_one((token_value&(1 << i)), (TokenNode)((int)TokenNode::VAL0 + i));
    }
    return;
}
template<class EncoderT> void put_one_signed_coefficient( EncoderT &e,
                                 const bool last_was_zero,
                                 bool eob,
                                 const int16_t coefficient )
{
  const bool coefficient_sign = coefficient < 0;

  if ( eob ) {
    assert( not last_was_zero );
    e.encode_one( true, TokenNode::EOB );
    return;
  }
  
  if ( not last_was_zero ) {
    e.encode_one( false, TokenNode::EOB );
  }

  if ( coefficient == 0 ) {
      e.encode_one( true, TokenNode::ZERO );
    return;
  }

  e.encode_one( false, TokenNode::ZERO );
  put_one_natural_coefficient(e, abs(coefficient));
  e.encode_one( coefficient_sign, TokenNode::NEGATIVE );
}

template<class EncoderT> void put_one_signed_nonzero_coefficient( EncoderT &e,
                                 const int16_t coefficient )
{
    const bool coefficient_sign = coefficient < 0;
    put_one_natural_coefficient(e, abs(coefficient));
    e.encode_one( coefficient_sign, TokenNode::NEGATIVE );
}

template<class EncoderT> void put_one_unsigned_coefficient( EncoderT &e,
                                                            const uint16_t coefficient )
{

  if ( coefficient == 0 ) {
      e.encode_one( true, TokenNode::ZERO );
      return;
  }

  e.encode_one( false, TokenNode::ZERO );
  put_one_natural_coefficient(e, coefficient);
}

#endif
