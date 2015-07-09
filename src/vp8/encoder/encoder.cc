#include "bool_encoder.hh"
#include "jpeg_meta.hh"
#include "block.hh"
#include "numeric.hh"
#include "model.hh"
#include "mmap.hh"
#include "encoder.hh"
#include "context.hh"
#include <fstream>

using namespace std;


template <class ParentContext> struct PerBitEncoderState : public ParentContext{
    BoolEncoder *encoder_;

public:
    template<typename... Targs> PerBitEncoderState(BoolEncoder * encoder,
                                                   Targs&&... Fargs)
        : ParentContext(std::forward<Targs>(Fargs)...){
        encoder_ = encoder;
    }

    // boiler-plate implementations
    void encode_one(bool value, TokenNodeNot index) {
        encode_one(value,(int)index);
    }
    void encode_one(bool value, int index) {
        encoder_->put(value, (*this)(index,
                                     min_from_entropy_node_index(index),
                                     max_from_entropy_node_index_inclusive(index)));
    }
    template<class TokenDecoderEnsembleNum> void encode_ensemble(uint16_t value) {
        TokenDecoderEnsembleNum::encode(*encoder_, value, *this);
    }
    void encode_ensemble1(uint16_t value) {
        TokenDecoderEnsemble::TokenDecoder1::encode(*encoder_, value, *this);
    }
    void encode_ensemble2(uint16_t value) {
        TokenDecoderEnsemble::TokenDecoder2::encode(*encoder_, value, *this);
    }
    void encode_ensemble3(uint16_t value) {
        TokenDecoderEnsemble::TokenDecoder3::encode(*encoder_, value, *this);
    }
    void encode_ensemble4(uint16_t value) {
        TokenDecoderEnsemble::TokenDecoder4::encode(*encoder_, value, *this);
    }
    void encode_ensemble5(uint16_t value) {
        TokenDecoderEnsemble::TokenDecoder5::encode(*encoder_, value, *this);
    }
};

typedef PerBitEncoderState<DefaultContext> EncoderState;
typedef PerBitEncoderState<PerBitContext2u> PerBitEncoderState2u;
typedef PerBitEncoderState<PerBitContext4s> PerBitEncoderState4s;
void Block::serialize_tokens( BoolEncoder & encoder,
			      ProbabilityTables & probability_tables ) const
{
  /* serialize the num zeros bitmap */
  int slice = BLOCK_SLICE;
  uint8_t divided_num_zeros = num_zeros_ / slice;
  if (divided_num_zeros == (64/slice)) divided_num_zeros--;
  assert(divided_num_zeros < 64/slice);

  const int16_t num_zeros = min( uint8_t(NUM_ZEROS_BINS-1), divided_num_zeros );
  const int16_t eob_bin = min( uint8_t(EOB_BINS-1), uint8_t(coded_length_/(64 / EOB_BINS)));

  uint16_t above_num_zeros = context().above.initialized() ? context().above.get()->num_zeros() + 1: 0;
  uint16_t left_num_zeros = context().left.initialized() ? context().left.get()->num_zeros() + 1: 0;


  uint16_t above_coded_length = 65;
  uint16_t left_coded_length = 65;
  if (context().left.initialized()) {
      left_coded_length = context().left.get()->coded_length();
  }
  if (context().above.initialized()) {
      above_coded_length = context().above.get()->coded_length();
  }
#ifdef DEBUGDECODE
  fprintf(stderr, "XXY %d %d %d %d\n", left_num_zeros,
          above_num_zeros,
          left_coded_length,
          above_coded_length);
#endif
  auto & num_zeros_prob
      = probability_tables.num_zeros_array(left_num_zeros,
                                           above_num_zeros,
                                           left_coded_length,
                                           above_coded_length);

  uint8_t last_block_element_index = min( uint8_t(63), coded_length_ );
  bool last_was_zero = false;
  for (unsigned int index = 0; index <= last_block_element_index; ++index) {
      uint8_t above_neighbor_is_zero = 0;
      uint8_t left_neighbor_is_zero = 0;
      if ( context().above.initialized() ) {
          uint16_t above_coef = context().above.get()->coefficients().at( jpeg_zigzag.at( index ) );
          if (!above_coef) {
              if (index + 1< above_coded_length) {
                  above_neighbor_is_zero = 1;
              } else {
                  above_neighbor_is_zero = 2; // eob context
              }
          } else {
              above_neighbor_is_zero = 0;
          }
      }
      if ( context().left.initialized() ) {
          uint16_t left_coef = context().left.get()->coefficients().at( jpeg_zigzag.at( index ) );
          if (!left_coef) {
              if (index + 1< left_coded_length) {
                  left_neighbor_is_zero = 1;
              } else {
                  left_neighbor_is_zero = 2; // eob context
              }
          } else {
              left_neighbor_is_zero = 0;
          }
      }
      const int16_t coefficient = coefficients_.at( jpeg_zigzag.at( index ) );
#ifdef DEBUGDECODE
      fprintf(stderr, "XXZ %d %d %d %d => %d\n", (int)index, (int)left_neighbor_is_zero,(int)above_neighbor_is_zero, 0, coefficient? 0 : 1);
#endif
      encoder.put( coefficient ? false : true, num_zeros_prob.at(index).at(left_neighbor_is_zero).at(above_neighbor_is_zero).at(0) );
      if (!last_was_zero) {
#ifdef DEBUGDECODE
          fprintf(stderr, "XXZ %d %d %d %d => %d\n", (int)index, (int)left_neighbor_is_zero,(int)above_neighbor_is_zero, 1, index < last_block_element_index ? 0 : 1);
#endif
          encoder.put( index < last_block_element_index ? false : true,
                       num_zeros_prob.at(index>0).at(left_neighbor_is_zero).at(above_neighbor_is_zero).at(1) );
      }
      last_was_zero = (coefficient == 0);
  }
  
  for ( unsigned int index = 0; index <= last_block_element_index; index++ ) {
    const int16_t coefficient = coefficients_.at( jpeg_zigzag.at( index ) );
    if (!coefficient) {
#ifdef DEBUGDECODE
        fprintf(stderr,"XXB\n");
#endif
        continue;
    }
    /* select the tree probabilities based on the prediction context */
    Optional<int16_t> above_neighbor_context;
    Optional<int16_t> left_neighbor_context;
    if ( context().above.initialized() ) {
        above_neighbor_context = context().above.get()->coefficients().at( jpeg_zigzag.at( index ) );
    }
    if ( context().left.initialized() ) {
        left_neighbor_context = context().left.get()->coefficients().at( jpeg_zigzag.at( index ) );
    }
    uint8_t coord = jpeg_zigzag.at( index );
    Optional<int16_t> left_coef;
    Optional<int16_t> above_coef;
    if (index > 1) {
        if (coord % 8 == 0) {
            above_coef = coefficients().at(coord - 8);
        } else if (coord > 8) {
            left_coef = coefficients().at(coord - 1);
            above_coef = coefficients().at(coord - 8);
        } else {
            left_coef = coefficients().at(coord - 1);
        }
    }
    auto & prob = probability_tables.branch_array(std::min((unsigned int)type_,
                                                           BLOCK_TYPES - 1),
                                                  num_zeros,
                                                  eob_bin,
                                                  index_to_cat(index));
#ifdef DEBUGDECODE
      fprintf(stderr, "XXA %d %d(%d) %d %d => %d\n",
              std::min((unsigned int)type_, BLOCK_TYPES - 1),
              (int)num_zeros, (int)num_zeros_,
              (int)eob_bin,
              (int)index_to_cat(index),
              coefficients_.at( jpeg_zigzag.at( index ) )
          );
#endif
    PerBitEncoderState4s dct_encoder_state(&encoder, &prob,
                                           left_neighbor_context, above_neighbor_context,
                                           left_coef, above_coef);
    put_one_signed_nonzero_coefficient( dct_encoder_state, coefficients_.at( jpeg_zigzag.at( index ) ));
  }
}

bool filter(const Branch& a,
            const Branch* b) {
    if (a.true_count() == 0 && a.false_count() == 0) {
        return false;
    }
    if (b) {
        if (a.prob() + 1 == b->prob() ||
            a.prob() == b->prob() + 1 ||
            a.prob() == b->prob()) {
            return false;
        }
    } else {
        return a.true_count () > 300 && a.false_count() > 300;
    }
    return true;
}

const ProbabilityTables &ProbabilityTables::debug_print(const ProbabilityTables * other)const
{
    for ( unsigned int type = 0; type < model_->token_branch_counts_.size(); type++ ) {
        const auto & this_type = model_->token_branch_counts_.at( type );
        const auto *other_type = other ? &other->model_->token_branch_counts_.at( type ) : NULL;
        for ( unsigned int num_zeros_bin = 0; num_zeros_bin < this_type.size(); num_zeros_bin++ ) {
            const auto & this_num_zeros_bin = this_type.at( num_zeros_bin );
            const auto * other_num_zeros_bin = other ? &other_type->at( num_zeros_bin ) : NULL;
            for ( unsigned int eob_bin = 0; eob_bin < this_num_zeros_bin.size(); eob_bin++ ) {
                const auto & this_eob_bin = this_num_zeros_bin.at( eob_bin );
                const auto * other_eob_bin = other ? &other_num_zeros_bin->at( eob_bin ) : NULL;
                for ( unsigned int band = 0; band < this_eob_bin.size(); band++ ) {
                    const auto & this_band = this_eob_bin.at( band );
                    const auto * other_band = other? &other_eob_bin->at( band ) : NULL;
                    for ( unsigned int prev_context = 0; prev_context < this_band.size(); prev_context++ ) {
                        const auto & this_prev_context = this_band.at( prev_context );
                        const auto * other_prev_context = other? &other_band->at( prev_context ) : NULL;
                        for ( unsigned int neighbor_context = 0; neighbor_context < this_prev_context.size(); neighbor_context++ ) {
                            const auto & this_neighbor_context = this_prev_context.at( neighbor_context );
                            const auto * other_neighbor_context = other ? &other_prev_context->at( neighbor_context ): NULL;
                            bool print_first = false;
                            for ( unsigned int node = 0; node < this_neighbor_context.size(); node++ ) {
                                const auto & this_node = this_neighbor_context.at( node );
                                const auto * other_node = other? &other_neighbor_context->at( node ) : NULL;
                                if (filter(this_node,
                                           other_node)) {
                                    if (!print_first) {
                                        cout << "token_branch_counts[ " << type << " ][ "<< num_zeros_bin <<" ][ " << eob_bin << " ][ " << band << " ][ " << prev_context << " ]["<<neighbor_context<<"] = ";
                                        print_first = true;
                                    }
                                    if (other) {
                                        const auto & other_node = other_neighbor_context->at( node );
                                        cout << "( " << node << " : " << (int)this_node.prob() << " vs " << (int)other_node.prob() << " { " << (int)this_node.true_count() << " , " << (int)this_node.false_count() << " } ) ";
                                    } else {
                                        cout << "( " << node << " : " << (int)this_node.true_count() << " , " << (int)this_node.false_count() << " ) ";
                                    }
                                }
                            }
                            if (print_first) {
                                cout << endl;
                            }
                        }
                    }
                }
            } 
        }
    }
    return *this;
}

ProbabilityTables ProbabilityTables::get_probability_tables()
{
  const char * model_name = getenv( "LEPTON_COMPRESSION_MODEL" );
  if ( not model_name ) {
    cerr << "Using default (bad!) probability tables!" << endl;
    return {};
  } else {
    MMapFile model_file { model_name };
    ProbabilityTables model_tables { model_file.slice() };
    model_tables.normalize();
    return model_tables;
  }
}

ProbabilityTables::ProbabilityTables()
  : model_( new Model )
{
  model_->forall( [&] ( Branch & x ) { x = Branch(); } );
}


void ProbabilityTables::normalize()
{
  model_->forall( [&] ( Branch & x ) { x.normalize(); } );
}

ProbabilityTables::ProbabilityTables( const Slice & slice )
  : model_( new Model )
{
  const size_t expected_size = sizeof( *model_ );
  assert(slice.size() == expected_size && "unexpected model file size.");


  memcpy( model_.get(), slice.buffer(), slice.size() );
}

Branch::Branch()
{
  optimize();
}

inline void VP8BoolEncoder::put( const bool value, Branch & branch )
{
  put( value, branch.prob() );
  if ( value ) {
    branch.record_true_and_update();
  } else {
    branch.record_false_and_update();
  }
}
