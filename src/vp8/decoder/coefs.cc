#include "coefs.hh"
#include "bool_decoder.hh"
#include "fixed_array.hh"
#include "model.hh"
#include "block.hh"
#include "context.hh"
using namespace std;


template <unsigned int length, uint16_t base_value_, uint16_t prob_offset_> template <class ProbabilityFunctor>
uint16_t TokenDecoder<length, base_value_, prob_offset_>::decode( BoolDecoder & data, const ProbabilityFunctor& probAt)
{
  uint16_t increment = 0;
  for ( uint8_t i = 0; i < length; i++ ) {
      increment = ( increment << 1 ) + data.get( probAt( i + prob_offset_, base_value, upper_limit ) );
  }
  return base_value_ + increment;
}



template <class ParentContext> struct PerBitDecoderState : public ParentContext{
    BoolDecoder * decoder_;

    template<typename... Targs> PerBitDecoderState(BoolDecoder *decoder,
                                                   Targs&&... Fargs) :
        ParentContext(std::forward<Targs>(Fargs)...),
        decoder_(decoder) {
    }
    bool decode_one(TokenNodeNot index) {
        return decode_one((int)index);
    }
    bool decode_one(int index) {
        return decoder_->get((*this)(index,
                                     min_from_entropy_node_index(index),
                                     max_from_entropy_node_index_inclusive(index)));
    }
    uint16_t decode_ensemble1() {
        return TokenDecoderEnsemble::TokenDecoder1::decode(*decoder_, *this);
    }
    uint16_t decode_ensemble2() {
        return TokenDecoderEnsemble::TokenDecoder2::decode(*decoder_, *this);
    }
    uint16_t decode_ensemble3() {
        return TokenDecoderEnsemble::TokenDecoder3::decode(*decoder_, *this);
    }
    uint16_t decode_ensemble4() {
        return TokenDecoderEnsemble::TokenDecoder4::decode(*decoder_, *this);
    }
    uint16_t decode_ensemble5() {
        return TokenDecoderEnsemble::TokenDecoder5::decode(*decoder_, *this);
    }
};


PerBitContext2u::PerBitContext2u(NestedProbabilityArray  *prob,
                                 Optional<uint16_t> left_coded_length,
                                 Optional<uint16_t> above_coded_length)
   : left_value_(left_coded_length),
     above_value_(above_coded_length) {
    put_one_unsigned_coefficient(left_bits_, left_coded_length.get_or(0));
    put_one_unsigned_coefficient(above_bits_, above_coded_length.get_or(0));
    
    probability_ = prob;
}

PerBitContext4s::PerBitContext4s(NestedProbabilityArray  *prob,
                                 Optional<int16_t> left_block_value,
                                 Optional<int16_t> above_block_value,
                                 Optional<int16_t> left_coef_value,
                                 Optional<int16_t> above_coef_value)
    : left_block_value_(left_block_value),
     above_block_value_(above_block_value),
     left_coef_value_(left_coef_value),
     above_coef_value_(above_coef_value) {
    put_one_signed_coefficient(left_block_bits_, false, false, left_block_value.get_or(0));
    put_one_signed_coefficient(above_block_bits_, false, false, above_block_value.get_or(0));
    put_one_signed_coefficient(left_coef_bits_, false, false, left_coef_value.get_or(0));
    put_one_signed_coefficient(above_coef_bits_, false, false, above_coef_value.get_or(0));
    
        probability_ = prob;
}

typedef PerBitDecoderState<DefaultContext> DecoderState;
typedef PerBitDecoderState<PerBitContext2u> DecoderState2u;
typedef PerBitDecoderState<PerBitContext4s> DecoderState4s;


template<class DecoderT> uint16_t get_one_natural_coefficient(DecoderT& d) {
    uint16_t value = 0;
    if ( not d.decode_one(TokenNodeNot::ONE) ) {
        value = 1;
    } else {
        if ( not d.decode_one(TokenNodeNot::TWO_THREE_OR_FOUR) ) {
            if ( not d.decode_one(TokenNodeNot::TWO) ) {
                value = 2;
            } else {
                if ( not d.decode_one(TokenNodeNot::THREE)) {
                    value = 3;
                } else {
                    value = 4;
                }
            }
        } else {
            if ( not d.decode_one(TokenNodeNot::FIVE_SIX_OR_ENSEMBLE1)) {
                if ( not d.decode_one(TokenNodeNot::FIVE_SIX)) {
                    value = (d.decode_one(TokenNodeNot::FIVE) ? 6 : 5); /* new bit! */
                } else {
                    value = d.decode_ensemble1();
                }
            } else {
                if ( not d.decode_one(TokenNodeNot::ENSEMBLE2_OR_ENSEMBLE3)) {
                    if ( not d.decode_one(TokenNodeNot::ENSEMBLE2)) {
                        value = d.decode_ensemble2();
                    } else {
                        value = d.decode_ensemble3();
                    }
                } else {
                    if ( not d.decode_one(TokenNodeNot::ENSEMBLE4)) {
                        value = d.decode_ensemble4();
                    } else {
                        value = d.decode_ensemble5();
                    }
                }
            }
        }
    }

    return value;
}

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */

template<class DecoderT> pair<bool, int16_t> get_one_signed_coefficient( DecoderT &d,
                                                                         const bool last_was_zero )
{
    
    // decode the token
    if ( not last_was_zero ) {
        if ( not d.decode_one(TokenNodeNot::EOB)) {
          // EOB
          return { true, 0 };
      }
    }

    if ( not d.decode_one(TokenNodeNot::ZERO)) {
      return { false, 0 };
    }
    bool invert_sign = d.decode_one(TokenNodeNot::POSITIVE);
    int16_t value = get_one_natural_coefficient(d);
    return {false, (invert_sign ? -value : value)};
}
template<class DecoderT> uint16_t get_one_unsigned_coefficient( DecoderT & d) {

    if ( not d.decode_one(TokenNodeNot::ZERO)) {
      return 0;
    }
    return get_one_natural_coefficient(d);
}

void Block::parse_tokens( BoolDecoder & data,
			  ProbabilityTables & probability_tables )
{
  /* read which EOB bin we're in */
  uint8_t above_num_zeros = context().above.initialized() ? context().above.get()->num_zeros() : 0;
  uint8_t left_num_zeros = context().left.initialized() ? context().left.get()->num_zeros() : 0;

  assert( above_num_zeros < NUM_ZEROS_BINS );
  assert( left_num_zeros < NUM_ZEROS_BINS );

  Optional<uint16_t> above_coded_length;
  Optional<uint16_t> left_coded_length;
  if (context().left.initialized()) {
      left_coded_length = context().left.get()->coded_length();
      assert( left_coded_length.get() < NUM_ZEROS_EOB_PRIORS );
  }
  if (context().above.initialized()) {
      above_coded_length = context().above.get()->coded_length();
      assert( above_coded_length.get() < NUM_ZEROS_EOB_PRIORS );
  }


  
  FixedArray<Branch, LOG_NUM_ZEROS_BINS> & num_zeros_prob
      = probability_tables.num_zeros_array(left_num_zeros,
                                           above_num_zeros,
                                           left_coded_length.get_or(0),
                                           above_coded_length.get_or(0));

  static_assert(NUM_ZEROS_BINS == 64/Block::BLOCK_SLICE, "Block constants must match decoder");
  uint16_t num_zeros = 0;
  for (unsigned int i = 0; i < LOG_NUM_ZEROS_BINS; ++i) {
      num_zeros |= data.get( num_zeros_prob.at(i) ) * ( 1 << i);
  }
  DecoderState2u::NestedProbabilityArray & eob_prob = probability_tables.eob_array( num_zeros );
  DecoderState2u eob_decoder_state(&data, &eob_prob,
                                   left_coded_length.initialized()
                                   ? left_coded_length.get() / (64/EOB_BINS) : left_coded_length,
                                   above_coded_length.initialized()
                                   ? above_coded_length.get() / (64/EOB_BINS) : above_coded_length);
  uint16_t eob_bin = get_one_unsigned_coefficient( eob_decoder_state );

  if ( eob_bin >= int(EOB_BINS) ) {
    throw runtime_error( "invalid eob value " + to_string( eob_bin ) );
  }
  
  bool last_was_zero = false;

  for ( unsigned int index = 0;
	index < 64;
	index++ ) {
    /* select the tree probabilities based on the prediction context */
    uint8_t token_context = 0;
    Optional<int16_t> above_neighbor_context;
    Optional<int16_t> left_neighbor_context;
    if ( context().left.initialized() ) {
        left_neighbor_context = Optional<int16_t>(context().left.get()->coefficients().at( jpeg_zigzag.at( index ) ));
    }
    if ( context().above.initialized() ) {
        above_neighbor_context = context().above.get()->coefficients().at( jpeg_zigzag.at( index ) );
    }
    uint16_t neighbor_context = std::min(8, skew_log<3, 4>((abs(above_neighbor_context.get_or(0))
                                                            + abs(left_neighbor_context.get_or(0))) / 2));

    Optional<int16_t> left_coef;
    Optional<int16_t> above_coef;
    uint8_t coord = jpeg_zigzag.at( index );
    if (index > 1) {
        token_context = 0;
        if (coord % 8 == 0) {
            above_coef = coefficients().at( coord - 8);
            token_context = 1 + std::min(7, skew_log<3, 2>(abs(above_coef.get_or(0))));
        } else if (coord > 8) {
            left_coef = coefficients().at( coord - 1);
            above_coef = coefficients().at( coord - 8);
            uint8_t coord_x = coord % 8;
            uint8_t coord_y = coord / 8;
            if (coord_x > coord_y) {
                token_context = 1 + std::min(7,skew_log<3, 2>(abs(left_coef.get_or(0))));// + 8 * std::min(skew_log<3, 2>(abs(coefficients().at( coord - 8))),7);
            } else {
                token_context = 1 + std::min(7, skew_log<3, 2>(abs(above_coef.get_or(0))));
            }
        } else {
            left_coef = coefficients().at( coord - 1);
            token_context = 1 + std::min(7, skew_log<3, 2>(abs(left_coef.get_or(0))));
        }
    }
    
/*
    if (index > 1) {
        token_context = 0;
        if (index % 8 == 0) {
            token_context = 1 + coefficients().at( jpeg_zigzag.at( index ) - 8);
        } else if (index > 8) {
            token_context += 1 + combine_priors(coefficients().at( jpeg_zigzag.at( index ) - 1),
                                                coefficients().at( jpeg_zigzag.at( index ) - 8) );
        } else {
            token_context = 1 + coefficients().at( jpeg_zigzag.at( index ) - 1);
        }
    }
*/
    (void) token_context;
    (void) neighbor_context;
    auto & prob = probability_tables.branch_array( std::min((unsigned int)type_, BLOCK_TYPES - 1),
                                                   num_zeros,
                                                   eob_bin,
                                                   index_to_cat(index));
    DecoderState4s dct_decoder_state(&data, &prob,
                                     left_neighbor_context, above_neighbor_context,
                                     left_coef, above_coef);
    bool eob = false;
    int16_t value;
    tie( eob, value ) = get_one_signed_coefficient( dct_decoder_state, last_was_zero );
    if ( eob ) {
      break;
    }
    last_was_zero = value == 0;

    /* assign to block storage */
    coefficients_.at( jpeg_zigzag.at( index ) ) = value;
  }

  recalculate_coded_length();
}
