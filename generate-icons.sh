#!/bin/bash

for svg in assets/*.svg; do
      # Extract filename without path and extension
      basename=$(basename "$svg" .svg)
      # Extract width and height from SVG
      width=$(sed -n 's/.*width="\([0-9]*\)".*/\1/p' "$svg" | head -1)
      height=$(sed -n 's/.*height="\([0-9]*\)".*/\1/p' "$svg" | head -1)
      # Generate output path and variable name
      output="src/ui_assets/${basename}_img.c"
      varname="${basename}_img"
      echo "Converting $svg -> $output (${width}x${height})"
      python3 tools/convert_svg_to_lvgl.py "$svg" "$output" "$varname" "$width" "$height"
done  