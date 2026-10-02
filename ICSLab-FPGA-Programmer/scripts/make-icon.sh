#!/bin/sh
# Regenerates resources/icslab.ico (FPGA chip icon) with ImageMagick.
set -e
cd "$(dirname "$0")/../resources"
convert -size 256x256 xc:none \
  -fill '#1f4e99' -draw 'roundrectangle 8,8 248,248 40,40' \
  -fill '#e8eef8' \
  -draw 'rectangle 60,28 76,60'  -draw 'rectangle 100,28 116,60' -draw 'rectangle 140,28 156,60' -draw 'rectangle 180,28 196,60' \
  -draw 'rectangle 60,196 76,228' -draw 'rectangle 100,196 116,228' -draw 'rectangle 140,196 156,228' -draw 'rectangle 180,196 196,228' \
  -draw 'rectangle 28,60 60,76'  -draw 'rectangle 28,100 60,116' -draw 'rectangle 28,140 60,156' -draw 'rectangle 28,180 60,196' \
  -draw 'rectangle 196,60 228,76' -draw 'rectangle 196,100 228,116' -draw 'rectangle 196,140 228,156' -draw 'rectangle 196,180 228,196' \
  -fill '#0f2a55' -draw 'roundrectangle 56,56 200,200 14,14' \
  -fill '#ffffff' -font DejaVu-Sans-Bold -pointsize 50 -gravity center -annotate +0-14 'ICS' \
  -fill '#7fb2ff' -pointsize 30 -annotate +0+34 'FPGA' \
  icon-256.png
convert icon-256.png -define icon:auto-resize=256,64,48,32,24,16 icslab.ico
rm -f icon-256.png
