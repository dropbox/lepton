#ifndef _VP8_MODEL_NUMERIC_HH_
#define _VP8_MODEL_NUMERIC_HH_
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

template<class EncoderT> void put_one_natural_coefficient( EncoderT &e,
                                                           const uint16_t token_value )
{
    if ( token_value == 1 ) {
        e.encode_one(false, TokenNodeNot::ONE);
        return;
    }

    e.encode_one(true, TokenNodeNot::ONE);

    if ( token_value == 2 ) {
        e.encode_one(false, TokenNodeNot::TWO_THREE_OR_FOUR);
        e.encode_one(false, TokenNodeNot::TWO);
        return;
    }
    
    if ( token_value == 3 ) {
        e.encode_one(false, TokenNodeNot::TWO_THREE_OR_FOUR);
        e.encode_one(true, TokenNodeNot::TWO);
        e.encode_one(false, TokenNodeNot::THREE);
        return;
    }

    if ( token_value == 4 ) {
        e.encode_one(false, TokenNodeNot::TWO_THREE_OR_FOUR);
        e.encode_one(true, TokenNodeNot::TWO);
        e.encode_one(true, TokenNodeNot::THREE);
        return;
    }
    e.encode_one(true, TokenNodeNot::TWO_THREE_OR_FOUR);

    if ( token_value < TokenDecoderEnsemble::TokenDecoder1::base_value ) { /* category 1, 5..6 */
        e.encode_one(false, TokenNodeNot::FIVE_SIX_OR_ENSEMBLE1);
        e.encode_one(false, TokenNodeNot::FIVE_SIX);
        e.encode_one(token_value == 5 ? false : true, TokenNodeNot::FIVE);
        return;
    }

    if ( token_value < TokenDecoderEnsemble::TokenDecoder1::upper_limit ) { /* category 2, 7..10 */
        e.encode_one(false, TokenNodeNot::FIVE_SIX_OR_ENSEMBLE1);
        e.encode_one(true, TokenNodeNot::FIVE_SIX);
        e.encode_ensemble1(token_value);
        return;
    }

    e.encode_one( true, TokenNodeNot::FIVE_SIX_OR_ENSEMBLE1);

    if ( token_value < TokenDecoderEnsemble::TokenDecoder2::upper_limit ) { /* category 3, 11..18 */
        e.encode_one( false, TokenNodeNot::ENSEMBLE2_OR_ENSEMBLE3);
        e.encode_one( false, TokenNodeNot::ENSEMBLE2);
        e.encode_ensemble2(token_value);
        return;
    }

    if ( token_value < TokenDecoderEnsemble::TokenDecoder3::upper_limit ) { /* category 4, 19..34 */
        e.encode_one( false, TokenNodeNot::ENSEMBLE2_OR_ENSEMBLE3);
        e.encode_one( true, TokenNodeNot::ENSEMBLE2);
        e.encode_ensemble3(token_value);
        return;
    }

    e.encode_one( true, TokenNodeNot::ENSEMBLE2_OR_ENSEMBLE3);

    if ( token_value < TokenDecoderEnsemble::TokenDecoder4::upper_limit ) { /* category 5, 35..66 */
        e.encode_one( false, TokenNodeNot::ENSEMBLE4);
        e.encode_ensemble4(token_value);
        return;
    }
    
    if ( token_value < TokenDecoderEnsemble::TokenDecoder5::upper_limit ) { /* category 6, 67..2048 */
        e.encode_one( true, TokenNodeNot::ENSEMBLE4);
        e.encode_ensemble5(token_value);
        return;
    }
    
    assert(false && "token encoder; value too large" );
}
template<class EncoderT> void put_one_signed_coefficient( EncoderT &e,
                                 const bool last_was_zero,
                                 bool eob,
                                 const int16_t coefficient )
{
  const bool coefficient_sign = coefficient < 0;

  if ( eob ) {
    assert( not last_was_zero );
    e.encode_one( false, TokenNodeNot::EOB );
    return;
  }
  
  if ( not last_was_zero ) {
    e.encode_one( true, TokenNodeNot::EOB );
  }

  if ( coefficient == 0 ) {
      e.encode_one( false, TokenNodeNot::ZERO );
    return;
  }

  e.encode_one( true, TokenNodeNot::ZERO );
  e.encode_one( coefficient_sign, TokenNodeNot::POSITIVE );
  put_one_natural_coefficient(e, abs(coefficient));
}

template<class EncoderT> void put_one_signed_nonzero_coefficient( EncoderT &e,
                                 const int16_t coefficient )
{
    const bool coefficient_sign = coefficient < 0;
    e.encode_one( coefficient_sign, TokenNodeNot::POSITIVE );
    put_one_natural_coefficient(e, abs(coefficient));
}

template<class EncoderT> void put_one_unsigned_coefficient( EncoderT &e,
                                                            const uint16_t coefficient )
{

  if ( coefficient == 0 ) {
      e.encode_one( false, TokenNodeNot::ZERO );
      return;
  }

  e.encode_one( true, TokenNodeNot::ZERO );
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
