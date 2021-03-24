#ifndef BLOCK_CONTEXT_HH_
#define BLOCK_CONTEXT_HH_
#include "options.hh"

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include "tmmintrin.h"
#endif

enum {
    IDCTSCALE = 1,
    xIDCTSCALE = 8
};
struct NeighborSummary {
    enum {
        VERTICAL_LAST_PIXEL_OFFSET_FROM_FIRST_PIXEL = 14
    };
    int16_t edge_pixels[16];
    uint8_t num_nonzeros_;
    uint8_t num_nonzeros() const {
        return num_nonzeros_;
    }
    void set_num_nonzeros(uint8_t nz) {
        num_nonzeros_ = nz;
    }
    int16_t horizontal(int index) const {
        return edge_pixels[index + 8];
    }
    int16_t vertical(int index) const {
        return edge_pixels[index];
    }
    const int16_t* vertical_ptr_except_7() const {
        return &edge_pixels[0];
    }
    const int16_t* horizontal_ptr() const {
        return &edge_pixels[8];
    }

#define shift_right_round_zero_epi16(vec, imm8) (_mm_sign_epi16(_mm_srli_epi16(_mm_sign_epi16(vec, vec), imm8), vec));

    void set_horizontal(int16_t * data_aligned, uint16_t* quantization_table, int16_t dc) {
#ifdef USE_SCALAR
        for (int i = 0; i < 8 ; ++i) {
            int delta = data_aligned[i + 56] - data_aligned[i + 48];
            //if (i == 7) delta = 0;
            edge_pixels[i + 8] = dc * quantization_table[0] + data_aligned[i + 56] + 128 * xIDCTSCALE + (delta/2);
        }
#else
        __m128i cur_row = _mm_load_si128((const __m128i*)(data_aligned + 56));
        __m128i prev_row = _mm_load_si128((const __m128i*)(data_aligned + 48));
        __m128i delta = _mm_sub_epi16(cur_row, prev_row);
        __m128i half_delta = shift_right_round_zero_epi16(delta, 1);
        __m128i pred_row = _mm_add_epi16(_mm_add_epi16(cur_row, half_delta), _mm_set1_epi16(128 * xIDCTSCALE));
        pred_row = _mm_add_epi16(pred_row, _mm_set1_epi16(quantization_table[0] * dc));
        _mm_storeu_si128((__m128i*)&edge_pixels[8], pred_row);
#endif
    }

    void set_vertical(int16_t * data, uint16_t* quantization_table, int16_t dc) {
#ifdef USE_SCALAR
        for (int i = 0; i < 8 ; ++i) {
            int delta = data[i * 8 + 7] - data[i * 8 + 6];
            //if (i == 7) delta = 0;
            edge_pixels[i] = dc * quantization_table[0] + data[i * 8 + 7] + 128 * xIDCTSCALE + (delta/2);
        }
#else
        __m128i cur_row = _mm_set_epi16(data[63], data[55], data[47], data[39], data[31], data[23], data[15], data[7]);
        __m128i prev_row = _mm_set_epi16(data[62], data[54], data[46], data[38], data[30], data[22], data[14], data[6]);
        __m128i delta = _mm_sub_epi16(cur_row, prev_row);
        __m128i half_delta = shift_right_round_zero_epi16(delta, 1);
        __m128i pred_row = _mm_add_epi16(_mm_add_epi16(cur_row, half_delta), _mm_set1_epi16(128 * xIDCTSCALE));
        pred_row = _mm_add_epi16(pred_row, _mm_set1_epi16(quantization_table[0] * dc));
        _mm_storeu_si128((__m128i*)&edge_pixels[0], pred_row);
#endif
    }

    void set_horizontal_dc_included(int * data) {
        for (int i = 0; i < 8 ; ++i) {
            int delta = data[i + 56] - data[i + 48];
            //if (i == 7) delta = 0;
            edge_pixels[i + 8] = data[i + 56] + delta / 2;
        }
    }

    void set_vertical_dc_included(int * data) {
        for (int i = 0; i < 7 ; ++i) {
            int delta = data[i * 8 + 7] - data[i * 8 + 6];
            //if (i == 7) delta = 0;
            edge_pixels[i] = data[i * 8 + 7] + delta / 2;
        }
    }
};

// in raytracing we usually find that having 32 bit offsets to pointers ends up being more
// efficient in our datastructures, since array offseting instructions are so fast.
template <class ABlock> struct MBlockContext {
    ABlock * cur;
    ABlock * above; //offset from cur; 0 for unavail
    std::vector<NeighborSummary>::iterator num_nonzeros_here;
    std::vector<NeighborSummary>::iterator num_nonzeros_above;
    MBlockContext() {
        std::memset(this, 0, sizeof(*this));
        cur = nullptr;
        above = nullptr;
    }
    MBlockContext(ABlock *cur,
                  ABlock * above,
                  std::vector<NeighborSummary>::iterator num_nonzeros_here,
                  std::vector<NeighborSummary>::iterator num_nonzeros_above) {
        std::memset(this, 0, sizeof(*this));
        this->cur = cur;
        this->above = above;
        this->num_nonzeros_here = num_nonzeros_here;
        this->num_nonzeros_above = num_nonzeros_above;
    }
    MBlockContext<const AlignedBlock> copy() const {
        return {cur, above, num_nonzeros_here, num_nonzeros_above};
    }
    constexpr const ABlock& here() const {
        return cur[0];
    }
    constexpr const ABlock& left_unchecked() const {
        return cur[-1];
    }
    constexpr const ABlock& above_unchecked() const {
        return above[0];
    }
    constexpr const ABlock& above_left_unchecked() const {
        return above[-1];
    }
    static MBlockContext nil() {
        MBlockContext retval;
        memset(&retval, 0, sizeof(retval));
        retval.cur = nullptr;
        retval.above = nullptr;
        return retval;
    }
    bool isNil() {
        return cur == nullptr && above == nullptr;
    }
    ABlock& here() {
        return cur[0];
    }
    ABlock& left_unchecked() {
        return cur[-1];
    }
    ABlock& above_unchecked() {
        return above[0];
    }
    ABlock& above_left_unchecked() {
        return above[-1];
    }
    bool num_nonzeros_check(uint8_t nz7x7, ABlock& block) const{
        int nz = 0;
        for (int i = 1; i < 8; ++i) {
            for (int j = 1; j < 8; ++j) {
                if (block.coefficients_raster(i * 8 +j)) {
                    ++nz;
                }
            }
        }
        if (nz == nz7x7) {
            return true;
        }
        return false;
    }
    uint8_t nonzeros_above_7x7_unchecked() const{
        // too slow // assert(num_nonzeros_check(*num_nonzeros_above, above_unchecked()));
        return num_nonzeros_above->num_nonzeros();
    }
    uint8_t nonzeros_left_7x7_unchecked() const{
        std::vector<NeighborSummary>::iterator  tmp = num_nonzeros_here;
        --tmp;
        // too slow // assert(num_nonzeros_check(*tmp, left_unchecked()));
        return tmp->num_nonzeros();
    }
    const NeighborSummary& neighbor_context_above_unchecked() const{
        // too slow // assert(num_nonzeros_check(*num_nonzeros_above, above_unchecked()));
        return *num_nonzeros_above;
    }
    const NeighborSummary& neighbor_context_left_unchecked() const{
        std::vector<NeighborSummary>::iterator  tmp = num_nonzeros_here;
        --tmp;
        // too slow // assert(num_nonzeros_check(*tmp, left_unchecked()));
        return *tmp;
    }
};
typedef MBlockContext<AlignedBlock> BlockContext;
typedef MBlockContext<const AlignedBlock> ConstBlockContext;
#endif
