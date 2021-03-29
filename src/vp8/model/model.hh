#ifndef MODEL_HH
#define MODEL_HH

#include <vector>
#include <memory>

#include "../util/debug.hh"
#include "../util/options.hh"
#include "../util/nd_array.hh"
#include "../../lepton/idct.hh"
#include "numeric.hh"
#include "branch.hh"
#include "../util/aligned_block.hh"
#include "../util/block_based_image.hh"

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <tmmintrin.h>
#include "../util/mm_mullo_epi32.hh"
#endif

enum F_TYPE {
    JPEG = 0,
    UJG = 1,
    LEPTON=2,
    UNK = 3
};

extern F_TYPE filetype;

constexpr bool advanced_dc_prediction = true;
enum TableParams : unsigned int {
    MAX_EXPONENT = 11,
    BLOCK_TYPES = 2, // setting this to 3 gives us ~1% savings.. 2/3 from BLOCK_TYPES=2
    NUM_NONZEROS_BINS = 10,
    BSR_BEST_PRIOR_MAX = 11, // 1023 requires 11 bits to describe
    band_divisor = 1,
    COEF_BANDS = 64 / band_divisor,
    ENTROPY_NODES = 15,
    NUM_NONZEROS_EOB_PRIORS = 66,
    ZERO_OR_EOB = 3,
    RESIDUAL_NOISE_FLOOR = 7,
    COEF_BITS = MAX_EXPONENT - 1, // the last item of the length is always 1
};
int get_sum_median_8(int16_t*data16i);
void set_branch_range_identity(Branch *start, Branch* end);

template <class BranchArray> void set_branch_array_identity(BranchArray &branches) {
    auto begin = branches.begin();
    auto end = branches.end();
    set_branch_range_identity(begin, end);
    /*
    for (;false&&begin != end; ++begin) {
        begin->set_identity();
    }*/
}
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
                              (8 > NUM_NONZEROS_BINS?8:(unsigned int)NUM_NONZEROS_BINS),
                              COEF_BITS> ResidualNoiseCounts;

    ResidualNoiseCounts residual_noise_counts_;

    typedef Sirikata::Array2d<Branch,
                              NUMERIC_LENGTH_MAX,
                              COEF_BITS> ResidualNoiseCountsDc;

    ResidualNoiseCountsDc residual_noise_counts_dc_;

    typedef Sirikata::Array4d<Branch,
                              BLOCK_TYPES,
                              (1<<(1 + RESIDUAL_NOISE_FLOOR)),
                              1 + RESIDUAL_NOISE_FLOOR,
                              1<<RESIDUAL_NOISE_FLOOR > ResidualThresholdCounts;

    ResidualThresholdCounts residual_threshold_counts_;

    typedef Sirikata::Array5d<Branch,
                    BLOCK_TYPES,
                    NUM_NONZEROS_BINS,
                    15,
                    NUMERIC_LENGTH_MAX,
                    MAX_EXPONENT> ExponentCounts8;

    typedef Sirikata::Array5d<Branch,
                              BLOCK_TYPES,
                              NUM_NONZEROS_BINS,
                              49,
                              NUMERIC_LENGTH_MAX,
                              MAX_EXPONENT> ExponentCounts7x7;

typedef Sirikata::Array3d<Branch,
                          ((unsigned int)NUM_NONZEROS_BINS <= (unsigned int)NUMERIC_LENGTH_MAX
                           ? (unsigned int)NUMERIC_LENGTH_MAX : (unsigned int)NUM_NONZEROS_BINS),
                          17/*any 16 bit number should fit*/,
                          MAX_EXPONENT> ExponentCountsDC;

  ExponentCounts7x7 exponent_counts_;
  ExponentCounts8 exponent_counts_x_;
  ExponentCountsDC exponent_counts_dc_;
  void set_tables_identity() {
      set_branch_array_identity(num_nonzeros_counts_7x7_);
      set_branch_array_identity(num_nonzeros_counts_1x8_);
      set_branch_array_identity(num_nonzeros_counts_8x1_);
      set_branch_array_identity(residual_noise_counts_);
      set_branch_array_identity(residual_noise_counts_dc_);
      set_branch_array_identity(residual_threshold_counts_);
      set_branch_array_identity(exponent_counts_);
      set_branch_array_identity(exponent_counts_x_);
      set_branch_array_identity(exponent_counts_dc_);
      set_branch_array_identity(sign_counts_);
  }
  typedef Sirikata::Array3d<Branch, BLOCK_TYPES, 4, NUMERIC_LENGTH_MAX> SignCounts;
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
#if 0
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
#ifdef _WIN32
#define WINALIGN16 __declspec(align(16))
#define UNIXALIGN16
#else
#define WINALIGN16
#define UNIXALIGN16 __attribute__((aligned(16)))
#endif
extern volatile int volatile1024;
class ProbabilityTablesBase {
protected:
    Model model_;

    static WINALIGN16 int32_t icos_idct_edge_8192_dequantized_x_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;
    
    static WINALIGN16 int32_t icos_idct_edge_8192_dequantized_y_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;
    
    static WINALIGN16 int32_t icos_idct_linear_8192_dequantized_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;

    static WINALIGN16 uint16_t quantization_table_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;

    static WINALIGN16 uint16_t freqmax_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;

    static WINALIGN16 uint8_t bitlen_freqmax_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;

    static WINALIGN16 uint8_t min_noise_threshold_[(int)ColorChannel::NumBlockTypes][64] UNIXALIGN16;

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
            if (filetype != LEPTON && icos_idct_edge_8192_dequantized_x((int)color)[pixel_row * 8] == 0) {
                custom_exit(ExitCode::UNSUPPORTED_JPEG_WITH_ZERO_IDCT_0);
            }
            if (filetype != LEPTON && icos_idct_edge_8192_dequantized_y((int)color)[pixel_row * 8] == 0) {
                custom_exit(ExitCode::UNSUPPORTED_JPEG_WITH_ZERO_IDCT_0);
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
        always_assert(uint16bit_length(volatile1024) == 11);
        always_assert(uint16bit_length(1024) == 11);
        for (int coord = 0; coord < 64; ++coord) {
            freqmax_[(int)color][coord] = (freqmax[coord] + quantization_table_[(int)color][coord] - 1);
            if (quantization_table_[(int)color][coord]) {
                freqmax_[(int)color][coord] /= quantization_table_[(int)color][coord];
            }
            uint8_t max_len = uint16bit_length(freqmax_[(int)color][coord]);
            bitlen_freqmax_[(int)color][coord] = max_len;
            if (max_len > (int)RESIDUAL_NOISE_FLOOR) {
                min_noise_threshold_[(int)color][coord] = max_len - RESIDUAL_NOISE_FLOOR;
            } else {
                min_noise_threshold_[(int)color][coord] = 0;
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
#define TEMPLATE_ARG_COLOR3 BlockType::Ck

#else
#define TEMPLATE_ARG_COLOR0 BlockType::Y
#define TEMPLATE_ARG_COLOR1 BlockType::Y
#define TEMPLATE_ARG_COLOR2 BlockType::Y
#define TEMPLATE_ARG_COLOR3 BlockType::Y
#endif
template <bool all_present, BlockType
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
    const bool left_present;
    const bool above_present;
    const bool above_right_present;
public:
#ifdef USE_TEMPLATIZED_COLOR
    enum {
        COLOR = (int)color
    };
    ProbabilityTables(BlockType kcolor,
                      bool in_left_present,
                      bool in_above_present,
                      bool in_above_right_present)
        : left_present(in_left_present),
          above_present(in_above_present),
          above_right_present(in_above_right_present) {
       always_assert((left_present && above_present && above_right_present) == all_present);
       always_assert(kcolor == color);
    }
#else
    const BlockType COLOR;
    ProbabilityTables(BlockType color,
                      bool in_left_present,
                      bool in_above_present,
                      bool in_above_right_present)
        : left_present(in_left_present),
          above_present(in_above_present),
          above_right_present(in_above_right_present),
          COLOR(color) {
        always_assert((left_present && right_present && above_right_present) == all_present);
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
        if (BLOCK_TYPES == 2) {
            if (0 == (int)COLOR) {
                return 0;
            }
            return 1;
        } else {
            return std::min((int)(BLOCK_TYPES - 1), (int)COLOR);
        }
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context7x7(int coord,
                                       int aligned_zz,
                                       const ConstBlockContext block, uint8_t num_nonzeros_left) {
        ProbabilityTablesBase::CoefficientContext retval;
        retval.best_prior = compute_aavrg(coord, aligned_zz, block);
        retval.num_nonzeros_bin = num_nonzeros_to_bin(num_nonzeros_left);
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context7x7_precomp(int aligned_zz,
                                       int aavrg,
                                       const ConstBlockContext block, uint8_t num_nonzeros_left) {
        ProbabilityTablesBase::CoefficientContext retval;
        dev_assert(aavrg == compute_aavrg(aligned_to_raster.at(aligned_zz), aligned_zz, block));
        //This was to make sure the code was right compute_aavrg_vec(aligned_zz, block);
        retval.best_prior = aavrg;
        retval.num_nonzeros_bin = num_nonzeros_to_bin(num_nonzeros_left);
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
    ProbabilityTablesBase::CoefficientContext update_coefficient_context8(uint8_t coefficient,
                                                   const ConstBlockContext block, uint8_t num_nonzeros_x) {
        CoefficientContext retval = {0, 0, 0};
#ifndef USE_SCALAR
        if (MICROVECTORIZE) {
            retval.best_prior = (coefficient & 7)
            ? compute_lak_horizontal(block, coefficient) : compute_lak_vertical(block, coefficient);
        } else {
            retval.best_prior = compute_lak(block, coefficient);
        }
#else
        retval.best_prior = compute_lak(block, coefficient);
#endif
        retval.num_nonzeros_bin = num_nonzeros_x;
        retval.bsr_best_prior = bit_length(std::min(abs(retval.best_prior), 1023));
        return retval;
    }
#ifndef USE_SCALAR
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
#endif

    Sirikata::Array2d<Branch, 6, 32>::Slice nonzero_counts_7x7(ProbabilityTablesBase &pt,
                                                               const ConstBlockContext block) {
        uint8_t num_nonzeros_above = 0;
        uint8_t num_nonzeros_left = 0;
        if (all_present || above_present) {
            num_nonzeros_above = block.nonzeros_above_7x7_unchecked();
        }
        if (all_present || left_present) {
            num_nonzeros_left = block.nonzeros_left_7x7_unchecked();
        }

        uint8_t num_nonzeros_context = 0;
        if ((!all_present) && above_present && !left_present) {
            num_nonzeros_context = (num_nonzeros_above + 1) / 2;
        } else if ((!all_present) && left_present && !above_present) {
            num_nonzeros_context = (num_nonzeros_left + 1) / 2;
        } else if (all_present || (left_present && above_present)) {
            num_nonzeros_context = (num_nonzeros_above + num_nonzeros_left + 2) / 4;
        }
        ANNOTATE_CTX(0, ZEROS7x7, 0, num_nonzeros_context);
        return pt.model().num_nonzeros_counts_7x7_.at(color_index(),
                                                      num_nonzeros_to_bin(num_nonzeros_context));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice x_nonzero_counts_8x1(ProbabilityTablesBase &pt,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, ZEROS8x1, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, ZEROS8x1, 1, eob_x);
        return pt.model().num_nonzeros_counts_8x1_.at(color_index(), eob_x, ((num_nonzeros + 3) / 7));
    }
    Sirikata::Array2d<Branch, 3u, 4u>::Slice y_nonzero_counts_1x8(ProbabilityTablesBase &pt,
                                                          unsigned int eob_x,
                                                          unsigned int num_nonzeros) {
        ANNOTATE_CTX(0, ZEROS1x8, 0, ((num_nonzeros + 3) / 7));
        ANNOTATE_CTX(0, ZEROS1x8, 1, eob_x);
        return pt.model().num_nonzeros_counts_1x8_.at(color_index(), eob_x, ((num_nonzeros + 3) / 7));
    }
    Sirikata::Array1d<Branch, MAX_EXPONENT>::Slice exponent_array_x(ProbabilityTablesBase &pt, int band, int zig15, CoefficientContext context) {
        ANNOTATE_CTX(band, EXP8, 0, context.bsr_best_prior);
        ANNOTATE_CTX(band, EXP8, 1, context.num_nonzeros);
        dev_assert((band & 7)== 0 ? ((band >>3) + 7) : band - 1 == zig15);
        return pt.model().exponent_counts_x_.at(color_index(),
                                             context.num_nonzeros_bin,
                                             zig15,
                                             context.bsr_best_prior);
    }
    Sirikata::Array1d<Branch, MAX_EXPONENT>::Slice exponent_array_7x7(ProbabilityTablesBase &pt,
                                                                      const unsigned int band,
                                                                      const unsigned int zig49,
                                                                      const CoefficientContext context) {
        ANNOTATE_CTX(band, EXP7x7, 0, context.bsr_best_prior);
        ANNOTATE_CTX(band, EXP7x7, 1, context.num_nonzeros_bin);
        return pt.model().exponent_counts_.at(color_index(),
            context.num_nonzeros_bin,
            zig49,
            context.bsr_best_prior);
    }
    Sirikata::Array1d<Branch,
                      MAX_EXPONENT>::Slice exponent_array_dc(ProbabilityTablesBase &pt,
							     uint16_t len_abs_mxm,
							     uint16_t len_abs_offset_to_closest_edge) {
        return pt.model().exponent_counts_dc_.
	  at(std::min(len_abs_mxm,
		      (uint16_t)(Model::ExponentCountsDC::size0 - 1)),
	     std::min(len_abs_offset_to_closest_edge,
		      (uint16_t)(Model::ExponentCountsDC::size1 - 1)));
    }
    Sirikata::Array1d<Branch, COEF_BITS>::Slice residual_array_dc(ProbabilityTablesBase &pt,
                                                                     uint16_t len_abs_mxm
                                                                  , uint16_t len_abs_offset_to_closest_edge) {
        return pt.model().residual_noise_counts_dc_
	  .at(std::min((uint16_t)(Model::ResidualNoiseCountsDc::size0 - 1),
		       len_abs_mxm));
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
        if (all_present || left_present) {
            left_block = idct_2d_8x1(context.left_unchecked(), 0, 7);
            left_edge = idct_2d_8x1(context.here(), 1, 0);
        }
        if (all_present || above_present) {
            above_block = idct_2d_1x8(context.above_unchecked(), 0, 7);
            above_edge = idct_2d_1x8(context.here(), 1, 0);
        }
        if (all_present || left_present) {
            if (all_present || above_present) {
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
        if (all_present || left_present) {
            int a = context.left_unchecked().dc();
            if (all_present || above_present) {
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
        int max_value = (1 << (1 + MAX_EXPONENT)) - 1;
        int min_value = -max_value;
        int adjustment_factor = 2 * max_value + 1;
        int retval = //predict_locoi_dc_deprecated(block);
            predict_dc_dct(context);
        retval = context.here().dc() + (recover_original ? retval : -retval);
        if (retval < min_value) retval += adjustment_factor;
        if (retval > max_value) retval -= adjustment_factor;
        return retval;
    }
#define shift_right_round_zero_epi16(vec, imm8) (_mm_sign_epi16(_mm_srli_epi16(_mm_sign_epi16(vec, vec), imm8), vec));
    int adv_predict_dc_pix(const ConstBlockContext&context, int16_t*pixels_sans_dc, int32_t *uncertainty_val, int32_t *uncertainty2_val) {
        uint16_t *q = ProbabilityTablesBase::quantization_table((int)color);
        idct(context.here(), q, pixels_sans_dc, true);

        Sirikata::AlignedArray1d<int16_t, 16> dc_estimates;
        dc_estimates.memset(0);
        int32_t avgmed = 0;
        if(all_present || left_present || above_present) {
#ifndef USE_SCALAR
            if (all_present || above_present) { //above goes first to prime the cache
                __m128i neighbor_above = _mm_loadu_si128((const __m128i*)(const char*)context
                                                         .neighbor_context_above_unchecked()
                                                         .horizontal_ptr());
                __m128i pixels_sans_dc_reg = _mm_loadu_si128((const __m128i*)(const char*)pixels_sans_dc);
                __m128i pixels2_sans_dc_reg = _mm_loadu_si128((const __m128i*)(const char*)(pixels_sans_dc + 8));
                __m128i pixels_delta = _mm_sub_epi16(pixels_sans_dc_reg,
                                                     pixels2_sans_dc_reg);
                __m128i pixels_delta_div2 = shift_right_round_zero_epi16(pixels_delta, 1);
                __m128i pixels_sans_dc_recentered = _mm_add_epi16(pixels_sans_dc_reg,
                                                                  _mm_set1_epi16(1024));
                __m128i above_dc_estimate = _mm_sub_epi16(_mm_sub_epi16(neighbor_above, pixels_delta_div2),
                                                          pixels_sans_dc_recentered);

                _mm_store_si128((__m128i*)(char*)(dc_estimates.begin()
                                                  + ((all_present || left_present) ? 8 : 0)),
                                above_dc_estimate);
            }
            if (all_present || left_present) {
                const int16_t * horiz_data = context.neighbor_context_left_unchecked().vertical_ptr_except_7();
                __m128i neighbor_horiz = _mm_loadu_si128((const __m128i*)(const char*)horiz_data);
                //neighbor_horiz = _mm_insert_epi16(neighbor_horiz, horiz_data[NeighborSummary::VERTICAL_LAST_PIXEL_OFFSET_FROM_FIRST_PIXEL], 7);
                __m128i pixels_sans_dc_reg = _mm_set_epi16(pixels_sans_dc[56],
                                                           pixels_sans_dc[48],
                                                           pixels_sans_dc[40],
                                                           pixels_sans_dc[32],
                                                           pixels_sans_dc[24],
                                                           pixels_sans_dc[16],
                                                           pixels_sans_dc[8],
                                                           pixels_sans_dc[0]);
                __m128i pixels_delta = _mm_sub_epi16(pixels_sans_dc_reg,
                                                     _mm_set_epi16(pixels_sans_dc[57],
                                                                   pixels_sans_dc[49],
                                                                   pixels_sans_dc[41],
                                                                   pixels_sans_dc[33],
                                                                   pixels_sans_dc[25],
                                                                   pixels_sans_dc[17],
                                                                   pixels_sans_dc[9],
                                                                   pixels_sans_dc[1]));

                __m128i pixels_delta_div2 = shift_right_round_zero_epi16(pixels_delta, 1);
                __m128i left_dc_estimate = _mm_sub_epi16(_mm_sub_epi16(neighbor_horiz, pixels_delta_div2),
                                                          _mm_add_epi16(pixels_sans_dc_reg,
                                                                        _mm_set1_epi16(1024)));

                _mm_store_si128((__m128i*)(char*)dc_estimates.begin(), left_dc_estimate);
            }
#else
            if (all_present || left_present) {
                for (int i = 0; i < 8;++i) {
                    int a = pixels_sans_dc[i << 3] + 1024;
                    int pixel_delta = pixels_sans_dc[i << 3] - pixels_sans_dc[(i << 3) + 1];
                    int b = context.neighbor_context_left_unchecked().vertical(i) - (pixel_delta / 2); //round to zero
                    dc_estimates[i] = b - a;
                }
            }
            if (all_present || above_present) {
                for (int i = 0; i < 8;++i) {
                    int a = pixels_sans_dc[i] + 1024;
                    int pixel_delta = pixels_sans_dc[i] - pixels_sans_dc[i + 8];
                    int b = context.neighbor_context_above_unchecked().horizontal(i) - (pixel_delta / 2); //round to zero
                    dc_estimates[i + ((all_present || left_present) ? 8 : 0)] = b - a;
                }
            }
#endif
            int32_t avg_h_v[2] = {0, 0};
            int32_t min_dc = dc_estimates[0];
            int32_t max_dc = dc_estimates[0];
            size_t which_est = 0;
            for (int vert = 0; vert != 2; ++vert) {
                for (int i = 0; i < 8; ++which_est, ++i) {
                    int16_t cur_est = dc_estimates[which_est];
                    avg_h_v[vert] += cur_est;
                    if (min_dc > cur_est) {
                        min_dc = cur_est;
                    }
                    if (max_dc < cur_est) {
                        max_dc = cur_est;
                    }
                }
                if ((!all_present) && (above_present == false || left_present == false)) {
                    avg_h_v[1] = avg_h_v[0];
                    break;
                }
            }
            int32_t overall_avg = (avg_h_v[0] + avg_h_v[1]) >> 1;
            avgmed = overall_avg;
            *uncertainty_val = (max_dc - min_dc)>>3;
            avg_h_v[0] -= avgmed;
            avg_h_v[1] -= avgmed;
            int32_t far_afield_value = avg_h_v[1];
            if (abs(avg_h_v[0]) < abs(avg_h_v[1])) {
                far_afield_value = avg_h_v[0];
            }
            *uncertainty2_val = (far_afield_value) >> 3;

            if (false) { // this is to debug some of the differences
                debug_print_deltas(context, dc_estimates.begin(), avgmed);
            }
        }
        return ((avgmed / q[0] + 4) >> 3);
    }
    void debug_print_deltas(const ConstBlockContext&context, int16_t *dc_estimates, int avgmed) {
        int actual_dc = context.here().dc();
        uint16_t *q = ProbabilityTablesBase::quantization_table((int)color);
        int len_est = ((all_present || (left_present && above_present)) ? 16 : 8);
        int avg_estimated_dc = 0;
        int dc_sum = 0;
        for (int i = 0 ;i < len_est; ++i) {
            dc_sum += dc_estimates[i];
        }
        avg_estimated_dc = dc_sum;
        if (all_present || (left_present && above_present)) {
            avg_estimated_dc >>= 1;
        }
        
        avg_estimated_dc = (avg_estimated_dc/q[0] + xIDCTSCALE / 2) >> 3;
        int16_t dc_copy[16];
        memcpy(dc_copy, dc_estimates, len_est*sizeof(int16_t));
        std::sort(dc_copy, dc_copy + len_est);
        int mmed = dc_copy[len_est/2];
        int scaled_med = (mmed/q[0] + 4);
        int scaled_avgmed = (((avgmed/q[0]) + 4) >> 3);
        using namespace LeptonDebug;
        LeptonDebug::med_err += abs(scaled_med - actual_dc);
        LeptonDebug::amd_err += abs(scaled_avgmed - actual_dc);
        LeptonDebug::avg_err += abs(avg_estimated_dc - actual_dc);
        int locoi_pred = predict_locoi_dc_deprecated(context);
        int predicted_dc = predict_dc_dct(context);
        LeptonDebug::ori_err += abs(predicted_dc - actual_dc);
        LeptonDebug::loc_err += abs(locoi_pred - actual_dc);

        fprintf(stderr, "MXM: %d\n", dc_estimates[len_est - 1] - dc_estimates[0]);
        fprintf(stderr, "MED: %d (%d)\n", scaled_med, LeptonDebug::med_err);
        fprintf(stderr, "AMD: %d (%d)\n", scaled_avgmed, LeptonDebug::amd_err);
        fprintf(stderr, "AVG: %d (%d)\n", avg_estimated_dc, LeptonDebug::avg_err);
        fprintf(stderr, "ORI: %d (%d)\n", predicted_dc, LeptonDebug::ori_err);
        fprintf(stderr, "LOC: %d (%d)\n", locoi_pred, LeptonDebug::loc_err);
        fprintf(stderr, "DC : %d\n", actual_dc);
    }
    int adv_predict_or_unpredict_dc(int16_t saved_dc, bool recover_original, int predicted_val) {
        int max_value = (1 << (MAX_EXPONENT - 1));
        int min_value = -max_value;
        int adjustment_factor = 2 * max_value + 1;
        int retval = predicted_val;
        retval = saved_dc + (recover_original ? retval : -retval);
        if (retval < min_value) retval += adjustment_factor;
        if (retval > max_value) retval -= adjustment_factor;
        return retval;
    }
    int compute_aavrg_dc(ConstBlockContext context) {
        return compute_aavrg(0, raster_to_aligned.at(0), context);
        
        uint32_t total = 0;
        if (all_present || left_present) {
            total += abs(context.left_unchecked().dc());
        }
        if (all_present || above_present) {
            total += abs(context.above_unchecked().dc());
        }
        if (all_present || (left_present && above_present)) {
            constexpr unsigned int log_weight = 5;
            total *= 13;
            total += 6 * abs(context.above_left_unchecked().dc());
            return total >> log_weight;
        } else {
            return total;
        }
    }
    int16_t compute_aavrg(unsigned int coord, unsigned int aligned_zz, ConstBlockContext context) {
        int16_t total = 0;
        if (all_present || left_present) {
            total += abs(context.left_unchecked().coefficients_raster(coord));
        }
        if (all_present || above_present) {
            total += abs(context.above_unchecked().coefficients_raster(coord));
        }
        if (all_present || (left_present && above_present)) {
            constexpr unsigned int log_weight = 5;
            total *= 13;
            total += 6 * abs(context.above_left_unchecked().coefficients_raster(coord));
            return ((uint16_t)total) >> log_weight;
        } else {
            return total;
        }
        //if (block.context().above_right.initialized()) {
        //total += abs(block.context().above_right.get()->coefficients().at(0));
        //}
    }
#if defined(OPTIMIZED_7x7) && !defined(USE_SCALAR)
    bool aavrg_vec_matches(__m128i retval, unsigned int aligned_zz, ConstBlockContext context) {
        short ret[8];
        _mm_storeu_si128((__m128i*)(char*)ret, retval);
        short correct[8] = {compute_aavrg(aligned_to_raster.at(aligned_zz), aligned_zz +0, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+1), aligned_zz + 1, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+2), aligned_zz + 2, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+3), aligned_zz + 3, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+4), aligned_zz + 4, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+5), aligned_zz + 5, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+6), aligned_zz + 6, context),
            compute_aavrg(aligned_to_raster.at(aligned_zz+7), aligned_zz + 7, context)};
        return (memcmp(ret, correct, sizeof(correct)) == 0);
    }
    void compute_aavrg_vec(unsigned int aligned_zz, ConstBlockContext context, short* aligned_retval) {
        _mm_store_si128((__m128i*)(char*)aligned_retval, compute_aavrg_vec(aligned_zz, context));
    }
#if defined (__clang__) || defined(__GNUC__)
#define x_mm_loadu_si64(a) _mm_set1_epi64x(*(uint64_t*)(char*)(a))
#else
#define x_mm_loadu_si64 _mm_loadu_si64
#endif

    __m128i compute_aavrg_vec(unsigned int aligned_zz, ConstBlockContext context) {
        if (all_present == false && left_present == false && above_present == false) {
            return _mm_setzero_si128();
        }
        __m128i left;
        if (all_present || left_present) {
            left = _mm_abs_epi16(_mm_load_si128((const __m128i*)(const char*)&context.left_unchecked().coef.at(aligned_zz)));
            if ((!all_present) && !above_present) {
                return left;
            }
        }
        __m128i above = _mm_setzero_si128();
        if (all_present || above_present) {
            above = _mm_abs_epi16(_mm_load_si128((const __m128i*)(const char*)&context.above_unchecked().coef.at(aligned_zz)));
            if (all_present == false && !left_present) {
                return above;
            }
        }
        constexpr unsigned int log_weight = 5;
        __m128i total = _mm_add_epi16(left, above);
        total = _mm_mullo_epi16(total, _mm_set1_epi16(13)); // approximate (a*2+b*2 + c)/5 as (a *13 + b * 13 + c * 6)/32
        __m128i aboveleft = _mm_abs_epi16(_mm_load_si128((const __m128i*)(const char*)&context.above_left_unchecked().coef.at(aligned_zz)));
        total = _mm_add_epi16(total, _mm_mullo_epi16(aboveleft, _mm_set1_epi16(6)));
        __m128i retval = _mm_srli_epi16(total, log_weight);
        dev_assert(aavrg_vec_matches(retval, aligned_zz, context));
        return retval;
        //if (block.context().above_right.initialized()) {
        //total += abs(block.context().above_right.get()->coefficients().at(0));
        //}
    }
#endif

#ifndef USE_SCALAR
    static int32_t compute_lak_vec(__m128i coeffs_x_low, __m128i coeffs_x_high, __m128i coeffs_a_low, __m128i 
#ifdef _WIN32
        &
#endif
        indirect_coeffs_a_high, const int32_t *icos_deq) {
        __m128i sign_mask = _mm_set_epi32(-1, 1, -1, 1); // ((i & 1) ? -1 : 1)

        //coeffs_x[i] = ((i & 1) ? -1 : 1) * coeffs_a[i] - coeffs_x[i];
        coeffs_a_low = _mm_sign_epi32(coeffs_a_low, sign_mask);
        __m128i coeffs_a_high = _mm_sign_epi32(indirect_coeffs_a_high, sign_mask);
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
    
    template<int band>
#ifndef _WIN32
    __attribute__((always_inline))
#endif
    int32_t compute_lak_templ(const ConstBlockContext&context) {
        __m128i coeffs_x_low;
        __m128i coeffs_x_high;
        __m128i coeffs_a_low;
        __m128i coeffs_a_high;
        const int32_t * icos = nullptr;
        static_assert((band & 7) == 0 || (band >> 3) == 0, "This function only works on edges");
        if ((band >> 3) == 0) {
            if(all_present == false && !above_present) {
                return 0;
            }
            const auto &neighbor = context.above_unchecked();
            ITER(coeffs_x_low, coeffs_a_low, 0, 8);
            ITER(coeffs_x_high, coeffs_a_high, 4, 8);
            icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x((int)COLOR) + band * 8;
        } else {
            if (all_present == false && !left_present) {
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
        if (all_present == false && !above_present) {
            return 0;
        }
        __m128i coeffs_x_low;
        __m128i coeffs_x_high;
        __m128i coeffs_a_low;
        __m128i coeffs_a_high;
        dev_assert(band/8 == 0 && "this function only works for the top edge");
        const auto &neighbor = context.above_unchecked();
        ITER(coeffs_x_low, coeffs_a_low, 0, 8);
        ITER(coeffs_x_high, coeffs_a_high, 4, 8);
        const int32_t * icos = ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x((int)COLOR) + band * 8;
        return compute_lak_vec(coeffs_x_low, coeffs_x_high, coeffs_a_low, coeffs_a_high, icos);
    }
    int32_t compute_lak_vertical(const ConstBlockContext&context, unsigned int band) {
        dev_assert((band & 7) == 0 && "Must be used for veritcal");
        if (all_present == false && !left_present) {
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
#endif
    int32_t compute_lak(const ConstBlockContext&context, unsigned int band) {
        int coeffs_x[8];
        int coeffs_a[8];
        const int32_t *coef_idct = nullptr;
        if ((band & 7) && (all_present || above_present)) {
            // y == 0: we're the x
            dev_assert(band/8 == 0); //this function only works for the edge
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
#if _DEBUG
        // In DEBUG mode verify that the scalar compute_lak matches the vectorized ones
        dev_assert(((band & 7) ? compute_lak_horizontal(context,band): compute_lak_vertical(context,band)) == prediction
               && "Vectorized version must match sequential version");
#endif
        return prediction;
    }
    Sirikata::Array1d<Branch,
            (1<<RESIDUAL_NOISE_FLOOR)>::Slice
        residual_thresh_array(ProbabilityTablesBase &pt,
                              const unsigned int band,
                              const uint8_t cur_exponent,
                              const CoefficientContext context,
                              int min_threshold) {
        uint16_t ctx_abs = abs(context.best_prior);
        ANNOTATE_CTX(band, THRESH8, 0, ctx_abs >> min_threshold);
        ANNOTATE_CTX(band, THRESH8, 2, cur_exponent - min_threshold);
        return pt.model(
            ).residual_threshold_counts_.at(color_index(),
                                            std::min(ctx_abs >> min_threshold,
                                                     (uint16_t)Model::ResidualThresholdCounts::size1 - 1),
                                            std::min(cur_exponent - min_threshold,
                                                     Model::ResidualThresholdCounts::size2 - 1));
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
    Branch& sign_array_dc(ProbabilityTablesBase &pt,
                          int avg_delta,
                          int offset_to_closest_edge) {
        ANNOTATE_CTX(0, SIGNDC, 0, 1);
        return pt.model().sign_counts_.at(color_index(),
                                          0,
                                          offset_to_closest_edge >= 0
                                          ? offset_to_closest_edge == 0
                                          ? 3 : 2 : 1);
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
