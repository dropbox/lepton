/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef DECODER_HH
#define DECODER_HH
void parse_tokens(BlockContext context,
                  BlockColorContext color,
                  BoolDecoder & data,
                  ProbabilityTables & probability_tables);


#endif
