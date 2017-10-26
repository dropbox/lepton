/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef ENCODER_HH
#define ENCODER_HH
#include "model.hh"
template<bool all_neighbors_present, BlockType color, class BoolEncoder>
void serialize_tokens(ConstBlockContext context,
                      BoolEncoder & encoder,
                      ProbabilityTables<all_neighbors_present, color> & probability_tables,
                      ProbabilityTablesBase&);


#endif /* ENCODER_HH */
