#ifndef COLOR_CONTEXT_HH_
#define COLOR_CONTEXT_HH_

enum class BlockType { Y, Cb, Cr
#ifdef ALLOW_FOUR_COLORS
        , Ck
#endif
        };

class AlignedBlock;

struct BlockColorContext {
    uint8_t color; // 0 for Y 1 for Cb and 2 for Cr, 3 for (K in CMYK)
#ifdef USE_COLOR_VALUES
    const AlignedBlock *luminance[2][2];
    const AlignedBlock *chroma;
#endif
};
struct BlockColorContextIndices {
#ifdef USE_COLOR_VALUES
    Optional<std::pair<int, int> > luminanceIndex[2][2];
    Optional<std::pair<int, int> > chromaIndex;
#endif
};
#endif
