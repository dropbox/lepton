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
typedef PerBitEncoderState<ExponentContext> PerBitEncoderStateExp;
void Block::serialize_tokens( BoolEncoder & encoder,
                              ProbabilityTables & probability_tables ) const
{
    Optional<uint8_t> num_nonzeros_above;
    Optional<uint8_t> num_nonzeros_left;
    if (context().above.initialized()) {
        num_nonzeros_above = context().above.get()->num_nonzeros_7x7();
    }
    if (context().left.initialized()) {
        num_nonzeros_left = context().left.get()->num_nonzeros_7x7();
    }
    auto & num_nonzeros_prob = probability_tables.nonzero_counts_7x7(type_,
                                                               num_nonzeros_left,
                                                               num_nonzeros_above);
    for (unsigned int index = 0; index < 6; ++index) {
        encoder.put((num_nonzeros_7x7_ & (1 << index)) ? 1 : 0, num_nonzeros_prob.at(index));
    }
    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7_;
    for (unsigned int coord = 0; coord < 64; ++coord) {
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        int16_t coef = coefficients_.at( coord );
        uint16_t abs_coef = abs(coef);
        if (coord == 0 || (b_x > 0 && b_y > 0)) { // this does the DC and the lower 7x7 AC
            uint8_t length = bit_length(abs_coef);
            auto & exp_prob = probability_tables.exponent_array_7x7(type_, coord, num_nonzeros_7x7_, *this);
            for (int i = 0;i < 4;++i) {
                encoder.put((length & (1 << i)) ? 1 : 0, exp_prob.at(i));
            }
            if (length > 1){
                auto &res_prob = probability_tables.residual_noise_array_7x7(type_, coord, num_nonzeros_7x7_);
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
                
                for (int i = length - 2; i >= 0; --i) {
                   encoder.put((abs_coef & (1 << i)), res_prob.at(i));
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
                encoder.put(coef >= 0 ? 1 : 0, sign_prob);
                if (coord != 0) {
                    --num_nonzeros_left_7x7;
                }
            }
            
            if (num_nonzeros_left_7x7 == 0) {
                break; // done with the 49x49
            }
        }
    }
    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    for (unsigned int coord = 0; coord < 64; ++coord) {
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        if ((b_x > 0 && b_y > 0)) { // this does the DC and the lower 7x7 AC
            if (coefficients_.at( coord )){
                eob_x = std::max(eob_x, (uint8_t)b_x);
                eob_y = std::max(eob_y, (uint8_t)b_y);
            }
        }
    }
    auto &prob_x = probability_tables.nonzero_counts_1x8(type_,
                                                      eob_x,
                                                      num_nonzeros_7x7_);
    auto &prob_y = probability_tables.nonzero_counts_1x8(type_,
                                                      eob_y,
                                                      num_nonzeros_7x7_);
    for (int i= 0 ;i <3;++i) {
        encoder.put((num_nonzeros_x_ & (1 << i)) ? 1 : 0, prob_x.at(i));
    }
    for (int i= 0 ;i <3;++i) {
        encoder.put((num_nonzeros_y_ & (1 << i)) ? 1 : 0, prob_y.at(i));
    }
    uint8_t num_nonzeros_left_x = num_nonzeros_x_;
    uint8_t num_nonzeros_left_y = num_nonzeros_y_;
    for (unsigned int coord = 1; coord < 64; ++coord) {
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        int16_t coef = coefficients_.at( coord );
        uint16_t abs_coef = abs(coef);
        uint8_t num_nonzeros_edge = 0;
        if (b_y == 0 && num_nonzeros_left_x) {
            num_nonzeros_edge = num_nonzeros_x_;
        }
        if (b_x == 0 && num_nonzeros_left_y) {
            num_nonzeros_edge = num_nonzeros_y_;
        }
        if ((b_x == 0 && num_nonzeros_left_y) || (b_y == 0 && num_nonzeros_left_x)) {
            auto &exp_array = probability_tables.exponent_array_x(type_, coord, num_nonzeros_edge, *this);
            uint8_t length = bit_length(abs_coef);
            for (int i = 0;i < 4;++i) {
                encoder.put((length & (1 << i)), exp_array.at(i));
            }
            if (length > 0) {
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            }
            if (length > 1) {
                
                int i;
                for (i = length - 2; i >= (int)RESIDUAL_NOISE_FLOOR; --i) {
                    auto &thresh_prob = probability_tables.residual_thresh_array(type_, coord, length, *this);
                    encoder.put((abs_coef & (1 << i)) ? 1 : 0, thresh_prob.at(i - RESIDUAL_NOISE_FLOOR));
                }
                for (; i >= 0; --i) {
                    auto &res_prob = probability_tables.residual_noise_array_x(type_, coord, num_nonzeros_edge);
                    encoder.put((abs_coef & (1 << i)) ? 1 : 0, res_prob.at(i));
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
                encoder.put(coef >= 0, sign_prob);
                if ( b_x == 0) {
                    --num_nonzeros_left_y;
                }
                if ( b_y == 0) {
                    --num_nonzeros_left_x;
                }
            }
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

const ProbabilityTables &ProbabilityTables::debug_print(const ProbabilityTables * /*other*/)const
{
/*
    for ( unsigned int type = 0; type < model_->token_branch_counts_.size(); type++ ) {
        const auto & this_type = model_->token_branch_counts_.at( type );
        const auto *other_type = other ? &other->model_->token_branch_counts_.at( type ) : NULL;
        for ( unsigned int num_nonzeros_bin = 0; num_nonzeros_bin < this_type.size(); num_nonzeros_bin++ ) {
            const auto & this_num_nonzeros_bin = this_type.at( num_nonzeros_bin );
            const auto * other_num_nonzeros_bin = other ? &other_type->at( num_nonzeros_bin ) : NULL;
            for ( unsigned int eob_bin = 0; eob_bin < this_num_nonzeros_bin.size(); eob_bin++ ) {
                const auto & this_eob_bin = this_num_nonzeros_bin.at( eob_bin );
                const auto * other_eob_bin = other ? &other_num_nonzeros_bin->at( eob_bin ) : NULL;
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
                                        cout << "token_branch_counts[ " << type << " ][ "<< num_nonzeros_bin <<" ][ " << eob_bin << " ][ " << band << " ][ " << prev_context << " ]["<<neighbor_context<<"] = ";
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
*/
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
    quantization_table_ = nullptr;
    model_->forall( [&] ( Branch & x ) { x = Branch(); } );
}


void ProbabilityTables::normalize()
{
  model_->forall( [&] ( Branch & x ) { x.normalize(); } );
}

ProbabilityTables::ProbabilityTables( const Slice & slice )
  : model_( new Model )
{
    quantization_table_ = nullptr;
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
