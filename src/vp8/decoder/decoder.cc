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
    bool decode_one(TokenNode index) {
        return decode_one((int)index);
    }
    bool decode_one(int index) {
        return decoder_->get((*this)(index,
                                     min_from_entropy_node_index(index),
                                     max_from_entropy_node_index_inclusive(index)));
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
typedef PerBitDecoderState<ExponentContext> DecoderStateExp;
typedef PerBitDecoderState<PerBitContext2u> DecoderState2u;
typedef PerBitDecoderState<PerBitContext4s> DecoderState4s;

template<class DecoderT> uint8_t get_ceil_log2_coefficient(DecoderT& d) {
    bool l0 = d.decode_one(TokenNode::LENGTH0);
    bool l1 = d.decode_one(TokenNode::LENGTH1);
    if (l0 == false && l1 == false) {
        return 1;
    }
    bool l2 = d.decode_one(TokenNode::LENGTH2);
    if (l0 == true && l1 == false && l2 == false) {
        return 2;
    }
    bool l3 = d.decode_one(TokenNode::LENGTH3);
    //uint8_t prefix_coded_length = (l0 ? 1 : 0) + (l1 ? 2 : 0) + (l2 ? 4 : 0) + (l3 ? 8 : 0);
    return 3 + (l2 ? 1 : 0) + (l3 ? 2 : 0) + (l0? 4 : 0);
}
template<class DecoderT> uint16_t get_one_natural_significand_coefficient(DecoderT& d, uint8_t ceil_log2) {
    uint8_t length = ceil_log2;
    uint16_t value = (1 << (length - 1));
    for (uint8_t i = 0; i + 1 < length; ++i) {
        value += (1 << i) * d.decode_one((int)TokenNode::VAL0 + i);
    }
    return value;
}
template<class DecoderT> uint16_t get_one_natural_coefficient(DecoderT& d) {
    uint8_t length = get_ceil_log2_coefficient(d);
    uint16_t value = (1 << (length - 1));
    for (uint8_t i = 0; i + 1 < length; ++i) {
        value += (1 << i) * d.decode_one((int)TokenNode::VAL0 + i);
    }
    return value;
}

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */
template<class DecoderT>int16_t get_one_signed_nonzero_coefficient( DecoderT &d )
{
    
    int16_t value = get_one_natural_coefficient(d);
    bool invert_sign = d.decode_one(TokenNode::NEGATIVE);
    return (invert_sign ? -value : value);
}

/* The unfolded token decoder is not pretty, but it is considerably faster
   than using a tree decoder */

template<class DecoderT> pair<bool, int16_t> get_one_signed_coefficient( DecoderT &d,
                                                                         const bool last_was_zero )
{
    
    // decode the token
    if ( not last_was_zero ) {
        if ( d.decode_one(TokenNode::EOB)) {
          // EOB
          return { true, 0 };
      }
    }

    if ( d.decode_one(TokenNode::ZERO)) {
      return { false, 0 };
    }
    int16_t value = get_one_natural_coefficient(d);
    bool invert_sign = d.decode_one(TokenNode::NEGATIVE);
    return {false, (invert_sign ? -value : value)};
}
template<class DecoderT> uint16_t get_one_unsigned_coefficient( DecoderT & d) {

    if ( d.decode_one(TokenNode::ZERO)) {
      return 0;
    }
    return get_one_natural_coefficient(d);
}
uint8_t prefix_unremap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v - 3;
}

void Block::parse_tokens( BoolDecoder & data,
                          ProbabilityTables & probability_tables, const BlockColorContext& context )
{

    auto & num_nonzeros_prob = probability_tables.nonzero_counts_7x7(type_, *this);
    uint8_t num_nonzeros_7x7 = 0;
    int decoded_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (data.get(num_nonzeros_prob.at(index).at(decoded_so_far))?1:0);
        num_nonzeros_7x7 |= (cur_bit << index);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    { // dc
        unsigned int coord = 0;
        uint8_t length = 0;
        auto & exp_prob = probability_tables.exponent_array_7x7(type_, coord, num_nonzeros_7x7, *this);
        unsigned int decoded_so_far = 0;
        for (int i = 3; i >= 0; --i) {
            int cur_bit = data.get(exp_prob.at(i).at(decoded_so_far)) ? 1 : 0;
            length |= (cur_bit << i);
            decoded_so_far <<= 1;
            decoded_so_far |= cur_bit;
            if (length == 0 && i == 2) break;
        }
        length = prefix_unremap(length);
        int16_t coef = (1 << (length - 1));
        if (length > 1){
            auto &res_prob = probability_tables.residual_noise_array_7x7(type_, coord, num_nonzeros_7x7);
            for (int i = length - 2; i >= 0; --i) {
                coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
            }
        }
        if (length != 0) {
            auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
            if (!data.get(sign_prob)) {
                coef = -coef;
            }
        }
        coefficients_.at( coord ) = coef;
    }

    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    for (unsigned int zz = 0; zz < 64; ++zz) {
        unsigned int coord = unzigzag[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        if (b_x > 0 && b_y > 0) { // this does the DC and the lower 7x7 AC
            uint8_t length = 0;
            auto & exp_prob = probability_tables.exponent_array_7x7(type_, coord, num_nonzeros_left_7x7, *this);
            unsigned int decoded_so_far = 0;
            for (int i = 3; i >= 0; --i) {
                int cur_bit = data.get(exp_prob.at(i).at(decoded_so_far)) ? 1 : 0;
                length |= (cur_bit << i);
                decoded_so_far <<= 1;
                decoded_so_far |= cur_bit;
                if (length == 0 && i == 2) break;
            }
            length = prefix_unremap(length);
            int16_t coef = (1 << (length - 1));
            if (length > 1){
                auto &res_prob = probability_tables.residual_noise_array_7x7(type_, coord, num_nonzeros_left_7x7);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
                if (!data.get(sign_prob)) {
                    coef = -coef;
                }
                if (coord != 0) {
                    --num_nonzeros_left_7x7;
                }
            }
            coefficients_.at( coord ) = coef;
            if (num_nonzeros_left_7x7 == 0) {
                break; // done with the 49x49
            }
        }
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    for (unsigned int coord = 0; coord < 64; ++coord) {    
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        if ((b_x > 0 && b_y > 0)) { // this does the DC and the lower 7x7 AC
            if (coefficients_.at( coord )){
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
        }
    }
    auto &prob_x = probability_tables.nonzero_counts_1x8(type_,
                                                      eob_x,
                                                         num_nonzeros_7x7, true);
    auto &prob_y = probability_tables.nonzero_counts_1x8(type_,
                                                      eob_y,
                                                         num_nonzeros_7x7, false);
    uint8_t num_nonzeros_x = 0;
    decoded_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_x.at(i).at(decoded_so_far))?1:0;
        num_nonzeros_x |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    uint8_t num_nonzeros_y = 0;
    decoded_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_y.at(i).at(decoded_so_far))?1:0;
        num_nonzeros_y |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    uint8_t num_nonzeros_left_x = num_nonzeros_x;
    uint8_t num_nonzeros_left_y = num_nonzeros_y;
    for (unsigned int zz = 1; zz < 64; ++zz) {    
        unsigned int coord = unzigzag[zz];        
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        uint8_t num_nonzeros_edge = 0;
        if (b_y == 0 && num_nonzeros_left_x) {
            num_nonzeros_edge = num_nonzeros_left_x;
        }
        if (b_x == 0 && num_nonzeros_left_y) {
            num_nonzeros_edge = num_nonzeros_left_y;
        }
        if ((b_x == 0 && num_nonzeros_left_y) || (b_y == 0 && num_nonzeros_left_x)) {
            auto &exp_array = probability_tables.exponent_array_x(type_, coord, num_nonzeros_edge, *this);

            uint8_t length = 0;
            unsigned int decoded_so_far = 0;            
            for (int i = 3; i >= 0; --i) {
                int cur_bit = (data.get(exp_array.at(i).at(decoded_so_far)) ? 1 : 0);
                length |= (cur_bit << i);
                decoded_so_far <<= 1;
                decoded_so_far |= cur_bit;
                if (i == 2 && !length) break;
            }
            length = prefix_unremap(length);
            int16_t coef = 0;
            if (length > 0) {
                coef = (1 << (length - 1));
            }
            if (length > 1){
                int min_threshold = 0;

                int max_val = probability_tables.get_max_value(coord);
                int max_len = bit_length(max_val);
                if (max_len > (int)RESIDUAL_NOISE_FLOOR) {
                    min_threshold = max_len - RESIDUAL_NOISE_FLOOR;
                }
                int i = length - 2;
                if (length - 2 >= min_threshold) {
                    auto &thresh_prob = probability_tables.residual_thresh_array(type_, coord, length,
                                                                                 *this, min_threshold, max_val);
                    uint16_t decoded_so_far = 1;
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (data.get(thresh_prob.at(decoded_so_far)) ? 1 : 0);
                        coef |= (cur_bit << i);
                        decoded_so_far <<= 1;
                        if (cur_bit) {
                            decoded_so_far |= 1;
                        }
                    }
                    probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far / 2);
                }
                for (; i >= 0; --i) {
                    auto &res_prob = probability_tables.residual_noise_array_x(type_, coord, num_nonzeros_edge);
                    coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
                if (!data.get(sign_prob)) {
                    coef = -coef;
                }
                if ( b_y == 0) {
                    --num_nonzeros_left_x;
                }
                if ( b_x == 0) {
                    --num_nonzeros_left_y;
                }
            }
            coefficients_.at( coord ) = coef;
        }
    }
    coefficients_.at( 0 ) = probability_tables.predict_or_unpredict_dc(*this, true);
    recalculate_coded_length();
}
