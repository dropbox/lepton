#include "memory.hh"
#include <atomic>
#ifndef _BILLING_HH_
#define _BILLING_HH_
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
extern std::atomic<uint32_t> billing_map[2][(uint32_t)Billing::NUM_BILLING_ELEMENTS];
inline void write_bit_bill(Billing bt, bool is_compressed, uint32_t num_bits) {
#ifndef NDEBUG
    assert((uint32_t)bt < (uint32_t)Billing::NUM_BILLING_ELEMENTS);
    if (is_compressed && bt == Billing::HEADER) {
        //fprintf(stderr, "Header; %f bytes\n", num_bits / 8.0);
    }
    if (num_bits) {
        billing_map[is_compressed ? 1 : 0][(uint32_t)bt] += num_bits; // only happens in NDEBUG
    }
#endif
}
inline void write_byte_bill(Billing bt, bool is_compressed, uint32_t num_bytes) {
#ifndef NDEBUG
    if (num_bytes) {
        write_bit_bill(bt, is_compressed, num_bytes << 3);
    }
#endif
}
#undef BILLING_STRING_CB


void print_bill(int fd);
#endif
