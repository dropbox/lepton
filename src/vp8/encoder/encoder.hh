/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef ENCODER_HH
#define ENCODER_HH
#include "model.hh"
template<bool has_left, bool has_above, bool has_above_right, BlockType color> void serialize_tokens( ConstBlockContext context,
                       BoolEncoder & encoder,
                       ProbabilityTables<has_left, has_above, has_above_right, color> & probability_tables);


#endif /* ENCODER_HH */
