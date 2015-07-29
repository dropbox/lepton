#ifndef _COLOR_CONTEXT_HH_
#define _COLOR_CONTEXT_HH_
#include "../util/option.hh"
class Block;

struct BlockColorContext {
    const Block *luminance[2][2];
    const Block *chroma;
};

struct BlockColorContextIndices {
    Optional<std::pair<int, int> > luminanceIndex[2][2];
    Optional<std::pair<int, int> > chromaIndex;
};

#endif
