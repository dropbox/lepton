#include "weight.hh"
struct DefaultContext {
    FixedArray<Branch, ENTROPY_NODES> *prob_;

    DefaultContext(FixedArray<Branch, ENTROPY_NODES> *prob) :
        prob_(prob) {
    }
    Branch& operator()(unsigned int token_id, uint16_t /*min*/, uint16_t /*max*/) const {
        return prob_->at(token_id);
    }    
};

struct ExponentContext {
    FixedArray<Branch, NUMBER_OF_EXPONENT_BITS> *prob_;

    ExponentContext(FixedArray<FixedArray<Branch, NUMBER_OF_EXPONENT_BITS>,
                              NUMERIC_LENGTH_MAX> *prob, const Block&block, uint8_t coord, uint8_t zigzag)
        {
            Optional<uint16_t> toptop;
            Optional<uint16_t> topleft;
            Optional<uint16_t> top;
            Optional<uint16_t> topright;
            Optional<uint16_t> leftleft;
            Optional<uint16_t> left;
            uint32_t total = 0;
            uint32_t weights = 0;
            uint32_t coef_index = coord;
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
            uint32_t unweighted_total = 0;
            if (weights) {
                unweighted_total = total / weights;
            }
            auto prob_offset = log2(unweighted_total + 1);
            //fprintf(stderr, "%d vs %d\t\t<=> %d : %d\n", unweighted_total, block.coefficients().at(coef_index), prob_offset, log2(1 + abs(block.coefficients().at(coef_index))));
            //prob_offset = log2(abs(block.coefficients().at(coef_index)));// future looking
            prob_ = &prob->at(prob_offset);
    }
    Branch& operator()(unsigned int token_id, uint16_t /*min*/, uint16_t /*max*/) const {
        return prob_->at(token_id - (unsigned int)TokenNode::LENGTH0);
    }    
};

struct PerBitContext2u {

    Optional<uint16_t> left_value_;
    Optional<uint16_t> above_value_;

    BitsAndLivenessFromEncoding left_bits_;
    BitsAndLivenessFromEncoding above_bits_;

    typedef FixedArray<FixedArray<Branch,
                                NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                      ENTROPY_NODES > NestedProbabilityArray;
    NestedProbabilityArray *probability_;
public:
    Branch& operator()(unsigned int token_id, uint16_t min, uint16_t max) const {
        uint8_t left_context = context_from_value_bits_id_min_max(left_value_,
                                                                  left_bits_,
                                                                  token_id, min, max);
        uint8_t above_context = context_from_value_bits_id_min_max(above_value_,
                                                                   above_bits_,
                                                                   token_id, min, max);
        return probability_->at(token_id).at(left_context + (above_context * NUM_BIT_CONTEXTS));
    }
    PerBitContext2u(NestedProbabilityArray  *prob,
                    Optional<uint16_t> left_coded_length,
                    Optional<uint16_t> above_coded_length);

};

struct PerBitContext4s {

    Optional<int16_t> left_block_value_;
    Optional<int16_t> above_block_value_;

    Optional<int16_t> left_coef_value_;
    Optional<int16_t> above_coef_value_;

    BitsAndLivenessFromEncoding left_block_bits_;
    BitsAndLivenessFromEncoding above_block_bits_;

    BitsAndLivenessFromEncoding left_coef_bits_;
    BitsAndLivenessFromEncoding above_coef_bits_;

    typedef FixedArray<FixedArray<FixedArray<Branch,
                                          NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                                NUM_BIT_CONTEXTS * NUM_BIT_CONTEXTS>,
                      ENTROPY_NODES+2 > NestedProbabilityArray;
    NestedProbabilityArray *probability_;
public:
    Branch& operator()(unsigned int token_id, uint16_t min, uint16_t max) const {
        uint8_t left_block_context = context_from_value_bits_id_min_max(left_block_value_,
                                                                        left_block_bits_,
                                                                        token_id, min, max);
        uint8_t above_block_context = context_from_value_bits_id_min_max(above_block_value_,
                                                                         above_block_bits_,
                                                                         token_id, min, max);

        uint8_t left_coef_context = context_from_value_bits_id_min_max(left_coef_value_,
                                                                       left_coef_bits_,
                                                                       token_id, min, max);
        uint8_t above_coef_context = context_from_value_bits_id_min_max(above_coef_value_,
                                                                        above_coef_bits_,
                                                                        token_id, min, max);

        return probability_->at(token_id).at(left_block_context
                                             + (above_block_context * 5)).at(left_coef_context
                                                                             + (above_coef_context * 5));
    }
    PerBitContext4s(NestedProbabilityArray  *prob,
                    Optional<int16_t> left_block_value,
                    Optional<int16_t> above_block_value,
                    Optional<int16_t> left_coef_value,
                    Optional<int16_t> above_coef_value);

};
