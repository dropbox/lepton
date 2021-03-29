#ifndef VP8_MODEL_NUMERIC_HH_
#define VP8_MODEL_NUMERIC_HH_
//#define DEBUGDECODE
//for uint16_t
#include <cstdint>
//for pair
#include <utility>
// for std::min
#include <algorithm>
#include <assert.h>
#include "../util/memory.hh"

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <immintrin.h>
#include <tmmintrin.h>
#include "../util/mm_mullo_epi32.hh"
#endif

#ifdef __GNUC__
#if __GNUC__ == 7
#if __GNUC_MINOR__ == 1
// workaround https://gcc.gnu.org/bugzilla/show_bug.cgi?id=81015
#define WORKAROUND_CLZ_BUG
#endif
#endif
#endif

#ifdef _WIN32
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
static uint32_t __inline __builtin_clz(uint32_t x) {
    unsigned long r = 0;
    _BitScanReverse(&r, x);
    return 31 - r;
}
static uint64_t __inline __builtin_clzl(uint64_t x) {
    uint64_t first_half = x;
    first_half >>= 16;
    first_half >>= 16;
    if (first_half) {
        return __builtin_clz(first_half);
    }
    return 32 + __builtin_clz(x & 0xffffffffU);
}
#endif

static constexpr uint8_t LogTable16[16] = {
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3
};
static constexpr char LogTable256[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};
static constexpr uint8_t LenTable16[16] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};
static constexpr char LenTable256[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
    LT(5), LT(6), LT(6), LT(7), LT(7), LT(7), LT(7),
    LT(8), LT(8), LT(8), LT(8), LT(8), LT(8), LT(8), LT(8)
#undef LT
};

#if 0
inline constexpr uint8_t k16log2(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
    ? 12 + LogTable16[v >> 12]
    : 8 + LogTable16[v>>8]
    : 4 + LogTable16[v>>4]
    : LogTable16[v];
}
inline constexpr uint8_t uint16bit_length(uint16_t v) {
    return (v & 0xfff0) ? (v & 0xff00) ? (v & 0xf000)
    ? 12 + LenTable16[v >> 12]
    : 8 + LenTable16[v>>8]
    : 4 + LenTable16[v>>4]
    : LenTable16[v];
}
#else
inline constexpr uint8_t k16log2(uint16_t v) {
    return (v & 0xff00)
    ? 8 + LogTable256[v >> 8]
    : LogTable256[v];
}
inline constexpr uint8_t k16bit_length(uint16_t v) {
    return (v & 0xff00)
    ? 8 + LenTable256[v >> 8]
    : LenTable256[v];
}


inline uint8_t uint16log2(uint16_t v) {
#ifdef WORKAROUND_CLZ_BUG
    return k16log2(v);
#else
    return 31 - __builtin_clz((uint32_t)v);
#endif
}
inline uint8_t nonzero_bit_length(uint16_t v) {
    dev_assert(v);
#ifdef WORKAROUND_CLZ_BUG
    return k16bit_length(v);
#else
    return 32 - __builtin_clz((uint32_t)v);
#endif
    
}
inline uint8_t uint16bit_length(uint16_t v) {
#ifdef WORKAROUND_CLZ_BUG
    return k16bit_length(v);
#else
    return v ? 32 - __builtin_clz((uint32_t)v) : 0;
#endif
}
#endif

constexpr uint8_t log_max_numerator = 18;

inline constexpr uint32_t computeDivisor(uint16_t d) {
    return (((( 1 << k16bit_length(d)) - d) << log_max_numerator) / d) + 1;
}
#define COMPUTE_DIVISOR(off) \
   computeDivisor(off) \
   ,computeDivisor(off + 1) \
   ,computeDivisor(off + 2) \
   ,computeDivisor(off + 3) \
   ,computeDivisor(off + 4) \
   ,computeDivisor(off + 5) \
   ,computeDivisor(off + 6) \
   ,computeDivisor(off + 7) \
   ,computeDivisor(off + 8) \
   ,computeDivisor(off + 9) \
   ,computeDivisor(off + 10) \
   ,computeDivisor(off + 11) \
   ,computeDivisor(off + 12) \
   ,computeDivisor(off + 13) \
   ,computeDivisor(off + 14) \
   ,computeDivisor(off + 15)
#define COMPUTE_DIVISOR_x100(off) \
   COMPUTE_DIVISOR(off + 0x00) \
   ,COMPUTE_DIVISOR(off + 0x10) \
   ,COMPUTE_DIVISOR(off + 0x20) \
   ,COMPUTE_DIVISOR(off + 0x30)

#define COMPUTE_LOG2(off) \
 k16log2(off) \
,k16log2(off + 1) \
,k16log2(off + 2) \
,k16log2(off + 3) \
,k16log2(off + 4) \
,k16log2(off + 5) \
,k16log2(off + 6) \
,k16log2(off + 7) \
,k16log2(off + 8) \
,k16log2(off + 9) \
,k16log2(off + 10) \
,k16log2(off + 11) \
,k16log2(off + 12) \
,k16log2(off + 13) \
,k16log2(off + 14) \
,k16log2(off + 15)

#define COMPUTE_LOG2_x100(off) \
COMPUTE_LOG2(off + 0x00) \
,COMPUTE_LOG2(off + 0x10) \
,COMPUTE_LOG2(off + 0x20) \
,COMPUTE_LOG2(off + 0x30)


#define COMPUTE_DIVISOR_AND_LOG2(off) \
{computeDivisor(off), k16log2(off)} \
,{computeDivisor(off + 1), k16log2(off + 1)} \
,{computeDivisor(off + 2), k16log2(off + 2)} \
,{computeDivisor(off + 3), k16log2(off + 3)} \
,{computeDivisor(off + 4), k16log2(off + 4)} \
,{computeDivisor(off + 5), k16log2(off + 5)} \
,{computeDivisor(off + 6), k16log2(off + 6)} \
,{computeDivisor(off + 7), k16log2(off + 7)} \
,{computeDivisor(off + 8), k16log2(off + 8)} \
,{computeDivisor(off + 9), k16log2(off + 9)} \
,{computeDivisor(off + 10), k16log2(off + 10)} \
,{computeDivisor(off + 11), k16log2(off + 11)} \
,{computeDivisor(off + 12), k16log2(off + 12)} \
,{computeDivisor(off + 13), k16log2(off + 13)} \
,{computeDivisor(off + 14), k16log2(off + 14)} \
,{computeDivisor(off + 15), k16log2(off + 15)}
#define COMPUTE_DIVISOR_AND_LOG2_x100(off) \
COMPUTE_DIVISOR_AND_LOG2(off + 0x00) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x10) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x20) \
,COMPUTE_DIVISOR_AND_LOG2(off + 0x30)


struct DivisorLog2 {
    uint32_t divisor;
    uint8_t len;
};
static constexpr DivisorLog2 DivisorAndLog2Table[1026] = {
    {0,0}
    ,{computeDivisor(1), k16log2(1)}
    ,{computeDivisor(2), k16log2(2)}
    ,{computeDivisor(3), k16log2(3)}
    ,{computeDivisor(4), k16log2(4)}
    ,{computeDivisor(5), k16log2(5)}
    ,{computeDivisor(6), k16log2(6)}
    ,{computeDivisor(7), k16log2(7)}
    ,{computeDivisor(8), k16log2(8)}
    ,{computeDivisor(9), k16log2(9)}
    ,{computeDivisor(10), k16log2(10)}
    ,{computeDivisor(11), k16log2(11)}
    ,{computeDivisor(12), k16log2(12)}
    ,{computeDivisor(13), k16log2(13)}
    ,{computeDivisor(14), k16log2(14)}
    ,{computeDivisor(15), k16log2(15)}
    ,COMPUTE_DIVISOR_AND_LOG2(0x10)
    ,COMPUTE_DIVISOR_AND_LOG2(0x20)
    ,COMPUTE_DIVISOR_AND_LOG2(0x30)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x40)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x80)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0xc0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x100)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x140)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x180)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x1c0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x200)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x240)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x280)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x2c0)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x300)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x340)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x380)
    ,COMPUTE_DIVISOR_AND_LOG2_x100(0x3c0)
    ,{computeDivisor(0x400), k16log2(0x400)}
    ,{computeDivisor(0x401), k16log2(0x401)}
};

static constexpr uint32_t Log2Table[1026] = {
    COMPUTE_LOG2_x100(0x00)
    ,COMPUTE_LOG2_x100(0x40)
    ,COMPUTE_LOG2_x100(0x80)
    ,COMPUTE_LOG2_x100(0xc0)
    ,COMPUTE_LOG2_x100(0x100)
    ,COMPUTE_LOG2_x100(0x140)
    ,COMPUTE_LOG2_x100(0x180)
    ,COMPUTE_LOG2_x100(0x1c0)
    ,COMPUTE_LOG2_x100(0x200)
    ,COMPUTE_LOG2_x100(0x240)
    ,COMPUTE_LOG2_x100(0x280)
    ,COMPUTE_LOG2_x100(0x2c0)
    ,COMPUTE_LOG2_x100(0x300)
    ,COMPUTE_LOG2_x100(0x340)
    ,COMPUTE_LOG2_x100(0x380)
    ,COMPUTE_LOG2_x100(0x3c0)
    ,k16log2(0x400)
    ,k16log2(0x401)
};

static constexpr uint32_t DivisorMultipliers[1026] = {
    0
    ,computeDivisor(1)
    ,computeDivisor(2)
    ,computeDivisor(3)
    ,computeDivisor(4)
    ,computeDivisor(5)
    ,computeDivisor(6)
    ,computeDivisor(7)
    ,computeDivisor(8)
    ,computeDivisor(9)
    ,computeDivisor(10)
    ,computeDivisor(11)
    ,computeDivisor(12)
    ,computeDivisor(13)
    ,computeDivisor(14)
    ,computeDivisor(15)
    ,COMPUTE_DIVISOR(0x10)
    ,COMPUTE_DIVISOR(0x20)
    ,COMPUTE_DIVISOR(0x30)
    ,COMPUTE_DIVISOR_x100(0x40)
    ,COMPUTE_DIVISOR_x100(0x80)
    ,COMPUTE_DIVISOR_x100(0xc0)
    ,COMPUTE_DIVISOR_x100(0x100)
    ,COMPUTE_DIVISOR_x100(0x140)
    ,COMPUTE_DIVISOR_x100(0x180)
    ,COMPUTE_DIVISOR_x100(0x1c0)
    ,COMPUTE_DIVISOR_x100(0x200)
    ,COMPUTE_DIVISOR_x100(0x240)
    ,COMPUTE_DIVISOR_x100(0x280)
    ,COMPUTE_DIVISOR_x100(0x2c0)
    ,COMPUTE_DIVISOR_x100(0x300)
    ,COMPUTE_DIVISOR_x100(0x340)
    ,COMPUTE_DIVISOR_x100(0x380)
    ,COMPUTE_DIVISOR_x100(0x3c0)
    ,computeDivisor(0x400)
    ,computeDivisor(0x401)
};

constexpr uint32_t fast_divide18bit_by_10bit(uint32_t num, uint16_t denom) {
    return ((uint32_t)((DivisorAndLog2Table[denom].divisor * (uint64_t)num) >> log_max_numerator)
         + ((uint32_t)(num - (((uint64_t)DivisorAndLog2Table[denom].divisor * (uint64_t)num) >> log_max_numerator)) >> 1))
          >> DivisorAndLog2Table[denom].len;
}
constexpr uint32_t fast_divide16bit(uint32_t num, uint16_t denom) {
    return ((uint32_t)((DivisorAndLog2Table[denom].divisor * (uint32_t)num) >> log_max_numerator)
            + ((uint32_t)(num - (((uint32_t)DivisorAndLog2Table[denom].divisor * (uint32_t)num) >> log_max_numerator)) >> 1))
    >> DivisorAndLog2Table[denom].len;
}
template <uint16_t denom> constexpr uint32_t templ_divide16bit(uint32_t num) {
    static_assert(denom < 1024, "Only works for denominators < 1024");
    return ((uint32_t)((DivisorAndLog2Table[denom].divisor * (uint32_t)num) >> log_max_numerator)
            + ((uint32_t)(num - (((uint32_t)DivisorAndLog2Table[denom].divisor * (uint32_t)num) >> log_max_numerator)) >> 1))
    >> DivisorAndLog2Table[denom].len;
}

#ifndef USE_SCALAR
template <uint16_t denom> __m128i divide16bit_vec_signed(__m128i num) {
    static_assert(denom < 1024, "Only works for denominators < 1024");
    __m128i m = _mm_set1_epi32(DivisorAndLog2Table[denom].divisor);
    __m128i abs_num = _mm_abs_epi32(num);
    __m128i t = _mm_srli_epi32(_mm_mullo_epi32(m, abs_num), log_max_numerator);
    __m128i n_minus_t = _mm_sub_epi32(abs_num, t);
    __m128i t_plus_shr = _mm_add_epi32(t, _mm_srli_epi32(n_minus_t, 1));
    __m128i retval = _mm_srli_epi32(t_plus_shr, DivisorAndLog2Table[denom].len);
    return _mm_sign_epi32(retval, num);
}
template <uint16_t denom> __m128i divide16bit_vec(__m128i num) {
    static_assert(denom < 1024, "Only works for denominators < 1024");
    __m128i m = _mm_set1_epi32(DivisorAndLog2Table[denom].divisor);
    __m128i t = _mm_srli_epi32(_mm_mullo_epi32(m, num), log_max_numerator);
    __m128i n_minus_t = _mm_sub_epi32(num, t);
    __m128i t_plus_shr = _mm_add_epi32(t, _mm_srli_epi32(n_minus_t, 1));
    return _mm_srli_epi32(t_plus_shr, DivisorAndLog2Table[denom].len);
}
#endif

inline uint32_t slow_divide18bit_by_10bit(uint32_t num, uint16_t denom) {
#if 0
    uint64_t m = DivisorMultipliers[denom];
    int log2d = k16log2(denom);
    //assert(log2d==DivisorAndLog2Table[denom].len);
#else
    auto dl = DivisorAndLog2Table[denom];
    uint64_t m = dl.divisor;
    uint8_t log2d = dl.len;
#endif
    uint32_t t = (m * num) >> log_max_numerator;
    uint32_t n_minus_t = num - t;
    uint32_t t_plus_shr = t + (n_minus_t >> 1);
    //assert(uint16bit_length(denom) - 1 == log2d);
    uint32_t retval = t_plus_shr >> (log2d);
    //assert(num / denom == retval);
    return retval;

}

enum NumericConstants : uint8_t {
    NUMERIC_LENGTH_MAX = 12,
    NUMERIC_LENGTH_BITS = 4,
};
template<typename intt> intt local_log2(intt v) {
    constexpr int loop_max = (int)(sizeof(intt) == 1 ? 2
                                   : (sizeof(intt) == 2 ? 3
                                      : (sizeof(intt) == 4 ? 4
                                         : 5)));
    constexpr intt b[] = {0x2,
        0xC,
        0xF0,
        (intt)0xFF00,
        (intt)0xFFFF0000U,
        (intt)0xFFFFFFFF00000000ULL};
    constexpr intt S[] = {1, 2, 4, 8, 16, 32};
    intt r = 0; // result of log2(v) will go here
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


template <typename intt> intt bit_length(intt v) {
#ifndef WORKAROUND_CLZ_BUG
    if (sizeof(intt) <= 4) {
        return v ? 32 - __builtin_clz((uint32_t)v) : 0;
    } else {
        return v ? 64 - __builtin_clzl((uint64_t)v) : 0;
    }
#endif
    return v == 0 ? 0 : local_log2(v) + 1;
}

#endif
