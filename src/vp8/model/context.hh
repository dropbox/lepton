struct DefaultContext {
    FixedArray<Branch, ENTROPY_NODES> *prob_;

    DefaultContext(FixedArray<Branch, ENTROPY_NODES> *prob) :
        prob_(prob) {
    }
    Branch& operator()(unsigned int token_id, uint16_t /*min*/, uint16_t /*max*/) const {
        return prob_->at(token_id);
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
