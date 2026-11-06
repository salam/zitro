#!/usr/bin/env python3
"""
Extract JPEG from URF (Uncompressed Raster Format) file

URF is Apple's AirPrint raster format. This script extracts
the embedded JPEG image from a URF file created by AI-Stick.

Usage:
    python extract_urf.py input.urf output.jpg
"""

import sys
import struct

def extract_jpeg_from_urf(urf_file, output_file):
    """Extract JPEG data from URF file"""

    with open(urf_file, 'rb') as f:
        data = f.read()

    print(f"URF file size: {len(data)} bytes\n")

    # Check URF header
    if len(data) < 8:
        print("Error: File too small to be valid URF")
        return False

    header = data[:8]
    print(f"File header: {header}")

    if header != b'UNIRAST\x00':
        print("Error: Not a valid URF file!")
        print(f"Expected: b'UNIRAST\\x00'")
        print(f"Got: {repr(header)}")
        return False

    print("✓ Valid URF file header detected\n")

    # Parse page header
    pos = 8
    if len(data) < pos + 30:
        print("Error: File too small for page header")
        return False

    bits_per_pixel = data[pos]
    color_space = data[pos + 1]
    duplex = data[pos + 2]
    quality = data[pos + 3]
    media_type = data[pos + 4]
    copies = data[pos + 5]

    # Width and height (big-endian)
    width = struct.unpack('>I', data[pos + 6:pos + 10])[0]
    height = struct.unpack('>I', data[pos + 10:pos + 14])[0]
    h_resolution = struct.unpack('>I', data[pos + 14:pos + 18])[0]
    v_resolution = struct.unpack('>I', data[pos + 18:pos + 22])[0]

    print("URF Page Header:")
    print(f"  BitsPerPixel: 0x{bits_per_pixel:02X} {'(JPEG compressed)' if bits_per_pixel == 0x01 else ''}")
    print(f"  ColorSpace: 0x{color_space:02X} {'(sRGB)' if color_space == 0x01 else ''}")
    print(f"  Dimensions: {width}x{height}")
    print(f"  Resolution: {h_resolution}x{v_resolution} DPI")
    print(f"  Quality: 0x{quality:02X}")
    print(f"  Copies: {copies}")
    print(f"  Duplex: {duplex}")
    print(f"  MediaType: {media_type}\n")

    # Look for JPEG magic bytes (FF D8 FF)
    jpeg_start = data.find(b'\xFF\xD8\xFF', pos)

    if jpeg_start == -1:
        print("Error: No JPEG magic bytes (FF D8 FF) found in URF file")
        print("Searching for alternative start positions...")

        # Try some common offsets
        for offset in [30, 36, 40]:
            test_pos = 8 + offset
            if test_pos < len(data) and data[test_pos:test_pos+2] == b'\xFF\xD8':
                print(f"Found potential JPEG at offset {test_pos}")
                jpeg_start = test_pos
                break

        if jpeg_start == -1:
            return False

    print(f"Found JPEG at offset: {jpeg_start}")
    print(f"Page header size: {jpeg_start - 8} bytes")

    # Extract JPEG data
    jpeg_data = data[jpeg_start:]

    # Verify JPEG header
    if len(jpeg_data) < 2 or jpeg_data[:2] != b'\xFF\xD8':
        print("Warning: Extracted data doesn't start with JPEG magic bytes!")
        return False

    print(f"✓ Valid JPEG header detected")

    # Look for JPEG end marker (FF D9)
    jpeg_end = jpeg_data.find(b'\xFF\xD9')
    if jpeg_end > 0:
        jpeg_data = jpeg_data[:jpeg_end + 2]  # Include the end marker
        print(f"✓ JPEG end marker found at offset {jpeg_end}")

    print(f"\nExtracted JPEG size: {len(jpeg_data)} bytes")

    # Write to output file
    with open(output_file, 'wb') as f:
        f.write(jpeg_data)

    print(f"✓ Extracted JPEG saved to: {output_file}\n")

    # Print first few bytes for verification
    print("JPEG header (first 16 bytes):")
    print(" ".join(f"{b:02X}" for b in jpeg_data[:16]))

    return True


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        print("\nExample:")
        print("  python extract_urf.py 2-ai-stick_print.urf output.jpg")
        sys.exit(1)

    urf_file = sys.argv[1]
    output_file = sys.argv[2]

    print("="*60)
    print("URF to JPEG Extractor")
    print("="*60)
    print(f"Input:  {urf_file}")
    print(f"Output: {output_file}")
    print("="*60 + "\n")

    success = extract_jpeg_from_urf(urf_file, output_file)

    if success:
        print("="*60)
        print("SUCCESS!")
        print("="*60)
        print(f"\nYou can now view the image:")
        print(f"  open {output_file}")
        return 0
    else:
        print("="*60)
        print("FAILED")
        print("="*60)
        return 1


if __name__ == "__main__":
    sys.exit(main())
