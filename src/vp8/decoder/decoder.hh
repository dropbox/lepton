/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef DECODER_HH
#define DECODER_HH
typedef Sirikata::Array1d<BlockContext, 3> DecodeChannelContext;
template<bool all_neighbors_present, BlockType color>
void parse_tokens(DecodeChannelContext context,
                  BoolDecoder& data,
                  ProbabilityTables<all_neighbors_present, color> & probability_tables,
                  ProbabilityTablesBase&pt);


#endif
