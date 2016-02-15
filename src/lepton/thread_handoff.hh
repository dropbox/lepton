#ifndef THREAD_HANDOFF_HH_
#define THREAD_HANDOFF_HH_
#include <vector>
#include "../vp8/util/options.hh"
#include "../vp8/util/aligned_block.hh"
#include "../vp8/util/nd_array.hh"

class ThreadHandoff {
public:
    uint16_t luma_y_start;
    uint16_t luma_y_end;
    uint32_t segment_size;
    uint8_t overhang_byte;
    uint8_t num_overhang_bits;
    int16_t last_dc[(uint32_t)ColorChannel::NumBlockTypes];
    enum  {
        BYTES_PER_HANDOFF = (16 /* luma end is implicit*/ + 32 + 16 * 4 + 8 * 2) / 8
    };
    static std::vector<ThreadHandoff> deserialize(const unsigned char *data, size_t max_size);
    static Sirikata::Array1d<unsigned char,
                             NUM_THREADS * BYTES_PER_HANDOFF
                             + 2> serialize(const Sirikata::Array1d<ThreadHandoff,
                                                                  NUM_THREADS>&data);
    static std::vector<ThreadHandoff> make_rand(int num_items);
};

#endif
