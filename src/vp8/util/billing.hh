#include <atomic>
#include "memory.hh"
#include "nd_array.hh"
#ifndef BILLING_HH_
#define BILLING_HH_
#define FOREACH_BILLING_TYPE(CB)                \
    CB(HEADER)                                  \
    CB(DELIMITERS)                              \
    CB(RESERVED)                                \
    CB(NZ_7x7)                                  \
    CB(BITMAP_7x7)                              \
    CB(EXP1_7x7)                                \
    CB(EXP2_7x7)                                \
    CB(EXP3_7x7)                                \
    CB(EXPN_7x7)                                \
    CB(SIGN_7x7)                                \
    CB(RES_7x7)                                 \
    CB(NZ_EDGE)                                 \
    CB(BITMAP_EDGE)                             \
    CB(EXP1_EDGE)                               \
    CB(EXP2_EDGE)                               \
    CB(EXP3_EDGE)                               \
    CB(EXPN_EDGE)                               \
    CB(SIGN_EDGE)                               \
    CB(RES_EDGE)                                \
    CB(EXP0_DC)                                 \
    CB(EXP1_DC)                                 \
    CB(EXP2_DC)                                 \
    CB(EXP3_DC)                                 \
    CB(EXPN_DC)                                 \
    CB(SIGN_DC)                                 \
    CB(RES_DC)

#define BILLING_ENUM_CB(Name) Name,

enum class Billing {
    FOREACH_BILLING_TYPE(BILLING_ENUM_CB)
    NUM_BILLING_ELEMENTS
};
#undef BILLING_ENUM_CB
#define BILLING_STRING_CB(Name) #Name,
inline const char * BillingString(Billing bt) {
    static const char *const string_data[] = {
        FOREACH_BILLING_TYPE(BILLING_STRING_CB)
        "UNREACHABLE"
    };
    unsigned long long which = (unsigned long long)bt;
    if (which < sizeof(string_data) / sizeof(string_data[0])) {
        return string_data[which];
    }
    static char data[] = "XXXX_BILLING_DATA_BEYOND_BILLING_DATA_ARRAY";
    data[0] = (which / 1000) + '0';
    data[1] = (which / 100 % 10) + '0';
    data[2] = (which / 10 % 10) + '0';
    data[3] = (which % 10) + '0';
    return data;
}
extern Sirikata::Array1d<typename Sirikata::Array1d<std::atomic<uint32_t>,
                                                    (uint32_t)Billing::NUM_BILLING_ELEMENTS>, 2> billing_map;

inline void write_bit_bill(Billing bt, bool is_compressed, uint32_t num_bits) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    dev_assert((uint32_t)bt < (uint32_t)Billing::NUM_BILLING_ELEMENTS);
    if (is_compressed && bt == Billing::HEADER) {
        //fprintf(stderr, "Header; %f bytes\n", num_bits / 8.0);
    }
    if (num_bits) {
        billing_map[is_compressed ? 1 : 0][(uint32_t)bt] += num_bits; // only happens in NDEBUG
    }
#endif
}



inline void write_multi_bit_bill(uint32_t num_bits, bool is_compressed, Billing start_range, Billing end_range) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    dev_assert((uint32_t)start_range < (uint32_t)Billing::NUM_BILLING_ELEMENTS);
    dev_assert((uint32_t)end_range < (uint32_t)Billing::NUM_BILLING_ELEMENTS);
    for (uint32_t i = 0;i < num_bits; ++i) {
        ++billing_map[is_compressed ? 1 : 0][std::min(i + (uint32_t)start_range,
                                                      (uint32_t)end_range)]; // only happens in NDEBUG
    }
#endif
}
inline void write_byte_bill(Billing bt, bool is_compressed, uint32_t num_bytes) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    if (num_bytes) {
        write_bit_bill(bt, is_compressed, num_bytes << 3);
    }
#endif
}
#undef BILLING_STRING_CB
inline void write_eob_bill(int coefficient, bool encode, uint32_t num_bits) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    uint32_t num_edge_bits = 1;
    uint32_t num_7x7_bits = 1;
    if (coefficient > 46) {
        num_7x7_bits = 7;
    }
    if (coefficient > 30) {
        num_7x7_bits = 6;
    }
    if (coefficient > 18) {
        num_7x7_bits = 5;
    }
    if (coefficient > 12) {
        num_7x7_bits = 4;
    }
    if (coefficient > 8) {
        num_7x7_bits = 3;
    }
    if (coefficient > 3) {
        num_7x7_bits = 2;
    }
    if (coefficient > 0) {
        num_edge_bits = 2;
    }
    if (coefficient > 2) {
        num_edge_bits = 3;
    }
    if (coefficient > 10) {
        num_edge_bits = 4;
    }
    uint32_t num_tot_bits = 0;
    for (uint32_t i = 0; i < (uint32_t)Billing::NUM_BILLING_ELEMENTS;++i) {
       num_tot_bits += billing_map[encode ? 1 : 0][i];
    }
    uint32_t rand_val = (num_tot_bits / 7) % (num_edge_bits + num_7x7_bits);
    if (rand_val < num_edge_bits) {
        write_bit_bill(Billing::NZ_EDGE, encode, num_bits);
    } else {
        write_bit_bill(Billing::NZ_7x7, encode, num_bits);
    }
#endif
}

void print_bill(int fd);

inline bool is_edge(int bpos) {
#if defined(ENABLE_BILLING) || !defined(NDEBUG)
    (void)bpos;
    return false;
#else
    dev_assert(bpos < 64);
    return bpos == 0 || bpos == 1 || bpos == 5 || bpos == 6 || bpos == 14 || bpos == 15 || bpos == 27 || bpos == 28 || bpos == 2
        || bpos == 3 || bpos == 9 || bpos == 10 || bpos == 20 || bpos == 21 || bpos == 35;
#endif
}

#endif
