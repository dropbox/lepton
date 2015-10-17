#ifndef MODEL_HH
#define MODEL_HH

#include <vector>
#include <memory>

#include "../util/options.hh"
#include "../util/nd_array.hh"
#include "numeric.hh"
#include "branch.hh"
#include "block.hh"
#include "../util/aligned_block.hh"
#include "../util/block_based_image.hh"
#include <smmintrin.h>
#include <immintrin.h>
#include <emmintrin.h>

class BoolEncoder;
class Slice;


constexpr unsigned int MAX_EXPONENT = 12;
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

    typedef Sirikata::Array5d<Branch,
                    BLOCK_TYPES,
                    15,
                    NUM_NONZEROS_BINS,
                    NUMERIC_LENGTH_MAX,
                    MAX_EXPONENT> ExponentCounts8;

    typedef Sirikata::Array5d<Branch,
                              BLOCK_TYPES,
                              49,
                              NUM_NONZEROS_BINS,
                              NUMERIC_LENGTH_MAX,
                              MAX_EXPONENT> ExponentCounts7x7;

typedef Sirikata::Array4d<Branch,
                          BLOCK_TYPES,
                          NUM_NONZEROS_BINS,
                          NUMERIC_LENGTH_MAX,
                          MAX_EXPONENT> ExponentCountsDC;

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
    const Model& debug_print(const Model* other, PrintabilitySpecification spec)const;

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
void optimize_model(Model&model);
void serialize_model(const Model & model, int output_fd);
void reset_model(Model &model);
void normalize_model(Model &model);
void load_model(Model &model, const char* filename);

class ProbabilityTablesBase {
protected:
    Model model_;
    static int32_t icos_idct_edge_8192_dequantized_x_[3][64] __attribute__ ((aligned (16)));
    
    static int32_t icos_idct_edge_8192_dequantized_y_[3][64] __attribute__ ((aligned (16)));
    
    static int32_t icos_idct_linear_8192_dequantized_[3][64] __attribute__ ((aligned (16)));
    static uint16_t quantization_table_[3][64] __attribute__ ((aligned(16)));
    static uint16_t freqmax_[3][64] __attribute__ ((aligned (16)));
    static uint8_t bitlen_freqmax_[3][64] __attribute__ ((aligned (16)));
    static uint8_t min_noise_threshold_[3][64] __attribute__((aligned(16)));
public:
    Model &model() {return model_;}
    void load_probability_tables();
    static uint16_t* quantization_table(uint8_t color) {
        return quantization_table_[color];
    }

    static uint16_t quantization_table(uint8_t color, uint8_t coef) {
        return quantization_table_[color][coef];
    }
    static uint16_t freqmax(uint8_t color, uint8_t coef) {
        return freqmax_[color][coef];
    }
    static uint8_t bitlen_freqmax(uint8_t color, uint8_t coef) {
        return bitlen_freqmax_[color][coef];
    }
    static uint8_t min_noise_threshold(uint8_t color, uint8_t coef) {
        return min_noise_threshold_[color][coef];
    }
    static void set_quantization_table(BlockType color, const unsigned short quantization_table[64]) {
        for (int i = 0; i < 64; ++i) {
            quantization_table_[(int)color][i] = quantization_table[zigzag[i]];
        }
        for (int pixel_row = 0; pixel_row < 8; ++pixel_row) {
            for (int i = 0; i < 8; ++i) {
                icos_idct_linear_8192_dequantized((int)color)[pixel_row * 8 + i] = icos_idct_linear_8192_scaled[pixel_row * 8 + i] * quantization_table_[(int)color][i];
                icos_idct_edge_8192_dequantized_x((int)color)[pixel_row * 8 + i] = icos_base_8192_scaled[i * 8] * quantization_table_[(int)color][i * 8 + pixel_row];
                icos_idct_edge_8192_dequantized_y((int)color)[pixel_row * 8 + i] = icos_base_8192_scaled[i * 8] * quantization_table_[(int)color][pixel_row * 8 + i];
            }
        }
        static const unsigned short int freqmax[] =
        {
            1024, 931, 985, 968, 1020, 968, 1020, 1020,
            932, 858, 884, 840, 932, 838, 854, 854,
            985, 884, 871, 875, 985, 878, 871, 854,
            967, 841, 876, 844, 967, 886, 870, 837,
            1020, 932, 985, 967, 1020, 969, 1020, 1020,
            969, 838, 878, 886, 969, 838, 969, 838,
            1020, 854, 871, 870, 1010, 969, 1020, 1020,
            1020, 854, 854, 838, 1020, 838, 1020, 838
        };
        for (int coord = 0; coord < 64; ++coord) {
            freqmax_[(int)color][coord] = (freqmax[coord] + quantization_table_[(int)color][coord] - 1)
                / quantization_table_[(int)color][coord];
            uint8_t max_len = uint16bit_length(freqmax_[(int)color][coord]);
            bitlen_freqmax_[(int)color][coord] = max_len;
            if (max_len > (int)RESIDUAL_NOISE_FLOOR) {
                min_noise_threshold_[(int)color][coord] = max_len - RESIDUAL_NOISE_FLOOR;
            }
        }
    }
    static int32_t *icos_idct_edge_8192_dequantized_x(int color) {
        return icos_idct_edge_8192_dequantized_x_[(int)color];
    }
    static int32_t *icos_idct_edge_8192_dequantized_y(int color) {
        return icos_idct_edge_8192_dequantized_y_[(int)color];
    }
    static int32_t *icos_idct_linear_8192_dequantized(int color) {
        return icos_idct_linear_8192_dequantized_[(int)color];
    }
    struct CoefficientContext {
        int best_prior; //lakhani or aavrg depending on coefficient number
        uint8_t num_nonzeros_bin; // num_nonzeros mapped into a bin
        uint8_t bsr_best_prior;
    };
    enum {
        VECTORIZE = ::VECTORIZE,
        MICROVECTORIZE = ::MICROVECTORIZE
    };
};

#define USE_TEMPLATIZED_COLOR
#ifdef USE_TEMPLATIZED_COLOR
#define TEMPLATE_ARG_COLOR0 BlockType::Y
#define TEMPLATE_ARG_COLOR1 BlockType::Cb
#define TEMPLATE_ARG_COLOR2 BlockType::Cr

#else
#define TEMPLATE_ARG_COLOR0 BlockType::Y
#define TEMPLATE_ARG_COLOR1 BlockType::Y
#define TEMPLATE_ARG_COLOR2 BlockType::Y
#endif
template <bool left_present, bool above_present, bool above_right_present, BlockType
#ifdef USE_TEMPLATIZED_COLOR
              color
#else
              deprecated_color
#endif
>
class ProbabilityTables
{
private:
    typedef ProbabilityTablesBase::CoefficientContext CoefficientContext;
public:
#ifdef USE_TEMPLATIZED_COLOR
    enum {
        COLOR = (int)color
    };
    ProbabilityTables(BlockType kcolor) {
        assert(kcolor == color);
    }
#else
    const BlockType COLOR;
    ProbabilityTables(BlockType color) : COLOR(color){
        static_assert((int)deprecated_color == 0, "Using dynamic color");
    }
#endif
    void reset(ProbabilityTablesBase&base) {
        reset_model(base.model());
    }
    void load(ProbabilityTablesBase&base, const char * filename) {
        load_model(base.model(), filename);
    }
    int color_index() {
        if ((int)COLOR == BLOCK_TYPES || ((int)BLOCK_TYPES == 1 && (int)COLOR > (int)BLOCK_TYPES)) {
            return BLOCK_TYPES - 1;
        }
        return (int)COLOR;
    }
    ProbabilityTablesBase::CoefficientContext get_dc_coefficient_context(const ConstBlockContext block, uint8_t num_nonzeros) {
        CoefficientContext retval;
        retval.best_prior = compute_aavrg_dc(block);
        retval.bsr_best_prior = bit_length(retval.best_prior);
        retval.num_nonzeros_bin = num_nonzeros_to_bin(num_nonzeros);
        return retval;
    }
    void update_coefficient_context7x7(int coord,
                                       int aligned_zz,
                                       ProbabilityTablesBase::CoefficientContext & retval,
                                       const ConstBlockContext block, uint8_t num_nonzeros_left) {
        if (aligned_zz < 45) {
            //This was to make sure the code was right compute_aavrg_vec(aligned_zz, block);
        }
        retval.best_prior = compute_aavrg(coord, aligned_zz, block);
        retval.num_nonzeros_bin = num_nonzeros_to_bin(num_nonzeros_left);
        retval.bsr_best_prior = bit_length(retval.best_prior);
    }
    void update_coefficient_context7x7(int aligned_zz,
                                       ProbabilityTablesBase::CoefficientContext & retval,
                                       int aavrg,
                                       const ConstBlockContext block, uint8_t num_nonzeros_left) {
        assert(aavrg == compute_aavrg(aligned_to_raster.at(aligned_zz), aligned_zz, block));
        //This was to make sure the code was right compute_aavrg_vec(aligned_zz, block);
        retval.best_prior = aavrg;
        retval.num_nonzeros_bin = num_nonzeros_to_bin(num_nonzeros_left);
        retval.bsr_best_prior = bit_length(retval.best_prior);
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context8(uint8_t coefficient,
                                                   const ConstBlockContext block, uint8_t num_nonzeros_x) {
        CoefficientContext retval = {0, 0, 0};
        if (MICROVECTORIZE) {
            retval.best_prior = (coefficient & 7)
            ? compute_lak_horizontal(block, coefficient) : compute_lak_vertical(block, coefficient);
        } else {
            retval.best_prior = compute_lak(block, coefficient);
        }
        retval.num_nonzeros_bin = num_nonzeros_x;
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context8_horiz(uint8_t coefficient,
                                                   const ConstBlockContext block, uint8_t num_nonzeros_x) {
        CoefficientContext retval = {0, 0, 0};
        retval.best_prior = compute_lak_horizontal(block, coefficient);
        retval.num_nonzeros_bin = num_nonzeros_x;
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context8_vert(uint8_t coefficient,
                                                   const ConstBlockContext block, uint8_t num_nonzeros_x) {
        CoefficientContext retval = {0, 0, 0};
        retval.best_prior = compute_lak_vertical(block, coefficient);
        retval.num_nonzeros_bin = num_nonzeros_x;
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
#define INSTANTIATE_TEMPLATE_METHOD(N)  \
    ProbabilityTablesBase::CoefficientContext update_coefficient_context8_templ##N(const ConstBlockContext block, \
                                                   uint8_t num_nonzeros_x) { \
        ProbabilityTablesBase::CoefficientContext retval = {0, 0, 0};     \
        retval.best_prior = compute_lak_templ<N>(block); \
        retval.num_nonzeros_bin = num_nonzeros_x; \
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023)); \
        return retval; \
    }
    INSTANTIATE_TEMPLATE_METHOD(1)
    INSTANTIATE_TEMPLATE_METHOD(2)
    INSTANTIATE_TEMPLATE_METHOD(3)
    INSTANTIATE_TEMPLATE_METHOD(4)
    INSTANTIATE_TEMPLATE_METHOD(5)
    INSTANTIATE_TEMPLATE_METHOD(6)
    INSTANTIATE_TEMPLATE_METHOD(7)
    INSTANTIATE_TEMPLATE_METHOD(8)
    INSTANTIATE_TEMPLATE_METHOD(16)
    INSTANTIATE_TEMPLATE_METHOD(24)
    INSTANTIATE_TEMPLATE_METHOD(32)
    INSTANTIATE_TEMPLATE_METHOD(40)
    INSTANTIATE_TEMPLATE_METHOD(48)
    INSTANTIATE_TEMPLATE_METHOD(56)
    Sirikata::Array2d<Branch, 6, 32>::Slice nonzero_counts_7x7(ProbabilityTablesBase &pt,
                                                               const ConstBlockContext block) {
        uint8_t num_nonzeros_above = 0;
        uint8_t num_nonzeros_left = 0;
        if (above_present) {
            num_nonzeros_above = block.nonzeros_above_7x7_unchecked();
        }
        if (left_present) {
            num_nonzeros_left = block.nonzeros_left_7x7_unchecked();
        }

        uint8_t num_nonzeros_context = 0;
        if (above_present && !left_present) {
            num_nonzeros_context = (num_nonzeros_above + 1) / 2;
        } else if (left_present && !above_present) {
            num_nonzeros_context = (num_nonzeros_left + 1) / 2;
        } else if (left_present && above_present) {
            num_nonzeros_context = (num_nonzeros_above + num_nonzeros_left + 2) / 4;
        }
        ANNOTATE_CTX(0, ZEROS7x7, 0, num_nonzeros_context);
        return pt.model().num_nonzeros_counts_7x7_.at(color_index(),
                                                                     num_nonzeros_to_bin(num_nonzeros_context));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice x_nonzero_counts_8x1(ProbabilityTablesBase &pt,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 1, eob_x);
        return pt.model().num_nonzeros_counts_8x1_.at(color_index(), eob_x, ((num_nonzeros + 3) / 7));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice y_nonzero_counts_1x8(ProbabilityTablesBase &pt,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, is_x?ZEROS8x1:ZEROS1x8, 1, eob_x);
        return pt.model().num_nonzeros_counts_1x8_.at(color_index(), eob_x, ((num_nonzeros + 3) / 7));
    }
    Sirikata::Array1d<Branch, MAX_EXPONENT>::Slice exponent_array_x(ProbabilityTablesBase &pt, int band, int zig15, CoefficientContext context) {
        ANNOTATE_CTX(band, EXP8, 0, context.bsr_best_prior);
        ANNOTATE_CTX(band, EXP8, 1, context.num_nonzeros);
        assert((band & 7)== 0 ? ((band >>3) + 7) : band - 1 == zig15);
        return pt.model().exponent_counts_x_.at(color_index(),
                                             zig15,
                                             context.num_nonzeros_bin,
                                             context.bsr_best_prior);
    }
    Sirikata::Array1d<Branch, MAX_EXPONENT>::Slice exponent_array_7x7(ProbabilityTablesBase &pt,
                                                                      const unsigned int band,
                                                                      const unsigned int zig49,
                                                                      const CoefficientContext context) {
        ANNOTATE_CTX(band, EXP7x7, 0, context.bsr_best_prior);
        ANNOTATE_CTX(band, EXP7x7, 1, context.num_nonzeros_bin);
        return pt.model().exponent_counts_.at(color_index(),
            zig49,
            context.bsr_best_prior,
            context.num_nonzeros_bin);
    }
    Sirikata::Array1d<Branch, MAX_EXPONENT>::Slice exponent_array_dc(ProbabilityTablesBase &pt,
                                                                     const CoefficientContext context) {
        ANNOTATE_CTX(0, EXPDC, 0, context.bsr_best_prior);
        ANNOTATE_CTX(0, EXPDC, 1, context.num_nonzeros_bin);
        return pt.model().exponent_counts_dc_
            .at(color_index(),
                context.num_nonzeros_bin,
                context.bsr_best_prior);
    }
    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_x(ProbabilityTablesBase &pt,
                                                          const unsigned int band,
                                                          const CoefficientContext context) {
        ANNOTATE_CTX(band, RES8, 0, num_nonzeros_x);
        return residual_noise_array_shared(pt, band,
                                           context);
    }

    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_shared(ProbabilityTablesBase &pt,
                                                            const unsigned int band,
                                                            const CoefficientContext context) {
        return pt.model().residual_noise_counts_.at(color_index(),
                                                 band/band_divisor,
                                                 context.num_nonzeros_bin);
    }
    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_noise_array_7x7(ProbabilityTablesBase &pt,
                                                            const unsigned int band,
                                                            const CoefficientContext context) {
        if (band == 0) {
            ANNOTATE_CTX(0, RESDC, 0, num_nonzeros_to_bin(num_nonzeros));
        } else {
            ANNOTATE_CTX(band, RES7x7, 0, num_nonzeros_to_bin(num_nonzeros));
        }
        return residual_noise_array_shared(pt, band, context);
    }
    unsigned int num_nonzeros_to_bin(uint8_t num_nonzeros) {
        return nonzero_to_bin[NUM_NONZEROS_BINS-1][num_nonzeros];
    }
    int idct_2d_8x1(const AlignedBlock&block, bool ignore_first, int pixel_row) {
        int retval = 0;
        if (!ignore_first) {
            retval = block.coefficients_raster(0) * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 0];
        }
        retval += block.coefficients_raster(1)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 1];
        retval += block.coefficients_raster(2)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 2];
        retval += block.coefficients_raster(3)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 3];
        retval += block.coefficients_raster(4)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 4];
        retval += block.coefficients_raster(5)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 5];
        retval += block.coefficients_raster(6)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 6];
        retval += block.coefficients_raster(7)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 7];
        return retval;
    }

    int idct_2d_1x8(const AlignedBlock&block, bool ignore_first, int pixel_row) {
        int retval = 0;
        if (!ignore_first) {
            retval = block.dc() * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 0];
        }
        retval += block.coefficients_raster(8)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 1];
        retval += block.coefficients_raster(16)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 2];
        retval += block.coefficients_raster(24)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 3];
        retval += block.coefficients_raster(32)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 4];
        retval += block.coefficients_raster(40)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 5];
        retval += block.coefficients_raster(48)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 6];
        retval += block.coefficients_raster(56)
            * ProbabilityTablesBase::icos_idct_linear_8192_dequantized((int)COLOR)[pixel_row * 8 + 7];
        return retval;
    }

    int predict_dc_dct(const ConstBlockContext&context) {
        int prediction = 0;
        int left_block = 0;
        int left_edge = 0;
        int above_block = 0;
        int above_edge = 0;
        if (left_present) {
            left_block = idct_2d_8x1(context.left_unchecked(), 0, 7);
            left_edge = idct_2d_8x1(context.here(), 1, 0);
        }
        if (above_present) {
            above_block = idct_2d_1x8(context.above_unchecked(), 0, 7);
            above_edge = idct_2d_1x8(context.here(), 1, 0);
        }
        if (left_present) {
            if (above_present) {
                prediction = ( ( left_block - left_edge ) + (above_block - above_edge) ) * 4;
            } else {
                prediction = ( left_block - left_edge ) * 8;
            }
        } else if (above_present) {
            prediction = ( above_block - above_edge ) * 8;
        }
        int DCT_RSC = 8192; 
        prediction = std::max(-1024 * DCT_RSC, std::min(1016 * DCT_RSC, prediction));
        prediction /= ProbabilityTablesBase::quantization_table((int)COLOR, 0);
        int round = DCT_RSC/2;
        if (prediction < 0) {
            round = -round;
        }
        return (prediction + round) / DCT_RSC;
    }
    int predict_locoi_dc_deprecated(const ConstBlockContext&context) {
        if (left_present) {
            int a = context.left_unchecked().dc();
            if (above_present) {
                int b = context.above_unchecked().dc();
                int c = context.above_left_unchecked().dc();
                if (c >= std::max(a,b)) {
                    return std::min(a,b);
                } else if (c <= std::min(a,b)) {
                    return std::max(a,b);
                }
                return a + b - c;
            }else { 
                return a;
            }
        } else if (above_present) {
            return context.above_unchecked().dc();
        } else {
            return 0;
        }
    }
    int predict_or_unpredict_dc(const ConstBlockContext&context, bool recover_original) {
        int max_value = 0;
        if (ProbabilityTablesBase::quantization_table((int)COLOR, 0)){
            max_value = (1024 + ProbabilityTablesBase::quantization_table((int)COLOR,0) - 1) / ProbabilityTablesBase::quantization_table((int)COLOR, 0);
        }
        int min_value = -max_value;
        int adjustment_factor = 2 * max_value + 1;
        int retval = //predict_locoi_dc_deprecated(block);
            predict_dc_dct(context);
        retval = context.here().dc() + (recover_original ? retval : -retval);
        if (retval < min_value) retval += adjustment_factor;
        if (retval > max_value) retval -= adjustment_factor;
        return retval;
    }
    int compute_aavrg_dc(ConstBlockContext context) {
        return compute_aavrg(0, raster_to_aligned.at(0), context);
        
        uint32_t total = 0;
        if (left_present) {
            total += abs(context.left_unchecked().dc());
        }
        if (above_present) {
            total += abs(context.above_unchecked().dc());
        }
        if (left_present && above_present) {
            constexpr unsigned int log_weight = 10;
            total *= 205 * 2;
            total += 204 * abs(context.above_left_unchecked().dc());
            total += (1 << (log_weight - 1)); //rounding
            return total >> log_weight;
        } else {
            return total;
        }
    }
    int compute_aavrg(unsigned int coord, unsigned int aligned_zz, ConstBlockContext context) {
        uint32_t total = 0;
        if (left_present) {
            total += abs(context.left_unchecked().coefficients_raster(coord));
        }
        if (above_present) {
            total += abs(context.above_unchecked().coefficients_raster(coord));
        }
        if (left_present && above_present) {
            constexpr unsigned int log_weight = 10;
            total *= 205 * 2;
            total += 204 * abs(context.above_left_unchecked().coefficients_raster(coord));
            total += (1 << (log_weight - 1));
            return total >> log_weight;
        } else {
            return total;
        }
        //if (block.context().above_right.initialized()) {
        //total += abs(block.context().above_right.get()->coefficients().at(0));
        //}
    }
#ifdef OPTIMIZED_7x7
    bool aavrg_vec_matches(__m128i retval, unsigned int aligned_zz, ConstBlockContext context) {
        int ret[4];
        _mm_storeu_si128((__m128i*)(char*)ret, retval);
        int correct[4] = {compute_aavrg(aligned_to_raster.at(aligned_zz), aligned_zz +0, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+1), aligned_zz + 1, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+2), aligned_zz + 2, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+3), aligned_zz + 3, context)};
        return memcmp(ret, correct, sizeof(correct)) == 0;
    }
    void compute_aavrg_vec(unsigned int aligned_zz, ConstBlockContext context, int* aligned_retval) {
        _mm_store_si128((__m128i*)(char*)aligned_retval, compute_aavrg_vec(aligned_zz, context));
    }
#if defined (__clang__) || defined(__GNUC__)
#define x_mm_loadu_si64(a) _mm_set1_epi64x(*(uint64_t*)(char*)(a))
#else
#define x_mm_loadu_si64 _mm_loadu_si64
#endif
    __m128i compute_aavrg_vec(unsigned int aligned_zz, ConstBlockContext context) {
        if (left_present == false && above_present == false) {
            return _mm_setzero_si128();
        }
        __m128i left;
        if (left_present) {
            left = _mm_cvtepi16_epi32(_mm_abs_epi16(x_mm_loadu_si64(&context.left_unchecked().coef.at(aligned_zz))));
            if (!above_present) {
                return left;
            }
        }
        __m128i above;
        if (above_present) {
            above = _mm_cvtepi16_epi32(_mm_abs_epi16(x_mm_loadu_si64(&context.above_unchecked().coef.at(aligned_zz))));
            if (!left_present) {
                return above;
            }
        }
        constexpr unsigned int log_weight = 10;
        __m128i total = _mm_add_epi32(left, above);
        total = _mm_mullo_epi32(total, _mm_set1_epi32(205 * 2));
        __m128i aboveleft =_mm_cvtepi16_epi32(_mm_abs_epi16(x_mm_loadu_si64(&context.above_left_unchecked().coef.at(aligned_zz))));
        total = _mm_add_epi32(total, _mm_mullo_epi32(aboveleft, _mm_set1_epi32(204)));
        total = _mm_add_epi32(total, _mm_set1_epi32(1 << (log_weight - 1)));
        __m128i retval = _mm_srli_epi32(total, log_weight);
        assert(aavrg_vec_matches(retval, aligned_zz, context));
        return retval;
        //if (block.context().above_right.initialized()) {
        //total += abs(block.context().above_right.get()->coefficients().at(0));
        //}
    }
#endif
    static int32_t compute_lak_vec(__m128i coeffs_x_low, __m128i coeffs_x_high, __m128i coeffs_a_low, __m128i coeffs_a_high, const int32_t *icos_deq) {
        __m128i sign_mask = _mm_set_epi32(-1, 1, -1, 1); // ((i & 1) ? -1 : 1)

        //coeffs_x[i] = ((i & 1) ? -1 : 1) * coeffs_a[i] - coeffs_x[i];
        coeffs_a_low = _mm_sign_epi32(coeffs_a_low, sign_mask);
        coeffs_a_high = _mm_sign_epi32(coeffs_a_high, sign_mask);
        coeffs_x_low = _mm_sub_epi32(coeffs_a_low, coeffs_x_low);
        coeffs_x_high = _mm_sub_epi32(coeffs_a_high, coeffs_x_high);

        __m128i icos_low = _mm_load_si128((const __m128i*)(const char*)icos_deq);
        __m128i icos_high = _mm_load_si128((const __m128i*)(const char*)(icos_deq + 4));
        // coeffs_x[i] *= icos[i]
        __m128i deq_low = _mm_mullo_epi32(coeffs_x_low, icos_low);
        __m128i deq_high = _mm_mullo_epi32(coeffs_x_high, icos_high);

        __m128i sum = _mm_add_epi32(deq_low, deq_high);
        sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 8));
        sum = _mm_add_epi32(sum, _mm_srli_si128(sum, 4));
        // coeffs_x[0] = sum(coeffs_x)
        int32_t prediction = _mm_cvtsi128_si32(sum);
        //if (prediction > 0) { <-- rounding hurts prediction perf and costs compute  this rounding didn't round the same way as the unvectorized one anyhow
        //    prediction += icos_deq[0]/2;
        //} else {
        //    prediction -= icos_deq[0]/2; // round away from zero
        //}
        return prediction / icos_deq[0];
    }
#define ITER(x_var, a_var, i, step) \
        (x_var = _mm_set_epi32(   context.here().coefficients_raster(band + step * ((i) + 3)), \
                                  context.here().coefficients_raster(band + step * ((i) + 2)), \
                                  context.here().coefficients_raster(band + step * ((i) + 1)), \
                                  i == 0 ? 0 : context.here().coefficients_raster(band + step * (i))), \
         a_var = _mm_set_epi32(neighbor.coefficients_raster(band + step * ((i) + 3)), \
                                  neighbor.coefficients_raster(band + step * ((i) + 2)), \
                                  neighbor.coefficients_raster(band + step * ((i) + 1)), \
                                  neighbor.coefficients_raster(band + step * (i))))
    
    template<int band> __attribute__((always_inline))
    int32_t compute_lak_templ(const ConstBlockContext&context) {
        __m128i coeffs_x_low;
        __m128i coeffs_x_high;
        __m128i coeffs_a_low;
        __m128i coeffs_a_high;
        const int32_t * icos = nullptr;
        static_assert((band & 7) == 0 || (band >> 3) == 0, "This function only works on edges");
        if ((band >> 3) == 0) {
            if(!above_present) {
                return 0;
            }
            const auto &neighbor = context.above_unchecked();
            ITER(coeffs_x_low, coeffs_a_low, 0, 8);
            ITER(coeffs_x_high, coeffs_a_high, 4, 8);
            icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x((int)COLOR) + band * 8;
        } else {
            if (!left_present) {
                return 0;
            }
            const auto &neighbor = context.left_unchecked();
            ITER(coeffs_x_low, coeffs_a_low, 0, 1);
            ITER(coeffs_x_high, coeffs_a_high, 4, 1);
            icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_y((int)COLOR) + band;
        }
        return compute_lak_vec(coeffs_x_low, coeffs_x_high, coeffs_a_low, coeffs_a_high, icos);
    }
    int32_t compute_lak_horizontal(const ConstBlockContext&context, unsigned int band) {
        if (!above_present) {
            return 0;
        }
        __m128i coeffs_x_low;
        __m128i coeffs_x_high;
        __m128i coeffs_a_low;
        __m128i coeffs_a_high;
        assert(band/8 == 0 && "this function only works for the top edge");
        const auto &neighbor = context.above_unchecked();
        ITER(coeffs_x_low, coeffs_a_low, 0, 8);
        ITER(coeffs_x_high, coeffs_a_high, 4, 8);
        const int32_t * icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x((int)COLOR) + band * 8;
        return compute_lak_vec(coeffs_x_low, coeffs_x_high, coeffs_a_low, coeffs_a_high, icos);
    }
    int32_t compute_lak_vertical(const ConstBlockContext&context, unsigned int band) {
        assert((band & 7) == 0 && "Must be used for veritcal");
        if (!left_present) {
            return 0;
        }
        __m128i coeffs_x_low;
        __m128i coeffs_x_high;
        __m128i coeffs_a_low;
        __m128i coeffs_a_high;
        const auto &neighbor = context.left_unchecked();
        ITER(coeffs_x_low, coeffs_a_low, 0, 1);
        ITER(coeffs_x_high, coeffs_a_high, 4, 1);
#undef ITER
        const int32_t *icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_y((int)COLOR) + band;
        return compute_lak_vec(coeffs_x_low, coeffs_x_high, coeffs_a_low, coeffs_a_high,
                        icos);
    }
    int32_t compute_lak(const ConstBlockContext&context, unsigned int band) {
        int coeffs_x[8];
        int coeffs_a[8];
        const int32_t *coef_idct = nullptr;
        if ((band & 7) && above_present) {
            // y == 0: we're the x
            assert(band/8 == 0); //this function only works for the edge
            const auto &above = context.above_unchecked();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i * 8;
                coeffs_x[i]  = i ? context.here().coefficients_raster(cur_coef) : 0;
                coeffs_a[i]  = above.coefficients_raster(cur_coef);
            }
            coef_idct = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x((int)COLOR) + band * 8;
        } else if ((band & 7) == 0 && left_present) {
            // x == 0: we're the y
            const auto &left = context.left_unchecked();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i;
                coeffs_x[i]  = i ? context.here().coefficients_raster(cur_coef) : 0;
                coeffs_a[i]  = left.coefficients_raster(cur_coef);
            }
            coef_idct = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_y((int)COLOR) + band;
        } else {
            return 0;
        }
        int prediction = coeffs_a[0] * coef_idct[0]; // rounding towards zero before adding coeffs_a[0] helps ratio slightly, but this is cheaper
        for (int i = 1; i < 8; ++i) {
            int sign = (i & 1) ? 1 : -1;
            prediction -= coef_idct[i] * (coeffs_x[i] + sign * coeffs_a[i]);
        }
        prediction /= coef_idct[0];
        assert(((band & 7) ? compute_lak_horizontal(context,band): compute_lak_vertical(context,band)) == prediction
               && "Vectorized version must match sequential version");
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
        residual_thresh_array(ProbabilityTablesBase &pt,
                              const unsigned int band,
                              const uint8_t cur_exponent,
                              const CoefficientContext context,
                              int min_threshold,
                              int max_value) {
        uint16_t ctx_abs = abs(context.best_prior);
        if (ctx_abs >= max_value) {
            ctx_abs = max_value - 1;
        }
        ANNOTATE_CTX(band, THRESH8, 0, ctx_abs >> min_threshold);
        ANNOTATE_CTX(band, THRESH8, 2, cur_exponent - min_threshold);

        return pt.model().residual_threshold_counts_.at(color_index(),
                                                     ctx_abs >> min_threshold,
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
    Branch& sign_array_dc(ProbabilityTablesBase &pt, CoefficientContext context) {
        ANNOTATE_CTX(0, SIGNDC, 0, 1);
        return pt.model().sign_counts_.at(color_index(), 0, 1);
    }
    Branch& sign_array_7x7(ProbabilityTablesBase &pt, uint8_t band, CoefficientContext context) {
        ANNOTATE_CTX(band, SIGN7x7, 0, 0);
        return pt.model().sign_counts_.at(color_index(), 0, 0);
    }
    Branch& sign_array_8(ProbabilityTablesBase &pt, uint8_t band, CoefficientContext context) {

        int16_t val = context.best_prior;
        uint8_t ctx0 = context.bsr_best_prior;
        uint8_t ctx1 = (val == 0 ? 0 : (val > 0 ? 1 : 2));
        ANNOTATE_CTX(band, SIGN8, 0, ctx0);
        ANNOTATE_CTX(band, SIGN8, 1, ctx1);
        return pt.model().sign_counts_.at(color_index(), ctx1, ctx0);
    }
    int get_max_value(int coord) {
        return ProbabilityTablesBase::freqmax((int)COLOR, coord);
    }
    uint8_t get_noise_threshold(int coord) {
        return ProbabilityTablesBase::min_noise_threshold((int)COLOR, coord);
    }
    void optimize(ProbabilityTablesBase &pt) {
        optimize_model(pt.model());
    }
    void serialize(ProbabilityTablesBase &pt, int output_fd ) const{
        serialize_model(pt.model(), output_fd);
    }

    // this reduces the counts to something easier to override by new data
    void normalize(ProbabilityTablesBase &pt) {
        normalize_model(pt.model());
    }
    
};

#endif /* DECODER_HH */
