#!/usr/bin/env python3
"""
Convert SVG images to LVGL C array format (RGB565 with optional alpha channel)

This script converts SVG files to LVGL-compatible C arrays for use in embedded displays.
It first converts SVG to PNG using cairosvg, then converts to RGB565 format with optional
alpha channel support for transparency.

Requirements:
    pip3 install cairosvg pillow

Usage:
    python3 convert_svg_to_lvgl.py <input.svg> <output.c> <var_name> [width] [height] [--alpha]

Example:
    python3 tools/convert_svg_to_lvgl.py assets/layout.svg src/ui_assets/layout_img.c layout_img 480 480 --alpha
"""

import sys
from PIL import Image
import cairosvg
import tempfile
import os


def rgb888_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565 format"""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def convert_svg_to_lvgl_c(svg_file, output_file, var_name, width=None, height=None, use_alpha=False):
    """Convert SVG to LVGL C array (RGB565 format with optional alpha channel)"""
    
    # Convert SVG to PNG in memory
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as tmp:
        tmp_png = tmp.name
    
    try:
        # Convert SVG to PNG
        kwargs = {}
        if width and height:
            kwargs['output_width'] = width
            kwargs['output_height'] = height
        
        cairosvg.svg2png(url=svg_file, write_to=tmp_png, **kwargs)
        
        # Load PNG
        img = Image.open(tmp_png)
        img = img.convert('RGBA')
        img_width, img_height = img.size
        
        # Convert pixels to RGB565 (and optionally alpha)
        pixels = []
        alphas = []
        for y in range(img_height):
            for x in range(img_width):
                r, g, b, a = img.getpixel((x, y))
                # Convert to RGB565
                rgb565 = rgb888_to_rgb565(r, g, b)
                pixels.append(rgb565)
                if use_alpha:
                    alphas.append(a)
        
        # Write C array
        with open(output_file, 'w') as f:
            f.write('#ifdef __has_include\n')
            f.write('    #if __has_include("lvgl.h")\n')
            f.write('        #ifndef LV_LVGL_H_INCLUDE_SIMPLE\n')
            f.write('            #define LV_LVGL_H_INCLUDE_SIMPLE\n')
            f.write('        #endif\n')
            f.write('    #endif\n')
            f.write('#endif\n\n')
            f.write('#if defined(LV_LVGL_H_INCLUDE_SIMPLE)\n')
            f.write('    #include "lvgl.h"\n')
            f.write('#else\n')
            f.write('    #include "lvgl/lvgl.h"\n')
            f.write('#endif\n\n')
            f.write('#ifndef LV_ATTRIBUTE_MEM_ALIGN\n')
            f.write('#define LV_ATTRIBUTE_MEM_ALIGN\n')
            f.write('#endif\n\n')
            
            if use_alpha:
                # For TRUE_COLOR_ALPHA, create combined data array (RGB565 + Alpha)
                # LVGL expects: [R5G6B5_byte0, R5G6B5_byte1] for each pixel, followed by all alpha values
                f.write(f'const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {var_name}_map[] = {{\n')
                
                # Write color data (2 bytes per pixel in little-endian)
                byte_count = 0
                for pixel in pixels:
                    if byte_count % 16 == 0:
                        f.write('    ')
                    # Write RGB565 as two bytes (little-endian)
                    f.write(f'0x{pixel & 0xFF:02x}, 0x{(pixel >> 8) & 0xFF:02x}, ')
                    byte_count += 2
                    if byte_count % 16 == 0:
                        f.write('\n')
                
                # Add newline if not already on new line
                if byte_count % 16 != 0:
                    f.write('\n')
                
                # Write alpha data
                for i, alpha in enumerate(alphas):
                    if (byte_count + i) % 16 == 0:
                        f.write('    ')
                    f.write(f'0x{alpha:02x}, ')
                    if ((byte_count + i + 1) % 16 == 0):
                        f.write('\n')
                
                if (byte_count + len(alphas)) % 16 != 0:
                    f.write('\n')
                f.write('};\n\n')
            else:
                # Pixel data array (RGB565 only)
                f.write(f'const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t {var_name}_map[] = {{\n')
                for i, pixel in enumerate(pixels):
                    if i % 16 == 0:
                        f.write('    ')
                    f.write(f'0x{pixel:04x}, ')
                    if (i + 1) % 16 == 0:
                        f.write('\n')
                if len(pixels) % 16 != 0:
                    f.write('\n')
                f.write('};\n\n')
            
            # Image descriptor
            f.write(f'const lv_img_dsc_t {var_name} = {{\n')
            if use_alpha:
                f.write('    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,\n')
                f.write('    .header.always_zero = 0,\n')
                f.write('    .header.reserved = 0,\n')
                f.write(f'    .header.w = {img_width},\n')
                f.write(f'    .header.h = {img_height},\n')
                f.write(f'    .data_size = {len(pixels) * 2 + len(alphas)},\n')
                # For TRUE_COLOR_ALPHA, LVGL expects color data followed by alpha data
                # We need to concatenate them in memory
                f.write(f'    .data = (uint8_t *)({var_name}_map),\n')
                f.write('};\n')
            else:
                f.write('    .header.cf = LV_IMG_CF_TRUE_COLOR,\n')
                f.write('    .header.always_zero = 0,\n')
                f.write('    .header.reserved = 0,\n')
                f.write(f'    .header.w = {img_width},\n')
                f.write(f'    .header.h = {img_height},\n')
                f.write(f'    .data_size = {len(pixels) * 2},\n')
                f.write(f'    .data = (uint8_t *)({var_name}_map),\n')
                f.write('};\n')
        
        alpha_str = " with alpha" if use_alpha else ""
        print(f"Successfully converted {svg_file} -> {output_file} ({img_width}x{img_height}){alpha_str}")
        
    finally:
        # Clean up temp file
        if os.path.exists(tmp_png):
            os.unlink(tmp_png)


def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)
    
    svg_file = sys.argv[1]
    output_file = sys.argv[2]
    var_name = sys.argv[3]
    
    # Parse optional arguments
    width = None
    height = None
    use_alpha = False
    
    for i, arg in enumerate(sys.argv[4:], start=4):
        if arg == '--alpha':
            use_alpha = True
        elif width is None and arg.isdigit():
            width = int(arg)
        elif height is None and arg.isdigit():
            height = int(arg)
    
    if not os.path.exists(svg_file):
        print(f"Error: Input file '{svg_file}' not found")
        sys.exit(1)
    
    convert_svg_to_lvgl_c(svg_file, output_file, var_name, width, height, use_alpha)


if __name__ == '__main__':
    main()
