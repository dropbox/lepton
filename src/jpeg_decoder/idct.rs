// Malicious JPEG files can cause operations in the idct to overflow.
// One example is tests/crashtest/images/imagetestsuite/b0b8914cc5f7a6eff409f16d8cc236c5.jpg
// That's why wrapping operators are needed.

// This is based on stb_image's 'stbi__idct_block'.
pub fn dequantize_and_idct_block(coefficients: &[i16], quantization_table: &[u16; 64], output_linestride: usize, output: &mut [u8]) {
    debug_assert_eq!(coefficients.len(), 64);

    let mut temp = [0i32; 64];

    // columns
    for i in 0 .. 8 {
        // if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing
        if coefficients[i + 8] == 0 && coefficients[i + 16] == 0 && coefficients[i + 24] == 0 &&
                coefficients[i + 32] == 0 && coefficients[i + 40] == 0 && coefficients[i + 48] == 0 &&
                coefficients[i + 56] == 0 {
            let dcterm = (coefficients[i] as i32 * quantization_table[i] as i32).wrapping_shl(2);
            temp[i]      = dcterm;
            temp[i + 8]  = dcterm;
            temp[i + 16] = dcterm;
            temp[i + 24] = dcterm;
            temp[i + 32] = dcterm;
            temp[i + 40] = dcterm;
            temp[i + 48] = dcterm;
            temp[i + 56] = dcterm;
        }
        else {
            let s0 = coefficients[i] as i32      * quantization_table[i] as i32;
            let s1 = coefficients[i + 8] as i32  * quantization_table[i + 8] as i32;
            let s2 = coefficients[i + 16] as i32 * quantization_table[i + 16] as i32;
            let s3 = coefficients[i + 24] as i32 * quantization_table[i + 24] as i32;
            let s4 = coefficients[i + 32] as i32 * quantization_table[i + 32] as i32;
            let s5 = coefficients[i + 40] as i32 * quantization_table[i + 40] as i32;
            let s6 = coefficients[i + 48] as i32 * quantization_table[i + 48] as i32;
            let s7 = coefficients[i + 56] as i32 * quantization_table[i + 56] as i32;

            let p2 = s2;
            let p3 = s6;
            let p1 = p2.wrapping_add(p3).wrapping_mul(stbi_f2f(0.5411961));
            let t2 = p1.wrapping_add(p3.wrapping_mul(stbi_f2f(-1.847759065)));
            let t3 = p1.wrapping_add(p2.wrapping_mul(stbi_f2f(0.765366865)));
            let p2 = s0;
            let p3 = s4;
            let t0 = stbi_fsh(p2.wrapping_add(p3));
            let t1 = stbi_fsh(p2.wrapping_sub(p3));
            let x0 = t0.wrapping_add(t3);
            let x3 = t0.wrapping_sub(t3);
            let x1 = t1.wrapping_add(t2);
            let x2 = t1.wrapping_sub(t2);
            let t0 = s7;
            let t1 = s5;
            let t2 = s3;
            let t3 = s1;
            let p3 = t0.wrapping_add(t2);
            let p4 = t1.wrapping_add(t3);
            let p1 = t0.wrapping_add(t3);
            let p2 = t1.wrapping_add(t2);
            let p5 = p3.wrapping_add(p4).wrapping_mul(stbi_f2f(1.175875602));
            let t0 = t0.wrapping_mul(stbi_f2f(0.298631336));
            let t1 = t1.wrapping_mul(stbi_f2f(2.053119869));
            let t2 = t2.wrapping_mul(stbi_f2f(3.072711026));
            let t3 = t3.wrapping_mul(stbi_f2f(1.501321110));
            let p1 = p5.wrapping_add(p1.wrapping_mul(stbi_f2f(-0.899976223)));
            let p2 = p5.wrapping_add(p2.wrapping_mul(stbi_f2f(-2.562915447)));
            let p3 = p3.wrapping_mul(stbi_f2f(-1.961570560));
            let p4 = p4.wrapping_mul(stbi_f2f(-0.390180644));
            let t3 = t3.wrapping_add(p1.wrapping_add(p4));
            let t2 = t2.wrapping_add(p2.wrapping_add(p3));
            let t1 = t1.wrapping_add(p2.wrapping_add(p4));
            let t0 = t0.wrapping_add(p1.wrapping_add(p3));

            // constants scaled things up by 1<<12; let's bring them back
            // down, but keep 2 extra bits of precision
            let x0 = x0.wrapping_add(512);
            let x1 = x1.wrapping_add(512);
            let x2 = x2.wrapping_add(512);
            let x3 = x3.wrapping_add(512);

            temp[i]      = x0.wrapping_add(t3).wrapping_shr(10);
            temp[i + 56] = x0.wrapping_sub(t3).wrapping_shr(10);
            temp[i + 8]  = x1.wrapping_add(t2).wrapping_shr(10);
            temp[i + 48] = x1.wrapping_sub(t2).wrapping_shr(10);
            temp[i + 16] = x2.wrapping_add(t1).wrapping_shr(10);
            temp[i + 40] = x2.wrapping_sub(t1).wrapping_shr(10);
            temp[i + 24] = x3.wrapping_add(t0).wrapping_shr(10);
            temp[i + 32] = x3.wrapping_sub(t0).wrapping_shr(10);
        }
    }

    for i in 0 .. 8 {
        // no fast case since the first 1D IDCT spread components out
        let s0 = temp[i * 8];
        let s1 = temp[i * 8 + 1];
        let s2 = temp[i * 8 + 2];
        let s3 = temp[i * 8 + 3];
        let s4 = temp[i * 8 + 4];
        let s5 = temp[i * 8 + 5];
        let s6 = temp[i * 8 + 6];
        let s7 = temp[i * 8 + 7];

        let p2 = s2;
        let p3 = s6;
        let p1 = p2.wrapping_add(p3).wrapping_mul(stbi_f2f(0.5411961));
        let t2 = p1.wrapping_add(p3.wrapping_mul(stbi_f2f(-1.847759065)));
        let t3 = p1.wrapping_add(p2.wrapping_mul(stbi_f2f(0.765366865)));
        let p2 = s0;
        let p3 = s4;
        let t0 = stbi_fsh(p2.wrapping_add(p3));
        let t1 = stbi_fsh(p2.wrapping_sub(p3));
        let x0 = t0.wrapping_add(t3);
        let x3 = t0.wrapping_sub(t3);
        let x1 = t1.wrapping_add(t2);
        let x2 = t1.wrapping_sub(t2);
        let t0 = s7;
        let t1 = s5;
        let t2 = s3;
        let t3 = s1;
        let p3 = t0.wrapping_add(t2);
        let p4 = t1.wrapping_add(t3);
        let p1 = t0.wrapping_add(t3);
        let p2 = t1.wrapping_add(t2);
        let p5 = p3.wrapping_add(p4).wrapping_mul(stbi_f2f(1.175875602));
        let t0 = t0.wrapping_mul(stbi_f2f(0.298631336));
        let t1 = t1.wrapping_mul(stbi_f2f(2.053119869));
        let t2 = t2.wrapping_mul(stbi_f2f(3.072711026));
        let t3 = t3.wrapping_mul(stbi_f2f(1.501321110));
        let p1 = p5.wrapping_add(p1.wrapping_mul(stbi_f2f(-0.899976223)));
        let p2 = p5.wrapping_add(p2.wrapping_mul(stbi_f2f(-2.562915447)));
        let p3 = p3.wrapping_mul(stbi_f2f(-1.961570560));
        let p4 = p4.wrapping_mul(stbi_f2f(-0.390180644));
        let t3 = t3.wrapping_add(p1.wrapping_add(p4));
        let t2 = t2.wrapping_add(p2.wrapping_add(p3));
        let t1 = t1.wrapping_add(p2.wrapping_add(p4));
        let t0 = t0.wrapping_add(p1.wrapping_add(p3));

        // constants scaled things up by 1<<12, plus we had 1<<2 from first
        // loop, plus horizontal and vertical each scale by sqrt(8) so together
        // we've got an extra 1<<3, so 1<<17 total we need to remove.
        // so we want to round that, which means adding 0.5 * 1<<17,
        // aka 65536. Also, we'll end up with -128 to 127 that we want
        // to encode as 0..255 by adding 128, so we'll add that before the shift
        let x0 = x0.wrapping_add(65536 + (128 << 17));
        let x1 = x1.wrapping_add(65536 + (128 << 17));
        let x2 = x2.wrapping_add(65536 + (128 << 17));
        let x3 = x3.wrapping_add(65536 + (128 << 17));

        output[i * output_linestride]     = stbi_clamp(x0.wrapping_add(t3).wrapping_shr(17));
        output[i * output_linestride + 7] = stbi_clamp(x0.wrapping_sub(t3).wrapping_shr(17));
        output[i * output_linestride + 1] = stbi_clamp(x1.wrapping_add(t2).wrapping_shr(17));
        output[i * output_linestride + 6] = stbi_clamp(x1.wrapping_sub(t2).wrapping_shr(17));
        output[i * output_linestride + 2] = stbi_clamp(x2.wrapping_add(t1).wrapping_shr(17));
        output[i * output_linestride + 5] = stbi_clamp(x2.wrapping_sub(t1).wrapping_shr(17));
        output[i * output_linestride + 3] = stbi_clamp(x3.wrapping_add(t0).wrapping_shr(17));
        output[i * output_linestride + 4] = stbi_clamp(x3.wrapping_sub(t0).wrapping_shr(17));
    }
}

// take a -128..127 value and stbi__clamp it and convert to 0..255
fn stbi_clamp(x: i32) -> u8
{
   // trick to use a single test to catch both cases
   if x as u32 > 255 {
      if x < 0 { return 0; }
      if x > 255 { return 255; }
   }

   x as u8
}

fn stbi_f2f(x: f32) -> i32 {
    (x * 4096.0 + 0.5) as i32
}

fn stbi_fsh(x: i32) -> i32 {
    x << 12
}
