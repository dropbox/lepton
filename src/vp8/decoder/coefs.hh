#ifndef COEFS_HH
#define COEFS_HH

#include "bool_decoder.hh"
#include "model.hh"
#include <limits>
class BoolEncoder;


template <uint64_t x, uint64_t n=32> struct static_log2 {
    enum uint64_t {
        c = ((x >> n ) > 0) ? 1 : 0
    };
    enum uint64_t {
        value = c * n + static_log2<(x >> (c * n)), n / 2>::value
    };
};
template <> struct static_log2 <1,0> {
    enum uint64_t {
        value = 0
    };
};
template <int n> struct static_ceil_log2 {
    enum uint64_t {
        value = (1 << static_log2<n>::value) < n ? static_log2<n>::value + 1 : static_log2<n>::value
    };
};

template<typename intt> intt log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    const intt b[] = {0x2,
                      0xC,
                      0xF0,
                      (intt)0xFF00,
                      (intt)0xFFFF0000U,
                      std::numeric_limits<intt>::max() - (intt)0xFFFFFFFFU};
    const intt S[] = {1, 2, 4, 8, 16, 32};

    register intt r = 0; // result of log2(v) will go here
    
    for (signed int i = loop_max; i >= 0; i--) // unroll for speed...
    {
        if (v & b[i])
        {
            v >>= S[i];
            r |= S[i];
        } 
    }
    return r;
}

template <int bits, int highest_likely_value> int skew_log(int number) {
    static_assert(static_ceil_log2<highest_likely_value>::value <= bits,
                  "The highest likely number must be less than the number of bits provided");
    if (number < highest_likely_value) {
        return number;
    }
    int offset = highest_likely_value - static_log2<highest_likely_value>::value;
    if (bits <= 8) {
        offset += log2<uint8_t>((uint8_t)number);
    } else if (bits <= 16) {
        offset += log2<uint16_t>((uint16_t)number);
    } else if (bits <= 32) {
        offset += log2<uint32_t>((uint32_t)number);
    } else {
        offset += log2<uint64_t>((uint64_t)number);
    }
    return std::min(offset, (1 << bits));
}


template<unsigned int prev_coef_contexts=PREV_COEF_CONTEXTS> int combine_priors(int16_t a, int16_t b) {
    const int max_likely_value = 6;
    int16_t al = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(a));
    int16_t bl = skew_log<static_ceil_log2<prev_coef_contexts-1>::value / 2,
                          max_likely_value>(abs(b));
    int retval = std::min(al + (1U << (static_ceil_log2<prev_coef_contexts-1>::value / 2)) * bl,
                          prev_coef_contexts-1);
    return retval;
}




#endif /* TOKENS_HH */
