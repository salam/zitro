# Canon TS5151 Printer Integration Findings

## Tested Configurations

### ❌ Failed: JPEG with 4:2:0 Chroma Subsampling
**Date**: 2026-01-17
**Configuration**:
```cpp
encoder.encodeBegin(&jpe, width, height, JPEGE_PIXEL_RGB888, JPEGE_SUBSAMPLE_420, JPEGE_Q_HIGH);
```

**Result**: Blank pages printed
**Reason**: Canon TS5151 cannot properly decode JPEG with 4:2:0 chroma subsampling
**Evidence**:
- JPEG conversion successful (PNG 197KB → JPEG 18KB)
- Valid JPEG header (FF D8 FF E0)
- Printer accepts job (HTTP 200 OK)
- Output: blank page

**Why It Fails**:
- 4:2:0 subsampling reduces color information by 75%
- Canon printer accepts the file but cannot render the reduced color data
- Printer sees valid JPEG structure but corrupted/missing pixel color information

---

### ❌ Failed: JPEG with 4:4:4 Chroma Subsampling (RGB888)
**Date**: 2026-01-17
**Configuration**:
```cpp
encoder.encodeBegin(&jpe, width, height, JPEGE_PIXEL_RGB888, JPEGE_SUBSAMPLE_444, JPEGE_Q_HIGH);
```

**Result**: Encoding error (code 4: JPEGE_ENCODE_ERROR)
**Reason**: JPEGENC library doesn't support 4:4:4 subsampling with RGB888 pixel type
**Evidence**:
- Encoder returns error code 4 immediately
- Library examples only show 4:4:4 used with GRAYSCALE pixel type
- RGB888 is only used with 4:2:0 in library examples

**Why It Fails**:
- JPEGENC library limitation - 4:4:4 only works with JPEGE_PIXEL_GRAYSCALE
- RGB888 color images require 4:2:0 subsampling in this library
- No middleware option (4:2:2) available

---

### ❌ Failed: PNG via application/octet-stream
**Date**: Previous testing (before 2026-01-17)
**Configuration**: Send PNG directly as `application/octet-stream`

**Result**: Printer rejects or prints blank
**Reason**: Canon printer does not auto-detect PNG format reliably
**Evidence**: User reported this was already tested and didn't work

---

### ✅ Success: Native JPEG (from test_image_data.h)
**Date**: 2026-01-17
**Configuration**: Embedded 27KB JPEG from test sketch
**Format**: Pre-encoded JPEG (uses 4:2:0 subsampling - confirmed via header analysis)

**Result**: Successfully prints
**Evidence**: Test sketch `print_test_image.ino` works correctly

**Why It Works**:
- JPEG is pre-encoded with Canon-compatible settings
- Created by professional tool (likely ImageMagick or similar)
- Proper color space markers and encoding
- Header: `FF D8 FF DB` (DQT tables present)
- Uses 4:2:0 subsampling: Y=2x2, Cb=1x1, Cr=1x1

---

### ✅ Success: Wikipedia JPEG Images
**Date**: 2026-01-17
**Configuration**: JPEG directly downloaded from Wikipedia (37KB, 330x411)
**Format**: Native JPEG with EXIF markers

**Result**: Successfully prints
**Evidence**: User tested and confirmed working print

**Why It Works**:
- Professional JPEG encoding (not JPEGENC)
- Header: `FF D8 FF E2` (APP2/EXIF marker)
- Proper metadata and color space information
- Standard-compliant encoding

---

### ❌ Failed: Downsampled JPEG via JPEGENC
**Date**: 2026-01-17
**Configuration**: gpt-image 1024x1024 downsampled to 256x256 using JPEGENC
**Format**: 32KB JPEG, header `FF D8 FF E0` (JFIF marker)

**Result**: Blank pages (same as PNG→JPEG conversion)
**Evidence**:
```
Re-encoding complete. Final JPEG size: 32262 bytes
First 4 bytes: FF D8 FF E0
Print job submitted successfully! (HTTP 200 OK)
```
But printer outputs blank page.

**Why It Fails**:
- Uses JPEGENC library for encoding (same as PNG conversion)
- JPEGENC creates structurally valid JPEG but Canon cannot render it
- Issue is NOT subsampling (working JPEGs also use 4:2:0)
- Issue is JPEGENC-specific encoding that Canon rejects

---

### ✅ Success: PWG raster, 600 dpi, sRGB 8-bit, sent with Content-Length
**Date**: 2026-11-05
**Configuration**: A4 page (4960 x 7015 px) from `sketch/sketch_jan12a/pwg_raster.h`, sent from a Mac with
`ipptool -L` (Content-Length), document-format `image/pwg-raster`, job attribute `media=iso_a4_210x297mm`.

**Result**: Prints correctly (colours, position and orientation as expected). With a 330x411 source image the
whole page is 0.28 MB, because nearest-neighbour scaling leaves long pixel runs and repeated lines.

**Notes**:
- The printer only offers **600 dpi** for PWG raster and URF (`pwg-raster-document-resolution-supported`, `RS600`).
- Header fields CrossFeedTransform/FeedTransform = 1, TotalPageCount = 1 and NumCopies = 1 are accepted.
- `tests/pwg/run.sh` checks the encoder against libcups' decoder.

### ❌ Failed: Same PWG page sent with chunked transfer encoding
**Date**: 2026-11-05
**Configuration**: `ipptool` default (HTTP chunked), otherwise identical.

**Result**: The printer accepted the job (job-k-octets 0), then went to `stopped` with
"Critical alert - other error" and "spool area full", and stopped accepting jobs. A Cancel-Job for that job
brought it back to idle. **Always send print data with Content-Length.**

### ✅ Success: PWG raster page rendered and sent by the stick
**Date**: 2026-11-05
**Configuration**: `print_layout.ino`, "Half + text" page with a 330x148 Wikipedia JPEG, 233 KB, sent in about 2 s.

**Result**: Prints correctly (checked on paper): picture, text with umlauts, footnote.

**Notes**:
- The Canon answers the Print-Job with `Transfer-Encoding: chunked`. Skip the chunk size line before reading the IPP status, or a successful print looks failed (status "0x0d0a").
- The loop task stack is 8 KB: keep the send buffer and raster header off the stack, or the stick reboots.
- The device only checks that the job was accepted. Look at the paper to confirm a new format.

### Other printers (checked with `tests/pwg/fake_printer.py` only)
**Date**: 2026-11-05

No printer other than the Canon has been tried on paper. The stick now reads the printer's raster resolutions and types (the Canon reports 600 dpi with srgb_8 and sgray_8), and on a rejection tries the next variant. Acted out without paper: only 300 dpi, a printer that reports 600 but takes 300, a printer that reports nothing, grey only, hanging up mid-upload, rejecting everything (plain image file last), no PWG raster, unreachable.

Not covered:
- A printer that accepts a page and aborts the job later: the stick only sees the reply to Print-Job, so it shows "Sent!".
- Printers with no A4 paper loaded or on offer: pages are always A4, and a retry at another resolution won't help.
- URF-only printers: they get the plain image file.
- Printers whose IPP path isn't `/ipp/print`: the path is fixed.

### ⚠ print-scaling is ignored
`print-scaling` is listed as supported, but is missing from `job-creation-attributes-supported`, and a
Print-Job with it returns `successful-ok-ignored-or-substituted-attributes`. Scale on the device instead.

---

## Printer Capabilities

**Supported Formats** (from IPP Get-Printer-Attributes):
- ✅ `application/octet-stream`
- ✅ `image/jpeg`
- ✅ `image/urf`
- ✅ `image/pwg-raster`

**NOT Supported**:
- ❌ `image/png`
- ❌ `image/gif`

---

## Recommended Solutions

### Option 1: Implement PWG-Raster Format (RECOMMENDED)

Canon supports `image/pwg-raster` format:

- **Uncompressed raster format** - no JPEG encoding needed
- No color subsampling issues
- Slightly larger files (~200-300KB for 256x256 RGB) but guaranteed compatibility
- Already have RGB565 pixels from M5Canvas, can convert to RGB888 or sRGB24
- Specification: [PWG 5102.4](https://ftp.pwg.org/pub/pwg/candidates/cs-ippraster10-20120420-5102.4.pdf)
- **Implementation complexity**: Medium (need to implement PWG-Raster format writer)

### Option 2: Use External Conversion Server

Send PNG to external server that converts using ImageMagick/Pillow:

- Server handles conversion with professional JPEG encoder
- Returns Canon-compatible JPEG like Wikipedia images
- Most reliable for production use
- **Implementation complexity**: Low on device (HTTP POST), requires external service

### Option 3: Use Alternative JPEG Encoder Library

Replace JPEGENC with a library that creates Canon-compatible JPEGs:

- **TJpgDec + TinyJPEG**: Decoder + encoder combination
- **ESP32-IDF's JPEG encoder**: If available
- **libjpeg-turbo** (if can compile for ESP32)
- **Implementation complexity**: High (library integration, memory management)

### Option 4: Analyze and Replicate Working JPEG Structure

Compare working JPEG headers with JPEGENC output to identify missing markers:

- Working JPEGs start with `FF D8 FF DB` (DQT first)
- JPEGENC JPEGs start with `FF D8 FF E0` (JFIF/APP0 first)
- May need to manually inject quantization tables or reorder markers
- **Implementation complexity**: Very High (requires deep JPEG format knowledge)

---

## Technical Details

### JPEGENC Library Limitations
**File**: `/Users/matthias/Documents/Arduino/libraries/JPEGENC`

**Supported Combinations**:
- GRAYSCALE + 4:4:4 ✅
- GRAYSCALE + 4:2:0 ✅
- RGB565 + 4:2:0 ✅
- RGB888 + 4:2:0 ✅
- ARGB8888 + 4:2:0 ✅
- YUV422 + 4:2:0 ✅

**NOT Supported**:
- RGB565 + 4:4:4 ❌
- RGB888 + 4:4:4 ❌
- ARGB8888 + 4:4:4 ❌

### Canon TS5151 Decoder Limitations
Based on testing, Canon TS5151:
- ✅ Accepts 4:2:0 JPEG (HTTP 200 OK)
- ❌ Cannot render 4:2:0 JPEG (blank page output)
- ✅ Can render properly-encoded JPEG from test_image_data.h
- ❓ Requires specific JPEG encoding parameters (unknown)

### M5Stack Display Capabilities
- Output: RGB565 framebuffer
- PNG decode: Native support via M5Canvas
- JPEG decode: Native support via M5Canvas
- YUV output: Not available

---

## Critical Analysis: JPEG Header Differences

### Working JPEG Structure (test_image_data.h, Wikipedia images)

**Header sequence**:

```text
FF D8       - SOI (Start of Image)
FF DB       - DQT (Define Quantization Table) - appears FIRST
00 43 00    - Length and table data
[quantization table data for Y component]
FF DB       - Second DQT
00 43 01    - Length and table data
[quantization table data for Cb/Cr components]
FF C0       - SOF0 (Start of Frame - baseline DCT)
[frame header with dimensions and sampling factors]
FF C4       - DHT (Define Huffman Table)
[huffman tables]
FF DA       - SOS (Start of Scan)
[compressed image data]
```

### JPEGENC JPEG Structure (blank page output)

**Header sequence**:

```text
FF D8       - SOI (Start of Image)
FF E0       - APP0 (JFIF marker) - appears FIRST
[JFIF application data]
FF DB       - DQT appears later
[rest of JPEG structure...]
```

### Key Difference

**Working JPEGs**: Quantization tables (FF DB) appear **immediately after SOI**, before any application markers

**JPEGENC JPEGs**: Application marker (FF E0) appears first, pushing quantization tables later in the stream

This suggests Canon's JPEG decoder expects quantization tables early in the stream, or has issues with the JFIF application marker structure created by JPEGENC.

### Confirmation

Both types use 4:2:0 chroma subsampling (confirmed by FF C0 SOF0 frame header analysis showing Y=2x2, Cb=1x1, Cr=1x1), which proves the issue is **NOT** related to chroma subsampling but to JPEG structure/markers.

---

## Next Steps

1. **Implement PWG-Raster** (RECOMMENDED, encoder done and verified on the Canon 2026-11-05): Most reliable path forward
   - Simple uncompressed format
   - Guaranteed Canon compatibility
   - Slightly larger files but acceptable for 256x256 images
   - No JPEG encoding issues

2. **Or use External Conversion Server**: Production-ready solution
   - Send PNG to server with ImageMagick/Pillow
   - Returns Canon-compatible JPEG
   - Most reliable but requires infrastructure

3. **Test alternative JPEG encoders** (if PWG-Raster too complex):
   - ESP32 native JPEG encoder (if available in ESP-IDF)
   - TinyJPEG library
   - libjpeg-turbo (if can compile for ESP32)

4. **Last resort - Manual JPEG marker manipulation**:
   - Attempt to reorder JPEGENC output to put DQT before APP0
   - Very complex, not recommended unless other options fail

---

## References

- [Canon Printer IPP Issues - Linux Mint Forums](https://forums.linuxmint.com/viewtopic.php?t=430916)
- [IPP & IPP Everywhere Documentation](https://return42.github.io/handsOn/print/ipp.html)
- [Canon SELPHY Printing Issues - GitHub](https://github.com/OpenPrinting/cups-filters/issues/492)
- [JPEGENC Library - GitHub](https://github.com/bitbank2/JPEGENC)
- [PWG Raster Format Specification](https://ftp.pwg.org/pub/pwg/candidates/cs-ippraster10-20120420-5102.4.pdf)
