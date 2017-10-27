#include "bool_decoder.hh"
#include "boolreader.hh"
#include "model.hh"
#include "../../lepton/idct.hh"
using namespace std;


uint8_t prefix_unremap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v - 3;
}
#define LOG_DELTA_X_EDGE LogTable256[raster_to_aligned.kat<2>() - raster_to_aligned.kat<1>()]
#define LOG_DELTA_Y_EDGE LogTable256[raster_to_aligned.kat<16>() - raster_to_aligned.kat<8>()]
#ifdef _WIN32
#define log_delta_x_edge LOG_DELTA_X_EDGE
#define log_delta_y_edge LOG_DELTA_Y_EDGE

#else
enum {
    log_delta_x_edge = LOG_DELTA_X_EDGE,
    log_delta_y_edge = LOG_DELTA_Y_EDGE,
};
#endif

template<bool all_neighbors_present, BlockType color,
         bool horizontal, class BoolDecoder>
void decode_one_edge(BlockContext mcontext,
                 BoolDecoder& decoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t est_eob,
                 ProbabilityTablesBase& pt) {

    ConstBlockContext context = mcontext.copy();
    auto prob_edge_eob = horizontal
        ? probability_tables.x_nonzero_counts_8x1(pt, est_eob,
                                                  num_nonzeros_7x7)
        : probability_tables.y_nonzero_counts_1x8(pt, est_eob,
                                                  num_nonzeros_7x7);

    uint8_t aligned_block_offset = raster_to_aligned.at(1);
    unsigned int log_edge_step = log_delta_x_edge;
    uint8_t delta = 1;
    uint8_t zig15offset = 0;
    if (!horizontal) {
        delta = 8;
        log_edge_step = log_delta_y_edge;
        zig15offset = 7;
        aligned_block_offset = raster_to_aligned.at(8);
    }
    uint8_t num_nonzeros_edge = 0;
    int16_t decoded_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = decoder.get(prob_edge_eob.at(i, decoded_so_far), Billing::NZ_EDGE) ? 1 : 0;
        num_nonzeros_edge |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    if (num_nonzeros_edge > 7) {
        custom_exit(ExitCode::STREAM_INCONSISTENT);
    }
    unsigned int coord = delta;
    for (int lane = 0; lane < 7 && num_nonzeros_edge; ++lane, coord += delta, ++zig15offset) {
        ProbabilityTablesBase::CoefficientContext prior = {0, 0, 0};
#ifndef USE_SCALAR
        if (ProbabilityTablesBase::MICROVECTORIZE) {
            if (horizontal) {
                prior = probability_tables.update_coefficient_context8_horiz(coord,
                                                                             context,
                                                                             num_nonzeros_edge);
            } else {
                prior = probability_tables.update_coefficient_context8_vert(coord,
                                                                            context,
                                                                            num_nonzeros_edge);
            }
        } else {
            prior = probability_tables.update_coefficient_context8(coord, context, num_nonzeros_edge);
        }
#else
        prior = probability_tables.update_coefficient_context8(coord, context, num_nonzeros_edge);
#endif
        auto exp_array = probability_tables.exponent_array_x(pt,
                                                             coord,
                                                             zig15offset,
                                                             prior);
        uint8_t length = 0;
        bool nonzero = false;
        auto * exp_branch = exp_array.begin();
        for (; length != MAX_EXPONENT; ++length) {
            bool cur_bit = decoder.get(*exp_branch++, (Billing)((int)Billing::BITMAP_EDGE + std::min((int)length, 4)));
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
            auto &sign_prob = probability_tables.sign_array_8(pt, coord, prior);
            bool neg = !decoder.get(sign_prob, Billing::SIGN_EDGE);
            coef = (1 << (length - 1));
            --num_nonzeros_edge;
            if (length > 1){
                int i = length - 2;
                if (i >= min_threshold) {
                    auto thresh_prob = probability_tables.residual_thresh_array(pt,
                                                                                coord,
                                                                                length,
                                                                                prior,
                                                                                min_threshold);
                    uint16_t decoded_so_far = 1;
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (decoder.get(thresh_prob.at(decoded_so_far), Billing::RES_EDGE) ? 1 : 0);
                        coef |= (cur_bit << i);
                        decoded_so_far <<= 1;
                        if (cur_bit) {
                            decoded_so_far |= 1;
                        }
                        // since we are not strict about rejecting jpegs with out of range coefs
                        // we just make those less efficient by reusing the same probability bucket
                        decoded_so_far = std::min(decoded_so_far,
                                                  (uint16_t)(thresh_prob.size() - 1));

                    }
#ifdef ANNOTATION_ENABLED
                    probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
#endif
                }
                auto res_prob = probability_tables.residual_noise_array_x(pt, coord, prior);
                for (; i >= 0; --i) {
                    coef |= ((decoder.get(res_prob.at(i), Billing::RES_EDGE) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        mcontext.here().raw_data()[aligned_block_offset + (lane << log_edge_step)] = coef;
    }
}

template<bool all_neighbors_present, BlockType color, class BoolDecoder>
void decode_edge(BlockContext mcontext,
                 BoolDecoder& decoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase& pt) {
    decode_one_edge<all_neighbors_present, color, true>(mcontext,
                                                                        decoder,
                                                                        probability_tables,
                                                                        num_nonzeros_7x7,
                                                                        eob_x,
                                                                        pt);
    decode_one_edge<all_neighbors_present, color, false>(mcontext,
                                                                        decoder,
                                                                        probability_tables,
                                                                        num_nonzeros_7x7,
                                                                        eob_y,
                                                                        pt);
}





template<bool all_neighbors_present, BlockType color, class BoolDecoder>
void parse_tokens(BlockContext context,
                  BoolDecoder& decoder,
                  ProbabilityTables<all_neighbors_present, color> & probability_tables,
                  ProbabilityTablesBase &pt) {
    context.here().bzero();
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(pt, context.copy());
    uint8_t num_nonzeros_7x7 = 0;
    int decoded_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (decoder.get(num_nonzeros_prob.at(index, decoded_so_far), Billing::NZ_7x7)?1:0);
        num_nonzeros_7x7 |= (cur_bit << index);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    if (num_nonzeros_7x7 > 49) {
        custom_exit(ExitCode::STREAM_INCONSISTENT); // this is a corrupt file: dont decode further
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    Sirikata::AlignedArray1d<short, 8> avg;
    for (unsigned int zz = 0; zz < 49 && num_nonzeros_left_7x7; ++zz) {
        unsigned int coord = unzigzag49[zz];
        if ((zz & 7) == 0) {
#if defined(OPTIMIZED_7x7)// && !defined(USE_SCALAR)
#if !defined(USE_SCALAR)
            probability_tables.compute_aavrg_vec(zz, context.copy(), avg.begin());
#else
            *((int16_t *)avg.begin()) = probability_tables.compute_aavrg(coord, zz, context.copy());
#endif
#endif
        }
        unsigned int b_x = (coord & 7);
        unsigned int b_y = (coord >> 3);
        dev_assert((coord & 7) > 0 && (coord >> 3) > 0 && "this does the DC and the lower 7x7 AC");
        {
            ProbabilityTablesBase::CoefficientContext prior;

#if defined(OPTIMIZED_7x7) && !defined(USE_SCALAR)
            prior = probability_tables.update_coefficient_context7x7_precomp(zz, avg[zz & 7], context.copy(), num_nonzeros_left_7x7);
#else
            prior = probability_tables.update_coefficient_context7x7(coord, zz, context.copy(), num_nonzeros_left_7x7);
#endif
            auto exp_prob = probability_tables.exponent_array_7x7(pt, coord, zz, prior);
            uint8_t length;
            bool nonzero = false;
            auto exp_branch = exp_prob.begin();
            for (length = 0; length != MAX_EXPONENT; ++length) {
                bool cur_bit = decoder.get(*exp_branch++, (Billing)((unsigned int)Billing::BITMAP_7x7 + std::min((int)length, 4)));
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
                neg = !decoder.get(sign_prob, Billing::SIGN_7x7);
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
                coef = (1 << (length - 1));
                if (length > 1){
                    auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                    for (int i = length - 2; i >= 0; --i) {
                        coef |= ((decoder.get(res_prob.at(i), Billing::RES_7x7) ? 1 : 0) << i);
                    }
                }
                if (neg) {
                    coef = -coef;
                }
            }
#if defined(OPTIMIZED_7x7)// && !defined(USE_SCALAR)
            context.here().coef.at(zz + AlignedBlock::AC_7x7_INDEX) = coef;
#else
            // this should work in all cases but doesn't utilize that the zz is related
            context.here().mutable_coefficients_raster(raster_to_aligned.at(coord)) = coef;
#endif
        }
    }
    decode_edge(context,
                decoder,
                probability_tables,
                num_nonzeros_7x7, eob_x, eob_y,
                pt);
    Sirikata::AlignedArray1d<int16_t, 64> outp_sans_dc;
    int uncertainty = 0;
    int uncertainty2 = 0;
    int predicted_dc;
    if (advanced_dc_prediction) {
        predicted_dc = probability_tables.adv_predict_dc_pix(context.copy(), outp_sans_dc.begin(),
                                                             &uncertainty, &uncertainty2);
    } else {
        predicted_dc = probability_tables.predict_dc_dct(context.copy());
    }
    { // dc
        uint8_t length;
        bool nonzero = false;
        uint16_t len_abs_mxm = uint16bit_length(abs(uncertainty));
        uint16_t len_abs_offset_to_closest_edge
          = uint16bit_length(abs(uncertainty2));
        if (!advanced_dc_prediction) {
            ProbabilityTablesBase::CoefficientContext prior;

            prior = probability_tables.update_coefficient_context7x7(0, raster_to_aligned.at(0), context.copy(), num_nonzeros_7x7);
            len_abs_mxm = prior.bsr_best_prior;
            len_abs_offset_to_closest_edge = prior.num_nonzeros_bin;
        }
        auto exp_prob = probability_tables.exponent_array_dc(pt,
                                                             len_abs_mxm,
                                                             len_abs_offset_to_closest_edge);
        auto *exp_branch = exp_prob.begin();
        for (length = 0; length < MAX_EXPONENT; ++length) {
            bool cur_bit = decoder.get(*exp_branch++, (Billing)((int)Billing::EXP0_DC + std::min((int)length, 4)));
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            auto &sign_prob = probability_tables.sign_array_dc(pt, uncertainty, uncertainty2);
            bool neg = !decoder.get(sign_prob, Billing::SIGN_DC);
            coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_array_dc(pt,
                                                                     len_abs_mxm,
                                                                     len_abs_offset_to_closest_edge);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((decoder.get(res_prob.at(i), Billing::RES_DC) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        context.here().dc() = coef;
    }
    context.here().dc() = probability_tables.adv_predict_or_unpredict_dc(context.here().dc(),
                                                                         true,
                                                                         predicted_dc);
    context.num_nonzeros_here->set_num_nonzeros(num_nonzeros_7x7);

    context.num_nonzeros_here->set_horizontal(outp_sans_dc.begin(),
                                              ProbabilityTablesBase::quantization_table((int)color),
                                              context.here().dc());
    context.num_nonzeros_here->set_vertical(outp_sans_dc.begin(),
                                            ProbabilityTablesBase::quantization_table((int)color),
                                            context.here().dc());
}
#ifdef ALLOW_FOUR_COLORS
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<false, BlockType::Ck>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<true, BlockType::Ck>&, ProbabilityTablesBase&);
#endif

template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<false, BlockType::Cr>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, VPXBoolReader&, ProbabilityTables<true, BlockType::Cr>&, ProbabilityTablesBase&);
#ifdef ENABLE_ANS_EXPERIMENTAL
#ifdef ALLOW_FOUR_COLORS
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<false, BlockType::Ck>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<true, BlockType::Ck>&, ProbabilityTablesBase&);
#endif

template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<false, BlockType::Cr>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, ANSBoolReader&, ProbabilityTables<true, BlockType::Cr>&, ProbabilityTablesBase&);
#endif
