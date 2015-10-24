/* -*-mode:c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
void idct(const AlignedBlock &block, const uint16_t q[64], int16_t outp[64], bool ignore_dc) {
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
