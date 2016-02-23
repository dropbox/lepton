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
    Sirikata::Array1d<int16_t, (uint32_t)ColorChannel::NumBlockTypes> last_dc;
    enum  {
        BYTES_PER_HANDOFF = (16 /* luma end is implicit*/ + 32 + 16 * 4 + 8 * 2) / 8,
        // num_overhang_bits is set to this for legacy formats which must be decoded single threaded
        LEGACY_OVERHANG_BITS = 0xff
    };
    static ThreadHandoff zero() {
        ThreadHandoff ret;
        memset(&ret, 0, sizeof(ret));
        return ret;
    }
    bool is_legacy_mode() const{ // legacy mode doesn't have access to handoff data
        return num_overhang_bits == LEGACY_OVERHANG_BITS;
    }
    static size_t get_remaining_data_size_from_two_bytes(unsigned char input[2]);
    static std::vector<ThreadHandoff> deserialize(const unsigned char *data, size_t max_size);
    static std::vector<unsigned char> serialize(const ThreadHandoff * data,
                                                unsigned int num_threads);
    static std::vector<ThreadHandoff> make_rand(int num_items);

    /* combine two ThreadHandoff objects into a range, starting with the initialization
       of the thread represented by the first object, and continuing until the end
       of the second object */
    ThreadHandoff operator-( const ThreadHandoff & other ) const;
};

#endif
