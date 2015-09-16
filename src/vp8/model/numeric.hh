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
static constexpr char LogTable256[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};
static constexpr uint8_t LenTable16[16] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};
static constexpr char LenTable256[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    LT(5), LT(6), LT(6), LT(7), LT(7), LT(7), LT(7),
    LT(8), LT(8), LT(8), LT(8), LT(8), LT(8), LT(8), LT(8)
#undef LT
};

#if 0
inline constexpr uint8_t k16log2(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
    ? 12 + LogTable16[v >> 12]
    : 8 + LogTable16[v>>8]
    : 4 + LogTable16[v>>4]
    : LogTable16[v];
}
inline constexpr uint8_t uint16bit_length(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
    ? 12 + LenTable16[v >> 12]
    : 8 + LenTable16[v>>8]
    : 4 + LenTable16[v>>4]
    : LenTable16[v];
}
#else
inline constexpr uint8_t k16log2(uint16_t v) {
    return (v & 0xff00)
    ? 8 + LogTable256[v >> 8]
    : LogTable256[v];
}
inline constexpr uint8_t k16bit_length(uint16_t v) {
    return (v & 0xff00)
    ? 8 + LenTable256[v >> 8]
    : LenTable256[v];
}
inline uint8_t uint16log2(uint16_t v) {
    return 31 - __builtin_clz((uint32_t)v);
}
inline uint8_t nonzero_bit_length(uint16_t v) {
    assert(v);
    return 32 - __builtin_clz((uint32_t)v);
}
inline uint8_t uint16bit_length(uint16_t v) {
    return v ? 32 - __builtin_clz((uint32_t)v) : 0;
}
#endif

constexpr uint8_t log_max_numerator = 18;

inline constexpr uint32_t computeDivisor(uint16_t d) {
    return (((( 1 << k16bit_length(d)) - d) << log_max_numerator) / d) + 1;
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
 k16log2(off) \
,k16log2(off + 1) \
,k16log2(off + 2) \
,k16log2(off + 3) \
,k16log2(off + 4) \
,k16log2(off + 5) \
,k16log2(off + 6) \
,k16log2(off + 7) \
,k16log2(off + 8) \
,k16log2(off + 9) \
,k16log2(off + 10) \
,k16log2(off + 11) \
,k16log2(off + 12) \
,k16log2(off + 13) \
,k16log2(off + 14) \
,k16log2(off + 15)

#define COMPUTE_LOG2_x100(off) \
COMPUTE_LOG2(off + 0x00) \
,COMPUTE_LOG2(off + 0x10) \
,COMPUTE_LOG2(off + 0x20) \
,COMPUTE_LOG2(off + 0x30)


#define COMPUTE_DIVISOR_AND_LOG2(off) \
{computeDivisor(off), k16log2(off)} \
,{computeDivisor(off + 1), k16log2(off + 1)} \
,{computeDivisor(off + 2), k16log2(off + 2)} \
,{computeDivisor(off + 3), k16log2(off + 3)} \
,{computeDivisor(off + 4), k16log2(off + 4)} \
,{computeDivisor(off + 5), k16log2(off + 5)} \
,{computeDivisor(off + 6), k16log2(off + 6)} \
,{computeDivisor(off + 7), k16log2(off + 7)} \
,{computeDivisor(off + 8), k16log2(off + 8)} \
,{computeDivisor(off + 9), k16log2(off + 9)} \
,{computeDivisor(off + 10), k16log2(off + 10)} \
,{computeDivisor(off + 11), k16log2(off + 11)} \
,{computeDivisor(off + 12), k16log2(off + 12)} \
,{computeDivisor(off + 13), k16log2(off + 13)} \
,{computeDivisor(off + 14), k16log2(off + 14)} \
,{computeDivisor(off + 15), k16log2(off + 15)}
#define COMPUTE_DIVISOR_AND_LOG2_x100(off) \
COMPUTE_DIVISOR_AND_LOG2(off + 0x00) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x10) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x20) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x30)


struct DivisorLog2 {
    uint32_t divisor;
    uint8_t len;
};
static constexpr DivisorLog2 DivisorAndLog2Table[1026] = {
    {0,0}
    ,{computeDivisor(1), k16log2(1)}
    ,{computeDivisor(2), k16log2(2)}
    ,{computeDivisor(3), k16log2(3)}
    ,{computeDivisor(4), k16log2(4)}
    ,{computeDivisor(5), k16log2(5)}
    ,{computeDivisor(6), k16log2(6)}
    ,{computeDivisor(7), k16log2(7)}
    ,{computeDivisor(8), k16log2(8)}
    ,{computeDivisor(9), k16log2(9)}
    ,{computeDivisor(10), k16log2(10)}
    ,{computeDivisor(11), k16log2(11)}
    ,{computeDivisor(12), k16log2(12)}
    ,{computeDivisor(13), k16log2(13)}
    ,{computeDivisor(14), k16log2(14)}
    ,{computeDivisor(15), k16log2(15)}
    ,COMPUTE_DIVISOR_AND_LOG2(0x10)
    ,COMPUTE_DIVISOR_AND_LOG2(0x20)
    ,COMPUTE_DIVISOR_AND_LOG2(0x30)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x40)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x80)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0xc0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x100)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x140)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x180)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x1c0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x200)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x240)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x280)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x2c0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x300)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x340)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x380)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x3c0)
    ,{computeDivisor(0x400), k16log2(0x400)}
    ,{computeDivisor(0x401), k16log2(0x401)}
};

static constexpr uint32_t Log2Table[1026] = {
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
    ,k16log2(0x400)
    ,k16log2(0x401)
};

static constexpr uint32_t DivisorMultipliers[1026] = {
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
    ,computeDivisor(0x400)
    ,computeDivisor(0x401)
};

constexpr uint32_t fast_divide10bit(uint32_t num, uint16_t denom) {
    return ((uint32_t)((DivisorAndLog2Table[denom].divisor * (uint64_t)num) >> log_max_numerator)
         + ((uint32_t)(num - (((uint64_t)DivisorAndLog2Table[denom].divisor * (uint64_t)num) >> log_max_numerator)) >> 1))
          >> DivisorAndLog2Table[denom].len;
}

inline uint32_t slow_divide10bit(uint32_t num, uint16_t denom) {
#if 0
    uint64_t m = DivisorMultipliers[denom];
    int log2d = k16log2(denom);
    //assert(log2d==DivisorAndLog2Table[denom].len);
#else
    auto dl = DivisorAndLog2Table[denom];
    uint64_t m = dl.divisor;
    uint8_t log2d = dl.len;
#endif
    uint32_t t = (m * num) >> log_max_numerator;
    uint32_t n_minus_t = num - t;
    uint32_t t_plus_shr = t + (n_minus_t >> 1);
    //assert(uint16bit_length(denom) - 1 == log2d);
    uint32_t retval = t_plus_shr >> (log2d);
    //assert(num / denom == retval);
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
    if (sizeof(int) <= 4) {
        return v ? 32 - __builtin_clz((uint32_t)v) : 0;
    } else {
        return v ? 64 - __builtin_clzl((uint64_t)v) : 0;
    }
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
