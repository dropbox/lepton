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
         bool horizontal,  class BoolEncoder>
void encode_one_edge(ConstBlockContext context,
                 BoolEncoder& encoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t est_eob,
                 ProbabilityTablesBase& pt) {
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
    int16_t serialized_so_far = 0;
    for (int i= 2; i >=0; --i) {
        int cur_bit = (num_nonzeros_edge & (1 << i)) ? 1 : 0;
        encoder.put(cur_bit, prob_edge_eob.at(i, serialized_so_far), Billing::NZ_EDGE);
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }

    unsigned int coord = delta;
    for (int lane = 0; lane < 7 && num_nonzeros_edge; ++lane, coord += delta, ++zig15offset) {

        ProbabilityTablesBase::CoefficientContext prior;
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
        int16_t coef = block.raw_data()[aligned_block_offset + (lane << log_edge_step)];
#ifdef TRACK_HISTOGRAM
            ++histogram[2][coef];
#endif
        uint16_t abs_coef = abs(coef);
        uint8_t length = bit_length(abs_coef);
        for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
            bool cur_bit = (length != i);
            encoder.put(cur_bit, exp_array.at(i), (Billing)((unsigned int)Billing::BITMAP_EDGE + std::min(i, 4U)));
            if (!cur_bit) {
                break;
            }
        }
        if (length > MAX_EXPONENT) {
            custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
        }
        if (coef) {
            uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
            auto &sign_prob = probability_tables.sign_array_8(pt, coord, prior);
            encoder.put(coef >= 0, sign_prob, Billing::SIGN_EDGE);
            --num_nonzeros_edge;
            if (length > 1){
                int i = length - 2;
                if (i >= min_threshold) {
                    auto thresh_prob = probability_tables.residual_thresh_array(pt,
                                                                                coord,
                                                                                length,
                                                                                prior,
                                                                                min_threshold);
                    uint16_t encoded_so_far = 1;
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                        encoder.put(cur_bit, thresh_prob.at(encoded_so_far), Billing::RES_EDGE);
                        encoded_so_far <<=1;
                        if (cur_bit) {
                            encoded_so_far |=1;
                        }
                        // since we are not strict about rejecting jpegs with out of range coefs
                        // we just make those less efficient by reusing the same probability bucket
                        encoded_so_far = std::min(encoded_so_far,
                                                  (uint16_t)(thresh_prob.size() - 1));
                    }
#ifdef ANNOTATION_ENABLED
                    probability_tables.residual_thresh_array_annot_update(coord, decoded_so_far >> 2);
#endif
                }
                auto res_prob = probability_tables.residual_noise_array_x(pt, coord, prior);
                for (; i >= 0; --i) {
                    encoder.put((abs_coef & (1 << i)) ? 1 : 0, res_prob.at(i), Billing::RES_EDGE);
                }
            }
        }
    }
}

template<bool all_neighbors_present, BlockType color, class BoolEncoder>
void encode_edge(ConstBlockContext context,
                 BoolEncoder& encoder,
                 ProbabilityTables<all_neighbors_present, color> & probability_tables,
                 uint8_t num_nonzeros_7x7, uint8_t eob_x, uint8_t eob_y,
                 ProbabilityTablesBase& pt) {
    encode_one_edge<all_neighbors_present, color, true>(context,
                                                                        encoder,
                                                                        probability_tables,
                                                                        num_nonzeros_7x7,
                                                                        eob_x,
                                                                        pt);
    encode_one_edge<all_neighbors_present, color, false>(context,
                                                                        encoder,
                                                                        probability_tables,
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

template <bool all_neighbors_present, BlockType color, class BoolEncoder>
void serialize_tokens(ConstBlockContext context,
                      BoolEncoder& encoder,
                      ProbabilityTables<all_neighbors_present, color> & probability_tables,
                      ProbabilityTablesBase &pt)
{
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(pt, context);
    int serialized_so_far = 0;
    uint8_t num_nonzeros_7x7 = context.num_nonzeros_here->num_nonzeros();
#if 0
    fprintf(stderr, "7\t%d\n", (int)block.num_nonzeros_7x7());
    fprintf(stderr, "x\t%d\n", (int)block.num_nonzeros_x());
    fprintf(stderr, "y\t%d\n", (int)block.num_nonzeros_y());
#endif
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (num_nonzeros_7x7 & (1 << index)) ? 1 : 0;
        encoder.put(cur_bit, num_nonzeros_prob.at(index, serialized_so_far), Billing::NZ_7x7);
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7;

    Sirikata::AlignedArray1d<short, 8> avg;
    for (unsigned int zz = 0; zz < 49 && num_nonzeros_left_7x7; ++zz) {

        unsigned int coord = unzigzag49[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord >> 3;
        (void)b_x;
        (void)b_y;
        if ((zz & 7) == 0) {
#if defined(OPTIMIZED_7x7)
#if !defined(USE_SCALAR)
            probability_tables.compute_aavrg_vec(zz, context.copy(), avg.begin());
#else
            *((int16_t *)avg.begin()) = probability_tables.compute_aavrg(coord, zz, context.copy());
#endif
#endif
        }
        dev_assert(b_x > 0 && b_y > 0 && "this does the DC and the lower 7x7 AC");
        {
            // this should work in all cases but doesn't utilize that the zz is related
            int16_t coef;
#if defined(OPTIMIZED_7x7)// && !defined(USE_SCALAR)
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
#if defined(OPTIMIZED_7x7) && !defined(USE_SCALAR)
            prior = probability_tables.update_coefficient_context7x7_precomp(zz, avg[zz & 7], context.copy(), num_nonzeros_left_7x7);
#else
            prior = probability_tables.update_coefficient_context7x7(coord, zz, context.copy(), num_nonzeros_left_7x7);
#endif
            auto exp_prob = probability_tables.exponent_array_7x7(pt, coord, zz, prior);
            uint8_t length = bit_length(abs_coef);
            for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
                bool cur_bit = (length != i);
                
                encoder.put(cur_bit, exp_prob.at(i), (Billing)((int)Billing::BITMAP_7x7 + std::min((int)i, 4)));
                if (!cur_bit) {
                    break;
                }
            }
            if (length > MAX_EXPONENT) {
                custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array_7x7(pt, coord, prior);
                encoder.put(coef >= 0 ? 1 : 0, sign_prob, Billing::SIGN_7x7);
                --num_nonzeros_left_7x7;
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(pt, coord, prior);
                dev_assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                dev_assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");

                for (int i = length - 2; i >= 0; --i) {
                    encoder.put((abs_coef & (1 << i)), res_prob.at(i), Billing::RES_7x7);
                }
            }
        }
    }
    encode_edge(context,
                encoder,
                probability_tables,
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
        for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
            bool cur_bit = (length != i);
            encoder.put(cur_bit, exp_prob.at(i), (Billing)((int)Billing::EXP0_DC + std::min(i, 4U)));
            if (!cur_bit) {
                break;
            }
        }
        if (length > MAX_EXPONENT) {
            custom_exit(ExitCode::COEFFICIENT_OUT_OF_RANGE);
        }
        if (length != 0) {
            auto &sign_prob = probability_tables.sign_array_dc(pt,
                                                               uncertainty,
                                                               //nb: needs mxm
                                                               //value, not abs
                                                               uncertainty2);
            encoder.put(coef >= 0 ? 1 : 0, sign_prob, Billing::SIGN_DC);
        }
        if (length > 1){
            auto res_prob = probability_tables.residual_array_dc(pt,
                                                                 len_abs_mxm,
                                                                 len_abs_offset_to_closest_edge);
            dev_assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
            dev_assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            for (int i = length - 2; i >= 0; --i) {
                encoder.put((abs_coef & (1 << i)), res_prob.at(i), Billing::RES_DC);
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
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<false, BlockType::Ck>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<true, BlockType::Ck>&, ProbabilityTablesBase&);
#endif

template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<false, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<false, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<false, BlockType::Cr>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<true, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<true, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, VPXBoolWriter&, ProbabilityTables<true, BlockType::Cr>&, ProbabilityTablesBase&);
#ifdef ENABLE_ANS_EXPERIMENTAL
#ifdef ALLOW_FOUR_COLORS
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<false, BlockType::Ck>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<true, BlockType::Ck>&, ProbabilityTablesBase&);
#endif

template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<false, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<false, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<false, BlockType::Cr>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<true, BlockType::Y>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<true, BlockType::Cb>&, ProbabilityTablesBase&);
template void serialize_tokens(ConstBlockContext, ANSBoolWriter&, ProbabilityTables<true, BlockType::Cr>&, ProbabilityTablesBase&);

#endif
