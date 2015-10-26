/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include <emmintrin.h>
#include <smmintrin.h>

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
void idct_scalar(const AlignedBlock &block, const uint16_t q[64], int16_t outp[64], bool ignore_dc) {
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


__m128i epi32l_to_epi16(__m128i lowvec) {
/*    int16_t a,b,c,d;
    a = _mm_extract_epi32(lowvec,0);
    b = _mm_extract_epi32(lowvec,1);
    c = _mm_extract_epi32(lowvec,2);
    d = _mm_extract_epi32(lowvec,3);
    return _mm_set_epi16(0,0,0,0,d,c,b,a);
 */
    return _mm_shuffle_epi8(lowvec, _mm_set_epi8(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                 0xd, 0xc, 0x9, 0x8, 0x5, 0x4, 0x1, 0x0));
}
void idct(const AlignedBlock &block, const uint16_t q[64], int16_t voutp[64], bool ignore_dc) {
    int16_t outp[64];
    char intermed_storage[64 * sizeof(int32_t) + 16];
    // align intermediate storage to 16 bytes
    int32_t *intermed = (int32_t*) (intermed_storage + 16 - ((intermed_storage - (char*)nullptr)&0xf));
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
                xv0 = _mm_insert_epi32(xv0, 0, 0);
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
        int32_t xvec0[4];
        int32_t xvec1[4];
        int32_t xvec2[4];
        int32_t xvec3[4];
        int32_t xvec4[4];
        int32_t xvec5[4];
        int32_t xvec6[4];
        int32_t xvec7[4];
        int32_t xvec8[4];
#define x0 xvec0[y&3]
#define x1 xvec1[y&3]
#define x2 xvec2[y&3]
#define x3 xvec3[y&3]
#define x4 xvec4[y&3]
#define x5 xvec5[y&3]
#define x6 xvec6[y&3]
#define x7 xvec7[y&3]
#define x8 xvec8[y&3]
        for (int y = yvec/8; y < yvec/8 + 4; ++y) {
        int y8 = y * 8;
         x0 = (((ignore_dc && y == 0)
                       ? 0 : (block.coefficients_raster(y8 + 0) * q[y8 + 0]) << 11)) + 128;
         x1 =(block.coefficients_raster(y8 + 4) * q[y8 + 4]) << 11;
         x2 = block.coefficients_raster(y8 + 6) * q[y8 + 6];
         x3 = block.coefficients_raster(y8 + 2) * q[y8 + 2];
         x4 = block.coefficients_raster(y8 + 1) * q[y8 + 1];
         x5 = block.coefficients_raster(y8 + 7) * q[y8 + 7];
         x6 = block.coefficients_raster(y8 + 5) * q[y8 + 5];
         x7 = block.coefficients_raster(y8 + 3) * q[y8 + 3];
            assert(x0 == _mm_extract_epi32(xv0, y&3));
            assert(x1 == _mm_extract_epi32(xv1, y&3));
            assert(x2 == _mm_extract_epi32(xv2, y&3));
            assert(x3 == _mm_extract_epi32(xv3, y&3));
            assert(x4 == _mm_extract_epi32(xv4, y&3));
            assert(x5 == _mm_extract_epi32(xv5, y&3));
            assert(x6 == _mm_extract_epi32(xv6, y&3));
            assert(x7 == _mm_extract_epi32(xv7, y&3));
        // Prescale.

        // Stage 1.
         x8 = w7 * (x4 + x5);
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
             index != 8;
             index += 4,
             row0 = _mm_srai_epi32(_mm_sub_epi32(xv8, xv6), 8),
             row1 = _mm_srai_epi32(_mm_sub_epi32(xv0, xv4), 8),
             row2 = _mm_srai_epi32(_mm_sub_epi32(xv3, xv2), 8),
             row3 = _mm_srai_epi32(_mm_sub_epi32(xv7, xv1), 8)) {
            
            __m128i intermed0 = _mm_unpacklo_epi32(row0, row1);
            __m128i intermed1 = _mm_unpacklo_epi32(row2, row3);
            __m128i intermed2 = _mm_unpackhi_epi32(row0, row1);
            __m128i intermed3 = _mm_unpackhi_epi32(row2, row3);

            __m128i col0 = _mm_unpacklo_epi64(intermed0, intermed1);
            __m128i col1 = _mm_unpackhi_epi64(intermed0, intermed1);
            __m128i col2 = _mm_unpacklo_epi64(intermed2, intermed3);
            __m128i col3 = _mm_unpackhi_epi64(intermed2, intermed3);

            _mm_store_si128((__m128i*)(vintermed + index + yvec), col0);
            _mm_store_si128((__m128i*)(vintermed + index + 8 + yvec), col1);
            _mm_store_si128((__m128i*)(vintermed + index + 16 + yvec), col2);
            _mm_store_si128((__m128i*)(vintermed + index + 24 + yvec), col3);
        }

    }
    for (int i= 0; i < 64; ++i) {
        assert(intermed[i] == vintermed[i]);
    }
    // Vertical 1-D IDCT.
    for (uint8_t xvec = 0; xvec < 8; xvec += 4) {
        __m128i yv0, yv1, yv2, yv3, yv4, yv5, yv6, yv7, yv8;
        int32_t yvec0[4];
        int32_t yvec1[4];
        int32_t yvec2[4];
        int32_t yvec3[4];
        int32_t yvec4[4];
        int32_t yvec5[4];
        int32_t yvec6[4];
        int32_t yvec7[4];
        int32_t yvec8[4];
#define y0 yvec0[x&3]
#define y1 yvec1[x&3]
#define y2 yvec2[x&3]
#define y3 yvec3[x&3]
#define y4 yvec4[x&3]
#define y5 yvec5[x&3]
#define y6 yvec6[x&3]
#define y7 yvec7[x&3]
#define y8 yvec8[x&3]
        yv0 = _mm_add_epi32(_mm_slli_epi32(_mm_load_si128((const __m128i*)(vintermed + xvec)), 8),
                            _mm_set1_epi32(8192));
        yv1 = _mm_slli_epi32(_mm_load_si128((const __m128i*)(vintermed + 8 * 4 + xvec)), 8);
        yv2 = _mm_load_si128((const __m128i*)(vintermed + 8 * 6 + xvec));
        yv3 = _mm_load_si128((const __m128i*)(vintermed + 8 * 2 + xvec));
        yv4 = _mm_load_si128((const __m128i*)(vintermed + 8 * 1 + xvec));
        yv5 = _mm_load_si128((const __m128i*)(vintermed + 8 * 7 + xvec));
        yv6 = _mm_load_si128((const __m128i*)(vintermed + 8 * 5 + xvec));
        yv7 = _mm_load_si128((const __m128i*)(vintermed + 8 * 3 + xvec));
    for (int32_t x = xvec; x < xvec + 4; ++x) {
        // Similar to the horizontal 1-D IDCT case, if all the AC components are zero, then the IDCT is trivial.
        // However, after performing the horizontal 1-D IDCT, there are typically non-zero AC components, so
        // we do not bother to check for the all-zero case.

        // Prescale.
         y0 = (intermed[8*0+x] << 8) + 8192;
         y1 = intermed[8*4+x] << 8;
         y2 = intermed[8*6+x];
         y3 = intermed[8*2+x];
         y4 = intermed[8*1+x];
         y5 = intermed[8*7+x];
         y6 = intermed[8*5+x];
         y7 = intermed[8*3+x];
        assert(y0 == _mm_extract_epi32(yv0, x & 3));
        assert(y1 == _mm_extract_epi32(yv1, x & 3));
        assert(y2 == _mm_extract_epi32(yv2, x & 3));
        assert(y3 == _mm_extract_epi32(yv3, x & 3));
        assert(y4 == _mm_extract_epi32(yv4, x & 3));
        assert(y5 == _mm_extract_epi32(yv5, x & 3));
        assert(y6 == _mm_extract_epi32(yv6, x & 3));
        assert(y7 == _mm_extract_epi32(yv7, x & 3));
        // Stage 1.
         y8 = w7*(y4+y5) + 4;
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
        for (int i = 0; i < 8; ++i) {
            for (int v = 0; v < 4; ++v) {
                assert(voutp[xvec + v + i * 8] == outp[xvec + v + i * 8]);
            }
        }
    }
}
