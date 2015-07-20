#include <fstream>

#include "model.hh"
Context *gctx = (Context*)memset(calloc(sizeof(Context),1), 0xff, sizeof(Context));
void ProbabilityTables::serialize( std::ofstream & output ) const
{
  output.write( reinterpret_cast<char*>( model_.get() ), sizeof( *model_ ) );
}

void ProbabilityTables::optimize()
{
  model_->forall( [&] ( Branch & x ) { x.optimize(); } );
}
static BitContexts helper_context_from(bool missing,
                                   int32_t signed_value,
                                   const BitsAndLivenessFromEncoding& bits,
                                   unsigned int token_id, uint16_t min, uint16_t max) {
    if (missing) {
        return CONTEXT_UNSET;
    }
    uint16_t unsigned_value = abs(signed_value);
    BitContexts context = CONTEXT_BIT_ZERO;
    if (unsigned_value < min) {
        context = CONTEXT_LESS_THAN;
    } else if (unsigned_value > max) {
        context = CONTEXT_GREATER_THAN;
    } else {
        uint64_t bit_to_check = 1ULL;
        bit_to_check <<= token_id;
        if (bit_to_check & bits.bits()) {
            context = CONTEXT_BIT_ONE;
        }
    }
    return context;
}
BitContexts context_from_value_bits_id_min_max(Optional<int16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max) {
    return helper_context_from(!value.initialized(),
                               value.get_or(0),
                               bits,
                               token_id,
                               min, max);
}

BitContexts context_from_value_bits_id_min_max(Optional<uint16_t> value,
                                           const BitsAndLivenessFromEncoding& bits,
                                           unsigned int token_id, uint16_t min, uint16_t max) {
    return helper_context_from(!value.initialized(),
                               value.get_or(0),
                               bits,
                               token_id,
                               min, max);
}
