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
        uint8_t length;
        bool nonzero = false;
        auto exp_prob = probability_tables.exponent_array_dc(prior);
        auto *exp_branch = exp_prob.begin();
        for (length = 0; length < MAX_EXPONENT; ++length) {
            bool cur_bit = data.get(*exp_branch++);
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            auto &sign_prob = probability_tables.sign_array_dc(prior);
            bool neg = !data.get(sign_prob);
        

            coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        context.here().coef.memset(0);
        context.here().dc() = coef;
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    uint8_t num_nonzeros_lag_left_7x7 = num_nonzeros_left_7x7;
    for (unsigned int zz = 0; zz < 49; ++zz) {
        if ((zz & 3) == 0) {
            num_nonzeros_lag_left_7x7 = num_nonzeros_left_7x7;
            if (num_nonzeros_lag_left_7x7 ==0) {
                break;
            }
        }
        unsigned int coord = unzigzag49[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = (coord >> 3);
        assert((coord & 7) > 0 && (coord >> 3) > 0 && "this does the DC and the lower 7x7 AC");
        {
            probability_tables.update_coefficient_context7x7(zz, prior, context.copy(), num_nonzeros_lag_left_7x7);
            auto exp_prob = probability_tables.exponent_array_7x7(coord, zz, prior);
            uint8_t length;
            bool nonzero = false;
            auto exp_branch = exp_prob.begin();
            for (length = 0; length != MAX_EXPONENT; ++length) {
                bool cur_bit = data.get(*exp_branch++);
                if (!cur_bit) {
                    break;
                }
                nonzero = true;
            }
            int16_t coef = 0;
            bool neg = false;
            if (nonzero) {
                --num_nonzeros_left_7x7;
                auto &sign_prob = probability_tables.sign_array_7x7(coord, prior);
                neg = !data.get(sign_prob);
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
                coef = (1 << (length - 1));
                if (length > 1){
                    auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                    for (int i = length - 2; i >= 0; --i) {
                        coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                    }
                }
                if (neg) {
                    coef = -coef;
                }
            }
            context.here().coef.at(zz + AlignedBlock::AC_7x7_INDEX) = coef;
            if (num_nonzeros_left_7x7 == 0) {
                break; // done with the 49x49
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
/*
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_x.at(i, decoded_so_far))?1:0;
        num_nonzeros_x |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
*/
    uint8_t num_nonzeros_y = 0;
    decoded_so_far = 0;
/*
    for (int i= 2; i >=0; --i) {
        int cur_bit = data.get(prob_y.at(i, decoded_so_far))?1:0;
        num_nonzeros_y |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
*/
    bool run_ends_early_x = data.get(prob_x.at(0, 0))?1:0;
    bool run_ends_early_y = data.get(prob_y.at(0, 0))?1:0;
    uint8_t aligned_block_offset = AlignedBlock::ROW_X_INDEX;
    for (uint8_t delta = 1, zig15offset = 0, num_nonzeros_edge = num_nonzeros_x; ; delta = 8,
             zig15offset = 7,
             num_nonzeros_edge = num_nonzeros_y,
             aligned_block_offset = AlignedBlock::ROW_Y_INDEX) {
        unsigned int coord = delta;
        uint8_t num_nonzeros_edge_left = num_nonzeros_edge;
        bool run_ends_early = delta == 1 ? run_ends_early_x : run_ends_early_y;
        for (int xx = 0; xx < 7 && (xx < 3 || !run_ends_early); ++xx, coord += delta, ++zig15offset) {
            probability_tables.update_coefficient_context8(prior, coord, context.copy(), delta == 1 ? eob_x : eob_y);
            auto exp_array = probability_tables.exponent_array_x(coord, zig15offset, prior);

            uint8_t length;
            bool nonzero = false;
            auto * exp_branch = exp_array.begin();
            for (length = 0; length != MAX_EXPONENT; ++length) {
                bool cur_bit = data.get(*exp_branch++);
                if (!cur_bit) {
                    break;
                }
                nonzero = true;
            }
            int16_t coef = 0;
            if (nonzero) {
                uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
                auto &sign_prob = probability_tables.sign_array_8(coord, prior);
                --num_nonzeros_edge_left;
                bool neg = !data.get(sign_prob);
                coef = (1 << (length - 1));
                if (length > 1){
                    int i = length - 2;
                    if (length - 2 >= min_threshold) {
                        auto thresh_prob = probability_tables.residual_thresh_array(coord, length,
                                                                                    prior, min_threshold,
                                                                                    probability_tables.get_max_value(coord));
                        uint16_t decoded_so_far = 1;
                        for (; i >= min_threshold; --i) {
                            int cur_bit = (data.get(thresh_prob.at(decoded_so_far)) ? 1 : 0);
                            coef |= (cur_bit << i);
                            decoded_so_far <<= 1;
                            if (cur_bit) {
                                decoded_so_far |= 1;
                            }
                        }
                        probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
                    }
                    auto res_prob = probability_tables.residual_noise_array_x(coord, prior);
                    for (; i >= 0; --i) {
                        coef |= ((data.get(res_prob.at(i)) ? 1 : 0) << i);
                    }
                }
                if (neg) {
                    coef = -coef;
                }
            }
            context.here().coef.at(aligned_block_offset + xx) = coef;
        }
        if (delta == 8) {
            break;
        }
    }
    context.here().mutable_coefficients_raster( 0 ) = probability_tables.predict_or_unpredict_dc(context.copy(), true);
    context.here().recalculate_coded_length(num_nonzeros_7x7, num_nonzeros_x, num_nonzeros_y);
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

