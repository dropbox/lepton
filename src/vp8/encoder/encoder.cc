#include "bool_encoder.hh"
#include "coefs.hh"
#include "jpeg_meta.hh"
#include "block.hh"
#include "numeric.hh"
#include "model.hh"
#include "mmap.hh"
#include "encoder.hh"
#include <fstream>

using namespace std;


/*
static pair<uint64_t, uint64_t> natural_coefficient_to_variable_encoding(uint16_t token_value) {
    
    switch(token_value) {
      case 1:
        return {0, 1};
      case 2:
        return {1, 7};
      case 3:
        return {1 | 4, 0xf};
      case 4:
        return {1 | 4 | 8, 0xf};
      default:
        break;
    }
    uint64_t bits_used = 3;
    uint64_t bits_set = 3;
    uint64_t cur_bit = 4;
    if ( token_value < TokenDecoderEnsemble::TokenDecoder1::base_value ) { // category 1, 5..6 
      return {1 | 2 | (token_value == 6 ? 0x10 : 0), 0x1f};
  } 
  if ( token_value < TokenDecoderEnsemble::TokenDecoder1::upper_limit ) { // category 1, 5..6 
      return {1 | 2 | 8 | (TokenDecoderEnsemble::TokenDecoder1::encode_bits(token_value) << 4),
              ((1 << 4) - 1) | ((TokenDecoderEnsemble::TokenDecoder1::num_bits  - 1 )<< 4)};
  }

}
*//*
uint8_t context_from_value_bits_id_min_max(uint16_t value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max) {
    uint8_t context = 0;
    if (value < min) {
        context = 2;
    } else if (value > max) {
        context = 3;
    } else if ((1 << token_id) & bits.bits()) {
        context = 1;
    }
    return context;
}
//FIXME: these are quite the approximation
uint16_t min_from_entropy_node_index(int index) {
    switch(index) {
      case 12:
      case 0:
        return 0;
      case 1:
        return 0;
      case 2:
        return 1;
      case 3:
      case 4:
      case 5:
        return 2;
      case 6:
      case 7:
      case 11:
        return std::min((int)TokenDecoderEnsemble::TokenDecoder1::base_value, 5); // this was 5
      case 8:
      case 9:
        return TokenDecoderEnsemble::TokenDecoder1::upper_limit;
      case 10:
        return TokenDecoderEnsemble::TokenDecoder3::upper_limit;
      default:
        assert(false && "Entropy node");
    }    
}
uint16_t max_from_entropy_node_index(int index) {
    if (index == 11) {
        return 6; // should this be 7?
    }
    return 2048;// should this be 2049?
}
*/
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
  /* serialize the EOB bin */
  //const int16_t eob_bin = min( uint8_t(EOB_BINS-1), coded_length_ );
  const int16_t num_zeros = min( uint8_t(NUM_ZEROS_BINS-1), num_zeros_ );
  const int16_t eob_bin = min( uint8_t(EOB_BINS-1), uint8_t(coded_length_/(64 / EOB_BINS)));

  uint16_t above_num_zeros = context().above.initialized() ? context().above.get()->num_zeros() : 0;
  uint16_t left_num_zeros = context().left.initialized() ? context().left.get()->num_zeros() : 0;

  assert( above_num_zeros < NUM_ZEROS_BINS );
  assert( left_num_zeros < NUM_ZEROS_BINS );

  Optional<uint16_t> above_coded_length;
  Optional<uint16_t> left_coded_length;
  if (context().left.initialized()) {
      left_coded_length = context().left.get()->coded_length();
      assert( left_coded_length.get() < NUM_ZEROS_EOB_PRIORS );
  }
  if (context().above.initialized()) {
      above_coded_length = context().above.get()->coded_length();
      assert( above_coded_length.get() < NUM_ZEROS_EOB_PRIORS );
  }

  FixedArray<Branch, LOG_NUM_ZEROS_BINS> & num_zeros_prob
      = probability_tables.num_zeros_array(left_num_zeros,
                                           above_num_zeros,
                                           left_coded_length.get_or(0),
                                           above_coded_length.get_or(0));

  for (unsigned int i = 0; i < LOG_NUM_ZEROS_BINS; ++i) {
      encoder.put( (( 1 << i) & num_zeros) ? true : false, num_zeros_prob.at(i) );
  }

  auto & eob_prob = probability_tables.eob_array(num_zeros);
  PerBitEncoderState2u eob_encoder_state(&encoder, &eob_prob,
                                         left_coded_length.initialized()
                                         ? left_coded_length.get() / (64 / EOB_BINS) : left_coded_length,
                                         above_coded_length.initialized()
                                         ? above_coded_length.get() / (64/EOB_BINS): above_coded_length);
  put_one_unsigned_coefficient( eob_encoder_state, eob_bin );
  
  bool last_was_zero = false;

  for ( unsigned int index = 0; index <= min( uint8_t(63), coded_length_ ); index++ ) {
    /* select the tree probabilities based on the prediction context */
    uint8_t token_context = 0;
    if ( context().above.initialized() &&  context().left.initialized() ) {
/*        token_context += 1 + combine_priors(context().above.get()->coefficients().at( jpeg_zigzag.at( index ) ),
                                            context().left.get()->coefficients().at( jpeg_zigzag.at( index ) ) );
*/
    }
    Optional<int16_t> above_neighbor_context;
    Optional<int16_t> left_neighbor_context;
    if ( context().above.initialized() ) {
        above_neighbor_context = context().above.get()->coefficients().at( jpeg_zigzag.at( index ) );
    }
    if ( context().left.initialized() ) {
        left_neighbor_context = context().left.get()->coefficients().at( jpeg_zigzag.at( index ) );
    }
    uint16_t neighbor_context = std::min(8, skew_log<3, 4>((abs(above_neighbor_context.get_or(0))
                                                            + abs(left_neighbor_context.get_or(0))) / 2));
    (void)token_context;
    (void)neighbor_context;
    uint8_t coord = jpeg_zigzag.at( index );
/*
    if (index > 1) {
        token_context = 0;
        if (coord % 8 == 0) {
            token_context = 1 + skew_log<8, 8>(abs(coefficients().at( coord - 8)));
        } else if (coord > 8) {
            token_context += 1 + combine_priors(coefficients().at( coord - 1),
                                                coefficients().at( coord - 8) );
        } else {
            token_context = 1 + skew_log<8, 8>(abs(coefficients().at( coord - 1)));
        }
    }
*/
    Optional<int16_t> left_coef;
    Optional<int16_t> above_coef;
    if (index > 1) {
        token_context = 0;
        if (coord % 8 == 0) {
            above_coef = coefficients().at(coord - 8);
            token_context = 1 + std::min(7, skew_log<3, 2>(abs(above_coef.get_or(0))));
        } else if (coord > 8) {
            left_coef = coefficients().at(coord - 1);
            above_coef = coefficients().at(coord - 8);
            uint8_t coord_x = coord % 8;
            uint8_t coord_y = coord / 8;
            if (coord_x > coord_y) {
                token_context = 1 + std::min(7,skew_log<3, 2>(abs(left_coef.get_or(0))));// + 8 * std::min(skew_log<3, 2>(abs(coefficients().at( coord - 8))),7);
            } else {
                token_context = 1 + std::min(7, skew_log<3, 2>(abs(above_coef.get_or(0))));
            }
        } else {
            left_coef = coefficients().at(coord - 1);
            token_context = 1 + std::min(7, skew_log<3, 2>(abs(left_coef.get_or(0))));
        }
    }
    auto & prob = probability_tables.branch_array(std::min((unsigned int)type_,
                                                           BLOCK_TYPES - 1),
                                                  num_zeros,
                                                  eob_bin,
                                                  index_to_cat(index));

    PerBitEncoderState4s dct_encoder_state(&encoder, &prob,
                                           left_neighbor_context, above_neighbor_context,
                                           left_coef, above_coef);

    if ( index < coded_length_ ) {
      const int16_t coefficient = coefficients_.at( jpeg_zigzag.at( index ) );
      put_one_signed_coefficient( dct_encoder_state, last_was_zero, false, coefficients_.at( jpeg_zigzag.at( index ) ) );
      last_was_zero = coefficient == 0;
    } else {
      assert( index == coded_length_ );
      assert( index < 64 );
      assert( last_was_zero == false );
      put_one_signed_coefficient( dct_encoder_state, last_was_zero, true, 0 );
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
  if ( slice.size() != expected_size ) {
    throw runtime_error( "unexpected model file size. expecting " + to_string( expected_size ) );
  }

  memcpy( model_.get(), slice.buffer(), slice.size() );
}

Branch::Branch()
{
  optimize();
}

inline void BoolEncoder::put( const bool value, Branch & branch )
{
  put( value, branch.prob() );
  if ( value ) {
    branch.record_true_and_update();
  } else {
    branch.record_false_and_update();
  }
  //branch.optimize();
}
