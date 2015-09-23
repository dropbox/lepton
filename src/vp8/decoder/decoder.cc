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
void decode_edge(BlockContext context,
                 Sirikata::Array1d<BoolDecoder, 4> & decoder,
                 ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase::CoefficientContext prior) {

    uint8_t aligned_block_offset = AlignedBlock::ROW_X_INDEX;
    auto prob_early_exit = probability_tables.x_nonzero_counts_8x1(eob_x,
                                                                   num_nonzeros_7x7);
    uint8_t est_eob = eob_x;
    for (uint8_t delta = 1, zig15offset = 0; ; delta = 8,
         zig15offset = 7,
         est_eob = eob_y,
         aligned_block_offset = AlignedBlock::ROW_Y_INDEX,
         prob_early_exit = probability_tables.y_nonzero_counts_1x8(eob_y,
                                                                   num_nonzeros_7x7)) {
             unsigned int coord = delta;
             int run_ends_early = decoder.at(0).get(prob_early_exit.at(0, 0))? 1 : 0;
             int lane = 0, lane_end = 3;
             for (int vec = 0; vec <= !run_ends_early; ++vec, lane_end = 7) {
                 for (; lane < lane_end; ++lane, coord += delta, ++zig15offset) {
                     probability_tables.update_coefficient_context8(prior, coord, context.copy(), est_eob);
                     auto exp_array = probability_tables.exponent_array_x(coord, zig15offset, prior);
                     uint8_t length;
                     bool nonzero = false;
                     auto * exp_branch = exp_array.begin();
                     for (length = 0; length != MAX_EXPONENT; ++length) {
                         bool cur_bit = decoder.at((lane + 1) & 3).get(*exp_branch++);
                         if (!cur_bit) {
                             break;
                         }
                         nonzero = true;
                     }
                     int16_t coef = 0;
                     if (nonzero) {
                         uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
                         auto &sign_prob = probability_tables.sign_array_8(coord, prior);
                         bool neg = !decoder.at((lane + 1) & 3).get(sign_prob);
                         coef = (1 << (length - 1));
                         if (length > 1){
                             int i = length - 2;
                             if (length - 2 >= min_threshold) {
                                 auto thresh_prob = probability_tables.residual_thresh_array(coord, length,
                                                                                             prior, min_threshold,
                                                                                             probability_tables.get_max_value(coord));
                                 uint16_t decoded_so_far = 1;
                                 for (; i >= min_threshold; --i) {
                                     int cur_bit = (decoder.at((lane + 1) & 3).get(thresh_prob.at(decoded_so_far)) ? 1 : 0);
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
                                 coef |= ((decoder.at((lane + 1) & 3).get(res_prob.at(i)) ? 1 : 0) << i);
                             }
                         }
                         if (neg) {
                             coef = -coef;
                         }
                     }
                     context.here().coef.at(aligned_block_offset + lane) = coef;
                 }
             }
             if (delta == 8) {
                 break;
             }
         }
        context.here().mutable_coefficients_raster( 0 ) = probability_tables.predict_or_unpredict_dc(context.copy(), true);
}


             //VECTORIZE HERE
             //the first of the two vectorized items will be
             // a vector of [run_ends_early, lane0, lane1, lane2]
             // if run_ends_early is false then the second set of 4 items will be
             // a vector of [lane3, lane4, lane5, lane6]



template<bool has_left, bool has_above, bool has_above_right, BlockType color>
void parse_tokens( BlockContext context,
                   Sirikata::Array1d<BoolDecoder, 4> & decoder,
                   ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables) {
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(context.copy());
    uint8_t num_nonzeros_7x7 = 0;
    int decoded_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (decoder.at(3).get(num_nonzeros_prob.at(index, decoded_so_far))?1:0);
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
            bool cur_bit = decoder.at(0).get(*exp_branch++);
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            auto &sign_prob = probability_tables.sign_array_dc(prior);
            bool neg = !decoder.at(0).get(sign_prob);
        

            coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((decoder.at(0).get(res_prob.at(i)) ? 1 : 0) << i);
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
        // VECTORIZE HERE (zz += 4 rather than ++zz)
        // this is a perfectly ordinary vectorization task if num_nonzeros_lag_left >= 4
        // however if num_nonzeros_lag_left == 3, 2, 1 or 0, we should probably start with a
        // scalar vectorized bool decoder...and if that works then we can look into making
        // a bool_decoder that speculatively tries 4 gets and cancels out if too many
        // are nonzero. It would probably also need to cancel out if any need to enter the
        // vpx_fill_buffer branch and load things from the streams--in those cases it could
        // fall back to (slow) scalar code.
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
                bool cur_bit = decoder.at(zz & 3).get(*exp_branch++);
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
                neg = !decoder.at(zz & 3).get(sign_prob);
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
                coef = (1 << (length - 1));
                if (length > 1){
                    auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                    for (int i = length - 2; i >= 0; --i) {
                        coef |= ((decoder.at(zz & 3).get(res_prob.at(i)) ? 1 : 0) << i);
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
    decode_edge(context,
                decoder,
                probability_tables,
                num_nonzeros_7x7, eob_x, eob_y,
                prior);
    context.here().recalculate_coded_length(num_nonzeros_7x7);
}

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4> &, ProbabilityTables<false, false, false, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, false, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, false, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, false, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, true, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, true, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<false, true, true, BlockType::Cr>&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, true, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, true, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, true, BlockType::Cr>&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, false, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, true, false, BlockType::Cr>&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, false, false, BlockType::Y>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, false, false, BlockType::Cb>&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>&, ProbabilityTables<true, false, false, BlockType::Cr>&);

