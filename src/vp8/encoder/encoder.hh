#ifndef ENCODER_HH
#define ENCODER_HH
#include <cassert>


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

template <unsigned int length, uint16_t base_value_, uint16_t prob_offset_> template <class ProbabilityFunctor>
void TokenDecoder<length, base_value_, prob_offset_>::encode( BoolEncoder & encoder,
                                                              const uint16_t value,
                                                              const ProbabilityFunctor &probAt)
{
    assert( value >= base_value_ );
    uint16_t increment = value - base_value_;
    for ( uint8_t i = 0; i < length; i++ ) {
        uint64_t bit_to_check = 1ULL;
        bit_to_check <<= (length - 1 - i);

        encoder.put( increment & bit_to_check, probAt( i + prob_offset_, base_value, upper_limit ) );
    }
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
    
    assert(false &&"token encoder" && "value too large" );
    abort();
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


#endif /* ENCODER_HH */
