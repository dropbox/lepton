/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <immintrin.h>
#include <tmmintrin.h>
#include "../vp8/util/mm_mullo_epi32.hh"
#endif

#include "../vp8/util/aligned_block.hh"

namespace idct_local{
enum {
    w1 = 2841, // 2048*sqrt(2)*cos(1*pi/16)
    w2 = 2676, // 2048*sqrt(2)*cos(2*pi/16)
    w3 = 2408, // 2048*sqrt(2)*cos(3*pi/16)
    w5 = 1609, // 2048*sqrt(2)*cos(5*pi/16)
    w6 = 1108, // 2048*sqrt(2)*cos(6*pi/16)
    w7 = 565,  // 2048*sqrt(2)*cos(7*pi/16)

    w1pw7 = w1 + w7,
    w1mw7 = w1 - w7,
    w2pw6 = w2 + w6,
    w2mw6 = w2 - w6,
    w3pw5 = w3 + w5,
    w3mw5 = w3 - w5,

    r2 = 181 // 256/sqrt(2)
};
}

#if ((!defined(__SSE2__)) && !(_M_IX86_FP >= 1)) || defined(USE_SCALAR)
static void
idct_scalar(const AlignedBlock &block, const uint16_t q[64], int16_t outp[64], bool ignore_dc) {
    int32_t intermed[64];
    using namespace idct_local;
    // Horizontal 1-D IDCT.
    for (int y = 0; y < 8; ++y) {
        int y8 = y * 8;
        int32_t x0 = (((ignore_dc && y == 0)
                       ? 0 : (block.coefficients_raster(y8 + 0) * q[y8 + 0]) << 11)) + 128;
        int32_t x1 = (block.coefficients_raster(y8 + 4) * q[y8 + 4]) << 11;
        int32_t x2 = block.coefficients_raster(y8 + 6) * q[y8 + 6];
        int32_t x3 = block.coefficients_raster(y8 + 2) * q[y8 + 2];
        int32_t x4 = block.coefficients_raster(y8 + 1) * q[y8 + 1];
        int32_t x5 = block.coefficients_raster(y8 + 7) * q[y8 + 7];
        int32_t x6 = block.coefficients_raster(y8 + 5) * q[y8 + 5];
        int32_t x7 = block.coefficients_raster(y8 + 3) * q[y8 + 3];
        // If all the AC components are zero, then the IDCT is trivial.
        if (x1 ==0 && x2 == 0 && x3 == 0 && x4 == 0 && x5 == 0 && x6 == 0 && x7 == 0) {
            int32_t dc = (x0 - 128) >> 8; // coefficients[0] << 3
            intermed[y8 + 0] = dc;
            intermed[y8 + 1] = dc;
            intermed[y8 + 2] = dc;
            intermed[y8 + 3] = dc;
            intermed[y8 + 4] = dc;
            intermed[y8 + 5] = dc;
            intermed[y8 + 6] = dc;
            intermed[y8 + 7] = dc;
            continue;
        }
        
        // Prescale.
        
        // Stage 1.
        int32_t x8 = w7 * (x4 + x5);
        x4 = x8 + w1mw7*x4;
        x5 = x8 - w1pw7*x5;
        x8 = w3 * (x6 + x7);
        x6 = x8 - w3mw5*x6;
        x7 = x8 - w3pw5*x7;
        
        // Stage 2.
        x8 = x0 + x1;
        x0 -= x1;
        x1 = w6 * (x3 + x2);
        x2 = x1 - w2pw6*x2;
        x3 = x1 + w2mw6*x3;
        x1 = x4 + x6;
        x4 -= x6;
        x6 = x5 + x7;
        x5 -= x7;
        
        // Stage 3.
        x7 = x8 + x3;
        x8 -= x3;
        x3 = x0 + x2;
        x0 -= x2;
        x2 = (r2*(x4+x5) + 128) >> 8;
        x4 = (r2*(x4-x5) + 128) >> 8;
        
        // Stage 4.
        intermed[y8+0] = (x7 + x1) >> 8;
        intermed[y8+1] = (x3 + x2) >> 8;
        intermed[y8+2] = (x0 + x4) >> 8;
        intermed[y8+3] = (x8 + x6) >> 8;
        intermed[y8+4] = (x8 - x6) >> 8;
        intermed[y8+5] = (x0 - x4) >> 8;
        intermed[y8+6] = (x3 - x2) >> 8;
        intermed[y8+7] = (x7 - x1) >> 8;
    }
    
    // Vertical 1-D IDCT.
    for (int32_t x = 0; x < 8; ++x) {
        // Similar to the horizontal 1-D IDCT case, if all the AC components are zero, then the IDCT is trivial.
        // However, after performing the horizontal 1-D IDCT, there are typically non-zero AC components, so
        // we do not bother to check for the all-zero case.
        
        // Prescale.
        int32_t y0 = (intermed[8*0+x] << 8) + 8192;
        int32_t y1 = intermed[8*4+x] << 8;
        int32_t y2 = intermed[8*6+x];
        int32_t y3 = intermed[8*2+x];
        int32_t y4 = intermed[8*1+x];
        int32_t y5 = intermed[8*7+x];
        int32_t y6 = intermed[8*5+x];
        int32_t y7 = intermed[8*3+x];
        
        // Stage 1.
        int32_t y8 = w7*(y4+y5) + 4;
        y4 = (y8 + w1mw7*y4) >> 3;
        y5 = (y8 - w1pw7*y5) >> 3;
        y8 = w3*(y6+y7) + 4;
        y6 = (y8 - w3mw5*y6) >> 3;
        y7 = (y8 - w3pw5*y7) >> 3;
        
        // Stage 2.
        y8 = y0 + y1;
        y0 -= y1;
        y1 = w6*(y3+y2) + 4;
        y2 = (y1 - w2pw6*y2) >> 3;
        y3 = (y1 + w2mw6*y3) >> 3;
        y1 = y4 + y6;
        y4 -= y6;
        y6 = y5 + y7;
        y5 -= y7;
        
        // Stage 3.
        y7 = y8 + y3;
        y8 -= y3;
        y3 = y0 + y2;
        y0 -= y2;
        y2 = (r2*(y4+y5) + 128) >> 8;
        y4 = (r2*(y4-y5) + 128) >> 8;
        
        // Stage 4.
        outp[8*0+x] = (y7 + y1) >> 11;
        outp[8*1+x] = (y3 + y2) >> 11;
        outp[8*2+x] = (y0 + y4) >> 11;
        outp[8*3+x] = (y8 + y6) >> 11;
        outp[8*4+x] = (y8 - y6) >> 11;
        outp[8*5+x] = (y0 - y4) >> 11;
        outp[8*6+x] = (y3 - y2) >> 11;
        outp[8*7+x] = (y7 - y1) >> 11;
    }
    for (int i = 0; i < 64;++i) {
        //outp[i]>>=3;
    }
}
#else /* At least SSE2 is available { */

template<int which_vec, int offset, int stride> __m128i vget_raster(const AlignedBlock&block) {
    return _mm_set_epi32(block.coefficients_raster(which_vec + 3 * stride + offset),
                         block.coefficients_raster(which_vec + 2 * stride + offset),
                         block.coefficients_raster(which_vec + 1 * stride + offset),
                         block.coefficients_raster(which_vec + offset));
}
template<int offset, int stride> __m128i vquantize(int which_vec, __m128i vec, const uint16_t q[64]) {
    return _mm_mullo_epi32(vec, _mm_set_epi32(q[which_vec + 3 * stride + offset],
                                              q[which_vec + 2 * stride + offset],
                                              q[which_vec + 1 * stride + offset],
                                              q[which_vec + offset]));
}

static __m128i
epi32l_to_epi16(__m128i lowvec) {
    return _mm_shuffle_epi8(lowvec, _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                                 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0));
}

#define TRANSPOSE_128i(row0, row1, row2, row3, ocol0, ocol1, ocol2, ocol3) \
    do { \
            __m128i intermed0 = _mm_unpacklo_epi32(row0, row1); \
            __m128i intermed1 = _mm_unpacklo_epi32(row2, row3); \
            __m128i intermed2 = _mm_unpackhi_epi32(row0, row1); \
            __m128i intermed3 = _mm_unpackhi_epi32(row2, row3); \
            ocol0 = _mm_unpacklo_epi64(intermed0, intermed1); \
            ocol1 = _mm_unpackhi_epi64(intermed0, intermed1); \
            ocol2 = _mm_unpacklo_epi64(intermed2, intermed3); \
            ocol3 = _mm_unpackhi_epi64(intermed2, intermed3); \
    }while(0)


void idct_sse(const AlignedBlock &block, const uint16_t q[64], int16_t voutp[64], bool ignore_dc) {
    
    char vintermed_storage[64 * sizeof(int32_t) + 16];
    // align intermediate storage to 16 bytes
    int32_t *vintermed = (int32_t*) (vintermed_storage + 16 - ((vintermed_storage - (char*)nullptr) &0xf));
    using namespace idct_local;
    // Horizontal 1-D IDCT.
    for (int yvec = 0; yvec < 64; yvec += 32) {
        __m128i xv0, xv1, xv2, xv3, xv4, xv5, xv6, xv7, xv8;
        if (yvec == 0) {
            xv0 = vget_raster<0, 0, 8>(block);
            xv1 = vget_raster<0, 4, 8>(block);
            xv2 = vget_raster<0, 6, 8>(block);
            xv3 = vget_raster<0, 2, 8>(block);
            xv4 = vget_raster<0, 1, 8>(block);
            xv5 = vget_raster<0, 7, 8>(block);
            xv6 = vget_raster<0, 5, 8>(block);
            xv7 = vget_raster<0, 3, 8>(block);
            if (__builtin_expect(ignore_dc, true)) {
#ifdef __SSE4_1__
                xv0 = _mm_insert_epi32(xv0, 0, 0);
#else
// See http://stackoverflow.com/questions/38384520/is-there-a-sse2-equivalent-for-mm-insert-epi32
                xv0 = _mm_and_si128(xv0, _mm_set_epi32(-1,-1,-1, 0));
#endif
            }
        } else {
            xv0 = vget_raster<32, 0, 8>(block);
            xv1 = vget_raster<32, 4, 8>(block);
            xv2 = vget_raster<32, 6, 8>(block);
            xv3 = vget_raster<32, 2, 8>(block);
            xv4 = vget_raster<32, 1, 8>(block);
            xv5 = vget_raster<32, 7, 8>(block);
            xv6 = vget_raster<32, 5, 8>(block);
            xv7 = vget_raster<32, 3, 8>(block);
        }
        xv0 = _mm_add_epi32(_mm_slli_epi32(vquantize<0, 8>(yvec, xv0, q), 11),
                            _mm_set1_epi32(128));
        
        xv1 = _mm_slli_epi32(vquantize<4, 8>(yvec, xv1, q), 11);
        xv2 = vquantize<6, 8>(yvec, xv2, q);
        xv3 = vquantize<2, 8>(yvec, xv3, q);
        xv4 = vquantize<1, 8>(yvec, xv4, q);
        xv5 = vquantize<7, 8>(yvec, xv5, q);
        xv6 = vquantize<5, 8>(yvec, xv6, q);
        xv7 = vquantize<3, 8>(yvec, xv7, q);
        // Stage 1.
        xv8 = _mm_mullo_epi32(_mm_set1_epi32(w7), _mm_add_epi32(xv4, xv5));
        xv4 = _mm_add_epi32(xv8, _mm_mullo_epi32(_mm_set1_epi32(w1mw7), xv4));
        xv5 = _mm_sub_epi32(xv8, _mm_mullo_epi32(_mm_set1_epi32(w1pw7), xv5));
        
        xv8 = _mm_mullo_epi32(_mm_set1_epi32(w3), _mm_add_epi32(xv6, xv7));
        xv6 = _mm_sub_epi32(xv8, _mm_mullo_epi32(_mm_set1_epi32(w3mw5), xv6));
        xv7 = _mm_sub_epi32(xv8, _mm_mullo_epi32(_mm_set1_epi32(w3pw5), xv7));
        
        xv8 = _mm_add_epi32(xv0, xv1);
        xv0 = _mm_sub_epi32(xv0, xv1);
        xv1 = _mm_mullo_epi32(_mm_set1_epi32(w6), _mm_add_epi32(xv3, xv2));
        xv2 = _mm_sub_epi32(xv1, _mm_mullo_epi32(_mm_set1_epi32(w2pw6), xv2));
        xv3 = _mm_add_epi32(xv1, _mm_mullo_epi32(_mm_set1_epi32(w2mw6), xv3));
        xv1 = _mm_add_epi32(xv4, xv6);
        xv4 = _mm_sub_epi32(xv4, xv6);
        xv6 = _mm_add_epi32(xv5, xv7);
        xv5 = _mm_sub_epi32(xv5, xv7);
        
        // Stage 3.
        xv7 = _mm_add_epi32(xv8, xv3);
        xv8 = _mm_sub_epi32(xv8, xv3);
        xv3 = _mm_add_epi32(xv0, xv2);
        xv0 = _mm_sub_epi32(xv0, xv2);
        xv2 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(r2),
                                                     _mm_add_epi32(xv4, xv5)),
                                     _mm_set1_epi32(128)), 8);
        xv4 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(r2),
                                                           _mm_sub_epi32(xv4, xv5)),
                                           _mm_set1_epi32(128)), 8);
        // Stage 4.
        int index = 0;
        for (__m128i row0 = _mm_srai_epi32(_mm_add_epi32(xv7, xv1), 8),
                     row1 = _mm_srai_epi32(_mm_add_epi32(xv3, xv2), 8),
                     row2 = _mm_srai_epi32(_mm_add_epi32(xv0, xv4), 8),
                     row3 = _mm_srai_epi32(_mm_add_epi32(xv8, xv6), 8);
             true; // will break if index == 4 at the end of this loop
             index += 4,
             row0 = _mm_srai_epi32(_mm_sub_epi32(xv8, xv6), 8),
             row1 = _mm_srai_epi32(_mm_sub_epi32(xv0, xv4), 8),
             row2 = _mm_srai_epi32(_mm_sub_epi32(xv3, xv2), 8),
             row3 = _mm_srai_epi32(_mm_sub_epi32(xv7, xv1), 8)) {
            __m128i col0, col1, col2, col3;
            TRANSPOSE_128i(row0, row1, row2, row3, col0, col1, col2, col3);

            _mm_store_si128((__m128i*)(vintermed + index + yvec), col0);
            _mm_store_si128((__m128i*)(vintermed + index + 8 + yvec), col1);
            _mm_store_si128((__m128i*)(vintermed + index + 16 + yvec), col2);
            _mm_store_si128((__m128i*)(vintermed + index + 24 + yvec), col3);
            if (index == 4) {
                break; // only iterate twice
            }
        }
    }
    // Vertical 1-D IDCT.
    for (uint8_t xvec = 0; xvec < 8; xvec += 4) {
        __m128i yv0, yv1, yv2, yv3, yv4, yv5, yv6, yv7, yv8;
        yv0 = _mm_add_epi32(_mm_slli_epi32(_mm_load_si128((const __m128i*)(vintermed + xvec)), 8),
                            _mm_set1_epi32(8192));
        yv1 = _mm_slli_epi32(_mm_load_si128((const __m128i*)(vintermed + 8 * 4 + xvec)), 8);
        yv2 = _mm_load_si128((const __m128i*)(vintermed + 8 * 6 + xvec));
        yv3 = _mm_load_si128((const __m128i*)(vintermed + 8 * 2 + xvec));
        yv4 = _mm_load_si128((const __m128i*)(vintermed + 8 * 1 + xvec));
        yv5 = _mm_load_si128((const __m128i*)(vintermed + 8 * 7 + xvec));
        yv6 = _mm_load_si128((const __m128i*)(vintermed + 8 * 5 + xvec));
        yv7 = _mm_load_si128((const __m128i*)(vintermed + 8 * 3 + xvec));
        // Stage 1.
        yv8 = _mm_add_epi32(_mm_mullo_epi32(_mm_add_epi32(yv4, yv5), _mm_set1_epi32(w7)), _mm_set1_epi32(4));
        yv4 = _mm_srai_epi32(_mm_add_epi32(yv8, _mm_mullo_epi32(_mm_set1_epi32(w1mw7), yv4)), 3);
        yv5 = _mm_srai_epi32(_mm_sub_epi32(yv8, _mm_mullo_epi32(_mm_set1_epi32(w1pw7), yv5)), 3);
        yv8 = _mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(w3), _mm_add_epi32(yv6, yv7)), _mm_set1_epi32(4));
        yv6 = _mm_srai_epi32(_mm_sub_epi32(yv8, _mm_mullo_epi32(_mm_set1_epi32(w3mw5), yv6)), 3);
        yv7 = _mm_srai_epi32(_mm_sub_epi32(yv8, _mm_mullo_epi32(_mm_set1_epi32(w3pw5), yv7)), 3);
        // Stage 2.
        yv8 = _mm_add_epi32(yv0, yv1);
        yv0 = _mm_sub_epi32(yv0, yv1);
        yv1 = _mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(w6), _mm_add_epi32(yv3, yv2)), _mm_set1_epi32(4));
        yv2 = _mm_srai_epi32(_mm_sub_epi32(yv1, _mm_mullo_epi32(_mm_set1_epi32(w2pw6), yv2)), 3);
        yv3 = _mm_srai_epi32(_mm_add_epi32(yv1, _mm_mullo_epi32(_mm_set1_epi32(w2mw6), yv3)), 3);
        yv1 = _mm_add_epi32(yv4, yv6);
        yv4 = _mm_sub_epi32(yv4, yv6);
        yv6 = _mm_add_epi32(yv5, yv7);
        yv5 = _mm_sub_epi32(yv5, yv7);
        
        // Stage 3.
        yv7 = _mm_add_epi32(yv8, yv3);
        yv8 = _mm_sub_epi32(yv8, yv3);
        yv3 = _mm_add_epi32(yv0, yv2);
        yv0 = _mm_sub_epi32(yv0, yv2);
        yv2 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(r2),
                                                           _mm_add_epi32(yv4, yv5)),
                                           _mm_set1_epi32(128)), 8);
        yv4 = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(_mm_set1_epi32(r2),
                                                           _mm_sub_epi32(yv4, yv5)),
                                           _mm_set1_epi32(128)), 8);
        __m128i row0 = _mm_srai_epi32(_mm_add_epi32(yv7, yv1), 11);
        __m128i row1 = _mm_srai_epi32(_mm_add_epi32(yv3, yv2), 11);
        __m128i row2 = _mm_srai_epi32(_mm_add_epi32(yv0, yv4), 11);
        __m128i row3 = _mm_srai_epi32(_mm_add_epi32(yv8, yv6), 11);
        __m128i row4 = _mm_srai_epi32(_mm_sub_epi32(yv8, yv6), 11);
        __m128i row5 = _mm_srai_epi32(_mm_sub_epi32(yv0, yv4), 11);
        __m128i row6 = _mm_srai_epi32(_mm_sub_epi32(yv3, yv2), 11);
        __m128i row7 = _mm_srai_epi32(_mm_sub_epi32(yv7, yv1), 11);
        __m128i row0short = epi32l_to_epi16(row0);
        _mm_storel_epi64((__m128i*)(char*)(voutp + xvec), row0short);
        _mm_storel_epi64((__m128i*)(char*)(voutp + 8 + xvec), epi32l_to_epi16(row1));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 2 * 8 + xvec), epi32l_to_epi16(row2));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 3 * 8 + xvec), epi32l_to_epi16(row3));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 4 * 8 + xvec), epi32l_to_epi16(row4));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 5 * 8 + xvec), epi32l_to_epi16(row5));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 6 * 8 + xvec), epi32l_to_epi16(row6));
        _mm_storel_epi64((__m128i*)(char*)(voutp + 7 * 8 + xvec), epi32l_to_epi16(row7));
    }
}


#define vget_raster256(offset, stride, block) \
    _mm256_set_epi32(block.coefficients_raster(7 * stride + offset), \
                            block.coefficients_raster(6 * stride + offset), \
                            block.coefficients_raster(5 * stride + offset), \
                            block.coefficients_raster(4 * stride + offset), \
                            block.coefficients_raster(3 * stride + offset), \
                         block.coefficients_raster(2 * stride + offset), \
                         block.coefficients_raster(1 * stride + offset), \
                         block.coefficients_raster(offset))

#define vquantize256(offset, stride, vec, q) \
    _mm256_mullo_epi32(vec, _mm256_set_epi32(q[7 * stride + offset], \
                                                    q[6 * stride + offset], \
                                                    q[5 * stride + offset], \
                                                    q[4 * stride + offset], \
                                                    q[3 * stride + offset], \
                                                    q[2 * stride + offset], \
                                                    q[1 * stride + offset], \
                                                    q[offset]))

#define m256_set_m128i(a,b) _mm256_insertf128_si256(_mm256_castsi128_si256(b), a, 1)
#define m256_to_epi16(vec) \
    _mm_or_si128(_mm_shuffle_epi8(_mm256_extractf128_si256(vec, 0), _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, \
                                                   0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0)), \
                 _mm_shuffle_epi8(_mm256_extractf128_si256(vec, 1), _mm_set_epi8(0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0, \
                                                   -1, -1, -1, -1, -1, -1, -1, -1)))
/*
__m128i m256_to_epi16(__m256i vec) {
    __m128i lo = _mm256_extractf128_si256(vec, 0);
    __m128i hi = _mm256_extractf128_si256(vec, 1);
    __m128i lopacked = _mm_shuffle_epi8(lo, _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                                         0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0));
    __m128i hipacked = _mm_shuffle_epi8(hi, _mm_set_epi8(0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0,
                                                         -1, -1, -1, -1, -1, -1, -1, -1));
    return _mm_or_si128(lopacked, hipacked);

    }*/
#ifdef __AVX2__
static void
idct_avx(const AlignedBlock &block, const uint16_t q[64], int16_t voutp[64], bool ignore_dc) {
    // align intermediate storage to 16 bytes
    using namespace idct_local;
    // Horizontal 1-D IDCT.
    __m256i col0, col1, col2, col3, col4, col5, col6, col7;
    {
        __m256i xv0, xv1, xv2, xv3, xv4, xv5, xv6, xv7, xv8;
        xv0 = vget_raster256(0, 8, block);
        xv1 = vget_raster256(4, 8, block);
        xv2 = vget_raster256(6, 8, block);
        xv3 = vget_raster256(2, 8, block);
        xv4 = vget_raster256(1, 8, block);
        xv5 = vget_raster256(7, 8, block);
        xv6 = vget_raster256(5, 8, block);
        xv7 = vget_raster256(3, 8, block);
        if (__builtin_expect(ignore_dc, true)) {
#ifdef _WIN32
            __m128i zero_first = _mm256_extractf128_si256(xv0, 0);
            xv0 = _mm256_insertf128_si256(xv0, _mm_insert_epi32(zero_first, 0, 0), 0);
#else
            xv0 = _mm256_insert_epi32(xv0, 0, 0);
#endif
        }
        xv0 = _mm256_add_epi32(_mm256_slli_epi32(vquantize256(0, 8, xv0, q), 11),
                            _mm256_set1_epi32(128));
        
        xv1 = _mm256_slli_epi32(vquantize256(4, 8, xv1, q), 11);
        xv2 = vquantize256(6, 8, xv2, q);
        xv3 = vquantize256(2, 8, xv3, q);
        xv4 = vquantize256(1, 8, xv4, q);
        xv5 = vquantize256(7, 8, xv5, q);
        xv6 = vquantize256(5, 8, xv6, q);
        xv7 = vquantize256(3, 8, xv7, q);
        // Stage 1.
        xv8 = _mm256_mullo_epi32(_mm256_set1_epi32(w7), _mm256_add_epi32(xv4, xv5));
        xv4 = _mm256_add_epi32(xv8, _mm256_mullo_epi32(_mm256_set1_epi32(w1mw7), xv4));
        xv5 = _mm256_sub_epi32(xv8, _mm256_mullo_epi32(_mm256_set1_epi32(w1pw7), xv5));
        
        xv8 = _mm256_mullo_epi32(_mm256_set1_epi32(w3), _mm256_add_epi32(xv6, xv7));
        xv6 = _mm256_sub_epi32(xv8, _mm256_mullo_epi32(_mm256_set1_epi32(w3mw5), xv6));
        xv7 = _mm256_sub_epi32(xv8, _mm256_mullo_epi32(_mm256_set1_epi32(w3pw5), xv7));
        
        xv8 = _mm256_add_epi32(xv0, xv1);
        xv0 = _mm256_sub_epi32(xv0, xv1);
        xv1 = _mm256_mullo_epi32(_mm256_set1_epi32(w6), _mm256_add_epi32(xv3, xv2));
        xv2 = _mm256_sub_epi32(xv1, _mm256_mullo_epi32(_mm256_set1_epi32(w2pw6), xv2));
        xv3 = _mm256_add_epi32(xv1, _mm256_mullo_epi32(_mm256_set1_epi32(w2mw6), xv3));
        xv1 = _mm256_add_epi32(xv4, xv6);
        xv4 = _mm256_sub_epi32(xv4, xv6);
        xv6 = _mm256_add_epi32(xv5, xv7);
        xv5 = _mm256_sub_epi32(xv5, xv7);
        
        // Stage 3.
        xv7 = _mm256_add_epi32(xv8, xv3);
        xv8 = _mm256_sub_epi32(xv8, xv3);
        xv3 = _mm256_add_epi32(xv0, xv2);
        xv0 = _mm256_sub_epi32(xv0, xv2);
        xv2 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(r2),
                                                     _mm256_add_epi32(xv4, xv5)),
                                     _mm256_set1_epi32(128)), 8);
        xv4 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(r2),
                                                           _mm256_sub_epi32(xv4, xv5)),
                                           _mm256_set1_epi32(128)), 8);
        // Stage 4.
        __m256i row0 = _mm256_srai_epi32(_mm256_add_epi32(xv7, xv1), 8),
            row1 = _mm256_srai_epi32(_mm256_add_epi32(xv3, xv2), 8),
            row2 = _mm256_srai_epi32(_mm256_add_epi32(xv0, xv4), 8),
            row3 = _mm256_srai_epi32(_mm256_add_epi32(xv8, xv6), 8),
            row4 = _mm256_srai_epi32(_mm256_sub_epi32(xv8, xv6), 8),
            row5 = _mm256_srai_epi32(_mm256_sub_epi32(xv0, xv4), 8),
            row6 = _mm256_srai_epi32(_mm256_sub_epi32(xv3, xv2), 8),
            row7 = _mm256_srai_epi32(_mm256_sub_epi32(xv7, xv1), 8);
        __m128i row0lo = _mm256_extractf128_si256(row0, 0);
        __m128i row1lo = _mm256_extractf128_si256(row1, 0);
        __m128i row2lo = _mm256_extractf128_si256(row2, 0);
        __m128i row3lo = _mm256_extractf128_si256(row3, 0);
        __m128i row4lo = _mm256_extractf128_si256(row4, 0);
        __m128i row5lo = _mm256_extractf128_si256(row5, 0);
        __m128i row6lo = _mm256_extractf128_si256(row6, 0);
        __m128i row7lo = _mm256_extractf128_si256(row7, 0);
        __m128i col0lo, col1lo, col2lo, col3lo;
        __m128i col0hi, col1hi, col2hi, col3hi;
        TRANSPOSE_128i(row0lo, row1lo, row2lo, row3lo, col0lo, col1lo, col2lo, col3lo);
        TRANSPOSE_128i(row4lo, row5lo, row6lo, row7lo, col0hi, col1hi, col2hi, col3hi);
        col0 = m256_set_m128i(col0hi, col0lo);
        col1 = m256_set_m128i(col1hi, col1lo);
        col2 = m256_set_m128i(col2hi, col2lo);
        col3 = m256_set_m128i(col3hi, col3lo);
        __m128i row0hi = _mm256_extractf128_si256(row0, 1);
        __m128i row1hi = _mm256_extractf128_si256(row1, 1);
        __m128i row2hi = _mm256_extractf128_si256(row2, 1);
        __m128i row3hi = _mm256_extractf128_si256(row3, 1);
        __m128i row4hi = _mm256_extractf128_si256(row4, 1);
        __m128i row5hi = _mm256_extractf128_si256(row5, 1);
        __m128i row6hi = _mm256_extractf128_si256(row6, 1);
        __m128i row7hi = _mm256_extractf128_si256(row7, 1);
        __m128i col4lo, col5lo, col6lo, col7lo;
        __m128i col4hi, col5hi, col6hi, col7hi;
        TRANSPOSE_128i(row0hi, row1hi, row2hi, row3hi, col4lo, col5lo, col6lo, col7lo);
        TRANSPOSE_128i(row4hi, row5hi, row6hi, row7hi, col4hi, col5hi, col6hi, col7hi);
        col4 = m256_set_m128i(col4hi, col4lo);
        col5 = m256_set_m128i(col5hi, col5lo);
        col6 = m256_set_m128i(col6hi, col6lo);
        col7 = m256_set_m128i(col7hi, col7lo);

/*            
        __m256i intermed0 = _mm256_unpacklo_epi32(row0, row1);
        __m256i intermed2 = _mm256_unpacklo_epi32(row2, row3);
        __m256i intermed4 = _mm256_unpacklo_epi32(row4, row5);
        __m256i intermed6 = _mm256_unpacklo_epi32(row6, row7);

        __m256i intermed1 = _mm256_unpackhi_epi32(row0, row1);
        __m256i intermed3 = _mm256_unpackhi_epi32(row2, row3);
        __m256i intermed5 = _mm256_unpackhi_epi32(row4, row5);
        __m256i intermed7 = _mm256_unpackhi_epi32(row6, row7);

        __m256i nearcol0 = _mm256_shuffle_epi32(row0, row2, _MM_SHUFFLE(1,0,1,0));
        __m256i nearcol1 = _mm256_shuffle_epi32(row0, row2, _MM_SHUFFLE(3,2,3,2));
        __m256i nearcol2 = _mm256_shuffle_epi32(row1, row3, _MM_SHUFFLE(1,0,1,0));
        __m256i nearcol3 = _mm256_shuffle_epi32(row1, row3, _MM_SHUFFLE(3,2,3,2));

        __m256i nearcol4 = _mm256_shuffle_epi32(row4, row6, _MM_SHUFFLE(1,0,1,0));
        __m256i nearcol5 = _mm256_shuffle_epi32(row4, row6, _MM_SHUFFLE(3,2,3,2));
        __m256i nearcol6 = _mm256_shuffle_epi32(row5, row7, _MM_SHUFFLE(1,0,1,0));
        __m256i nearcol7 = _mm256_shuffle_epi32(row5, row7, _MM_SHUFFLE(3,2,3,2));

           

        col0 = _mm256_permute2x128_si256(intermed0, intermed4, 0x20);
        col1 = _mm256_permute2x128_si256(intermed1, intermed5, 0x20);
        col2 = _mm256_permute2x128_si256(intermed2, intermed6, 0x20);
        col3 = _mm256_permute2x128_si256(intermed3, intermed7, 0x20);
        col4 = _mm256_permute2x128_si256(intermed0, intermed4, 0x31);
        col5 = _mm256_permute2x128_si256(intermed1, intermed5, 0x31);
        col6 = _mm256_permute2x128_si256(intermed2, intermed6, 0x31);
        col7 = _mm256_permute2x128_si256(intermed3, intermed7, 0x31);
*/
    }
    // Vertical 1-D IDCT.
    {
        __m256i yv0, yv1, yv2, yv3, yv4, yv5, yv6, yv7, yv8;
        yv0 = _mm256_add_epi32(_mm256_slli_epi32(col0, 8),
                            _mm256_set1_epi32(8192));
        yv1 = _mm256_slli_epi32(col4, 8);
        yv2 = col6;
        yv3 = col2;
        yv4 = col1;
        yv5 = col7;
        yv6 = col5;
        yv7 = col3;
        // Stage 1.
        yv8 = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_add_epi32(yv4, yv5), _mm256_set1_epi32(w7)), _mm256_set1_epi32(4));
        yv4 = _mm256_srai_epi32(_mm256_add_epi32(yv8, _mm256_mullo_epi32(_mm256_set1_epi32(w1mw7), yv4)), 3);
        yv5 = _mm256_srai_epi32(_mm256_sub_epi32(yv8, _mm256_mullo_epi32(_mm256_set1_epi32(w1pw7), yv5)), 3);
        yv8 = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(w3), _mm256_add_epi32(yv6, yv7)), _mm256_set1_epi32(4));
        yv6 = _mm256_srai_epi32(_mm256_sub_epi32(yv8, _mm256_mullo_epi32(_mm256_set1_epi32(w3mw5), yv6)), 3);
        yv7 = _mm256_srai_epi32(_mm256_sub_epi32(yv8, _mm256_mullo_epi32(_mm256_set1_epi32(w3pw5), yv7)), 3);
        // Stage 2.
        yv8 = _mm256_add_epi32(yv0, yv1);
        yv0 = _mm256_sub_epi32(yv0, yv1);
        yv1 = _mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(w6), _mm256_add_epi32(yv3, yv2)), _mm256_set1_epi32(4));
        yv2 = _mm256_srai_epi32(_mm256_sub_epi32(yv1, _mm256_mullo_epi32(_mm256_set1_epi32(w2pw6), yv2)), 3);
        yv3 = _mm256_srai_epi32(_mm256_add_epi32(yv1, _mm256_mullo_epi32(_mm256_set1_epi32(w2mw6), yv3)), 3);
        yv1 = _mm256_add_epi32(yv4, yv6);
        yv4 = _mm256_sub_epi32(yv4, yv6);
        yv6 = _mm256_add_epi32(yv5, yv7);
        yv5 = _mm256_sub_epi32(yv5, yv7);
        
        // Stage 3.
        yv7 = _mm256_add_epi32(yv8, yv3);
        yv8 = _mm256_sub_epi32(yv8, yv3);
        yv3 = _mm256_add_epi32(yv0, yv2);
        yv0 = _mm256_sub_epi32(yv0, yv2);
        yv2 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(r2),
                                                           _mm256_add_epi32(yv4, yv5)),
                                           _mm256_set1_epi32(128)), 8);
        yv4 = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(_mm256_set1_epi32(r2),
                                                           _mm256_sub_epi32(yv4, yv5)),
                                           _mm256_set1_epi32(128)), 8);
        __m256i row0 = _mm256_srai_epi32(_mm256_add_epi32(yv7, yv1), 11);
        __m256i row1 = _mm256_srai_epi32(_mm256_add_epi32(yv3, yv2), 11);
        __m256i row2 = _mm256_srai_epi32(_mm256_add_epi32(yv0, yv4), 11);
        __m256i row3 = _mm256_srai_epi32(_mm256_add_epi32(yv8, yv6), 11);
        __m256i row4 = _mm256_srai_epi32(_mm256_sub_epi32(yv8, yv6), 11);
        __m256i row5 = _mm256_srai_epi32(_mm256_sub_epi32(yv0, yv4), 11);
        __m256i row6 = _mm256_srai_epi32(_mm256_sub_epi32(yv3, yv2), 11);
        __m256i row7 = _mm256_srai_epi32(_mm256_sub_epi32(yv7, yv1), 11);
        _mm_store_si128((__m128i*)(char*)(voutp), m256_to_epi16(row0));
        _mm_store_si128((__m128i*)(char*)(voutp + 8), m256_to_epi16(row1));
        _mm_store_si128((__m128i*)(char*)(voutp + 2 * 8), m256_to_epi16(row2));
        _mm_store_si128((__m128i*)(char*)(voutp + 3 * 8), m256_to_epi16(row3));
        _mm_store_si128((__m128i*)(char*)(voutp + 4 * 8), m256_to_epi16(row4));
        _mm_store_si128((__m128i*)(char*)(voutp + 5 * 8), m256_to_epi16(row5));
        _mm_store_si128((__m128i*)(char*)(voutp + 6 * 8), m256_to_epi16(row6));
        _mm_store_si128((__m128i*)(char*)(voutp + 7 * 8), m256_to_epi16(row7));
#ifndef NDEBUG

        static bool nevermore = false;
        if (!nevermore) {
            Sirikata::AlignedArray1d<int16_t, 64> test_case;
            idct_sse(block, q, test_case.begin(), ignore_dc);
            if (memcmp(test_case.begin(), voutp, 64 * sizeof(int16_t)) != 0) {
                nevermore = true;
                idct_sse(block, q, test_case.begin(), ignore_dc);
                idct_avx(block, q, test_case.begin(), ignore_dc);
                dev_assert(false);
            }
        }
#endif
    }
}
#endif
#endif /* } SSE2 or higher is available */

void
idct(const AlignedBlock &block, const uint16_t q[64], int16_t voutp[64], bool ignore_dc) {
#ifdef USE_SCALAR
    idct_scalar(block, q, voutp, ignore_dc);
#else
#ifdef __AVX2__
    idct_avx(block, q, voutp, ignore_dc);
#else
#if defined(__SSE2__) || (_M_IX86_FP >= 1)
    idct_sse(block, q, voutp, ignore_dc);
#else
    idct_scalar(block, q, voutp, ignore_dc);
#endif
#endif
#endif
}
