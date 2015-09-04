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

template < unsigned int length , uint16_t base_value_, uint16_t prob_offset_>
struct TokenDecoder
{
    enum {
        prob_offset = prob_offset_,
        bits = length,
        base_value = base_value_,
        upper_limit = base_value_ + (1 << length)
    };
    template <class ProbabilityFunctor> static uint16_t decode( BoolDecoder & data, const ProbabilityFunctor &probAt);
    template <class ProbabilityFunctor> static void encode( BoolEncoder & encoder, const uint16_t value, const ProbabilityFunctor& probAt);
    static std::pair<uint64_t, uint64_t> bits_and_liveness(const uint16_t value);
};
static constexpr uint8_t NUMERIC_LENGTH_MAX = 12;
static constexpr uint8_t NUMERIC_LENGTH_BITS = 4;
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
constexpr uint8_t NUMBER_OF_EXPONENT_BITS = (uint8_t)TokenNode::LENGTH3;
class BitsAndLivenessFromEncoding {
    uint64_t bits_;
    uint64_t liveness_;
public:
   BitsAndLivenessFromEncoding() {
        bits_ = 0;
        liveness_ = 0;
    }
    void encode_one(bool value, int entropy_node_index) {
        liveness_ |= (1 << entropy_node_index);
        if (value) {
            bits_ |= (1 << entropy_node_index);
        }
    }
    void encode_one(bool value, TokenNode entropy_node_index) {
        encode_one(value, (int)entropy_node_index);
    }
    uint64_t bits()const {
        return bits_;
    }
    uint64_t liveness()const {
        return liveness_;
    }
};

inline uint16_t min_from_entropy_node_index(int index) {
    switch((TokenNode)index) {
      case TokenNode::EOB:
        return 0;
      case TokenNode::LENGTH0:
      case TokenNode::LENGTH1:
        return 1;
      case TokenNode::LENGTH2:
        return 2;
      case TokenNode::LENGTH3:
        return 4;
      case TokenNode::VAL0:
        return 2;
      case TokenNode::VAL1:
      case TokenNode::VAL2:
      case TokenNode::VAL3:
      case TokenNode::VAL4:
      case TokenNode::VAL5:
      case TokenNode::VAL6:
      case TokenNode::VAL7:
      case TokenNode::VAL8:
        return 1+ (1 << (1 + index - (int)TokenNode::VAL0));
      case TokenNode::NEGATIVE:
        return 1;
      case TokenNode::ZERO:
        return 0;
      default:
        assert(false && "Entropy node");
    }    
}

constexpr uint16_t max_from_entropy_node_index_inclusive(int /*index*/) {
    return 2048;
}
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
template <unsigned int length, uint16_t base_value_, uint16_t prob_offset_>
std::pair<uint64_t, uint64_t> TokenDecoder<length, base_value_, prob_offset_>::bits_and_liveness(const uint16_t value) {
    uint64_t bits = 0;
    uint64_t liveness = 0;
    assert( value >= base_value_ );
    uint16_t increment = value - base_value_;
    for ( uint8_t i = 0; i < length; i++ ) {
        uint64_t prob_offset_bit = 1ULL; // this needs to be 64 bit math here
        prob_offset_bit <<= (i + prob_offset);
        uint64_t bit_to_check = 1ULL;
        bit_to_check <<= (length - 1 - i);
        if (increment & bit_to_check) {
            bits |= prob_offset_bit;
        }
        liveness |= prob_offset_bit;
    }
    return {bits, liveness};
}

#endif
