#include "bool_encoder.hh"
#include "boolwriter.hh"
#include "jpeg_meta.hh"
#include "block.hh"
#include "numeric.hh"
#include "model.hh"
#include "mmap.hh"
#include "encoder.hh"
#include <map>
#include "weight.hh"
#include <fstream>

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
template <bool has_left, bool has_above, bool has_above_right, BlockType color>
void serialize_tokens(ConstBlockContext context,
                      Sirikata::Array1d<BoolEncoder, 4> & encoder,
                      ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables)
{
    const AlignedBlock &block = context.here();
    auto num_nonzeros_prob = probability_tables.nonzero_counts_7x7(context);
    int serialized_so_far = 0;
    uint8_t num_nonzeros_7x7 = block.num_nonzeros_7x7();
#if 0
    fprintf(stderr, "7\t%d\n", (int)block.num_nonzeros_7x7());
    fprintf(stderr, "x\t%d\n", (int)block.num_nonzeros_x());
    fprintf(stderr, "y\t%d\n", (int)block.num_nonzeros_y());
#endif
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (num_nonzeros_7x7 & (1 << index)) ? 1 : 0;
        encoder.at(3).put(cur_bit, num_nonzeros_prob.at(index, serialized_so_far));
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }
    ProbabilityTablesBase::
    CoefficientContext prior = probability_tables.get_dc_coefficient_context(context,
                                                                             num_nonzeros_7x7);
    {
        // do DC
        uint8_t coord = 0;
        int16_t coef = probability_tables.predict_or_unpredict_dc(context, false);
#ifdef TRACK_HISTOGRAM
        ++histogram[1][coef];
#endif
        uint16_t abs_coef = abs(coef);
        uint8_t length = bit_length(abs_coef);
        auto exp_prob = probability_tables.exponent_array_dc(prior);
        for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
            bool cur_bit = (length != i);
            encoder.at(0).put(cur_bit, exp_prob.at(i));
            if (!cur_bit) {
                break;
            }
        }
        if (length != 0) {
            auto &sign_prob = probability_tables.sign_array_dc(prior);
            encoder.at(0).put(coef >= 0 ? 1 : 0, sign_prob);
        }
        if (length > 1){
            auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
            assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
            assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            for (int i = length - 2; i >= 0; --i) {
                encoder.at(0).put((abs_coef & (1 << i)), res_prob.at(i));
            }
        }
    }


    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    uint8_t num_nonzeros_left_7x7 = block.num_nonzeros_7x7();
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
        unsigned int b_y = coord >> 3;
        (void)b_x;
        (void)b_y;
        assert(b_x > 0 && b_y > 0 && "this does the DC and the lower 7x7 AC");
        {
            int16_t coef = block.coef.at(zz + AlignedBlock::AC_7x7_INDEX);
            uint16_t abs_coef = abs(coef);
#ifdef TRACK_HISTOGRAM
            ++histogram[0][coef];
#endif
            probability_tables.update_coefficient_context7x7(zz, prior, context, num_nonzeros_lag_left_7x7);
            auto exp_prob = probability_tables.exponent_array_7x7(coord, zz, prior);
            uint8_t length = bit_length(abs_coef);
            for (unsigned int i = 0;i < MAX_EXPONENT; ++i) {
                bool cur_bit = (i != length);
                encoder.at(zz&3).put(cur_bit, exp_prob.at(i));
                if (!cur_bit) {
                    break;
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array_7x7(coord, prior);
                encoder.at(zz&3).put(coef >= 0 ? 1 : 0, sign_prob);
                --num_nonzeros_left_7x7;
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
            if (length > 1){
                auto res_prob = probability_tables.residual_noise_array_7x7(coord, prior);
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");

                for (int i = length - 2; i >= 0; --i) {
                   encoder.at(zz&3).put((abs_coef & (1 << i)), res_prob.at(i));
                }
            }
            if (num_nonzeros_left_7x7 ==0) {
                break;
            }

        }
    }

    auto prob_early_exit = probability_tables.x_nonzero_counts_8x1(
                                                      eob_x,
                                                         block.num_nonzeros_7x7());

    bool run_ends_early_x = !(block.coefficients_raster(4) || block.coefficients_raster(5) || block.coefficients_raster(6) || block.coefficients_raster(7));
    bool run_ends_early_y = !(block.coefficients_raster(4 * 8) || block.coefficients_raster(5 * 8) || block.coefficients_raster(6 * 8) || block.coefficients_raster(7*8));
    uint8_t aligned_block_offset = AlignedBlock::ROW_X_INDEX;
    for (int delta = 1; delta <= 8; delta += 7,
             aligned_block_offset = AlignedBlock::ROW_Y_INDEX,
             prob_early_exit = probability_tables.y_nonzero_counts_1x8(eob_y,
                                                                       block.num_nonzeros_7x7())) {
        unsigned int coord = delta;
        uint8_t zig15offset = delta - 1; // the loop breaks early, so we need to reset here
        bool run_ends_early =delta == 1 ?  run_ends_early_x : run_ends_early_y;
        encoder.at(0).put(run_ends_early, prob_early_exit.at(0, 0));
        for (int xx = 0;xx < 7&& (xx < 3 || !run_ends_early); ++xx,coord += delta, ++zig15offset) {
#ifdef TRACK_HISTOGRAM
            ++histogram[2][coef];
#endif

            assert(coord != 9);
            prior = probability_tables.update_coefficient_context8(coord, context, delta == 1 ? eob_x : eob_y);
            auto exp_array = probability_tables.exponent_array_x(coord, zig15offset, prior);
            int16_t coef = block.coef.at(aligned_block_offset + xx);
            uint16_t abs_coef = abs(coef);
            uint8_t length = bit_length(abs_coef);
            for (unsigned int i = 0; i < MAX_EXPONENT; ++i) {
                bool cur_bit = (i != length);
                encoder.at((xx + 1) & 3).put(cur_bit, exp_array.at(i));
                if (!cur_bit) {
                    break;
                }
            }
            if (length > 0) {
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array_8(coord, prior);
                encoder.at((xx + 1) & 3).put(coef >= 0, sign_prob);
            }
            if (length > 1) {
                
                uint8_t min_threshold = probability_tables.get_noise_threshold(coord);
                int i = length - 2;
                if (length - 2 >= min_threshold) {
                    uint16_t encoded_so_far = 1;
                    auto thresh_prob = probability_tables.residual_thresh_array(coord, length,
                                                                                prior, min_threshold,
                                                                                probability_tables.get_max_value(coord));
                    for (; i >= min_threshold; --i) {
                        int cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                        encoder.at((xx + 1) & 3).put(cur_bit, thresh_prob.at(encoded_so_far));
                        encoded_so_far <<=1;
                        if (cur_bit) {
                            encoded_so_far |=1;
                        }
                    }
                    probability_tables.residual_thresh_array_annot_update(coord, encoded_so_far >> 1);
                }
                auto res_prob = probability_tables.residual_noise_array_x(coord, prior);
                for (; i >= 0; --i) {
                    encoder.at((xx + 1) & 3).put((abs_coef & (1 << i)) ? 1 : 0, res_prob.at(i));
                }
            }
        }
    }
}

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, false, false, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, false, false, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, false, false, BlockType::Cr>&);

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, false, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, false, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, false, BlockType::Cr>&);

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, true, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, true, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<false, true, true, BlockType::Cr>&);

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, true, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, true, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, true, BlockType::Cr>&);

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, false, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, false, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, true, false, BlockType::Cr>&);

template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, false, false, BlockType::Y>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, false, false, BlockType::Cb>&);
template void serialize_tokens(ConstBlockContext, Sirikata::Array1d<BoolEncoder, 4>&, ProbabilityTables<true, false, false, BlockType::Cr>&);



inline void VP8BoolEncoder::put( const bool value, Branch & branch )
{
  put( value, branch.prob() );
  branch.record_obs_and_update(value);
}
