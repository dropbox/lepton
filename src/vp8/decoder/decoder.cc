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
void decode_edge(BlockContext mcontext,
                 Sirikata::Array1d<BoolDecoder, 4>::Slice decoder,
                 ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase::CoefficientContext input_prior,
                 ProbabilityTablesBase& pt) {
    ConstBlockContext context = mcontext.copy();
    uint8_t aligned_block_offset = raster_to_aligned.at(1);
    auto prob_early_exit = probability_tables.x_nonzero_counts_8x1(pt, eob_x,
                                                                   num_nonzeros_7x7);
    uint8_t est_eob = eob_x;
    ProbabilityTablesBase::CoefficientContext prior[4] = {};
    if (ProbabilityTablesBase::VECTORIZE) {
        prior[1] = probability_tables.update_coefficient_context8_templ1(context, eob_x);
        prior[2] = probability_tables.update_coefficient_context8_templ2(context, eob_x);
        prior[3] = probability_tables.update_coefficient_context8_templ3(context, eob_x);
    }
    unsigned int log_edge_step = 0;
    for (uint8_t delta = 1, zig15offset = 0; ; delta = 8,
         zig15offset = 7,
         est_eob = eob_y,
         aligned_block_offset = raster_to_aligned.at(8),
         log_edge_step = uint16log2(raster_to_aligned.at(16) - raster_to_aligned.at(8)),
         prob_early_exit = probability_tables.y_nonzero_counts_1x8(pt, eob_y,
                                                                   num_nonzeros_7x7)) {
             unsigned int coord = delta;
             int run_ends_early = decoder.at(0).get(prob_early_exit.at(0, 0))? 1 : 0;
             int lane = 0, lane_end = 3;
             for (int vec = 0; ; ++vec, lane_end = 7) {
                 for (; lane < lane_end; ++lane, coord += delta, ++zig15offset) {
             //VECTORIZE HERE
             //the first of the two VECTORIZE items will be
             // a vector of [run_ends_early, lane0, lane1, lane2]
             // if run_ends_early is false then the second set of 4 items will be
             // a vector of [lane3, lane4, lane5, lane6]
                     int cur_vec = (lane + 1) & 3;
                     if (!ProbabilityTablesBase::VECTORIZE) {
                         if (ProbabilityTablesBase::MICROVECTORIZE) {
                             prior
                             [cur_vec]
                             = probability_tables.update_coefficient_context8(coord, context, est_eob);
                         } else {
                             prior
                             [cur_vec]
                             = probability_tables.update_coefficient_context8(coord, context, est_eob);
                         }
                     }
                     auto exp_array = probability_tables.exponent_array_x(pt, coord, zig15offset, prior
                                                                          [cur_vec]

                                                                          );
                     uint8_t length;
                     bool nonzero = false;
                     auto * exp_branch = exp_array.begin();
                     for (length = 0; length != MAX_EXPONENT; ++length) {
                         bool cur_bit = decoder.at(cur_vec).get(*exp_branch++);
                         if (!cur_bit) {
                             break;
                         }
                         nonzero = true;
                     }
                     int16_t coef = 0;
                     if (nonzero) {
                         uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
                         auto &sign_prob = probability_tables.sign_array_8(pt, coord, prior
                                                                           [cur_vec]
);
                         bool neg = !decoder.at(cur_vec).get(sign_prob);
                         coef = (1 << (length - 1));
                         if (length > 1){
                             int i = length - 2;
                             if (length - 2 >= min_threshold) {
                                 auto thresh_prob = probability_tables.residual_thresh_array(pt, coord, length,
                                                                                             prior
                                                                                             [cur_vec]
, min_threshold,
                                                                                             probability_tables.get_max_value(coord));
                                 uint16_t decoded_so_far = 1;
                                 for (; i >= min_threshold; --i) {
                                     int cur_bit = (decoder.at(cur_vec).get(thresh_prob.at(decoded_so_far)) ? 1 : 0);
                                     coef |= (cur_bit << i);
                                     decoded_so_far <<= 1;
                                     if (cur_bit) {
                                         decoded_so_far |= 1;
                                     }
                                 }
                                 probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
                             }
                             auto res_prob = probability_tables.residual_noise_array_x(pt, coord, prior
                                                                                       [cur_vec]
);
                             for (; i >= 0; --i) {
                                 coef |= ((decoder.at(cur_vec).get(res_prob.at(i)) ? 1 : 0) << i);
                             }
                         }
                         if (neg) {
                             coef = -coef;
                         }
                     }
                     mcontext.here().raw_data()[aligned_block_offset + (lane << log_edge_step)] = coef;
                 }
                 if (vec == !run_ends_early) {
                     break;
                 }
                 if (ProbabilityTablesBase::VECTORIZE && delta == 1) {
                     prior[0] = probability_tables.update_coefficient_context8_templ4(context, est_eob);
                     prior[1] = probability_tables.update_coefficient_context8_templ5(context, est_eob);
                     prior[2] = probability_tables.update_coefficient_context8_templ6(context, est_eob);
                     prior[3] = probability_tables.update_coefficient_context8_templ7(context, est_eob);
                 } else if (ProbabilityTablesBase::VECTORIZE) {
                     prior[0] = probability_tables.update_coefficient_context8_templ32(context, est_eob);
                     prior[1] = probability_tables.update_coefficient_context8_templ40(context, est_eob);
                     prior[2] = probability_tables.update_coefficient_context8_templ48(context, est_eob);
                     prior[3] = probability_tables.update_coefficient_context8_templ56(context, est_eob);
                 }
             }
             if (delta == 8) {
                 break;
             }
             if (ProbabilityTablesBase::VECTORIZE) {
                 prior[0] = ProbabilityTablesBase::CoefficientContext();
                 prior[1] = probability_tables.update_coefficient_context8_templ8(context, eob_y);
                 prior[2] = probability_tables.update_coefficient_context8_templ16(context, eob_y);
                 prior[3] = probability_tables.update_coefficient_context8_templ24(context, eob_y);
             }
         }
}





template<bool has_left, bool has_above, bool has_above_right, BlockType color>
void parse_tokens(BlockContext context,
                  Sirikata::Array1d<BoolDecoder, SIMD_WIDTH>::Slice decoder,
                  ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
                  ProbabilityTablesBase &pt) {
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(pt, context.copy());
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
        auto exp_prob = probability_tables.exponent_array_dc(pt, prior);
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
            auto &sign_prob = probability_tables.sign_array_dc(pt, prior);
            bool neg = !decoder.at(0).get(sign_prob);
        

            coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((decoder.at(0).get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        context.here().bzero();
        context.here().dc() = coef;
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    uint8_t num_nonzeros_lag_left_7x7 = num_nonzeros_left_7x7;
    int avg[4] __attribute__((aligned(16)));
    for (unsigned int zz = 0; zz < 49; ++zz) {
        // VECTORIZE HERE (zz += 4 rather than ++zz)
        // this is a perfectly ordinary vectorization task if num_nonzeros_lag_left >= 4
        // however if num_nonzeros_lag_left == 3, 2, 1 or 0, we should probably start with a
        // scalar VECTORIZE bool decoder...and if that works then we can look into making
        // a bool_decoder that speculatively tries 4 gets and cancels out if too many
        // are nonzero. It would probably also need to cancel out if any need to enter the
        // vpx_fill_buffer branch and load things from the streams--in those cases it could
        // fall back to (slow) scalar code.
        unsigned int coord = unzigzag49[zz];
        if ((zz & 3) == 0) {
            num_nonzeros_lag_left_7x7 = num_nonzeros_left_7x7;
            if (num_nonzeros_lag_left_7x7 ==0) {
                break;
            }
#ifdef OPTIMIZED_7x7
            probability_tables.compute_aavrg_vec(zz, context.copy(), avg);
#endif
        }
        unsigned int b_x = (coord & 7);
        unsigned int b_y = (coord >> 3);
        assert((coord & 7) > 0 && (coord >> 3) > 0 && "this does the DC and the lower 7x7 AC");
        {
#ifdef OPTIMIZED_7x7
            probability_tables.update_coefficient_context7x7(zz, prior, avg[zz & 3], context.copy(), num_nonzeros_lag_left_7x7);
#else
            probability_tables.update_coefficient_context7x7(coord, zz, prior, context.copy(), num_nonzeros_lag_left_7x7);
#endif
            auto exp_prob = probability_tables.exponent_array_7x7(pt, coord, zz, prior);
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
                auto &sign_prob = probability_tables.sign_array_7x7(pt, coord, prior);
                neg = !decoder.at(zz & 3).get(sign_prob);
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
                coef = (1 << (length - 1));
                if (length > 1){
                    auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                    for (int i = length - 2; i >= 0; --i) {
                        coef |= ((decoder.at(zz & 3).get(res_prob.at(i)) ? 1 : 0) << i);
                    }
                }
                if (neg) {
                    coef = -coef;
                }
            }
#ifdef OPTIMIZED_7x7
            context.here().coef.at(zz + AlignedBlock::AC_7x7_INDEX) = coef;
#else
            // this should work in all cases but doesn't utilize that the zz is related
            context.here().mutable_coefficients_raster(raster_to_aligned.at(coord)) = coef;
#endif
            if (num_nonzeros_left_7x7 == 0) {
                break; // done with the 49x49
            }
        }
    }
    decode_edge(context,
                decoder,
                probability_tables,
                num_nonzeros_7x7, eob_x, eob_y,
                prior,
                pt);
    context.here().dc() = probability_tables.predict_or_unpredict_dc(context.copy(), true);
    *context.num_nonzeros_here = num_nonzeros_7x7;
}

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice , ProbabilityTables<false, false, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, false, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, false, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<false, true, true, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, true, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, true, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, false, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, false, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, Sirikata::Array1d<BoolDecoder, 4>::Slice, ProbabilityTables<true, false, false, BlockType::Cr>&, ProbabilityTablesBase&);

