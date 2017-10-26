/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef DECODER_HH
#define DECODER_HH
template<bool all_neighbors_present, BlockType color, class BoolDecoder>
void parse_tokens(BlockContext context,
                  BoolDecoder& data,
                  ProbabilityTables<all_neighbors_present, color> & probability_tables,
                  ProbabilityTablesBase&pt);


#endif
