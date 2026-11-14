// SPDX-License-Identifier: AGPL-3.0-or-later
// 为原尺寸增量帧提供高质量调色；只量化实际发生变化的像素。
use imagequant::{Attributes, RGBA};

#[no_mangle]
pub unsafe extern "C" fn shuntie_quantize_rgba(
    rgba: *const RGBA, count: usize, indices: *mut u8, palette: *mut u32,
) -> i32 {
    if rgba.is_null() || indices.is_null() || palette.is_null() || count == 0 { return 0; }
    let result = std::panic::catch_unwind(|| -> Result<i32, imagequant::Error> {
        let input = unsafe { std::slice::from_raw_parts(rgba, count) };
        let width = count.min(4096);
        let height = count.div_ceil(width);
        let mut pixels = input.to_vec();
        pixels.resize(width * height, *input.last().unwrap());
        let mut attributes = Attributes::new();
        attributes.set_speed(10)?;
        attributes.set_quality(0, 95)?;
        // 保留一个透明索引，让未变化像素在增量矩形内保持原样。
        attributes.set_max_colors(255)?;
        let mut image = attributes.new_image(pixels, width, height, 0.)?;
        let mut quantized = attributes.quantize(&mut image)?;
        quantized.set_dithering_level(0.)?;
        let (colors, remapped) = quantized.remapped(&mut image)?;
        unsafe {
            std::ptr::copy_nonoverlapping(remapped.as_ptr(), indices, count);
            for (index, color) in colors.iter().enumerate() {
                *palette.add(index) = 0xff000000 | (u32::from(color.r) << 16)
                    | (u32::from(color.g) << 8) | u32::from(color.b);
            }
        }
        Ok(colors.len() as i32)
    });
    match result { Ok(Ok(count)) => count, _ => 0 }
}
