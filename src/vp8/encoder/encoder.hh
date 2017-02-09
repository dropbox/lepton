/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef ENCODER_HH
#define ENCODER_HH
#include "model.hh"
typedef Sirikata::Array1d<ConstBlockContext, 3> EncodeChannelContext;
template<bool all_neighbors_present, BlockType color>
void serialize_tokens(EncodeChannelContext context,
                      BoolEncoder & encoder,
                      ProbabilityTables<all_neighbors_present, color> & probability_tables,
                      ProbabilityTablesBase&);


#endif /* ENCODER_HH */
