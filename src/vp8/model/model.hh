#ifndef MODEL_HH
#define MODEL_HH

#include <vector>
#include <memory>

#include "../util/nd_array.hh"
#include "numeric.hh"
#include "branch.hh"
#include "block.hh"
#include "weight.hh"
#include "../util/option.hh"
#include "../util/aligned_block.hh"
#include "../util/block_based_image.hh"

class BoolEncoder;
class Slice;


constexpr unsigned int BLOCK_TYPES        = 2; // setting this to 3 gives us ~1% savings.. 2/3 from BLOCK_TYPES=2
constexpr unsigned int NUM_NONZEROS_BINS     =  10;
constexpr unsigned int band_divisor = 1;
constexpr unsigned int COEF_BANDS         = 64 / band_divisor;
constexpr unsigned int ENTROPY_NODES      = 15;
constexpr unsigned int NUM_NONZEROS_EOB_PRIORS = 66;
constexpr unsigned int ZERO_OR_EOB = 3;
constexpr unsigned int RESIDUAL_NOISE_FLOOR  = 7;
constexpr unsigned int COEF_BITS = 10;



struct Model
{
    typedef Sirikata::Array4d<Branch, BLOCK_TYPES, 26, 6, 32> NonzeroCounts7x7;
    NonzeroCounts7x7 num_nonzeros_counts_7x7_;

    typedef Sirikata::Array5d<Branch, BLOCK_TYPES, 8, 8, 3, 4> NonzeroCounts1x8;
    NonzeroCounts1x8 num_nonzeros_counts_1x8_;
    NonzeroCounts1x8 num_nonzeros_counts_8x1_;

    typedef Sirikata::Array4d<Branch,
                              BLOCK_TYPES,
                              COEF_BANDS,
                              (8>NUM_NONZEROS_BINS?8:NUM_NONZEROS_BINS),
                              COEF_BITS> ResidualNoiseCounts;

    ResidualNoiseCounts residual_noise_counts_;

    typedef Sirikata::Array4d<Branch,
                              BLOCK_TYPES,
                              (1<<(1 + RESIDUAL_NOISE_FLOOR)),
                              1 + RESIDUAL_NOISE_FLOOR,
                              1<<RESIDUAL_NOISE_FLOOR > ResidualThresholdCounts;

    ResidualThresholdCounts residual_threshold_counts_;

    typedef Sirikata::Array6d<Branch,
                    BLOCK_TYPES,
                    15,
                    NUM_NONZEROS_BINS,
                    NUMERIC_LENGTH_MAX,
                    NUMBER_OF_EXPONENT_BITS,
                    1<< (NUMBER_OF_EXPONENT_BITS - 1)> ExponentCounts8;

    typedef Sirikata::Array6d<Branch,
                              BLOCK_TYPES,
                              49,
                              NUM_NONZEROS_BINS,
                              NUMERIC_LENGTH_MAX,
                              NUMBER_OF_EXPONENT_BITS,
                              1 << (NUMBER_OF_EXPONENT_BITS - 1)> ExponentCounts7x7;

typedef Sirikata::Array5d<Branch,
                          BLOCK_TYPES,
                          NUM_NONZEROS_BINS,
                          NUMERIC_LENGTH_MAX,
                          NUMBER_OF_EXPONENT_BITS,
                          1<<(NUMBER_OF_EXPONENT_BITS - 1)> ExponentCountsDC;

  ExponentCounts7x7 exponent_counts_;
  ExponentCounts8 exponent_counts_x_;
  ExponentCountsDC exponent_counts_dc_;

  typedef Sirikata::Array3d<Branch, BLOCK_TYPES, 4, (COEF_BITS + 2 > 9 ? COEF_BITS + 2 : 9)> SignCounts;
  SignCounts sign_counts_;
  
  template <typename lambda>
  void forall( const lambda & proc )
  {
      num_nonzeros_counts_7x7_.foreach(proc);
      num_nonzeros_counts_1x8_.foreach(proc);
      num_nonzeros_counts_8x1_.foreach(proc);
      exponent_counts_x_.foreach(proc);
      exponent_counts_.foreach(proc);
      exponent_counts_dc_.foreach(proc);

      residual_noise_counts_.foreach(proc);
      residual_threshold_counts_.foreach(proc);
      sign_counts_.foreach(proc);
  }

};

enum ContextTypes{
    ZDSTSCAN,
    ZEROS7x7,
    EXPDC,
    RESDC,
    SIGNDC,
    EXP7x7,
    RES7x7,
    SIGN7x7,
    ZEROS1x8,
    ZEROS8x1,
    EXP8,
    THRESH8,
    RES8,
    SIGN8,
    NUMCONTEXT
};
struct Context {
    enum {
        H = 2448,
        W = 3264
    };
    int cur_cmp;
    int cur_jpeg_x;
    int cur_jpeg_y;
    ContextTypes annot;
    int p[3][H/8][W/8][8][8][NUMCONTEXT][3];
};
extern Context *gctx;
#if 0
#define ANNOTATION_ENABLED
#define ANNOTATE_CTX(bpos,annot_type,ctxnum,value) \
    (gctx->annot = annot_type, \
     gctx->p[gctx->cur_cmp][gctx->cur_jpeg_y][gctx->cur_jpeg_x][bpos/8][bpos%8][annot_type][ctxnum] = value)
#else
#define ANNOTATE_CTX(bpos, annot_type, ctxnum, value)
#endif
class Slice;

struct ProbabilityTables
{
private:
    static Model model_;
    static int32_t icos_idct_linear_8192_dequantized_[64] __attribute__ ((aligned (16)));
    static int32_t *icos_idct_linear_8192_dequantized() {
        return icos_idct_linear_8192_dequantized_;
#if 0
        unsigned char * retval = (icos_idct_linear_8192_dequantized_x + 15);
        size_t mask = (size_t)(retval - (unsigned char*)nullptr);
        mask &= 15;
        retval -= mask;
        return (uint32_t*)retval;
#endif
    }
    const unsigned short *quantization_table_;
public:
    ProbabilityTables();
    ProbabilityTables(const Slice & slice);
    void set_quantization_table(const unsigned short quantization_table[64]) {
        quantization_table_ = quantization_table;
        for (int pixel_row = 0; pixel_row < 8; ++pixel_row) {
            for (int i = 0; i < 8; ++i) {
                icos_idct_linear_8192_dequantized()[pixel_row * 8 + i] = icos_idct_linear_8192_scaled[pixel_row * 8 + i] * quantization_table[zigzag[i]];
            }
        }
    }
    Sirikata::Array2d<Branch, 6, 32>::Slice nonzero_counts_7x7(unsigned int block_type,
                                                            const BlockContext block) {
        Optional<uint8_t> num_nonzeros_above;
        Optional<uint8_t> num_nonzeros_left;
        if (block.above()) {
            num_nonzeros_above = block.above()->num_nonzeros_7x7();
        }
        if (block.left()) {
            num_nonzeros_left = block.left()->num_nonzeros_7x7();
        }

        uint8_t num_nonzeros_context = 0;
        if (!num_nonzeros_left.initialized()) {
            num_nonzeros_context = (num_nonzeros_above.get_or(0) + 1) / 2;
        } else if (!num_nonzeros_above.initialized()) {
            num_nonzeros_context = (num_nonzeros_left.get_or(0) + 1) / 2;
        } else {
            num_nonzeros_context = (num_nonzeros_above.get() + num_nonzeros_left.get() + 2) / 4;
        }
        ANNOTATE_CTX(0, ZEROS7x7, 0, num_nonzeros_context);
        return model_.num_nonzeros_counts_7x7_.at(std::min(block_type, BLOCK_TYPES - 1),
                                                   num_nonzeros_to_bin(num_nonzeros_context));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice x_nonzero_counts_8x1(unsigned int block_type,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 1, eob_x);
        return model_.num_nonzeros_counts_8x1_.at(std::min(block_type, BLOCK_TYPES -1), eob_x, ((num_nonzeros + 3) / 7));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice y_nonzero_counts_1x8(unsigned int block_type,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 1, eob_x);
        return model_.num_nonzeros_counts_1x8_.at(std::min(block_type, BLOCK_TYPES -1))
            .at(eob_x)
            .at(((num_nonzeros + 3) / 7));
    }
    Sirikata::Array2d<Branch,
                      NUMBER_OF_EXPONENT_BITS,
                      1<<(NUMBER_OF_EXPONENT_BITS - 1)>::Slice exponent_array_x(unsigned int block_type,
                                                                                 unsigned int band,
                                                                                 unsigned int num_nonzeros_x,
                                                                                 const BlockContext&for_lak) {
        ANNOTATE_CTX(band, EXP8, 0, exp_len(abs(compute_lak(for_lak, band))));
        ANNOTATE_CTX(band, EXP8, 1, num_nonzeros_x);
        return model_.exponent_counts_x_.at(std::min(block_type, BLOCK_TYPES - 1),
                                             (band & 7)== 0 ? ((band >>3) + 7) : band - 1 ,
                                             num_nonzeros_x,
                                             exp_len(abs(compute_lak(for_lak, band))));
    }
    Sirikata::Array2d<Branch,
                      NUMBER_OF_EXPONENT_BITS,
                      1 << (NUMBER_OF_EXPONENT_BITS - 1)>::Slice exponent_array_7x7(const unsigned int block_type,
                                                               const unsigned int band,
                                                               const unsigned int num_nonzeros,
                                                                   const BlockContext&context) {
        if (band > 0) {
            ANNOTATE_CTX(band, EXP7x7, 0, exp_len(abs(compute_aavrg(block_type, context, band))));
            ANNOTATE_CTX(band, EXP7x7, 1, num_nonzeros_to_bin(num_nonzeros));
        } else {
            ANNOTATE_CTX(0, EXPDC, 0, exp_len(abs(compute_aavrg(block_type, context, band))));
            ANNOTATE_CTX(0, EXPDC, 1, num_nonzeros_to_bin(num_nonzeros));
        return model_.exponent_counts_dc_
            .at( std::min(block_type, BLOCK_TYPES - 1) )
            .at(num_nonzeros_to_bin(num_nonzeros))
            .at(exp_len(abs(compute_aavrg(block_type, context, band))));
        }
        return model_.exponent_counts_
            .at( std::min(block_type, BLOCK_TYPES - 1) )
            .at( band - 8 - band / 8).at(num_nonzeros_to_bin(num_nonzeros))
            .at(exp_len(abs(compute_aavrg(block_type, context, band))));
    }
    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_x(const unsigned int block_type,
                                                          const unsigned int band,
                                                          const uint8_t num_nonzeros_x) {
        ANNOTATE_CTX(band, RES8, 0, num_nonzeros_x);
        return residual_noise_array_shared(block_type,
                                           band,
                                           num_nonzeros_x);
    }

    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_shared(const unsigned int block_type,
                                                            const unsigned int band,
                                                            const uint8_t num_nonzeros_x) {
        return model_.residual_noise_counts_.at(std::min(block_type, BLOCK_TYPES - 1),
                                                 band/band_divisor,
                                                 num_nonzeros_x);
    }
    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_7x7(const unsigned int block_type,
                                                            const unsigned int band,
                                                            const uint8_t num_nonzeros) {
        if (band == 0) {
            ANNOTATE_CTX(0, RESDC, 0, num_nonzeros_to_bin(num_nonzeros));
        } else {
            ANNOTATE_CTX(band, RES7x7, 0, num_nonzeros_to_bin(num_nonzeros));
        }
        return residual_noise_array_shared(block_type, band, num_nonzeros_to_bin(num_nonzeros));
    }
    unsigned int num_nonzeros_to_bin(unsigned int num_nonzeros) {

        // this divides by 7 to get the initial value
        return nonzero_to_bin[NUM_NONZEROS_BINS-1][num_nonzeros];
    }
    int idct_2d_8x1(const AlignedBlock&block, bool ignore_first, int pixel_row) {
        int retval = 0;
        if (!ignore_first) {
            retval = block.coefficients().raster(0) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 0];
        }
        retval += block.coefficients().raster(1) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 1];
        retval += block.coefficients().raster(2) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 2];
        retval += block.coefficients().raster(3) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 3];
        retval += block.coefficients().raster(4) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 4];
        retval += block.coefficients().raster(5) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 5];
        retval += block.coefficients().raster(6) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 6];
        retval += block.coefficients().raster(7) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 7];
        return retval;
    }

    int idct_2d_1x8(const AlignedBlock&block, bool ignore_first, int pixel_row) {
        int retval = 0;
        if (!ignore_first) {
            retval = block.coefficients().raster(0) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 0];
        }
        retval += block.coefficients().raster(8) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 1];
        retval += block.coefficients().raster(16) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 2];
        retval += block.coefficients().raster(24) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 3];
        retval += block.coefficients().raster(32) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 4];
        retval += block.coefficients().raster(40) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 5];
        retval += block.coefficients().raster(48) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 6];
        retval += block.coefficients().raster(56) * icos_idct_linear_8192_dequantized()[pixel_row * 8 + 7];
        return retval;
    }

    int predict_dc_dct(const BlockContext&context) {
        int prediction = 0;
        Optional<int> left_block;
        Optional<int> left_edge;
        Optional<int> above_block;
        Optional<int> above_edge;
        if (context.has_left()) {
            left_block = idct_2d_8x1(context.left_unchecked(), 0, 7);
            left_edge = idct_2d_8x1(context.here(), 1, 0);
        }
        if (context.has_above()) {
            above_block = idct_2d_1x8(context.above_unchecked(), 0, 7);
            above_edge = idct_2d_1x8(context.here(), 1, 0);
        }
        if (left_block.initialized()) {
            if (above_block.initialized()) {
                prediction = ( ( left_block.get() - left_edge.get() ) + (above_block.get() - above_edge.get()) ) * 4;
            } else {
                prediction = ( left_block.get() - left_edge.get() ) * 8;
            }
        } else if (above_block.initialized()) {
            prediction = ( above_block.get() - above_edge.get() ) * 8;
        }
        int DCT_RSC = 8192; 
        prediction = std::max(-1024 * DCT_RSC, std::min(1016 * DCT_RSC, prediction));
        prediction /= quantization_table_[0];
        int round = DCT_RSC/2;
        if (prediction < 0) {
            round = -round;
        }
        return (prediction + round) / DCT_RSC;
    }
    int predict_locoi_dc_deprecated(const BlockContext&context) {
        if (context.has_left()) {
            int a = context.left_unchecked().coefficients().raster(0);
            if (context.has_above()) {
                int b = context.above_unchecked().coefficients().raster(0);
                int c = context.above_left_unchecked().coefficients().raster(0);
                if (c >= std::max(a,b)) {
                    return std::min(a,b);
                } else if (c <= std::min(a,b)) {
                    return std::max(a,b);
                }
                return a + b - c;
            }else { 
                return a;
            }
        } else if (context.has_above()) {
            return context.above_unchecked().coefficients().raster(0);
        } else {
            return 0;
        }
    }
    int predict_or_unpredict_dc(const BlockContext&context, bool recover_original) {
        int max_value = 0;
        if (quantization_table_[0]){
            max_value = (1024 + quantization_table_[0] - 1) / quantization_table_[0];
        }
        int min_value = -max_value;
        int adjustment_factor = 2 * max_value + 1;
        int retval = //predict_locoi_dc_deprecated(block);
            predict_dc_dct(context);
        retval = context.here().coefficients().raster(0) + (recover_original ? retval : -retval);
        if (retval < min_value) retval += adjustment_factor;
        if (retval > max_value) retval -= adjustment_factor;
        return retval;
    }
    int compute_aavrg_dc(int component, BlockContext context) {
        Optional<uint16_t> topleft;
        Optional<uint16_t> top;
        Optional<uint16_t> left;
        uint32_t total = 0;
        uint32_t weights = 0;
        if (context.has_above()) {
            top = abs(context.above_unchecked().coefficients().raster(0));
            if (context.has_left()) {
                topleft = abs(context.above_left_unchecked().coefficients().raster(0));
            }
        }
        //if (block.context().above_right.initialized()) {
        //topright = abs(block.context().above_right.get()->coefficients().at(0));
        //}
        if (context.has_left()) {
            left = abs(context.left_unchecked().coefficients().raster(0));
        }
       
        const auto &weight_array = component ? abs_ctx_weights_chr : abs_ctx_weights_lum;
         if (topleft.initialized()) {
            total += weight_array[0][1][1] * (int)topleft.get();
            weights += weight_array[0][1][1];
        }
        if (top.initialized()) {
            total += weight_array[0][1][2] * (int)top.get();
            weights += weight_array[0][1][2];
        }
        if (left.initialized()) {
            total += weight_array[0][2][1] * (int)left.get();
            weights += weight_array[0][2][1];
        }
        if (weights == 0) {
            weights = 1;
        }
        return (total + weights / 2) / weights;
    }
    int compute_aavrg(int component, BlockContext context, unsigned int band) {
        if (band == 0) {
            return compute_aavrg_dc(component, context);
        }
        Optional<uint16_t> topleft;
        Optional<uint16_t> top;
        Optional<uint16_t> left;
        uint32_t total = 0;
        uint32_t weights = 0;
        uint32_t coef_index = band;
        uint8_t zz = zigzag[band];
        if (context.has_above()) {
            top = abs(context.above_unchecked().coefficients().raster(coef_index));
            if (context.has_left()) {
                topleft = abs(context.above_left_unchecked().coefficients().raster(coef_index));
            }
        }
        //if (block.context().above_right.initialized()) {
        //topright = abs(block.context().above_right.get()->coefficients().at(coef_index));
        //}
        if (context.has_left()) {
            left = abs(context.left_unchecked().coefficients().raster(coef_index));
        }
        const auto &weight_array = component ? abs_ctx_weights_chr : abs_ctx_weights_lum;

        if (topleft.initialized()) {
            total += weight_array[zz][1][1] * (int)topleft.get();
            weights += weight_array[zz][1][1];
        }
        if (top.initialized()) {
            total += weight_array[zz][1][2] * (int)top.get();
            weights += weight_array[zz][1][2];
        }
        if (left.initialized()) {
            total += weight_array[zz][2][1] * (int)left.get();
            weights += weight_array[zz][2][1];
        }
        if (weights == 0) {
            weights = 1;
        }
        return (total + weights / 2) /weights;
    }
    unsigned int exp_len(int v) {
        if (v < 0) {
            v = -v;
        }
        return bit_length(std::min(v, 1023));
    }
    int compute_lak(const BlockContext&context, unsigned int band) {
        int16_t coeffs_x[8];
        int16_t coeffs_a[8];
        int32_t coef_idct[8];
        assert(quantization_table_);
        if ((band & 7) && context.has_above()) {
            // y == 0: we're the x
            assert(band/8 == 0); //this function only works for the edge
            const auto &above = context.above_unchecked().coefficients();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i * 8;
                coeffs_x[i]  = i ? context.here().coefficients().raster(cur_coef) : -32768;
                coeffs_a[i]  = above.raster(cur_coef);
                coef_idct[i] = icos_base_8192_scaled[i * 8]
                    * quantization_table_[zigzag[cur_coef]];
            }
        } else if ((band & 7) == 0 && context.has_left()) {
            // x == 0: we're the y
            const auto &left = context.left_unchecked().coefficients();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i;
                coeffs_x[i]  = i ? context.here().coefficients().raster(cur_coef) : -32768;
                coeffs_a[i]  = left.raster(cur_coef);
                coef_idct[i] = icos_base_8192_scaled[i * 8]
                    * quantization_table_[zigzag[cur_coef]];
            }
        } else if (context.has_above()) {
            return 0; // we don't have data in the correct direction
        } else {
            return 0;
        }
        int prediction = 0;
        for (int i = 1; i < 8; ++i) {
            int sign = (i & 1) ? 1 : -1;
            prediction -= coef_idct[i] * (coeffs_x[i] + sign * coeffs_a[i]);
        }
        if (prediction >0) {
            prediction += coef_idct[0]/2;
        } else {
            prediction -= coef_idct[0]/2; // round away from zero
        }
        prediction /= coef_idct[0];
        prediction += coeffs_a[0];
        return prediction;
    }/*
    SignValue compute_sign(const Block&block, unsigned int band) {
        if (block.context().left.initialized()) {
            return block.context().left.get()->coefficients().at(band);
        } else if (block.context().above.initialized()) {
            return block.context().above.get()->coefficients().at(band);
        }
        return 0;
        }*/
    Sirikata::Array1d<Branch,
            (1<<RESIDUAL_NOISE_FLOOR)>::Slice
        residual_thresh_array(const unsigned int block_type,
                              const unsigned int band,
                              const uint8_t cur_exponent,
                              const BlockContext&context,
                              int min_threshold,
                              int max_value) {
        uint16_t ctx_abs = abs(compute_lak(context, band));
        if (ctx_abs >= max_value) {
            ctx_abs = max_value - 1;
        }
        ANNOTATE_CTX(band, THRESH8, 0, ctx_abs >> min_threshold);
        ANNOTATE_CTX(band, THRESH8, 2, cur_exponent - min_threshold);

        return model_.residual_threshold_counts_.at(std::min(block_type, BLOCK_TYPES - 1),
                                                     std::min(abs(compute_lak(context, band)), max_value - 1) >> min_threshold,
                                                     cur_exponent - min_threshold);
    }
    void residual_thresh_array_annot_update(const unsigned int band,
                                            uint16_t cur_serialized_thresh_value) {
        (void)band;
        (void)cur_serialized_thresh_value;
        ANNOTATE_CTX(band, THRESH8, 1, cur_serialized_thresh_value);
    }
    enum SignValue {
        ZERO_SIGN=0,
        POSITIVE_SIGN=1,
        NEGATIVE_SIGN=2,
    };
    Branch& sign_array(const unsigned int block_type,
                       const unsigned int band,
                       BlockContext context) {
        int ctx0 = 0;
        int ctx1 = 0;
        if (band == 0) {
            ANNOTATE_CTX(0, SIGNDC, 0, 1);
        } else if (band < 8 || band % 8 == 0) {
            int16_t val = compute_lak(context, band);
            ctx0 = exp_len(abs(val));
            ctx1 = (val == 0 ? 0 : (val > 0 ? 1 : 2));
            ANNOTATE_CTX(band, SIGN8, 0, ctx0);
            ANNOTATE_CTX(band, SIGN8, 1, ctx1);
            ctx0 += 1; // so as not to interfere with SIGNDC
        } else {
            if (context.has_left()) {
                int16_t coef = context.left_unchecked().coefficients().raster(band);
                if (coef < 0) {
                    ctx0 += 2;
                } else if (coef > 0) {
                    ctx0 += 1;
                }
            }
            if (context.has_above()) {
                int16_t coef = context.above_unchecked().coefficients().raster(band);
                if (coef < 0) {
                    ctx0 += 6;
                } else if (coef > 0) {
                    ctx0 += 3;
                }
            }
            ANNOTATE_CTX(band, SIGN7x7, 0, ctx0);
            ctx0 = 0;
            ctx1 = 0;
        }
        return model_.sign_counts_
            .at(std::min(block_type, BLOCK_TYPES - 1))
            .at(ctx1).at(ctx0);
    }
    int get_max_value(int coord) {
        static const unsigned short int freqmax[] =
            {
                1024,  931,  932,  985,  858,  985,  968,  884, 
                884,  967, 1020,  841,  871,  840, 1020,  968, 
                932,  875,  876,  932,  969, 1020,  838,  985, 
                844,  985,  838, 1020, 1020,  854,  878,  967, 
                967,  878,  854, 1020,  854,  871,  886, 1020, 
                886,  871,  854,  854,  870,  969,  969,  870, 
                854,  838, 1010,  838, 1020,  837, 1020,  969, 
                969, 1020,  838, 1020,  838, 1020, 1020,  838
            };
        return (freqmax[zigzag[coord]] + quantization_table_[zigzag[coord]] - 1) / quantization_table_[zigzag[coord]];
    }
    void optimize();
    void serialize( std::ofstream & output ) const;

    static ProbabilityTables get_probability_tables();

    // this reduces the counts to something easier to override by new data
    void normalize();
    

    enum Printability{
        PRINTABLE_INSIGNIFICANT = 1,
        PRINTABLE_OK = 2,
        CLOSE_TO_50 = 4,
        CLOSE_TO_ONE_ANOTHER = 8
    };
    struct PrintabilitySpecification {
        uint64_t printability_bitmask;
        double tolerance;
        uint64_t min_samples;
    };
    const ProbabilityTables& debug_print(const ProbabilityTables* other, PrintabilitySpecification spec)const;
};

#endif /* DECODER_HH */
