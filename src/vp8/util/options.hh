#ifndef OPTIONS_HH_
#define OPTIONS_HH_

enum {
    VECTORIZE = 1,
    MICROVECTORIZE = 1,
    MAX_NUM_THREADS = 8,
    SIMD_WIDTH = 1
};
extern unsigned int NUM_THREADS;
#endif
