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
uint8_t prefix_remap(uint8_t v) {
    if (v == 0) {
        return 0;
    }
    return v + 3;
}
typedef PerBitEncoderState<DefaultContext> EncoderState;
typedef PerBitEncoderState<PerBitContext2u> PerBitEncoderState2u;
typedef PerBitEncoderState<PerBitContext4s> PerBitEncoderState4s;
typedef PerBitEncoderState<ExponentContext> PerBitEncoderStateExp;
void Block::serialize_tokens( BoolEncoder & encoder,
                              ProbabilityTables & probability_tables ) const
{

    auto & num_nonzeros_prob = probability_tables.nonzero_counts_7x7(type_, *this);
    int serialized_so_far = 0;
    for (int index = 5; index >= 0; --index) {
        int cur_bit = (num_nonzeros_7x7_ & (1 << index)) ? 1 : 0; 
        encoder.put(cur_bit, num_nonzeros_prob.at(index).at(serialized_so_far));
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }

    {
        // do DC
        uint8_t coord = 0;
        int16_t coef = probability_tables.predict_or_unpredict_dc(*this, false);
        uint16_t abs_coef = abs(coef);
        uint8_t length = bit_length(abs_coef);
        auto & exp_prob = probability_tables.exponent_array_7x7(type_, coord, num_nonzeros_7x7_, *this);
        uint8_t slen = prefix_remap(length);
        unsigned int serialized_so_far = 0;
        for (int i = 3;i >= 0; --i) {
            bool cur_bit = (slen & (1 << i)) ? true : false;
            encoder.put(cur_bit, exp_prob.at(i).at(serialized_so_far));
            serialized_so_far <<= 1;
            if (cur_bit) {
                serialized_so_far |=1;
            }
            if (i == 2 && !length) break;
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
        }
    }


    uint8_t num_nonzeros_left_7x7 = num_nonzeros_7x7_;
    for (unsigned int zz = 0; zz < 64; ++zz) {
        unsigned int coord = unzigzag[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        int16_t coef = coefficients_.at( coord );
        uint16_t abs_coef = abs(coef);
        if (b_x > 0 && b_y > 0) { // this does the DC and the lower 7x7 AC
            uint8_t length = bit_length(abs_coef);
            auto & exp_prob = probability_tables.exponent_array_7x7(type_, coord, num_nonzeros_left_7x7, *this);
            uint8_t slen = prefix_remap(length);
            unsigned int serialized_so_far = 0;
            for (int i = 3;i >= 0; --i) {
                bool cur_bit = (slen & (1 << i)) ? true : false;
                encoder.put(cur_bit, exp_prob.at(i).at(serialized_so_far));
                serialized_so_far <<= 1;
                if (cur_bit) {
                    serialized_so_far |=1;
                }
                if (i == 2 && !length) break;
            }
            if (length > 1){
                auto &res_prob = probability_tables.residual_noise_array_7x7(type_, coord, num_nonzeros_left_7x7);
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
                
                for (int i = length - 2; i >= 0; --i) {
                   encoder.put((abs_coef & (1 << i)), res_prob.at(i));
                }
            }
            if (length != 0) {
                auto &sign_prob = probability_tables.sign_array(type_, coord, *this);
                encoder.put(coef >= 0 ? 1 : 0, sign_prob);
                --num_nonzeros_left_7x7;
            }
            
            if (num_nonzeros_left_7x7 == 0) {
                break; // done with the 49x49
            }
        }
    }

    uint8_t eob_x = 0;
    uint8_t eob_y = 0;
    for (unsigned int zz = 0; zz < 64; ++zz) {
        unsigned int coord = unzigzag[zz];
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
                                                         num_nonzeros_7x7_, true);
    auto &prob_y = probability_tables.nonzero_counts_1x8(type_,
                                                      eob_y,
                                                         num_nonzeros_7x7_, false);
    serialized_so_far = 0;
    for (int i= 2; i >= 0; --i) {
        int cur_bit = (num_nonzeros_x_ & (1 << i)) ? 1 : 0;
        encoder.put(cur_bit, prob_x.at(i).at(serialized_so_far));
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;

    }
    serialized_so_far = 0;
    for (int i= 2; i >= 0; --i) {
        int cur_bit = (num_nonzeros_y_ & (1 << i)) ? 1 : 0;
        encoder.put(cur_bit, prob_y.at(i).at(serialized_so_far));
        serialized_so_far <<= 1;
        serialized_so_far |= cur_bit;
    }
    uint8_t num_nonzeros_left_x = num_nonzeros_x_;
    uint8_t num_nonzeros_left_y = num_nonzeros_y_;
    for (unsigned int zz = 1; zz < 64; ++zz) {
        unsigned int coord = unzigzag[zz];
        unsigned int b_x = (coord & 7);
        unsigned int b_y = coord / 8;
        int16_t coef = coefficients_.at( coord );
        uint16_t abs_coef = abs(coef);
        uint8_t num_nonzeros_edge = 0;
        if (b_y == 0 && num_nonzeros_left_x) {
            num_nonzeros_edge = num_nonzeros_left_x;
        }
        if (b_x == 0 && num_nonzeros_left_y) {
            num_nonzeros_edge = num_nonzeros_left_y;
        }
        if ((b_x == 0 && num_nonzeros_left_y) || (b_y == 0 && num_nonzeros_left_x)) {
            assert(coord != 9);
            auto &exp_array = probability_tables.exponent_array_x(type_, coord, num_nonzeros_edge, *this);
            uint8_t length = bit_length(abs_coef);
            uint8_t slen = prefix_remap(length);
            unsigned int serialized_so_far = 0;
            for (int i = 3; i >= 0; --i) {
                bool cur_bit = ((slen & (1 << i)) ? true : false);
                encoder.put(cur_bit, exp_array.at(i).at(serialized_so_far));
                serialized_so_far <<= 1;
                if (cur_bit) {
                    serialized_so_far |= 1;
                }
                if (i == 2 && !length) break;
            }
            if (length > 0) {
                assert((abs_coef & ( 1 << (length - 1))) && "Biggest bit must be set");
                assert((abs_coef & ( 1 << (length)))==0 && "Beyond Biggest bit must be zero");
            }
            if (length > 1) {
                
                int min_threshold = 0;

                int max_val = probability_tables.get_max_value(coord);
                int max_len = bit_length(max_val);
                    
                if (max_len > (int)RESIDUAL_NOISE_FLOOR) {
                    min_threshold = max_len - RESIDUAL_NOISE_FLOOR;
                }
                int i = length - 2;
                if (length - 2 >= min_threshold) {
                    uint16_t encoded_so_far = 1;
                    auto &thresh_prob = probability_tables.residual_thresh_array(type_, coord, length,
                                                                                 *this, min_threshold, max_val);
                    for (; i >= min_threshold; --i) {
                    
                        int cur_bit = (abs_coef & (1 << i)) ? 1 : 0;
                        encoder.put(cur_bit, thresh_prob.at(encoded_so_far));
                        encoded_so_far <<=1;
                        if (cur_bit) {
                            encoded_so_far |=1;
                        }
                    }
                    probability_tables.residual_thresh_array_annot_update(coord, encoded_so_far / 2);
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
template<class BranchArray> void print_helper(const BranchArray& ba,
                                              const std::string &table_name,
                                              const std::vector<std::string> &names,
                                              std::vector<uint32_t> &values) {
    values.push_back(0);
    for (size_t i = 0; i < ba.size(); ++i) {
        values.back() = i;
        print_helper(ba.at(i), table_name, names, values);
    }
    values.pop_back();
}
template<> void print_helper(const Branch& ba,
                             const std::string&table_name,
                        const std::vector<std::string> &names,
                        std::vector<uint32_t> &values) {
    double ratio = (ba.true_count() + 1) / (double)(ba.false_count() + ba.true_count() + 2);
    if (ba.true_count() > 0 ||  ba.false_count() > 1) {
        //if (ba.true_count() > 10 && ba.false_count() > 10 && ratio > .45 && ratio < .55)
        {
            assert(names.size() == values.size());
            cout <<table_name<<"::";
            for (size_t i = 0; i < names.size(); ++i) {
                cout << names[i]<<'['<<values[i]<<']';
            }
            cout << " = (" << ba.true_count() <<", "<<  (ba.false_count() - 1) << ")\n";
        }
    }
}
template<class BranchArray> void print_all(const BranchArray &ba,
                                           const std::string &table_name,
                                           const std::vector<std::string> &names) {
    std::vector<uint32_t> tmp;
    print_helper(ba, table_name, names, tmp);
}  
                                  
const ProbabilityTables &ProbabilityTables::debug_print()const
{
    print_all(model_->num_nonzeros_counts_7x7_,
              "NONZERO 7x7",
              {"cmp","nbr","bit","prevbits"});

    print_all(model_->num_nonzeros_counts_1x8_,
              "NONZERO_1x8",
              {"cmp","eobx","num_nonzeros","bit","prevbits"});
    print_all(model_->num_nonzeros_counts_8x1_,
              "NONZERO_8x1",
              {"cmp","eobx","num_nonzeros","bit","prevbits"});
    print_all(model_->exponent_counts_dc_,
              "EXP_DC",
              {"cmp","num_nonzeros","neigh_exp","bit","prevbits"});
    print_all(model_->exponent_counts_,
              "EXP7x7",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"});
    print_all(model_->exponent_counts_x_,
              "EXP_8x1",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"});
    print_all(model_->residual_noise_counts_,
              "NOISE",
              {"cmp","coef","num_nonzeros","bit"});
    print_all(model_->residual_threshold_counts_,
              "THRESH8",
              {"cmp","max","exp","prevbits"});
    print_all(model_->sign_counts_,
              "SIGN",
              {"cmp","lakh","exp"});
            
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
