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

inline constexpr uint32_t computeDivisor(uint16_t d) {
    return (((( 1 << uint16bit_length(d)) - d) << 16) / d) + 1;
}
#define COMPUTE_DIVISOR(off) \
   computeDivisor(off) \
   ,computeDivisor(off + 1) \
   ,computeDivisor(off + 2) \
   ,computeDivisor(off + 3) \
   ,computeDivisor(off + 4) \
   ,computeDivisor(off + 5) \
   ,computeDivisor(off + 6) \
   ,computeDivisor(off + 7) \
   ,computeDivisor(off + 8) \
   ,computeDivisor(off + 9) \
   ,computeDivisor(off + 10) \
   ,computeDivisor(off + 11) \
   ,computeDivisor(off + 12) \
   ,computeDivisor(off + 13) \
   ,computeDivisor(off + 14) \
   ,computeDivisor(off + 15)
#define COMPUTE_DIVISOR_x100(off) \
   COMPUTE_DIVISOR(off + 0x00) \
   ,COMPUTE_DIVISOR(off + 0x10) \
   ,COMPUTE_DIVISOR(off + 0x20) \
   ,COMPUTE_DIVISOR(off + 0x30)

#define COMPUTE_LOG2(off) \
 uint16log2(off) \
,uint16log2(off + 1) \
,uint16log2(off + 2) \
,uint16log2(off + 3) \
,uint16log2(off + 4) \
,uint16log2(off + 5) \
,uint16log2(off + 6) \
,uint16log2(off + 7) \
,uint16log2(off + 8) \
,uint16log2(off + 9) \
,uint16log2(off + 10) \
,uint16log2(off + 11) \
,uint16log2(off + 12) \
,uint16log2(off + 13) \
,uint16log2(off + 14) \
,uint16log2(off + 15)

#define COMPUTE_LOG2_x100(off) \
COMPUTE_LOG2(off + 0x00) \
,COMPUTE_LOG2(off + 0x10) \
,COMPUTE_LOG2(off + 0x20) \
,COMPUTE_LOG2(off + 0x30)

static constexpr uint32_t Log2Table[1024] = {
    COMPUTE_LOG2_x100(0x00)
    ,COMPUTE_LOG2_x100(0x40)
    ,COMPUTE_LOG2_x100(0x80)
    ,COMPUTE_LOG2_x100(0xc0)
    ,COMPUTE_LOG2_x100(0x100)
    ,COMPUTE_LOG2_x100(0x140)
    ,COMPUTE_LOG2_x100(0x180)
    ,COMPUTE_LOG2_x100(0x1c0)
    ,COMPUTE_LOG2_x100(0x200)
    ,COMPUTE_LOG2_x100(0x240)
    ,COMPUTE_LOG2_x100(0x280)
    ,COMPUTE_LOG2_x100(0x2c0)
    ,COMPUTE_LOG2_x100(0x300)
    ,COMPUTE_LOG2_x100(0x340)
    ,COMPUTE_LOG2_x100(0x380)
    ,COMPUTE_LOG2_x100(0x3c0)

};

static constexpr uint32_t DivisorMultipliers[1024] = {
    0
    ,computeDivisor(1)
    ,computeDivisor(2)
    ,computeDivisor(3)
    ,computeDivisor(4)
    ,computeDivisor(5)
    ,computeDivisor(6)
    ,computeDivisor(7)
    ,computeDivisor(8)
    ,computeDivisor(9)
    ,computeDivisor(10)
    ,computeDivisor(11)
    ,computeDivisor(12)
    ,computeDivisor(13)
    ,computeDivisor(14)
    ,computeDivisor(15)
    ,COMPUTE_DIVISOR(0x10)
    ,COMPUTE_DIVISOR(0x20)
    ,COMPUTE_DIVISOR(0x30)
    ,COMPUTE_DIVISOR_x100(0x40)
    ,COMPUTE_DIVISOR_x100(0x80)
    ,COMPUTE_DIVISOR_x100(0xc0)
    ,COMPUTE_DIVISOR_x100(0x100)
    ,COMPUTE_DIVISOR_x100(0x140)
    ,COMPUTE_DIVISOR_x100(0x180)
    ,COMPUTE_DIVISOR_x100(0x1c0)
    ,COMPUTE_DIVISOR_x100(0x200)
    ,COMPUTE_DIVISOR_x100(0x240)
    ,COMPUTE_DIVISOR_x100(0x280)
    ,COMPUTE_DIVISOR_x100(0x2c0)
    ,COMPUTE_DIVISOR_x100(0x300)
    ,COMPUTE_DIVISOR_x100(0x340)
    ,COMPUTE_DIVISOR_x100(0x380)
    ,COMPUTE_DIVISOR_x100(0x3c0)
};
/*
template<int N>
struct minus_one {
    enum {
       value = N -1
    };
};
template<int N, uint32_t... RemainingValues>
struct DivisorTableGen {
    static constexpr auto & value = DivisorTableGen<N - 1,
    computeDivisor(N ),
                                                   RemainingValues... >:: value;
};
template<uint32_t... RemainingValues>
struct DivisorTableGen<0, RemainingValues...> {
    static constexpr int value[] = {0, computeDivisor(1), computeDivisor(2), computeDivisor(3), RemainingValues... };
};
template<uint32_t... RemainingValues>
constexpr int DivisorTableGen<0, RemainingValues...>::value[];



template<int N, uint32_t... RemainingValues>
struct Log2TableGen {
    static constexpr auto & value = Log2TableGen<N - 4,
    uint16log2(N),
    RemainingValues... >:: value;
};
template<uint32_t... RemainingValues>
struct Log2TableGen<0, RemainingValues...> {
    static constexpr int value[] = {0, RemainingValues... };
};
template<uint32_t... RemainingValues>
constexpr int Log2TableGen<0, RemainingValues...>::value[];
*/

constexpr uint16_t fast_divide10bit(uint16_t num, uint16_t denom) {
    return (((DivisorMultipliers[denom] * (uint32_t)num) >> 16)
         + ((num - (((uint32_t)DivisorMultipliers[denom] * (uint32_t)num) >> 16)) >> 1))
          >> Log2Table[denom];
    /*
    return ((DivisorTableGen<10>::value[denom] * num
            + ((num - DivisorTableGen<10>::value[denom] * num) >> 1)) >> Log2TableGen<10>::value[denom]);
     */
}

constexpr uint16_t slow_divide10bit(uint16_t num, uint16_t denom) {
    uint64_t m = DivisorMultipliers[denom];
    uint64_t t = (m * num) >> 16;
    uint64_t n_minus_t = num - t;
    uint64_t t_plus_shr = t + (n_minus_t >> 1);
    int log2d = Log2Table[denom];
    int lend = uint16bit_length(denom);
    assert(lend - 1 == log2d);
    uint64_t retval = t_plus_shr >> (log2d);
    assert(num / denom == retval);
    return retval;

}


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
