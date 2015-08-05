/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifndef ENCODER_HH
#define ENCODER_HH
#include <cassert>

void serialize_tokens( BlockContext context,
                       BlockColorContext,
                       BoolEncoder & encoder,
                       ProbabilityTables & probability_tables);

#endif /* ENCODER_HH */
