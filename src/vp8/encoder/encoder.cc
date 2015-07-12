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
    void encode_one(bool value, TokenNode index) {
        encode_one(value,(int)index);
    }
    void encode_one(bool value, int index) {
        encoder_->put(value, (*this)(index,
                                     min_from_entropy_node_index(index),
                                     max_from_entropy_node_index_inclusive(index)));
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
  int written_num_zeros = 0;
  for (unsigned int index = 0; index <= last_block_element_index; ++index) {
      const int16_t coefficient = coefficients_.at( jpeg_zigzag.at( index ) );
      int num_zeros_context = std::max(15 - written_num_zeros, 0);
#ifdef DEBUGDECODE
      fprintf(stderr, "XXZ %d %d %d %d %d => %d\n", (int)index, 666, 666, num_zeros_context, 0, coefficient? 0 : 1);
#endif
      encoder.put( coefficient ? false : true, num_zeros_prob.at(index).at(num_zeros_context).at(0) );
      if (!last_was_zero) {
#ifdef DEBUGDECODE
          fprintf(stderr, "XXZ %d %d %d %d %d => %d\n", (int)index, 666, 666, num_zeros_context, 1, index < last_block_element_index ? 0 : 1);
#endif
          encoder.put( index < last_block_element_index ? false : true,
                       num_zeros_prob.at(index>0).at(num_zeros_context).at(1) );
      }
      if (!coefficient) {
          written_num_zeros++;
      }
      last_was_zero = (coefficient == 0);
  }
  
  for (
#ifdef BLOCK_ENCODE_BACKWARDS
      int index = last_block_element_index; index >= 0; --index 
#else
      unsigned int index = 0; index <= last_block_element_index; ++index
#endif
      ) {
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
    std::pair<Optional<int16_t>,
              Optional<int16_t> > intra_block_neighbors = get_near_coefficients(coord);
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
      int16_t coef = coefficients_.at( jpeg_zigzag.at( index ));
    PerBitEncoderState4s dct_encoder_state(&encoder, &prob,
                                           left_neighbor_context, above_neighbor_context,
                                           intra_block_neighbors.first, intra_block_neighbors.second);
    if (false && index == 0) {
        if (false && left_neighbor_context.initialized() && above_neighbor_context.initialized()) {
            int16_t tl = context().above.get()->context().left.get()->coefficients().at( jpeg_zigzag.at( index ) );
            
            int16_t a = coef - left_neighbor_context.get();
            int16_t b = coef - above_neighbor_context.get();
            int16_t c = coef - tl;
            
            if (abs(a) < abs(b)) {
                if (abs(a) < abs(c)) {
                    encoder.put( true,
                                 prob.at(ENTROPY_NODES).at(0).at(0));
                    coef = a;
                } else {
                    encoder.put( false,
                                 prob.at(ENTROPY_NODES).at(0).at(0));
                    encoder.put( false,
                                 prob.at(ENTROPY_NODES+1).at(0).at(0));
                    coef = c;
                }
            } else {
                if (abs(b) < abs(c)) {
                    encoder.put( false,
                                 prob.at(ENTROPY_NODES).at(0).at(0));
                    encoder.put( true,
                                 prob.at(ENTROPY_NODES+1).at(0).at(0));
                    coef = b;
                } else {
                    encoder.put( false,
                                 prob.at(ENTROPY_NODES).at(0).at(0));
                    encoder.put( false,
                                 prob.at(ENTROPY_NODES+1).at(0).at(0));
                    coef = c;
                }
            }
        }else if (left_neighbor_context.initialized()) {
            coef = coef - left_neighbor_context.get();
        } else if (above_neighbor_context.initialized()) {
            coef = coef - above_neighbor_context.get();
        }
        put_one_signed_coefficient( dct_encoder_state, true, false, coef );
    } else {
        uint8_t length = put_ceil_log_coefficient(dct_encoder_state, abs(coef) );
        put_one_natural_significand_coefficient( dct_encoder_state, length, abs(coef) );
        dct_encoder_state.encode_one(coef < 0 , TokenNode::NEGATIVE);
    }
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
