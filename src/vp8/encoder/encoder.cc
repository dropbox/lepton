#include "../util/memory.hh"
#include "bool_encoder.hh"
#include "boolwriter.hh"
#include "jpeg_meta.hh"
#include "numeric.hh"
#include "model.hh"
#include "encoder.hh"
#include <map>
#include <fstream>
#include "../../lepton/idct.hh"
#include "../util/debug.hh"
using namespace std;

uint8_t prefix_remap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v + 3;
}
#ifdef TRACK_HISTOGRAM
map<int, int> histogram[3];// 0 is center, 1 is dc, 2 is edge
struct Blah {
    ~Blah() {
        for (int typ = 0; typ < 3; ++typ) {
            for (map<int,int>::iterator i = histogram[typ].begin(); i != histogram[typ].end(); ++i) {
                printf("%c\t%d\t%d\n", 'c' + typ, i->second, i->first);
            }
        }
    }
} blah;
#endif


enum {
    log_delta_x_edge = LogTable256[raster_to_aligned.kat<2>() - raster_to_aligned.kat<1>()],
    log_delta_y_edge = LogTable256[raster_to_aligned.kat<16>() - raster_to_aligned.kat<8>()]
};

template<bool all_neighbors_present, BlockType color,
         bool horizontal>
void encode_one_edge(EncodeChannelContext chan_context,
                 BoolEncoder& encoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 UniversalPrior &uprior,
                 uint8_t num_nonzeros_7x7, uint8_t est_eob,
                 ProbabilityTablesBase& pt) {
    auto context = chan_context.at(0);
    uint8_t num_nonzeros_edge;
    const AlignedBlock &block = context.here();

    if (horizontal) {
        num_nonzeros_edge= (!!block.coefficients_raster(1))
            + (!!block.coefficients_raster(2)) + (!!block.coefficients_raster(3))
            + (!!block.coefficients_raster(4)) + (!!block.coefficients_raster(5))
            + (!!block.coefficients_raster(6)) + (!!block.coefficients_raster(7));
    } else {
        num_nonzeros_edge = (!!block.coefficients_raster(1 * 8))
            + (!!block.coefficients_raster(2 * 8)) + (!!block.coefficients_raster(3 * 8))
            + (!!block.coefficients_raster(4*8)) + (!!block.coefficients_raster(5*8))
            + (!!block.coefficients_raster(6*8)) + (!!block.coefficients_raster(7*8));
    }

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
    int16_t serialized_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = (num_nonzeros_edge & (1 << i)) ? 1 : 0;
        uprior.set_8x1_nz_bit_id(horizontal, i, serialized_so_far);
        Branch&ubranch=probability_tables.get_universal_prob(pt, uprior);
        encoder.put(cur_bit, ubranch, Billing::NZ_EDGE);
        probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }
    uprior.set_nonzero_edge(horizontal, num_nonzeros_edge);
    unsigned int coord = delta;

    constexpr bool should_predict = all_neighbors_present; // && color == BlockType::Y;

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
        uprior.update_by_prior(aligned_block_offset + (lane << log_edge_step), prior);

        int16_t coef = block.raw_data()[aligned_block_offset + (lane << log_edge_step)];
#ifdef TRACK_HISTOGRAM
            ++histogram[2][coef];
#endif
        uint16_t abs_coef = abs(coef);
        uint8_t length = bit_length(abs_coef);
        for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
            uprior.set_8x1_exp_id(horizontal, i);
            bool cur_bit = (length != i);
            Branch&ubranch=probability_tables.get_universal_prob(pt, uprior);
            encoder.put(cur_bit, ubranch,
                        (Billing)((unsigned int)Billing::BITMAP_EDGE + std::min(i, 4U)));
            probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
            if (!cur_bit) {
                break;
            }
        }
        if (length > MAX_EXPONENT) {
            custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
        }
        int16_t coef_so_far = 0;
        if (coef) {
            uprior.update_nonzero_edge(horizontal, lane);
            uint8_t min_threshold = probability_tables.get_noise_threshold(coord);

            float prediction = uprior.predict_at_index<color>((horizontal ? 1 : 8) * (lane + 1));
            SIGN_PREDICTION sign_prediction = should_predict ? predict_8x1_sign(prediction/ (1 << length)) : SIGN_PREDICTION::UNKNOWN;
            uint8_t predicted_length = all_neighbors_present ? bit_length(abs(static_cast<int16_t>(prediction))) : length;

            uprior.set_8x1_sign(horizontal, sign_prediction, length, predicted_length);

            Branch&ubranch=probability_tables.get_universal_prob(pt, uprior);
            encoder.put(coef >= 0, ubranch,
                        Billing::SIGN_EDGE);
            probability_tables.update_universal_prob(pt, uprior, ubranch, coef >= 0);
            --num_nonzeros_edge;
            coef_so_far = (1 << (length - 1));
            if (length > 1){
                int i = length - 2;
                if (i >= min_threshold) {
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                        uprior.set_8x1_residual(horizontal, i, coef_so_far);
                        Branch &ubranch=probability_tables.get_universal_prob(pt, uprior);
                        encoder.put(cur_bit, ubranch,
                                    Billing::RES_EDGE);
                        probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
                        coef_so_far |= (cur_bit << i);
                        // since we are not strict about rejecting jpegs with out of range coefs
                        // we just make those less efficient by reusing the same probability bucket
                    }
#ifdef ANNOTATION_ENABLED
                    probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
#endif
                }
                for (; i >= 0; --i) {
                    uprior.set_8x1_residual(horizontal, i, coef_so_far);
                    int16_t cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                    Branch&ubranch=probability_tables.get_universal_prob(pt, uprior);
                    encoder.put(cur_bit, ubranch,
                                Billing::RES_EDGE);
                    probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
                    coef_so_far |= (cur_bit << i);
                }
            }
            uprior.update_coef(aligned_block_offset + (lane << log_edge_step), coef);
        }
    }
}

template<bool all_neighbors_present, BlockType color>
void encode_edge(EncodeChannelContext context,
                 BoolEncoder& encoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 UniversalPrior &uprior,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase& pt) {
    encode_one_edge<all_neighbors_present, color, true>(context,
                                                        encoder,
                                                        probability_tables,
                                                        uprior,
                                                        num_nonzeros_7x7,
                                                        eob_x,
                                                        pt);
    encode_one_edge<all_neighbors_present, color, false>(context,
                                                         encoder,
                                                         probability_tables,
                                                         uprior,
                                                         num_nonzeros_7x7,
                                                         eob_y,
                                                         pt);
}
// used for debugging
static int k_debug_block[(int)ColorChannel::NumBlockTypes];
int total_error = 0;
int total_signed_error = 0;
int amd_err = 0;
int med_err = 0;
int avg_err = 0;
int ori_err = 0;

template <bool all_neighbors_present, BlockType color>
void serialize_tokens(EncodeChannelContext chan_context,
                      BoolEncoder& encoder,
                      ProbabilityTables<all_neighbors_present, color> & probability_tables,
                      ProbabilityTablesBase &pt)
{
    UniversalPrior uprior;
    uprior.init(chan_context, color,
                all_neighbors_present || probability_tables.left_present,
                all_neighbors_present || probability_tables.above_present,
                all_neighbors_present || probability_tables.above_right_present);
    //auto shadow_context0 = chan_context.at(1);
    //auto shadow_context1 = chan_context.at(2);
    //auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7_chan(pt, shadow_context0, shadow_context1, uprior);
    auto context = chan_context.at(0);
    //auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(pt, context, uprior);
    int serialized_so_far = 0;
    uint8_t num_nonzeros_7x7 = context.num_nonzeros_here->num_nonzeros();
#if 0
    fprintf(stderr, "7\t%d\n", (int)block.num_nonzeros_7x7());
    fprintf(stderr, "x\t%d\n", (int)block.num_nonzeros_x());
    fprintf(stderr, "y\t%d\n", (int)block.num_nonzeros_y());
#endif
    for (int index = 5; index >= 0; --index) {
        uprior.set_7x7_nz_bit_id(index, serialized_so_far);
        int cur_bit = (num_nonzeros_7x7 & (1 << index)) ? 1 : 0;
        Branch &ubranch=probability_tables.get_universal_prob(pt, uprior);
        encoder.put(cur_bit, ubranch,
                    Billing::NZ_7x7);
        probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;
    uprior.set_nonzeros7x7(num_nonzeros_7x7);
    Sirikata::AlignedArray1d<short, 8> avg;
    for (unsigned int zz = 0; zz < 49 && num_nonzeros_left_7x7; ++zz) {
        if ((zz & 7) == 0) {
#ifdef OPTIMIZED_7x7
            probability_tables.compute_aavrg_vec(zz, context.copy(), avg.begin());
#endif
        }

        unsigned int coord = unzigzag49[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord >> 3;
        (void)b_x;
        (void)b_y;
        dev_assert(b_x > 0 && b_y > 0 && "this does the DC and the lower 7x7 AC");
        {
            // this should work in all cases but doesn't utilize that the zz is related
            int16_t coef;
#ifdef OPTIMIZED_7x7
            coef = context.here().coef.at(zz + AlignedBlock::AC_7x7_INDEX);
#else
            // this should work in all cases but doesn't utilize that the zz is related
            coef = context.here().coefficients_raster(raster_to_aligned.at(coord));
#endif
            uint16_t abs_coef = abs(coef);
#ifdef TRACK_HISTOGRAM
            ++histogram[0][coef];
#endif
            ProbabilityTablesBase::CoefficientContext prior = {0, 0, 0};
#ifdef OPTIMIZED_7x7
            prior = probability_tables.update_coefficient_context7x7_precomp(zz, avg[zz & 7], context.copy(), num_nonzeros_left_7x7);
#else
            prior = probability_tables.update_coefficient_context7x7(coord, zz, context.copy(), num_nonzeros_left_7x7);
#endif
            uprior.update_by_prior(zz + AlignedBlock::AC_7x7_INDEX, prior);

            float prediction = uprior.predict_at_index<color>(b_y * 8 + b_x);

            uint8_t length = bit_length(abs_coef);
            uint8_t predicted_length = all_neighbors_present ? bit_length(abs(static_cast<int16_t>(prediction))) : 0u;
            for (unsigned int i = 0; i < MAX_EXPONENT; ++i) {
                uprior.set_7x7_exp_id(i, predicted_length);
                bool cur_bit = (length != i);
                Branch&ubranch=probability_tables.get_universal_prob(pt, uprior);
                encoder.put(cur_bit, ubranch,
                            (Billing)((int)Billing::BITMAP_7x7 + std::min((int)i, 4)));
                probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
                if (!cur_bit) {
                    break;
                }
            }
            if (length > MAX_EXPONENT) {
                custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
            }
            if (length != 0) {
                uprior.update_nonzero(b_x, b_y);
                SIGN_PREDICTION sign_prediction = all_neighbors_present ? predict_7x7_sign(prediction / (1 << length)) : SIGN_PREDICTION::UNKNOWN;
                uprior.set_7x7_sign(sign_prediction, length, predicted_length);

                Branch &ubranch=probability_tables.get_universal_prob(pt, uprior);
                encoder.put(coef >= 0 ? 1 : 0,
                            ubranch,
                            Billing::SIGN_7x7);
                probability_tables.update_universal_prob(pt, uprior, ubranch, coef >= 0);
                --num_nonzeros_left_7x7;
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
            int16_t coef_so_far = (1 << (length - 1));
            if (length > 1){
                dev_assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                dev_assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
                for (int i = length - 2; i >= 0; --i) {
                    uprior.set_7x7_residual(i, coef_so_far);
                    int cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                    Branch &ubranch=probability_tables.get_universal_prob(pt, uprior);
                    encoder.put(cur_bit, ubranch,
                                Billing::RES_7x7);
                    probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
                    coef_so_far |= (cur_bit << i);
                }
            }
#ifdef OPTIMIZED_7x7
            uprior.update_coef(zz + AlignedBlock::AC_7x7_INDEX, coef);
#else
            uprior.update_coef(raster_to_aligned.at(coord), coef);
#endif
        }
    }
    encode_edge(chan_context,
                encoder,
                probability_tables,
                uprior,
                num_nonzeros_7x7, eob_x, eob_y,
                pt);


    Sirikata::AlignedArray1d<int16_t, 64> outp_sans_dc;
    int uncertainty = 0; // this is how far off our max estimate vs min estimate is
    int uncertainty2 = 0;
    int predicted_val;
    if (advanced_dc_prediction) {
        predicted_val = probability_tables.adv_predict_dc_pix(context,
                                                              outp_sans_dc.begin(),
                                                              &uncertainty,
                                                              &uncertainty2);
    } else {
        predicted_val = probability_tables.predict_dc_dct(context);
    }
   int adv_predicted_dc = probability_tables.adv_predict_or_unpredict_dc(context.here().dc(),
                                                                          false,
                                                                          predicted_val);

    if (context.here().dc() != probability_tables.adv_predict_or_unpredict_dc(adv_predicted_dc,
                                                                              true,
                                                                              predicted_val)) {
          custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE); // value out of range
    }
    {
        // do DC
        int16_t coef = adv_predicted_dc;
#ifdef TRACK_HISTOGRAM
        ++histogram[1][coef];
#endif
        uint16_t abs_coef = abs(coef);
        uint8_t length = bit_length(abs_coef);


        if (!advanced_dc_prediction) {
            ProbabilityTablesBase::CoefficientContext prior;

            prior = probability_tables.update_coefficient_context7x7(0, raster_to_aligned.at(0), context.copy(), num_nonzeros_7x7);
            uprior.update_by_prior(AlignedBlock::DC_INDEX, prior);
        } else {
            uprior.priors[UniversalPrior::OFFSET_BEST_PRIOR] = uncertainty;
            uprior.priors[UniversalPrior::OFFSET_BEST_PRIOR_SCALED] = uint16bit_length(abs(uncertainty));
            uprior.priors[UniversalPrior::OFFSET_BEST_PRIOR2] = uncertainty2;
            uprior.priors[UniversalPrior::OFFSET_BEST_PRIOR2_SCALED] = uint16bit_length(abs(uncertainty2));
            uprior.priors[UniversalPrior::OFFSET_ZZ_INDEX] = AlignedBlock::DC_INDEX;
        }

        for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
            bool cur_bit = (length != i);
            uprior.set_dc_exp_id(i);
            Branch & ubranch=probability_tables.get_universal_prob(pt, uprior);
            encoder.put(cur_bit, ubranch,
                        (Billing)((int)Billing::EXP0_DC + std::min(i, 4U)));
            probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
            if (!cur_bit) {
                break;
            }
        }
        if (length > MAX_EXPONENT) {
            custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
        }
        if (length != 0) {
            uprior.set_dc_sign();
            Branch & ubranch=probability_tables.get_universal_prob(pt, uprior);
            encoder.put(coef >= 0 ? 1 : 0,
                        ubranch,
                        Billing::SIGN_DC);
            probability_tables.update_universal_prob(pt, uprior, ubranch, coef >= 0 ? 1 : 0);
        }
        if (length > 1){
            int16_t coef_so_far = (1 << (length - 1));
            dev_assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
            dev_assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            for (int i = length - 2; i >= 0; --i) {
                uprior.set_dc_residual(i, coef_so_far);
                int16_t cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                Branch & ubranch=probability_tables.get_universal_prob(pt, uprior);
                encoder.put(cur_bit, ubranch,
                            Billing::RES_DC);
                probability_tables.update_universal_prob(pt, uprior, ubranch, cur_bit);
                coef_so_far |= (cur_bit << i);
            }
        }
    }
    {
        int dc = context.here().dc();
        context.num_nonzeros_here->set_horizontal(outp_sans_dc.begin(),
                                                  ProbabilityTablesBase::quantization_table((int)color),
                                                  dc);
        context.num_nonzeros_here->set_vertical(outp_sans_dc.begin(),
                                                ProbabilityTablesBase::quantization_table((int)color),
                                                dc);
    }

    if ((!g_threaded) && LeptonDebug::raw_YCbCr[(int)color]) {
        int16_t outp[64];
        idct(context.here(), ProbabilityTablesBase::quantization_table((int)color), outp, false);
        for (int i = 0; i < 64; ++i) {
            outp[i] >>= 3;
        }

        double delta = 0;
        for (int i = 0; i < 64; ++i) {
            delta += outp[i] - outp_sans_dc[i];
            //fprintf (stderr, "%d + %d = %d\n", outp_sans_dc[i], context.here().dc(), outp[i]);
        }
        delta /= 64;
        //fprintf (stderr, "==== %f = %f =?= %d\n", delta, delta * 8, context.here().dc());

        int debug_width = LeptonDebug::getDebugWidth((int)color);
        int offset = k_debug_block[(int)color];
        for (int y = 0; y  < 8; ++y) {
            for (int x = 0; x  < 8; ++x) {
                LeptonDebug::raw_YCbCr[(int)color][offset + y * debug_width + x] = std::max(std::min(outp[(y << 3) + x] + 128, 255),0);
            }
        }
        k_debug_block[(int)color] += 8;
        if (k_debug_block[(int)color] % debug_width == 0) {
            k_debug_block[(int)color] += debug_width * 7;
        }
    }
}
#ifdef ALLOW_FOUR_COLORS
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<false, BlockType::Ck>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<true, BlockType::Ck>&, ProbabilityTablesBase&);
#endif

template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<false, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<false, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<false, BlockType::Cr>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<true, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<true, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(EncodeChannelContext, BoolEncoder&, ProbabilityTables<true, BlockType::Cr>&, ProbabilityTablesBase&);

