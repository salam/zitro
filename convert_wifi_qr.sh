#!/bin/bash
# Convert wifi_qr.png to C header file for Arduino

INPUT="designs/wifi_qr.png"
OUTPUT="sketch/sketch_jan12a/wifi_qr_data.h"

echo "Converting WiFi QR code to C header file..."

# Convert to C byte array
echo "// Zitro WiFi Setup QR Code" > "$OUTPUT"
echo "// Auto-generated from designs/wifi_qr.png" >> "$OUTPUT"
echo "// QR code for WIFI:T:nopass;S:zitro;P:;;" >> "$OUTPUT"
echo "" >> "$OUTPUT"
echo "const unsigned char wifi_qr_png[] PROGMEM = {" >> "$OUTPUT"

# Use xxd to convert to hex bytes
xxd -i < "$INPUT" | sed 's/unsigned char//' | sed 's/\[\]//' >> "$OUTPUT"

echo "};" >> "$OUTPUT"
echo "const unsigned int wifi_qr_png_len = sizeof(wifi_qr_png);" >> "$OUTPUT"

echo ""
echo "Generated $OUTPUT"
echo "Input size: $(ls -lh "$INPUT" | awk '{print $5}')"
echo "Header size: $(ls -lh "$OUTPUT" | awk '{print $5}')"
