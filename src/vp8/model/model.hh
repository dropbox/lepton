#ifndef DECODER_HH
#define DECODER_HH

#include <vector>
#include <memory>

#include "../util/fixed_array.hh"
#include "../util/option.hh"
#include "numeric.hh"
#include "branch.hh"
class BoolEncoder;
class Slice;


constexpr unsigned int BLOCK_TYPES        = 2; // setting this to 3 gives us ~1% savings.. 2/3 from BLOCK_TYPES=2
constexpr unsigned int EOB_BINS           = 4;
constexpr unsigned int LOG_NUM_ZEROS_BINS = 2;
constexpr unsigned int NUM_ZEROS_BINS     =  1 << LOG_NUM_ZEROS_BINS;
constexpr unsigned int NUM_ZEROS_COEF_BINS     =  4;
constexpr unsigned int COEF_BANDS         = 64;
constexpr unsigned int PREV_COEF_CONTEXTS = 25;
constexpr unsigned int NEIGHBOR_COEF_CONTEXTS = 25;
constexpr unsigned int ENTROPY_NODES      = 41;
constexpr unsigned int NUM_ZEROS_EOB_PRIORS = 66;
constexpr unsigned int ZERO_OR_EOB = 3;
constexpr unsigned int AVG_ZEROS_EOB_PRIORS = 66;
constexpr unsigned int AVG_EOB = 66;
enum BitContexts : uint8_t {
    CONTEXT_BIT_ZERO,
    CONTEXT_BIT_ONE,
    CONTEXT_LESS_THAN,
    CONTEXT_GREATER_THAN,
    CONTEXT_UNSET,
    NUM_BIT_CONTEXTS
};
template<typename intt> intt log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    const intt b[] = {0x2,
                      0xC,
                      0xF0,
                      (intt)0xFF00,
                      (intt)0xFFFF0000U,
                      std::numeric_limits<intt>::max() - (intt)0xFFFFFFFFU};
    const intt S[] = {1, 2, 4, 8, 16, 32};

    register intt r = 0; // result of log2(v) will go here
    
    for (signed int i = loop_max; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i])
        {
            v >>= S[i];
            r |= S[i];
        } 
    }
    return r;
}

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
  typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES + 2>,
					COEF_BANDS>,
			      EOB_BINS>,
                NUM_ZEROS_COEF_BINS>,
		    BLOCK_TYPES> BranchCounts;

  BranchCounts token_branch_counts_;
  
    typedef FixedArray<FixedArray<FixedArray<Branch,
                          25>,
                     ENTROPY_NODES>,
                NUM_ZEROS_BINS> EOBCounts;

  EOBCounts eob_counts_;

  typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch, 2>, // nonzero, eob
                                                  1>,
                                        16>,
                              COEF_BANDS>,
//                          AVG_EOB>,
                  AVG_ZEROS_EOB_PRIORS> NumZeroCounts;

  NumZeroCounts num_zero_counts_;
  
  template <typename lambda>
  void forall( const lambda & proc )
  {
      for ( auto & a : token_branch_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  for ( auto & d : c ) {
                      for ( auto & e : d ) {
                          for ( auto & f : e ) {
                              for ( auto & g : f ) {
                                  proc( g );
                              }
                          }
                      }
                  }
              }
          }
      }
      for ( auto & a : eob_counts_ ) {
          for ( auto & b : a ) {
              for ( auto & c : b ) {
                  proc( c );
              }
          }
      }
      for ( auto & a : num_zero_counts_ ) {
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
class Chunk;

class ProbabilityTables
{
private:
  std::unique_ptr<Model> model_;


public:
  ProbabilityTables();
  ProbabilityTables( const Slice & chunk );

  const FixedArray<FixedArray<Branch,
                         25>,
                  ENTROPY_NODES> & eob_array( const int8_t num_zeros) const
  {
      return model_->eob_counts_.at(num_zeros);
  }

   FixedArray<FixedArray<Branch,
                            25>,
                  ENTROPY_NODES> & eob_array( const int8_t num_zeros )
  {
      return model_->eob_counts_.at(num_zeros);
  }
  const FixedArray<FixedArray<FixedArray<FixedArray<Branch, 2>, // nonzero vs eob
                            1>,
                            16>, COEF_BANDS>& num_zeros_array( const int8_t num_zeros_above,
                                                          const int8_t num_zeros_left,
                                                          const int16_t /*eob_above*/,
                                                          const int16_t /*eob_left*/) const {
      if (num_zeros_above && num_zeros_left) {
          return model_->num_zero_counts_.at((num_zeros_above + num_zeros_left) / 2);
      } else if (num_zeros_above) {
          return model_->num_zero_counts_.at(num_zeros_above);
      } else {
          return model_->num_zero_counts_.at(num_zeros_left);
      }
  }

    FixedArray<FixedArray<FixedArray<FixedArray<Branch, 2>, // nonzero vs eob
                        1>,
              16>, COEF_BANDS> & num_zeros_array( const int8_t num_zeros_above,
                                                      const int8_t num_zeros_left,
                                                      const int16_t /*eob_above*/,
                                                      const int16_t /*eob_left*/) {
      if (num_zeros_above && num_zeros_left) {
          return model_->num_zero_counts_.at((num_zeros_above + num_zeros_left) / 2);
      } else if (num_zeros_above) {
          return model_->num_zero_counts_.at(num_zeros_above);
      } else {
          return model_->num_zero_counts_.at(num_zeros_left);
      }
  }

    const FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES+2> & branch_array( const unsigned int block_type,
                                                         const unsigned int zeros_bin,
                                                         const unsigned int eob_bin,
                                                         const unsigned int band) const
  {
      return model_->token_branch_counts_.at( block_type ).at(zeros_bin / (NUM_ZEROS_BINS / NUM_ZEROS_COEF_BINS)).at( eob_bin ).at( band );
  }

  FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES+2> & branch_array( const unsigned int block_type,
                                                   const unsigned int zeros_bin,
                                                   const unsigned int eob_bin,
                                                   const unsigned int band)
  {
      return model_->token_branch_counts_.at( block_type ).at(zeros_bin / (NUM_ZEROS_BINS / NUM_ZEROS_COEF_BINS)).at( eob_bin ).at( band );
  }

  void optimize();
  void serialize( std::ofstream & output ) const;

  static ProbabilityTables get_probability_tables();

  // this reduces the counts to something easier to override by new data
  void normalize();

  const ProbabilityTables& debug_print(const ProbabilityTables*other=NULL)const;
};

#endif /* DECODER_HH */
