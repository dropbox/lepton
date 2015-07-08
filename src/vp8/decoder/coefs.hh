#ifndef _COEFS_HH
#define _COEFS_HH

#include "bool_decoder.hh"
#include "decoder.hh"
#include <limits>
class BoolEncoder;

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

template <uint64_t x, uint64_t n=32> struct static_log2 {
    enum uint64_t {
        c = ((x >> n ) > 0) ? 1 : 0
    };
    enum uint64_t {
        value = c * n + static_log2<(x >> (c * n)), n / 2>::value
    };
};
template <> struct static_log2 <1,0> {
    enum uint64_t {
        value = 0
    };
};
template <int n> struct static_ceil_log2 {
    enum uint64_t {
        value = (1 << static_log2<n>::value) < n ? static_log2<n>::value + 1 : static_log2<n>::value
    };
};

template<typename intt> intt log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    const intt b[] = {0x2,
                      0xC,
                      0xF0,
                      (intt)0xFF00,
                      (intt)0xFFFF0000U,
                      std::numeric_limits<intt>::max() - (intt)0xFFFFFFFFU};
    const intt S[] = {1, 2, 4, 8, 16, 32};

    register intt r = 0; // result of log2(v) will go here
    
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

template <int bits, int highest_likely_value> int skew_log(int number) {
    static_assert(static_ceil_log2<highest_likely_value>::value <= bits,
                  "The highest likely number must be less than the number of bits provided");
    if (number < highest_likely_value) {
        return number;
    }
    int offset = highest_likely_value - static_log2<highest_likely_value>::value;
    if (bits <= 8) {
        offset += log2<uint8_t>((uint8_t)number);
    } else if (bits <= 16) {
        offset += log2<uint16_t>((uint16_t)number);
    } else if (bits <= 32) {
        offset += log2<uint32_t>((uint32_t)number);
    } else {
        offset += log2<uint64_t>((uint64_t)number);
    }
    return std::min(offset, (1 << bits));
}


template<unsigned int prev_coef_contexts=PREV_COEF_CONTEXTS> int combine_priors(int16_t a, int16_t b) {
    const int max_likely_value = 6;
    int16_t al = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(a));
    int16_t bl = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(b));
    int retval = std::min(al + (1U << (static_ceil_log2<prev_coef_contexts-1>::value / 2)) * bl,
                          prev_coef_contexts-1);
    return retval;
}
inline int index_to_cat(int index) {
    return index;
/*
        int where = unzigzag[index];
        int x = where % 8;
        int y = where / 8;
        if (x == y) {
            return 1;
        }
        if (x == 0) {
            return 2;
        }
        if (y == 0) {
            return 3;
        }
        if (x > y) {
            return 4;
        }
        return 5;
*/
}



enum class TokenNodeNot : uint8_t {
    EOB = 0,
    ZERO = 1,
    ONE = 2,
    TWO_THREE_OR_FOUR = 3,
    TWO = 4,
    THREE = 5,
    FIVE_SIX_OR_ENSEMBLE1 =6,
    FIVE_SIX = 7,
    ENSEMBLE2_OR_ENSEMBLE3 = 8,
    ENSEMBLE2 = 9,
    ENSEMBLE4 = 10,
    FIVE = 11,
    POSITIVE = 12,
    BaseOffset // do not use
};

struct TokenDecoderEnsemble
{
    typedef TokenDecoder<2, 7, (uint16_t)TokenNodeNot::BaseOffset> TokenDecoder1;
    typedef TokenDecoder<3,
                         TokenDecoder1::base_value + (1 << TokenDecoder1::bits),
                         TokenDecoder1::prob_offset + TokenDecoder1::bits> TokenDecoder2;
    typedef TokenDecoder<5, 
                         TokenDecoder2::base_value + (1 << TokenDecoder2::bits),
                         TokenDecoder2::prob_offset + TokenDecoder2::bits> TokenDecoder3;
    typedef TokenDecoder<7,
                         TokenDecoder3::base_value + (1 << TokenDecoder3::bits),
                         TokenDecoder3::prob_offset + TokenDecoder3::bits> TokenDecoder4;
    typedef TokenDecoder<10,
                         TokenDecoder4::base_value + (1 << TokenDecoder4::bits),
                         TokenDecoder4::prob_offset + TokenDecoder4::bits> TokenDecoder5;
    TokenDecoder1 token_decoder_1;
    TokenDecoder2 token_decoder_2;
    TokenDecoder3 token_decoder_3;
    TokenDecoder4 token_decoder_4;
    TokenDecoder5 token_decoder_5;

  TokenDecoderEnsemble( );
};

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
    void encode_one(bool value, TokenNodeNot entropy_node_index) {
        encode_one(value, (int)entropy_node_index);
    }
    void encode_ensemble1(uint16_t value) {
        std::pair<uint64_t, uint64_t> bl = TokenDecoderEnsemble::TokenDecoder1::bits_and_liveness(value);
        bits_ |= bl.first;
        liveness_ |= bl.second;
    }
    void encode_ensemble2(uint16_t value) {
        std::pair<uint64_t, uint64_t> bl = TokenDecoderEnsemble::TokenDecoder2::bits_and_liveness(value);
        bits_ |= bl.first;
        liveness_ |= bl.second;
    }
    void encode_ensemble3(uint16_t value) {
        std::pair<uint64_t, uint64_t> bl = TokenDecoderEnsemble::TokenDecoder3::bits_and_liveness(value);
        bits_ |= bl.first;
        liveness_ |= bl.second;
    }
    void encode_ensemble4(uint16_t value) {
        std::pair<uint64_t, uint64_t> bl = TokenDecoderEnsemble::TokenDecoder4::bits_and_liveness(value);
        bits_ |= bl.first;
        liveness_ |= bl.second;
    }
    void encode_ensemble5(uint16_t value) {
        std::pair<uint64_t, uint64_t> bl = TokenDecoderEnsemble::TokenDecoder5::bits_and_liveness(value);
        bits_ |= bl.first;
        liveness_ |= bl.second;
    }
    uint64_t bits()const {
        return bits_;
    }
    uint64_t liveness()const {
        return liveness_;
    }
};

inline uint16_t min_from_entropy_node_index(int index) {
    switch(index) {
      case 0:
        return 0;
      case 1:
        return 0;
      case 12:
      case 2:
        return 1;
      case 3:
      case 4:
        return 2;
      case 5:
        return 3;
      case 6:
      case 7:
      case 11:
        return std::min((int)TokenDecoderEnsemble::TokenDecoder1::base_value, 5); // this was 5
      case 8:
      case 9:
        return TokenDecoderEnsemble::TokenDecoder1::upper_limit;
      case 10:
        return TokenDecoderEnsemble::TokenDecoder3::upper_limit;
      default:
        assert(false && "Entropy node");
    }    
}

constexpr uint16_t max_from_entropy_node_index_inclusive(int index) {
    return index == 11 ? 
        6
        : (index == 7 ?
           10
           : (index == 9 ?
              50
              :
              ((index == 4 || index == 5) ?
               4
               : 1024)));
}
enum BitContexts : uint8_t {
    CONTEXT_BIT_ZERO,
    CONTEXT_BIT_ONE,
    CONTEXT_LESS_THAN,
    CONTEXT_GREATER_THAN,
    CONTEXT_UNSET,
    NUM_BIT_CONTEXTS
};

BitContexts context_from_value_bits_id_min_max(Optional<int16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max);
BitContexts context_from_value_bits_id_min_max(Optional<uint16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max);

struct DefaultContext {
    FixedArray<Branch, ENTROPY_NODES> *prob_;

    DefaultContext(FixedArray<Branch, ENTROPY_NODES> *prob) :
        prob_(prob) {
    }
    Branch& operator()(unsigned int token_id, uint16_t /*min*/, uint16_t /*max*/) const {
        return prob_->at(token_id);
    }    
};

struct PerBitContext2u {

    Optional<uint16_t> left_value_;
    Optional<uint16_t> above_value_;

    BitsAndLivenessFromEncoding left_bits_;
    BitsAndLivenessFromEncoding above_bits_;

    typedef FixedArray<FixedArray<Branch,
                                NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                      ENTROPY_NODES > NestedProbabilityArray;
    NestedProbabilityArray *probability_;
public:
    Branch& operator()(unsigned int token_id, uint16_t min, uint16_t max) const {
        uint8_t left_context = context_from_value_bits_id_min_max(left_value_,
                                                                  left_bits_,
                                                                  token_id, min, max);
        uint8_t above_context = context_from_value_bits_id_min_max(above_value_,
                                                                   above_bits_,
                                                                   token_id, min, max);
        return probability_->at(token_id).at(left_context + (above_context * NUM_BIT_CONTEXTS));
    }
    PerBitContext2u(NestedProbabilityArray  *prob,
                    Optional<uint16_t> left_coded_length,
                    Optional<uint16_t> above_coded_length);

};

struct PerBitContext4s {

    Optional<int16_t> left_block_value_;
    Optional<int16_t> above_block_value_;

    Optional<int16_t> left_coef_value_;
    Optional<int16_t> above_coef_value_;

    BitsAndLivenessFromEncoding left_block_bits_;
    BitsAndLivenessFromEncoding above_block_bits_;

    BitsAndLivenessFromEncoding left_coef_bits_;
    BitsAndLivenessFromEncoding above_coef_bits_;

    typedef FixedArray<FixedArray<FixedArray<Branch,
                                          NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                                NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                      ENTROPY_NODES > NestedProbabilityArray;
    NestedProbabilityArray *probability_;
public:
    Branch& operator()(unsigned int token_id, uint16_t min, uint16_t max) const {
        uint8_t left_block_context = context_from_value_bits_id_min_max(left_block_value_,
                                                                        left_block_bits_,
                                                                        token_id, min, max);
        uint8_t above_block_context = context_from_value_bits_id_min_max(above_block_value_,
                                                                         above_block_bits_,
                                                                         token_id, min, max);

        uint8_t left_coef_context = context_from_value_bits_id_min_max(left_coef_value_,
                                                                       left_coef_bits_,
                                                                       token_id, min, max);
        uint8_t above_coef_context = context_from_value_bits_id_min_max(above_coef_value_,
                                                                        above_coef_bits_,
                                                                        token_id, min, max);

        return probability_->at(token_id).at(left_block_context
                                             + (above_block_context * 5)).at(left_coef_context
                                                                             + (above_coef_context * 5));
    }
    PerBitContext4s(NestedProbabilityArray  *prob,
                    Optional<int16_t> left_block_value,
                    Optional<int16_t> above_block_value,
                    Optional<int16_t> left_coef_value,
                    Optional<int16_t> above_coef_value);

};

#endif /* TOKENS_HH */
