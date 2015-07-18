#ifndef DECODER_HH
#define DECODER_HH

#include <vector>
#include <memory>

#include "../util/fixed_array.hh"
#include "../util/option.hh"
#include "numeric.hh"
#include "branch.hh"
#include "block.hh"
#include "weight.hh"
class BoolEncoder;
class Slice;


constexpr unsigned int BLOCK_TYPES        = 2; // setting this to 3 gives us ~1% savings.. 2/3 from BLOCK_TYPES=2
constexpr unsigned int NUM_ZEROS_BINS     =  10;
constexpr unsigned int COEF_BANDS         = 64;
constexpr unsigned int ENTROPY_NODES      = 15;
constexpr unsigned int NUM_ZEROS_EOB_PRIORS = 66;
constexpr unsigned int ZERO_OR_EOB = 3;
constexpr unsigned int AVG_ZEROS_EOB_PRIORS = 66;
constexpr unsigned int AVG_EOB = 66;
constexpr unsigned int RESIDUAL_NOISE_FLOOR  = 6;
constexpr unsigned int COEF_BITS = 10;
enum BitContexts : uint8_t {
    CONTEXT_BIT_ZERO,
    CONTEXT_BIT_ONE,
    CONTEXT_LESS_THAN,
    CONTEXT_GREATER_THAN,
    CONTEXT_UNSET,
    NUM_BIT_CONTEXTS
};


BitContexts context_from_value_bits_id_min_max(Optional<int16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max);
BitContexts context_from_value_bits_id_min_max(Optional<uint16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max);


inline int index_to_cat(int index) {
    return index;
    const int unzigzag[] =
{
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

        int where = unzigzag[index];
        int x = where % 8;
        int y = where / 8;
        if (x == y) {
            return 0;
        }
        if (x == 0 || y == 0) {
            return 1;
        }
        if (x > y) {
            return 2;
        }
        return 3;
}



struct Model
{
    
    typedef FixedArray<FixedArray<FixedArray<Branch, 6>,
                        26>, //neighboring zero counts added + 2/ 4
            BLOCK_TYPES> ZeroCounts7x7;
    ZeroCounts7x7 num_zeros_counts_7x7_;

    typedef FixedArray<FixedArray<FixedArray<FixedArray<Branch, 3>,
            8>, //lower num_zeros_count
          8>, //eob in this dimension
    
        BLOCK_TYPES> ZeroCounts1x8;
    ZeroCounts1x8 num_zeros_counts_1x8_;

    typedef FixedArray<FixedArray<FixedArray<FixedArray<Branch,
				          COEF_BITS>,
                      (8>NUM_ZEROS_BINS?8:NUM_ZEROS_BINS)>, //num zeros
					COEF_BANDS>,
		    BLOCK_TYPES> ResidualNoiseCounts;

    ResidualNoiseCounts residual_noise_counts_;

    
    typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch,
                COEF_BITS - RESIDUAL_NOISE_FLOOR>,
               1 + COEF_BITS - RESIDUAL_NOISE_FLOOR>, // the exponent minus the current bit
            ((1<<(1 + COEF_BITS))/(1<<RESIDUAL_NOISE_FLOOR)) >, // max number over noise floor = 1<<4
        COEF_BANDS>,
    BLOCK_TYPES> ResidualThresholdCounts;
    
    ResidualThresholdCounts residual_threshold_counts_;

    
  typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch, NUMBER_OF_EXPONENT_BITS>,
                         NUMERIC_LENGTH_MAX>, //neighboring block exp
                      NUM_ZEROS_BINS>,
                    COEF_BANDS>,
      BLOCK_TYPES> ExponentCounts;

  ExponentCounts exponent_counts_;
  ExponentCounts exponent_counts_x_;

  typedef FixedArray<FixedArray<FixedArray<FixedArray<Branch, 3>, 3>,COEF_BANDS>,BLOCK_TYPES> SignCounts;
  SignCounts sign_counts_;
  
  template <typename lambda>
  void forall( const lambda & proc )
  {
      for ( auto & a : num_zeros_counts_7x7_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  proc( c );
              }
          }
      }
      for ( auto & a : num_zeros_counts_1x8_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for (auto &d : c) {
                      proc( d );
                  }
              }
          }
      }
      for ( auto & a : sign_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for (auto &d : c) {
                      proc( d );
                  }
              }
          }
      }
      for ( auto & a : residual_noise_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for ( auto & d : c ) {
                      proc( d );
                  }
              }
          }
      }
      for ( auto & a : residual_threshold_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for ( auto & d : c ) {
                      for ( auto & e : d ) {
                          proc( e );
                      }
                  }
              }
          }
      }
      for ( auto & a : exponent_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for ( auto & d : c ) {
                      for ( auto & e : d ) {
                          proc( e );
                      }
                  }
              }
          }
      }
      for ( auto & a : exponent_counts_x_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for ( auto & d : c ) {
                      for ( auto & e : d ) {
                          proc( e );
                      }
                  }
              }
          }
      }
  }

};
class Slice;

struct ProbabilityTables
{
private:
    std::unique_ptr<Model> model_;
    const unsigned short *quantization_table_;
public:
    ProbabilityTables();
    ProbabilityTables(const Slice & slice);
    void set_quantization_table(const unsigned short quantization_table[64]) {
        quantization_table_ = quantization_table;
    }
    FixedArray<Branch, 6>& zero_counts_7x7(unsigned int block_type,
                                         Optional<uint8_t> left_block_count,
                                         Optional<uint8_t> above_block_count) {
        uint8_t num_zeros_context = 0;
        if (!left_block_count.initialized()) {
            num_zeros_context = above_block_count.get_or(0);
        } else if (!above_block_count.initialized()) {
            num_zeros_context = above_block_count.get_or(0);
        } else {
            num_zeros_context = (above_block_count.get() + left_block_count.get() + 2) / 4;
        }
        return model_->num_zeros_counts_7x7_
            .at(std::min(block_type, BLOCK_TYPES - 1))
            .at(num_zeros_to_bin(num_zeros_context));
    }
    FixedArray<Branch, 3>& zero_counts_1x8(unsigned int block_type,
                                         unsigned int eob_x,
                                         unsigned int num_zeros) {
        return model_->num_zeros_counts_1x8_.at(std::min(block_type, BLOCK_TYPES -1))
            .at(eob_x)
            .at(((num_zeros + 3) / 7));
    }
    FixedArray<Branch, NUMBER_OF_EXPONENT_BITS>& exponent_array_x(const unsigned int block_type,
                                                                 const unsigned int band,
                                                                 const unsigned int num_zeros_x,
                                                                 const Block&for_lak) {
        return model_->exponent_counts_x_.at( std::min(block_type, BLOCK_TYPES - 1) )
            .at( band ).at(num_zeros_x)
            .at(exp_len(abs(compute_lak(for_lak, band))));
    }
    FixedArray<Branch, NUMBER_OF_EXPONENT_BITS>& exponent_array_7x7(const unsigned int block_type,
                                                               const unsigned int band,
                                                               const unsigned int num_zeros,
                                                                   const Block&block) {
        return model_->exponent_counts_
            .at( std::min(block_type, BLOCK_TYPES - 1) )
            .at( band ).at(num_zeros_to_bin(num_zeros))
            .at(exp_len(abs(compute_aavrg(block, band))));
    }
    FixedArray<Branch, COEF_BITS> & residual_noise_array_x(const unsigned int block_type,
                                                            const unsigned int band,
                                                            const uint8_t num_zeros_x) {
        return model_->residual_noise_counts_.at( std::min(block_type, BLOCK_TYPES - 1) )
            .at( band )
            .at(num_zeros_x);
    }
    FixedArray<Branch, COEF_BITS> & residual_noise_array_7x7(const unsigned int block_type,
                                                            const unsigned int band,
                                                            const uint8_t num_zeros) {
        return residual_noise_array_x(block_type, band, num_zeros_to_bin(num_zeros));
    }
    unsigned int num_zeros_to_bin(unsigned int num_zeros) {
        // this divides by 7 to get the initial value
        return num_zeros / ((64 / NUM_ZEROS_BINS) + (64 % NUM_ZEROS_BINS  ? 1 : 0));
    }
    int compute_aavrg(const Block&block, unsigned int band) {
        Optional<uint16_t> toptop;
        Optional<uint16_t> topleft;
        Optional<uint16_t> top;
        Optional<uint16_t> topright;
        Optional<uint16_t> leftleft;
        Optional<uint16_t> left;
        uint32_t total = 0;
        uint32_t weights = 0;
        uint32_t coef_index = band;
        if (block.context().above.initialized()) {
            if (block.context().above.get()->context().above.initialized()) {
                toptop = abs(block.context().above.get()->context().above.get()->coefficients().at(coef_index));
            }
            top = abs(block.context().above.get()->coefficients().at(coef_index));
        }
        if (block.context().above_left.initialized()) {
            topleft = abs(block.context().above_left.get()->coefficients().at(coef_index));
        }
        if (block.context().above_right.initialized()) {
            topright = abs(block.context().above_right.get()->coefficients().at(coef_index));
        }
        if (block.context().left.initialized()) {
            if (block.context().left.get()->context().left.initialized()) {
                leftleft = abs(block.context().left.get()->context().left.get()->coefficients().at(coef_index));
            }
            left = abs(block.context().left.get()->coefficients().at(coef_index));
        }
        if (toptop.initialized()) {
            total += abs_ctx_weights_lum[0][0][2] * (int)toptop.get();
            weights += abs_ctx_weights_lum[0][0][2];
        }
        if (topleft.initialized()) {
            total += abs_ctx_weights_lum[0][1][1] * (int)topleft.get();
            weights += abs_ctx_weights_lum[0][1][1];
        }
        if (top.initialized()) {
            total += abs_ctx_weights_lum[0][1][2] * (int)top.get();
            weights += abs_ctx_weights_lum[0][1][2];
        }
        if (topright.initialized()) {
            total += abs_ctx_weights_lum[0][1][3] * (int)topright.get();
            weights += abs_ctx_weights_lum[0][1][3];
        }
        if (leftleft.initialized()) {
            total += abs_ctx_weights_lum[0][2][0] * (int)leftleft.get();
            weights += abs_ctx_weights_lum[0][2][0];
        }
        if (left.initialized()) {
            total += abs_ctx_weights_lum[0][2][1] * (int)left.get();
            weights += abs_ctx_weights_lum[0][2][1];
        }
        if (weights == 0) {
            weights = 1;
        }
        return total/weights;
    }
    unsigned int exp_len(int v) {
        if (v < 0) {
            v = -v;
        }
        return log2(std::min(v + 1, 1023));
    }
    int compute_lak(const Block&block, unsigned int band) {
        int16_t coeffs_x[8];
        int16_t coeffs_a[8];
        int32_t coef_idct[8];
        assert(quantization_table_);
        if ((band & 7) && block.context().above.initialized()) {
            // y == 0: we're the x
            assert(band/8 == 0); //this function only works for the edge
            const auto &above = block.context().above.get()->coefficients();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i * 8;
                coeffs_x[i]  = i ? block.coefficients().at(cur_coef) : -32768;
                coeffs_a[i]  = above.at(cur_coef);
                coef_idct[i] = icos_idct_8192_scaled[i * 8]
                    * quantization_table_[zigzag[cur_coef]];
            }
        } else if ((band & 7) == 0 && block.context().left.initialized()) {
            // x == 0: we're the y
            const auto &left = block.context().left.get()->coefficients();
            for (int i = 0; i < 8; ++i) {
                uint8_t cur_coef = band + i;
                coeffs_x[i]  = i ? block.coefficients().at(cur_coef) : -32768;
                coeffs_a[i]  = left.at(cur_coef);
                coef_idct[i] = icos_idct_8192_scaled[i * 8]
                    * quantization_table_[zigzag[cur_coef]];
            }
        } else if (block.context().above.initialized()) {
            return block.context().above.get()->coefficients().at(band);
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
    FixedArray<Branch, COEF_BITS - RESIDUAL_NOISE_FLOOR> & residual_thresh_array(const unsigned int block_type,
                                                                                const unsigned int band,
                                                                                const uint8_t cur_exponent,
                                                                                const Block&block) {
        return model_->residual_threshold_counts_.at( std::min(block_type, BLOCK_TYPES - 1) )
            .at( band )
            .at((abs(compute_lak(block, band)) + (1 << RESIDUAL_NOISE_FLOOR)/2) / (1 << RESIDUAL_NOISE_FLOOR))
            .at(cur_exponent - RESIDUAL_NOISE_FLOOR);
    }
    enum SignValue {
        ZERO_SIGN=0,
        POSITIVE_SIGN=1,
        NEGATIVE_SIGN=2,
    };
    Branch& sign_array(const unsigned int block_type,
                                     const unsigned int band,
                                     const Block&block){
        SignValue left_sign = ZERO_SIGN;
        SignValue above_sign = ZERO_SIGN;
        if (band == 0) {
            if (block.context().left.initialized()) {
                if (block.context().left.get()->coefficients().at(0) > 0) {
                    left_sign = POSITIVE_SIGN;
                }
                if (block.context().left.get()->coefficients().at(0) < 0) {
                    left_sign = NEGATIVE_SIGN;
                }
            }
            if (block.context().above.initialized()) {
                if (block.context().above.get()->coefficients().at(0) > 0) {
                    above_sign = POSITIVE_SIGN;
                }
                if (block.context().above.get()->coefficients().at(0) < 0) {
                    above_sign = NEGATIVE_SIGN;
                }
            }
            
        } else if (band < 8 || band % 8 == 0) {
            int16_t val = compute_lak(block, band);
            if (band == 16) {
                //printf("The band is here");
            }
            if (val > 0) {
                left_sign = above_sign = POSITIVE_SIGN;
            }
            if (val < 0) {
                left_sign = above_sign = NEGATIVE_SIGN;
            }
        } else {
            if (band < 16 || band % 8 == 1) {
                // haven't deserialized these yet: use neighbors
                if (block.context().left.initialized()) {
                    if (block.context().left.get()->coefficients().at(0) > 0) {
                        left_sign = POSITIVE_SIGN;
                    }
                    if (block.context().left.get()->coefficients().at(0) < 0) {
                        left_sign = NEGATIVE_SIGN;
                    }
                }
                if (block.context().above.initialized()) {
                    if (block.context().above.get()->coefficients().at(0) > 0) {
                        above_sign = POSITIVE_SIGN;
                    }
                    if (block.context().above.get()->coefficients().at(0) < 0) {
                        above_sign = NEGATIVE_SIGN;
                    }
                }                
            } else {
                if (block.coefficients().at(band - 1) < 0) {
                    left_sign = NEGATIVE_SIGN;
                }
                if (block.coefficients().at(band - 1) > 0) {
                    left_sign = POSITIVE_SIGN;
                }
                if (block.coefficients().at(band - 8) < 0) {
                    above_sign = NEGATIVE_SIGN;
                }
                if (block.coefficients().at(band - 8) > 0) {
                    above_sign = POSITIVE_SIGN;
                }
            }
        }
        return model_->sign_counts_
            .at(std::min(block_type, BLOCK_TYPES - 1))
            .at(band).at(left_sign).at(above_sign);
    }
    void optimize();
    void serialize( std::ofstream & output ) const;

    static ProbabilityTables get_probability_tables();

    // this reduces the counts to something easier to override by new data
    void normalize();

    const ProbabilityTables& debug_print(const ProbabilityTables*other=NULL)const;
};

#endif /* DECODER_HH */
