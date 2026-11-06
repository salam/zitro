// PWG raster page writer (PWG 5102.4), portable C++ without Arduino dependencies.
//
// The page is streamed line by line, so a 600 dpi A4 page (4960 x 7015 px, ~100 MB raw) never
// sits in memory. Content is composed from:
//   - one RGB565 image, scaled with nearest neighbour into a destination rectangle
//   - up to PWG_MAX_BITMAPS 1-bit bitmaps (text), scaled by a whole number
// Nearest-neighbour scaling keeps runs of identical pixels and identical lines, which the PWG
// run-length encoding compresses to a few MB at most.
//
// Pages can be sRGB or grey (black-and-white printers), at any resolution. pwgParseCaps() and
// pwgPlanAttempts() read what a printer offers and decide which variants to try, in which order.
//
// Used by print_layout.ino on the device and by tests/pwg_raster_test.cpp on the host.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PWG_HEADER_SIZE 1796
#define PWG_MAX_BITMAPS 4

class PwgSink {
public:
  virtual ~PwgSink() {}
  // Returns false to abort the page.
  virtual bool write(const uint8_t *data, size_t len) = 0;
};

// Counts bytes without storing them (first pass, for Content-Length).
class PwgCountingSink : public PwgSink {
public:
  size_t total = 0;
  bool write(const uint8_t *, size_t len) override { total += len; return true; }
};

struct PwgImage {
  const uint16_t *pixels = nullptr;  // RGB565, row-major
  bool byteSwapped = false;          // true for LovyanGFX sprite buffers (swap565)
  int width = 0, height = 0;         // full buffer size
  int cropX = 0, cropY = 0, cropW = 0, cropH = 0;  // source rectangle
  int dstX = 0, dstY = 0, dstW = 0, dstH = 0;      // page rectangle (pixels)
};

struct PwgBitmap {
  const uint8_t *bits = nullptr;  // 1 bit per pixel, MSB first, rows padded to whole bytes
  int width = 0, height = 0;      // in bitmap pixels
  int scale = 1;                  // each bitmap pixel becomes scale x scale page pixels
  int x = 0, y = 0;               // page position of the top-left corner
  uint8_t r = 0, g = 0, b = 0;    // ink colour for set bits
};

struct PwgPage {
  int width = 4960, height = 7015, dpi = 600;
  int pointsW = 595, pointsH = 841;
  const char *sizeName = "iso_a4_210x297mm";
  bool grayscale = false;  // sgray_8 instead of srgb_8
  PwgImage image;  // image.pixels == nullptr means no image
  PwgBitmap bitmaps[PWG_MAX_BITMAPS];
  int bitmapCount = 0;
};

// A4 in pixels, rounded down like libcups: 4960 x 7015 at 600 dpi.
static inline void pwgSetA4(PwgPage &page, int dpi) {
  page.dpi = dpi;
  page.width = (int)(21000L * dpi / 2540);
  page.height = (int)(29700L * dpi / 2540);
}

namespace pwgraster {

static inline void put32(uint8_t *h, int off, uint32_t v) {
  h[off] = (uint8_t)(v >> 24);
  h[off + 1] = (uint8_t)(v >> 16);
  h[off + 2] = (uint8_t)(v >> 8);
  h[off + 3] = (uint8_t)v;
}

// Fills a 1796-byte 8-bit sRGB or grey page header (big-endian), matching libcups' PWG output.
static inline void buildHeader(uint8_t *h, const PwgPage &p) {
  memset(h, 0, PWG_HEADER_SIZE);
  memcpy(h, "PwgRaster", 9);                 // MediaClass
  put32(h, 276, p.dpi);                      // HWResolution x
  put32(h, 280, p.dpi);                      // HWResolution y
  put32(h, 292, p.pointsW);                  // (libcups writes the imaging box here)
  put32(h, 296, p.pointsH);
  put32(h, 340, 1);                          // NumCopies
  put32(h, 352, p.pointsW);                  // PageSize
  put32(h, 356, p.pointsH);
  put32(h, 372, p.width);                    // Width
  put32(h, 376, p.height);                   // Height
  put32(h, 384, 8);                          // BitsPerColor
  put32(h, 388, p.grayscale ? 8 : 24);       // BitsPerPixel
  put32(h, 392, p.width * (p.grayscale ? 1 : 3));  // BytesPerLine
  put32(h, 400, p.grayscale ? 18 : 19);      // ColorSpace sGray / sRGB
  put32(h, 420, p.grayscale ? 1 : 3);        // NumColors
  put32(h, 452, 1);                          // TotalPageCount
  put32(h, 456, 1);                          // CrossFeedTransform
  put32(h, 460, 1);                          // FeedTransform
  put32(h, 480, 0x00FFFFFF);                 // AlternatePrimary (white)
  strncpy((char *)h + 1732, p.sizeName, 63); // PageSizeName
}

// PackBits-style PWG line encoding with bpp-byte pixels.
// Runs: 0..127 = pixel repeated n+1 times; 129..255 = 257-n literal pixels follow.
static inline size_t encodeLine(const uint8_t *px, int w, int bpp, uint8_t *out) {
  uint8_t *o = out;
  int i = 0;
  while (i < w) {
    const uint8_t *p = px + i * bpp;
    int run = 1;
    while (i + run < w && run < 128 && memcmp(p, p + run * bpp, bpp) == 0) run++;
    if (run >= 2) {
      *o++ = (uint8_t)(run - 1);
      memcpy(o, p, bpp);
      o += bpp;
      i += run;
      continue;
    }
    // Literal: extend until two equal neighbours start a run.
    int start = i, n = 1;
    i++;
    while (i < w && n < 128) {
      if (i + 1 < w && memcmp(px + i * bpp, px + (i + 1) * bpp, bpp) == 0) break;
      n++;
      i++;
    }
    if (n == 1) {
      *o++ = 0;
    } else {
      *o++ = (uint8_t)(257 - n);
    }
    memcpy(o, px + start * bpp, n * bpp);
    o += n * bpp;
  }
  return o - out;
}

// Worst case encoded size of one line.
static inline size_t maxEncodedLine(int w) { return (size_t)w * 3 + (w + 127) / 128 + 8; }

// Rec. 601 luma, as used for sgray pages.
static inline uint8_t toGray(uint8_t r, uint8_t g, uint8_t b) {
  return (uint8_t)((r * 77 + g * 150 + b * 29) >> 8);
}

static inline int imageRow(const PwgImage &im, int y) {
  if (!im.pixels || y < im.dstY || y >= im.dstY + im.dstH) return -1;
  return im.cropY + (int)((int64_t)(y - im.dstY) * im.cropH / im.dstH);
}

static inline int bitmapRow(const PwgBitmap &bm, int y) {
  if (!bm.bits || y < bm.y || y >= bm.y + bm.height * bm.scale) return -1;
  return (y - bm.y) / bm.scale;
}

}  // namespace pwgraster

// Streams one page: optional "RaS2" sync word, header, then encoded lines.
// Deterministic, so a counting pass and a sending pass produce the same bytes.
// Returns false if memory runs out or the sink aborts.
static inline bool pwgWritePage(PwgSink &sink, const PwgPage &page, bool writeSync,
                                void *(*allocFn)(size_t) = malloc) {
  using namespace pwgraster;
  const int W = page.width;
  if (writeSync && !sink.write((const uint8_t *)"RaS2", 4)) return false;
  uint8_t *header = (uint8_t *)allocFn(PWG_HEADER_SIZE);  // not on the stack: the device's is small
  if (!header) return false;
  buildHeader(header, page);
  bool headerOk = sink.write(header, PWG_HEADER_SIZE);
  free(header);
  if (!headerOk) return false;

  const PwgImage &im = page.image;
  int dstW = im.pixels ? im.dstW : 0;
  uint8_t *line = (uint8_t *)allocFn(W * 3);
  uint8_t *enc = (uint8_t *)allocFn(maxEncodedLine(W) + 1);
  uint16_t *xmap = dstW > 0 ? (uint16_t *)allocFn(dstW * sizeof(uint16_t)) : nullptr;
  uint8_t *srcRow = im.pixels ? (uint8_t *)allocFn(im.cropW * 3) : nullptr;
  bool ok = line && enc && (dstW == 0 || (xmap && srcRow));
  if (ok && dstW > 0) {
    for (int x = 0; x < dstW; x++) xmap[x] = (uint16_t)((int64_t)x * im.cropW / dstW);
  }

  // Row key: which image row and bitmap rows feed this page line. Equal keys = equal lines.
  int prevKey[1 + PWG_MAX_BITMAPS];
  int key[1 + PWG_MAX_BITMAPS];
  int keyLen = 1 + page.bitmapCount;
  size_t encLen = 0;
  int repeat = 0;

  for (int y = 0; ok && y <= page.height; y++) {
    bool last = (y == page.height);
    if (!last) {
      key[0] = imageRow(im, y);
      for (int b = 0; b < page.bitmapCount; b++) key[1 + b] = bitmapRow(page.bitmaps[b], y);
    }
    bool same = !last && repeat > 0 && repeat < 256 && memcmp(key, prevKey, keyLen * sizeof(int)) == 0;
    if (same) {
      repeat++;
      continue;
    }
    if (repeat > 0) {  // flush the previous line group
      enc[0] = (uint8_t)(repeat - 1);
      ok = sink.write(enc, encLen + 1);
      if (!ok || last) break;
    }
    if (last) break;

    // Build the line.
    memset(line, 0xFF, W * 3);
    if (key[0] >= 0) {
      const uint16_t *row = im.pixels + (size_t)key[0] * im.width + im.cropX;
      for (int sx = 0; sx < im.cropW; sx++) {
        uint16_t v = row[sx];
        if (im.byteSwapped) v = (uint16_t)((v >> 8) | (v << 8));
        uint8_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, bl = v & 0x1F;
        srcRow[sx * 3] = (uint8_t)((r << 3) | (r >> 2));
        srcRow[sx * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
        srcRow[sx * 3 + 2] = (uint8_t)((bl << 3) | (bl >> 2));
      }
      int x0 = im.dstX < 0 ? -im.dstX : 0;
      int x1 = im.dstX + dstW > W ? W - im.dstX : dstW;
      for (int x = x0; x < x1; x++) memcpy(line + (im.dstX + x) * 3, srcRow + xmap[x] * 3, 3);
    }
    for (int b = 0; b < page.bitmapCount; b++) {
      int by = key[1 + b];
      if (by < 0) continue;
      const PwgBitmap &bm = page.bitmaps[b];
      const uint8_t *bits = bm.bits + (size_t)by * ((bm.width + 7) / 8);
      for (int bx = 0; bx < bm.width; bx++) {
        if (!(bits[bx >> 3] & (0x80 >> (bx & 7)))) continue;
        for (int s = 0; s < bm.scale; s++) {
          int x = bm.x + bx * bm.scale + s;
          if (x < 0 || x >= W) continue;
          uint8_t *p = line + x * 3;
          p[0] = bm.r;
          p[1] = bm.g;
          p[2] = bm.b;
        }
      }
    }
    if (page.grayscale) {  // lines are built in RGB, squeezed to grey here
      for (int x = 0; x < W; x++) line[x] = toGray(line[x * 3], line[x * 3 + 1], line[x * 3 + 2]);
    }
    encLen = encodeLine(line, W, page.grayscale ? 1 : 3, enc + 1);
    memcpy(prevKey, key, keyLen * sizeof(int));
    repeat = 1;
  }

  free(line);
  free(enc);
  free(xmap);
  free(srcRow);
  return ok;
}

// ---------- What a printer offers ----------

#define PWG_MAX_RESOLUTIONS 8
#define PWG_MIN_DPI 72
#define PWG_MAX_DPI 1200

struct PwgCaps {
  int dpis[PWG_MAX_RESOLUTIONS];
  int dpiCount = 0;         // 0: the printer did not say
  bool typesKnown = false;  // false: the printer did not say
  bool srgb8 = false;
  bool sgray8 = false;
};

struct PwgAttempt {
  int dpi;
  bool grayscale;
};

// Reads pwg-raster-document-resolution-supported and pwg-raster-document-type-supported from a
// Get-Printer-Attributes reply (IPP body, without HTTP). Returns false for a malformed reply; what
// was understood until then stays in caps. Non-square resolutions can't be rendered and are skipped.
static inline bool pwgParseCaps(const uint8_t *ipp, size_t len, PwgCaps &caps) {
  static const char RES[] = "pwg-raster-document-resolution-supported";
  static const char TYPES[] = "pwg-raster-document-type-supported";
  if (len < 8) return false;
  size_t pos = 8;
  bool inRes = false, inTypes = false;  // additional values have an empty name
  while (pos < len) {
    uint8_t tag = ipp[pos++];
    if (tag == 0x03) return true;  // end of attributes
    if (tag < 0x10) continue;      // group delimiter
    if (pos + 2 > len) return false;
    size_t nameLen = ((size_t)ipp[pos] << 8) | ipp[pos + 1];
    pos += 2;
    if (pos + nameLen + 2 > len) return false;
    if (nameLen > 0) {
      inRes = nameLen == sizeof(RES) - 1 && memcmp(ipp + pos, RES, nameLen) == 0;
      inTypes = nameLen == sizeof(TYPES) - 1 && memcmp(ipp + pos, TYPES, nameLen) == 0;
    }
    pos += nameLen;
    size_t valueLen = ((size_t)ipp[pos] << 8) | ipp[pos + 1];
    pos += 2;
    if (pos + valueLen > len) return false;
    const uint8_t *v = ipp + pos;
    pos += valueLen;
    if (inRes && tag == 0x32 && valueLen == 9) {  // resolution: x, y, units
      int32_t x = (int32_t)(((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) | ((uint32_t)v[2] << 8) | v[3]);
      int32_t y = (int32_t)(((uint32_t)v[4] << 24) | ((uint32_t)v[5] << 16) | ((uint32_t)v[6] << 8) | v[7]);
      if (x != y) continue;
      int dpi = v[8] == 4 ? (int)((x * 254 + 50) / 100) : (v[8] == 3 ? (int)x : 0);  // 4: per cm, 3: per inch
      if (v[8] == 4) {  // per-cm values are rounded: 236 means 600 dpi, not 599
        int nearest = (dpi + 25) / 50 * 50;
        if (dpi - nearest >= -3 && dpi - nearest <= 3) dpi = nearest;
      }
      if (dpi < PWG_MIN_DPI || dpi > PWG_MAX_DPI || caps.dpiCount >= PWG_MAX_RESOLUTIONS) continue;
      caps.dpis[caps.dpiCount++] = dpi;
    } else if (inTypes && tag == 0x44) {  // keyword
      caps.typesKnown = true;
      if (valueLen == 6 && memcmp(v, "srgb_8", 6) == 0) caps.srgb8 = true;
      if (valueLen == 7 && memcmp(v, "sgray_8", 7) == 0) caps.sgray8 = true;
    }
  }
  return false;  // no end-of-attributes tag
}

// Preference: 600 dpi, then the next lower ones (smaller pages), then higher ones.
static inline int pwgDpiRank(int dpi) { return dpi <= 600 ? 600 - dpi : dpi; }

// Fills out with the page variants to try, best first: colour before grey, in resolution order.
// With nothing known it tries the common values. Returns 0 if the printer offers no page we can make.
static inline int pwgPlanAttempts(const PwgCaps &caps, PwgAttempt *out, int maxOut) {
  int dpis[PWG_MAX_RESOLUTIONS];
  int dpiCount = 0;
  if (caps.dpiCount == 0) {
    dpis[dpiCount++] = 600;
    dpis[dpiCount++] = 300;
  }
  for (int i = 0; i < caps.dpiCount; i++) {
    int d = caps.dpis[i], at = dpiCount;
    bool dup = false;
    for (int j = 0; j < dpiCount; j++) dup = dup || dpis[j] == d;
    if (dup) continue;
    while (at > 0 && pwgDpiRank(dpis[at - 1]) > pwgDpiRank(d)) {
      dpis[at] = dpis[at - 1];
      at--;
    }
    dpis[at] = d;
    dpiCount++;
  }
  bool colour = !caps.typesKnown || caps.srgb8;
  bool grey = !caps.typesKnown || caps.sgray8;
  int n = 0;
  for (int t = 0; t < 2; t++) {
    if (t == 0 ? !colour : !grey) continue;
    for (int i = 0; i < dpiCount && n < maxOut; i++) out[n++] = {dpis[i], t == 1};
  }
  return n;
}
