#include "bool_decoder.hh"
#include "boolreader.hh"
#include "model.hh"
#include "block.hh"
#include "weight.hh"
using namespace std;


uint8_t prefix_unremap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v - 3;
}

template<bool has_left, bool has_above, bool has_above_right, BlockType color>
void parse_tokens( BlockContext context,
                   BoolDecoder & data,
                   ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables) {
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(context.copy());
    uint8_t num_nonzeros_7x7 = 0;
    int decoded_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (data.get(num_nonzeros_prob.at(index, decoded_so_far))?1:0);
        num_nonzeros_7x7 |= (cur_bit << index);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    ProbabilityTablesBase::CoefficientContext prior = probability_tables.get_dc_coefficient_context(context.copy(),
                                                                                                    num_nonzeros_7x7);
    { // dc
        const unsigned int coord = 0;
        uint8_t length = 0;
        auto exp_prob = probability_tables.exponent_array_dc(prior);
        length = 10;
        for (int i = 0; i < length; ++i) {
            bool cur_bit = data.get(exp_prob.at(i));
            if (!cur_bit) {
                length = i;
                break;
            }
        }
        int16_t coef = (1 << (length - 1));
        if (length > 1){
            auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
            for (int i = length - 2; i >= 0; --i) {
                coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
            }
        }
        if (length != 0) {
            auto &sign_prob = probability_tables.sign_array(coord, prior);
            if (!data.get(sign_prob)) {
                coef = -coef;
            }
        }
        context.here().mutable_coefficients().memset(0);
        context.here().mutable_coefficients().raster( coord ) = coef;
    }

    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    for (unsigned int zz = 0; zz < 49; ++zz) {
        unsigned int coord = unzigzag49[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = (coord >> 3);
        assert(b_x > 0 && b_y > 0 && "this does the DC and the lower 7x7 AC");
        {
            probability_tables.update_coefficient_context7x7(prior, coord, context.copy(), num_nonzeros_left_7x7);
            auto exp_prob = probability_tables.exponent_array_7x7(coord, zz, prior);
            uint8_t length = MAX_EXPONENT;
            for (unsigned int i = 0; i < MAX_EXPONENT; ++i) {
                bool cur_bit = data.get(exp_prob.at(i));
                if (!cur_bit) {
                    length = i;
                    break;
                }
            }

            int16_t coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(coord, prior);
                if (!data.get(sign_prob)) {
                    coef = -coef;
                }
                if (coord != 0) {
                    --num_nonzeros_left_7x7;
                }
            }
            context.here().mutable_coefficients().raster( coord ) = coef;
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
            if (context.here().coefficients().raster( coord )){
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
        }
    }
    auto prob_x = probability_tables.x_nonzero_counts_8x1(
                                                      eob_x,
                                                         num_nonzeros_7x7);
    auto prob_y = probability_tables.y_nonzero_counts_1x8(
                                                      eob_y,
                                                         num_nonzeros_7x7);
    uint8_t num_nonzeros_x = 0;
    decoded_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_x.at(i, decoded_so_far))?1:0;
        num_nonzeros_x |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    uint8_t num_nonzeros_y = 0;
    decoded_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_y.at(i, decoded_so_far))?1:0;
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
            probability_tables.update_coefficient_context8(prior, coord, context.copy(), num_nonzeros_edge);
            auto exp_array = probability_tables.exponent_array_x(coord, prior);

            uint8_t length = MAX_EXPONENT;
            for (unsigned int i = 0; i < MAX_EXPONENT; ++i) {
                bool cur_bit = data.get(exp_array.at(i));
                if (!cur_bit) {
                    length = i;
                    break;
                }
            }
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
                    auto thresh_prob = probability_tables.residual_thresh_array(coord, length,
                                                                                 prior, min_threshold, max_val);
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
                auto res_prob = probability_tables.residual_noise_array_x(coord, prior);
                for (; i >= 0; --i) {
                    coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(coord, prior);
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
            context.here().mutable_coefficients().raster( coord ) = coef;
        }
    }
    context.here().mutable_coefficients().raster( 0 ) = probability_tables.predict_or_unpredict_dc(context.copy(), true);
    context.here().recalculate_coded_length();
}

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Cr>&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Cr>&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Y>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Cr>&);

