#ifndef ENCODER_HH
#define ENCODER_HH
#include <cassert>


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




#endif /* ENCODER_HH */
