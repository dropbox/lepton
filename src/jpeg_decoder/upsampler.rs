use super::error::{JpegError, JpegResult, UnsupportedFeature};
use super::parser::Component;

pub struct Upsampler {
    components: Vec<UpsamplerComponent>,
    line_buffer_size: usize,
}

struct UpsamplerComponent {
    upsampler: Box<Upsample + Sync>,
    width: usize,
    height: usize,
    row_stride: usize,
}

impl Upsampler {
    pub fn new(
        components: &[Component],
        output_width: u16,
        output_height: u16,
    ) -> JpegResult<Upsampler> {
        let h_max = components
            .iter()
            .map(|c| c.horizontal_sampling_factor)
            .max()
            .unwrap();
        let v_max = components
            .iter()
            .map(|c| c.vertical_sampling_factor)
            .max()
            .unwrap();
        let mut upsampler_components = Vec::with_capacity(components.len());

        for component in components {
            let upsampler = choose_upsampler(
                (
                    component.horizontal_sampling_factor,
                    component.vertical_sampling_factor,
                ),
                (h_max, v_max),
                output_width,
                output_height,
            )?;
            upsampler_components.push(UpsamplerComponent {
                upsampler: upsampler,
                width: component.size.width as usize,
                height: component.size.height as usize,
                row_stride: component.size_in_block.width as usize * 8,
            });
        }

        let buffer_size =
            components.iter().map(|c| c.size.width).max().unwrap() as usize * h_max as usize;

        Ok(Upsampler {
            components: upsampler_components,
            line_buffer_size: buffer_size,
        })
    }

    pub fn upsample_and_interleave_row(
        &self,
        component_data: &[Vec<u8>],
        row: usize,
        output_width: usize,
        output: &mut [u8],
    ) {
        let component_count = component_data.len();
        let mut line_buffer = vec![0u8; self.line_buffer_size];

        debug_assert_eq!(component_count, self.components.len());

        for (i, component) in self.components.iter().enumerate() {
            component.upsampler.upsample_row(
                &component_data[i],
                component.width,
                component.height,
                component.row_stride,
                row,
                output_width,
                &mut line_buffer,
            );
            for x in 0..output_width {
                output[x * component_count + i] = line_buffer[x];
            }
        }
    }
}

struct UpsamplerH1V1;
struct UpsamplerH2V1;
struct UpsamplerH1V2;
struct UpsamplerH2V2;

struct UpsamplerGeneric {
    horizontal_scaling_factor: u8,
    vertical_scaling_factor: u8,
}

fn choose_upsampler(
    sampling_factors: (u8, u8),
    max_sampling_factors: (u8, u8),
    output_width: u16,
    output_height: u16,
) -> JpegResult<Box<Upsample + Sync>> {
    let h1 = sampling_factors.0 == max_sampling_factors.0 || output_width == 1;
    let v1 = sampling_factors.1 == max_sampling_factors.1 || output_height == 1;
    let h2 = sampling_factors.0 * 2 == max_sampling_factors.0;
    let v2 = sampling_factors.1 * 2 == max_sampling_factors.1;

    if h1 && v1 {
        Ok(Box::new(UpsamplerH1V1))
    } else if h2 && v1 {
        Ok(Box::new(UpsamplerH2V1))
    } else if h1 && v2 {
        Ok(Box::new(UpsamplerH1V2))
    } else if h2 && v2 {
        Ok(Box::new(UpsamplerH2V2))
    } else {
        if max_sampling_factors.0 % sampling_factors.0 != 0
            || max_sampling_factors.1 % sampling_factors.1 != 0
        {
            Err(JpegError::Unsupported(
                UnsupportedFeature::NonIntegerSubsamplingRatio,
            ))
        } else {
            Ok(Box::new(UpsamplerGeneric {
                horizontal_scaling_factor: max_sampling_factors.0 / sampling_factors.0,
                vertical_scaling_factor: max_sampling_factors.1 / sampling_factors.1,
            }))
        }
    }
}

trait Upsample {
    fn upsample_row(
        &self,
        input: &[u8],
        input_width: usize,
        input_height: usize,
        row_stride: usize,
        row: usize,
        output_width: usize,
        output: &mut [u8],
    );
}

impl Upsample for UpsamplerH1V1 {
    fn upsample_row(
        &self,
        input: &[u8],
        _input_width: usize,
        _input_height: usize,
        row_stride: usize,
        row: usize,
        output_width: usize,
        output: &mut [u8],
    ) {
        let input = &input[row * row_stride..];

        for i in 0..output_width {
            output[i] = input[i];
        }
    }
}

impl Upsample for UpsamplerH2V1 {
    fn upsample_row(
        &self,
        input: &[u8],
        input_width: usize,
        _input_height: usize,
        row_stride: usize,
        row: usize,
        _output_width: usize,
        output: &mut [u8],
    ) {
        let input = &input[row * row_stride..];

        if input_width == 1 {
            output[0] = input[0];
            output[1] = input[0];
            return;
        }

        output[0] = input[0];
        output[1] = ((input[0] as u32 * 3 + input[1] as u32 + 2) >> 2) as u8;

        for i in 1..input_width - 1 {
            let sample = 3 * input[i] as u32 + 2;
            output[i * 2] = ((sample + input[i - 1] as u32) >> 2) as u8;
            output[i * 2 + 1] = ((sample + input[i + 1] as u32) >> 2) as u8;
        }

        output[(input_width - 1) * 2] =
            ((input[input_width - 1] as u32 * 3 + input[input_width - 2] as u32 + 2) >> 2) as u8;
        output[(input_width - 1) * 2 + 1] = input[input_width - 1];
    }
}

impl Upsample for UpsamplerH1V2 {
    fn upsample_row(
        &self,
        input: &[u8],
        _input_width: usize,
        input_height: usize,
        row_stride: usize,
        row: usize,
        output_width: usize,
        output: &mut [u8],
    ) {
        let row_near = row as f32 / 2.0;
        // If row_near's fractional is 0.0 we want row_far to be the previous row and if it's 0.5 we
        // want it to be the next row.
        let row_far = (row_near + row_near.fract() * 3.0 - 0.25).min((input_height - 1) as f32);

        let input_near = &input[row_near as usize * row_stride..];
        let input_far = &input[row_far as usize * row_stride..];

        for i in 0..output_width {
            output[i] = ((3 * input_near[i] as u32 + input_far[i] as u32 + 2) >> 2) as u8;
        }
    }
}

impl Upsample for UpsamplerH2V2 {
    fn upsample_row(
        &self,
        input: &[u8],
        input_width: usize,
        input_height: usize,
        row_stride: usize,
        row: usize,
        _output_width: usize,
        output: &mut [u8],
    ) {
        let row_near = row as f32 / 2.0;
        // If row_near's fractional is 0.0 we want row_far to be the previous row and if it's 0.5 we
        // want it to be the next row.
        let row_far = (row_near + row_near.fract() * 3.0 - 0.25).min((input_height - 1) as f32);

        let input_near = &input[row_near as usize * row_stride..];
        let input_far = &input[row_far as usize * row_stride..];

        if input_width == 1 {
            let value = ((3 * input_near[0] as u32 + input_far[0] as u32 + 2) >> 2) as u8;
            output[0] = value;
            output[1] = value;
            return;
        }

        let mut t1 = 3 * input_near[0] as u32 + input_far[0] as u32;
        output[0] = ((t1 + 2) >> 2) as u8;

        for i in 1..input_width {
            let t0 = t1;
            t1 = 3 * input_near[i] as u32 + input_far[i] as u32;

            output[i * 2 - 1] = ((3 * t0 + t1 + 8) >> 4) as u8;
            output[i * 2] = ((3 * t1 + t0 + 8) >> 4) as u8;
        }

        output[input_width * 2 - 1] = ((t1 + 2) >> 2) as u8;
    }
}

impl Upsample for UpsamplerGeneric {
    // Uses nearest neighbor sampling
    fn upsample_row(
        &self,
        input: &[u8],
        input_width: usize,
        _input_height: usize,
        row_stride: usize,
        row: usize,
        _output_width: usize,
        output: &mut [u8],
    ) {
        let mut index = 0;
        let start = (row / self.vertical_scaling_factor as usize) * row_stride;
        let input = &input[start..(start + input_width)];
        for val in input {
            for _ in 0..self.horizontal_scaling_factor {
                output[index] = *val;
                index += 1;
            }
        }
    }
}
