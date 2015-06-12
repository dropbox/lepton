#ifndef DECODER_HH
#define DECODER_HH

#include <vector>
#include <memory>

#include "fixed_array.hh"
#include "jpeg_meta.hh"

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

constexpr unsigned int ENTROPY_NODES      = 40;
constexpr unsigned int NUM_ZEROS_EOB_PRIORS = 65;


struct Model
{
  typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES>,
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

  typedef FixedArray<FixedArray<FixedArray<FixedArray<FixedArray<Branch, LOG_NUM_ZEROS_BINS>,
                              NUM_ZEROS_EOB_PRIORS>,
                          NUM_ZEROS_EOB_PRIORS>,
                    NUM_ZEROS_BINS>,
                    NUM_ZEROS_BINS> NumZeroCounts;

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

  const FixedArray<Branch, LOG_NUM_ZEROS_BINS> & num_zeros_array( const int8_t num_zeros_above,
                                                            const int8_t num_zeros_left,
                                                      const int16_t eob_bin_above,
                                                      const int16_t eob_bin_left) const
  {
      return model_->num_zero_counts_.at(num_zeros_above).at(num_zeros_left).at( eob_bin_above ).at( eob_bin_left );
  }

  FixedArray<Branch, LOG_NUM_ZEROS_BINS> & num_zeros_array( const int8_t num_zeros_above,
                                                      const int8_t num_zeros_left,
                                                      const int16_t eob_bin_above,
                                                      const int16_t eob_bin_left
                                                )
  {
      return model_->num_zero_counts_.at(num_zeros_above).at(num_zeros_left).at( eob_bin_above ).at( eob_bin_left );
  }

    const FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES> & branch_array( const unsigned int block_type,
                                                         const unsigned int zeros_bin,
                                                         const unsigned int eob_bin,
                                                         const unsigned int band) const
  {
      return model_->token_branch_counts_.at( block_type ).at(zeros_bin / (NUM_ZEROS_BINS / NUM_ZEROS_COEF_BINS)).at( eob_bin ).at( band );
  }

  FixedArray<FixedArray<FixedArray<Branch,
                                NEIGHBOR_COEF_CONTEXTS>,
						    PREV_COEF_CONTEXTS>,
				        ENTROPY_NODES> & branch_array( const unsigned int block_type,
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
