#include "bool_decoder.hh"
#include "boolreader.hh"
#include "model.hh"
#include "block.hh"
#include "weight.hh"
#include "../../lepton/idct.hh"
using namespace std;


uint8_t prefix_unremap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v - 3;
}

enum {
    log_delta_x_edge = LogTable256[raster_to_aligned.kat<2>() - raster_to_aligned.kat<1>()],
    log_delta_y_edge = LogTable256[raster_to_aligned.kat<16>() - raster_to_aligned.kat<8>()]
};

template<bool has_left, bool has_above, bool has_above_right, BlockType color,
         bool horizontal>
void decode_one_edge(BlockContext mcontext,
                 BoolDecoder& decoder,
                 ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
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
        int cur_bit = decoder.get(prob_edge_eob.at(i, decoded_so_far)) ? 1 : 0;
        num_nonzeros_edge |= (cur_bit << i);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }

    unsigned int coord = delta;
    for (int lane = 0; lane < 7 && num_nonzeros_edge; ++lane, coord += delta, ++zig15offset) {
        ProbabilityTablesBase::CoefficientContext prior;
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
        auto exp_array = probability_tables.exponent_array_x(pt,
                                                             coord,
                                                             zig15offset,
                                                             prior);
        uint8_t length = 0;
        bool nonzero = false;
        auto * exp_branch = exp_array.begin();
        for (; length != MAX_EXPONENT; ++length) {
            bool cur_bit = decoder.get(*exp_branch++);
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
            auto &sign_prob = probability_tables.sign_array_8(pt, coord, prior);
            bool neg = !decoder.get(sign_prob);
            coef = (1 << (length - 1));
            --num_nonzeros_edge;
            if (length > 1){
                int i = length - 2;
                if (i >= min_threshold) {
                    auto thresh_prob = probability_tables.residual_thresh_array(pt,
                                                                                coord,
                                                                                length,
                                                                                prior,
                                                                                min_threshold,
                                                                                probability_tables.get_max_value(coord));
                    uint16_t decoded_so_far = 1;
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (decoder.get(thresh_prob.at(decoded_so_far)) ? 1 : 0);
                        coef |= (cur_bit << i);
                        decoded_so_far <<= 1;
                        if (cur_bit) {
                            decoded_so_far |= 1;
                        }
                    }
#ifdef ANNOTATION_ENABLED
                    probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
#endif
                }
                auto res_prob = probability_tables.residual_noise_array_x(pt, coord, prior);
                for (; i >= 0; --i) {
                    coef |= ((decoder.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        mcontext.here().raw_data()[aligned_block_offset + (lane << log_edge_step)] = coef;
    }
}

template<bool has_left, bool has_above, bool has_above_right, BlockType color>
void decode_edge(BlockContext mcontext,
                 BoolDecoder& decoder,
                 ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase::CoefficientContext input_prior,
                 ProbabilityTablesBase& pt) {
    decode_one_edge<has_left, has_above, has_above_right, color, true>(mcontext,
                                                                        decoder,
                                                                        probability_tables,
                                                                        num_nonzeros_7x7,
                                                                        eob_x,
                                                                        pt);
    decode_one_edge<has_left, has_above, has_above_right, color, false>(mcontext,
                                                                        decoder,
                                                                        probability_tables,
                                                                        num_nonzeros_7x7,
                                                                        eob_y,
                                                                        pt);
}





template<bool has_left, bool has_above, bool has_above_right, BlockType color>
void parse_tokens(BlockContext context,
                  BoolDecoder& decoder,
                  ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables,
                  ProbabilityTablesBase &pt) {
    context.here().bzero();
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(pt, context.copy());
    uint8_t num_nonzeros_7x7 = 0;
    int decoded_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (decoder.get(num_nonzeros_prob.at(index, decoded_so_far))?1:0);
        num_nonzeros_7x7 |= (cur_bit << index);
        decoded_so_far <<= 1;
        decoded_so_far |= cur_bit;
    }
    ProbabilityTablesBase::CoefficientContext prior;
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    int avg[4] __attribute__((aligned(16)));
    for (unsigned int zz = 0; zz < 49 && num_nonzeros_left_7x7; ++zz) {
        unsigned int coord = unzigzag49[zz];
        if ((zz & 3) == 0) {
#ifdef OPTIMIZED_7x7
            probability_tables.compute_aavrg_vec(zz, context.copy(), avg);
#endif
        }
        unsigned int b_x = (coord & 7);
        unsigned int b_y = (coord >> 3);
        assert((coord & 7) > 0 && (coord >> 3) > 0 && "this does the DC and the lower 7x7 AC");
        {
#ifdef OPTIMIZED_7x7
            probability_tables.update_coefficient_context7x7(zz, prior, avg[zz & 3], context.copy(), num_nonzeros_left_7x7);
#else
            probability_tables.update_coefficient_context7x7(coord, zz, prior, context.copy(), num_nonzeros_left_7x7);
#endif
            auto exp_prob = probability_tables.exponent_array_7x7(pt, coord, zz, prior);
            uint8_t length;
            bool nonzero = false;
            auto exp_branch = exp_prob.begin();
            for (length = 0; length != MAX_EXPONENT; ++length) {
                bool cur_bit = decoder.get(*exp_branch++);
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
                neg = !decoder.get(sign_prob);
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
                coef = (1 << (length - 1));
                if (length > 1){
                    auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                    for (int i = length - 2; i >= 0; --i) {
                        coef |= ((decoder.get(res_prob.at(i)) ? 1 : 0) << i);
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
        }
    }
    decode_edge(context,
                decoder,
                probability_tables,
                num_nonzeros_7x7, eob_x, eob_y,
                prior,
                pt);
    int32_t outp[64];
    idct(context.here(), ProbabilityTablesBase::quantization_table((int)color), outp, true);
    prior = probability_tables.get_dc_coefficient_context(context.copy(),num_nonzeros_7x7);
    { // dc
        const unsigned int coord = 0;
        uint8_t length;
        bool nonzero = false;
        auto exp_prob = probability_tables.exponent_array_dc(pt, prior);
        auto *exp_branch = exp_prob.begin();
        for (length = 0; length < MAX_EXPONENT; ++length) {
            bool cur_bit = decoder.get(*exp_branch++);
            if (!cur_bit) {
                break;
            }
            nonzero = true;
        }
        int16_t coef = 0;
        if (nonzero) {
            auto &sign_prob = probability_tables.sign_array_dc(pt, prior);
            bool neg = !decoder.get(sign_prob);
            coef = (1 << (length - 1));
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                for (int i = length - 2; i >= 0; --i) {
                    coef |= ((decoder.get(res_prob.at(i)) ? 1 : 0) << i);
                }
            }
            if (neg) {
                coef = -coef;
            }
        }
        context.here().dc() = coef;
    }
    idct(context.here(), ProbabilityTablesBase::quantization_table((int)color), outp, false);
    context.here().dc() = probability_tables.predict_or_unpredict_dc(context.copy(), true);
    context.num_nonzeros_here->set_num_nonzeros(num_nonzeros_7x7);
    context.num_nonzeros_here->set_horizontal(outp);
    context.num_nonzeros_here->set_vertical(outp);
}


template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, false, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<false, true, true, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, true, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, true, false, BlockType::Cr>&, ProbabilityTablesBase&);

template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Y>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Cb>&, ProbabilityTablesBase&);
template void parse_tokens(BlockContext, BoolDecoder&, ProbabilityTables<true, false, false, BlockType::Cr>&, ProbabilityTablesBase&);

