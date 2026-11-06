// Host test for sketch/sketch_jan12a/pwg_raster.h.
// Encodes pages with the device encoder, decodes them with libcups and compares every pixel
// against a straightforward reference renderer.
//
//   tests/pwg/run.sh            run the tests
//   tests/pwg/run.sh out.pwg    also write a printable A4 sample page to out.pwg
#include <cups/raster.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/pwg_raster.h"

class FileSink : public PwgSink {
public:
  FILE *f;
  explicit FileSink(FILE *file) : f(file) {}
  bool write(const uint8_t *d, size_t n) override { return fwrite(d, 1, n, f) == n; }
};

static void referencePixel(const PwgPage &p, int x, int y, uint8_t out[3]) {
  out[0] = out[1] = out[2] = 0xFF;
  const PwgImage &im = p.image;
  if (im.pixels && y >= im.dstY && y < im.dstY + im.dstH && x >= im.dstX && x < im.dstX + im.dstW) {
    int sy = im.cropY + (int)((int64_t)(y - im.dstY) * im.cropH / im.dstH);
    int sx = im.cropX + (int)((int64_t)(x - im.dstX) * im.cropW / im.dstW);
    uint16_t v = im.pixels[sy * im.width + sx];
    if (im.byteSwapped) v = (uint16_t)((v >> 8) | (v << 8));
    uint8_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
    out[0] = (r << 3) | (r >> 2);
    out[1] = (g << 2) | (g >> 4);
    out[2] = (b << 3) | (b >> 2);
  }
  for (int i = 0; i < p.bitmapCount; i++) {
    const PwgBitmap &bm = p.bitmaps[i];
    if (y < bm.y || y >= bm.y + bm.height * bm.scale || x < bm.x || x >= bm.x + bm.width * bm.scale) continue;
    int bx = (x - bm.x) / bm.scale, by = (y - bm.y) / bm.scale;
    if (bm.bits[by * ((bm.width + 7) / 8) + bx / 8] & (0x80 >> (bx & 7))) {
      out[0] = bm.r;
      out[1] = bm.g;
      out[2] = bm.b;
    }
  }
  if (p.grayscale) out[0] = pwgraster::toGray(out[0], out[1], out[2]);
}

static int failures = 0;
#define CHECK(cond, ...)                 \
  do {                                   \
    if (!(cond)) {                       \
      failures++;                        \
      printf("FAIL %s:%d: ", __FILE__, __LINE__); \
      printf(__VA_ARGS__);               \
      printf("\n");                      \
    }                                    \
  } while (0)

// Encodes the page, decodes it with libcups, compares header and pixels. Returns encoded size.
static size_t roundTrip(const char *name, const PwgPage &page, const char *keepPath = nullptr) {
  std::string path = keepPath ? keepPath : std::string("/tmp/pwg_test_") + name + ".pwg";
  FILE *f = fopen(path.c_str(), "wb");
  FileSink sink(f);
  PwgCountingSink counter;
  CHECK(pwgWritePage(counter, page, true), "%s: counting pass failed", name);
  CHECK(pwgWritePage(sink, page, true), "%s: write pass failed", name);
  long size = ftell(f);
  fclose(f);
  CHECK((size_t)size == counter.total, "%s: counted %zu but wrote %ld", name, counter.total, size);

  int fd = open(path.c_str(), O_RDONLY);
  cups_raster_t *r = cupsRasterOpen(fd, CUPS_RASTER_READ);
  cups_page_header2_t h;
  CHECK(r && cupsRasterReadHeader2(r, &h), "%s: libcups could not read header", name);
  CHECK((int)h.cupsWidth == page.width && (int)h.cupsHeight == page.height, "%s: size %ux%u", name,
        h.cupsWidth, h.cupsHeight);
  CHECK(page.grayscale ? (h.cupsBitsPerPixel == 8 && h.cupsColorSpace == CUPS_CSPACE_SW)
                       : (h.cupsBitsPerPixel == 24 && h.cupsColorSpace == CUPS_CSPACE_SRGB),
        "%s: format %u/%d", name, h.cupsBitsPerPixel, (int)h.cupsColorSpace);
  CHECK(h.HWResolution[0] == (unsigned)page.dpi, "%s: dpi", name);
  CHECK(std::string(h.cupsPageSizeName) == page.sizeName, "%s: size name '%s'", name, h.cupsPageSizeName);

  std::vector<uint8_t> line(h.cupsBytesPerLine);
  int bad = 0;
  for (int y = 0; y < page.height && bad < 5; y++) {
    if (cupsRasterReadPixels(r, line.data(), h.cupsBytesPerLine) != h.cupsBytesPerLine) {
      CHECK(false, "%s: short read at line %d", name, y);
      break;
    }
    for (int x = 0; x < page.width; x++) {
      uint8_t ref[3];
      referencePixel(page, x, y, ref);
      int bpp = page.grayscale ? 1 : 3;
      if (memcmp(ref, &line[x * bpp], bpp) != 0) {
        CHECK(false, "%s: pixel (%d,%d) differs, first byte %02x, want %02x", name, x, y, line[x * bpp], ref[0]);
        bad++;
        break;
      }
    }
  }
  CHECK(cupsRasterReadHeader2(r, &h) == 0, "%s: unexpected second page", name);
  cupsRasterClose(r);
  close(fd);
  if (!keepPath) unlink(path.c_str());
  return (size_t)size;
}

static std::vector<uint16_t> makeImage(int w, int h) {
  std::vector<uint16_t> px(w * h);
  uint32_t seed = 12345;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      uint16_t v;
      if (y < h / 3) {
        v = (uint16_t)(((x * 31 / w) << 11) | ((y * 63 / h) << 5) | 0x0F);  // gradient
      } else if (y < 2 * h / 3) {
        v = ((x / 8 + y / 8) & 1) ? 0xF800 : 0x001F;  // checkerboard
      } else {
        seed = seed * 1103515245 + 12345;
        v = (uint16_t)(seed >> 16);  // noise: no runs at all
      }
      px[y * w + x] = v;
    }
  }
  return px;
}

static std::vector<uint8_t> makeBits(int w, int h) {
  std::vector<uint8_t> bits(((w + 7) / 8) * h, 0);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      if ((x % 7 < 3 && y % 5 < 3) || x == y) bits[y * ((w + 7) / 8) + x / 8] |= 0x80 >> (x & 7);
  return bits;
}

// ---------- Printer capabilities ----------

static void ippAttr(std::vector<uint8_t> &b, uint8_t tag, const std::string &name, const std::vector<uint8_t> &value) {
  b.push_back(tag);
  b.push_back((uint8_t)(name.size() >> 8));
  b.push_back((uint8_t)name.size());
  b.insert(b.end(), name.begin(), name.end());
  b.push_back((uint8_t)(value.size() >> 8));
  b.push_back((uint8_t)value.size());
  b.insert(b.end(), value.begin(), value.end());
}

static std::vector<uint8_t> ippRes(int x, int y, uint8_t units) {
  return {(uint8_t)(x >> 24), (uint8_t)(x >> 16), (uint8_t)(x >> 8), (uint8_t)x,
          (uint8_t)(y >> 24), (uint8_t)(y >> 16), (uint8_t)(y >> 8), (uint8_t)y, units};
}

static std::vector<uint8_t> ippWord(const std::string &s) { return std::vector<uint8_t>(s.begin(), s.end()); }

static std::string planText(const PwgCaps &caps) {
  PwgAttempt a[6];
  int n = pwgPlanAttempts(caps, a, 6);
  std::string out;
  for (int i = 0; i < n; i++) {
    out += (i ? " " : "") + std::to_string(a[i].dpi) + (a[i].grayscale ? "g" : "c");
  }
  return out;
}

#define CHECK_PLAN(caps, want)                                                    \
  CHECK(planText(caps) == want, "plan is '%s', want '%s'", planText(caps).c_str(), want)

static void testCaps() {
  std::vector<uint8_t> b = {1, 1, 0, 0, 0, 0, 0, 1, 0x01};  // successful-ok, operation group
  ippAttr(b, 0x47, "attributes-charset", ippWord("utf-8"));
  b.push_back(0x04);  // printer attributes
  ippAttr(b, 0x49, "document-format-supported", ippWord("image/pwg-raster"));
  ippAttr(b, 0x32, "pwg-raster-document-resolution-supported", ippRes(300, 300, 3));
  ippAttr(b, 0x32, "", ippRes(600, 600, 3));
  ippAttr(b, 0x32, "", ippRes(600, 300, 3));   // not square: skipped
  ippAttr(b, 0x32, "", ippRes(236, 236, 4));   // 600 dpi given per cm: duplicate
  ippAttr(b, 0x32, "", ippRes(1200, 1200, 3));
  ippAttr(b, 0x32, "", ippRes(2400, 2400, 3)); // above the limit: skipped
  ippAttr(b, 0x32, "", ippRes(150, 150, 3));
  ippAttr(b, 0x44, "pwg-raster-document-type-supported", ippWord("black_1"));
  ippAttr(b, 0x44, "", ippWord("sgray_8"));
  ippAttr(b, 0x44, "", ippWord("srgb_8"));
  ippAttr(b, 0x44, "other-keyword", ippWord("srgb_8"));
  b.push_back(0x03);

  PwgCaps caps;
  CHECK(pwgParseCaps(b.data(), b.size(), caps), "valid reply rejected");
  CHECK(caps.typesKnown && caps.srgb8 && caps.sgray8, "types not read");
  CHECK_PLAN(caps, "600c 300c 150c 1200c 600g 300g");

  // Malformed or cut-off replies must not read past the end, and fall back to the defaults
  for (size_t cut = 0; cut < b.size(); cut++) {
    PwgCaps partial;
    CHECK(!pwgParseCaps(b.data(), cut, partial), "cut reply at %zu accepted", cut);
  }

  PwgCaps unknown;
  CHECK_PLAN(unknown, "600c 300c 600g 300g");

  PwgCaps only300;
  only300.dpis[0] = 300;
  only300.dpiCount = 1;
  only300.typesKnown = true;
  only300.srgb8 = true;
  CHECK_PLAN(only300, "300c");

  PwgCaps monoLaser;
  monoLaser.dpis[0] = 600;
  monoLaser.dpis[1] = 300;
  monoLaser.dpiCount = 2;
  monoLaser.typesKnown = true;
  monoLaser.sgray8 = true;
  CHECK_PLAN(monoLaser, "600g 300g");

  PwgCaps nothingUsable;
  nothingUsable.typesKnown = true;  // for example black_1 only
  CHECK_PLAN(nothingUsable, "");

  PwgAttempt a[4];
  CHECK(pwgPlanAttempts(caps, a, 4) == 4, "attempts not capped");
}

int main(int argc, char **argv) {
  testCaps();

  // 1. Encoder edge cases on a narrow page: single pixels, runs of exactly 128/129, literals of 128+.
  {
    std::vector<uint16_t> px(300 * 4);
    for (int x = 0; x < 300; x++) {
      px[x] = (uint16_t)(x * 211);                     // all different (long literal)
      px[300 + x] = x < 129 ? 0x1234 : 0x4321;         // run of 129 then run of 171
      px[600 + x] = (x % 2) ? 0xFFFF : 0x0000;         // alternating
      px[900 + x] = (x == 0 || x == 299) ? 0 : 0xFFFF;  // single pixels at the edges
    }
    PwgPage p;
    p.width = 300;
    p.height = 10;
    p.image.pixels = px.data();
    p.image.width = 300;
    p.image.height = 4;
    p.image.cropW = 300;
    p.image.cropH = 4;
    p.image.dstW = 300;
    p.image.dstH = 8;
    p.image.dstY = 1;
    roundTrip("edges", p);
  }
  // 2. Line repeat groups longer than 256 lines (blank page).
  {
    PwgPage p;
    p.width = 64;
    p.height = 1000;
    roundTrip("blank", p);
  }
  // 3. Upscaled image with crop, byte-swapped pixels, two bitmaps, clipping at the right edge.
  {
    std::vector<uint16_t> px = makeImage(120, 90);
    for (auto &v : px) v = (uint16_t)((v >> 8) | (v << 8));
    std::vector<uint8_t> bits = makeBits(50, 12);
    PwgPage p;
    p.width = 900;
    p.height = 700;
    p.image = {px.data(), true, 120, 90, 10, 5, 100, 80, 50, 40, 750, 500};
    p.bitmaps[0] = {bits.data(), 50, 12, 3, 60, 560, 0, 0, 0};
    p.bitmaps[1] = {bits.data(), 50, 12, 4, 800, 600, 0x60, 0x60, 0x60};  // runs past the right edge
    p.bitmapCount = 2;
    roundTrip("composed", p);
  }
  // 4. Full A4 at 600 dpi with a 330x411 image (Wikipedia thumbnail size): check the size stays small.
  {
    std::vector<uint16_t> px = makeImage(330, 411);
    std::vector<uint8_t> bits = makeBits(1400, 40);
    PwgPage p;  // defaults: A4, 600 dpi
    int margin = 236;
    p.image = {px.data(), false, 330, 411, 0, 0, 330, 411, margin, margin, 4960 - 2 * margin,
               (4960 - 2 * margin) * 411 / 330};
    p.bitmaps[0] = {bits.data(), 1400, 40, 3, margin, 6500, 0x55, 0x55, 0x55};
    p.bitmapCount = 1;
    size_t size = roundTrip("a4", p, argc > 1 ? argv[1] : nullptr);
    printf("A4 page with 330x411 image: %.2f MB\n", size / 1048576.0);
    CHECK(size < 3 * 1048576, "A4 page too large: %zu", size);
  }

  // 5. Other resolutions and grey pages, with geometry matching libcups.
  for (int dpi : {150, 300, 600, 1200}) {
    cups_page_header2_t ref;
    pwg_media_t *media = pwgMediaForPWG("iso_a4_210x297mm");
    CHECK(cupsRasterInitPWGHeader(&ref, media, "srgb_8", dpi, dpi, "one-sided", "rotated"), "libcups init");
    PwgPage p;
    pwgSetA4(p, dpi);
    CHECK(p.width == (int)ref.cupsWidth && p.height == (int)ref.cupsHeight, "%d dpi: %dx%d, libcups %ux%u", dpi,
          p.width, p.height, ref.cupsWidth, ref.cupsHeight);
    if (dpi > 300) continue;  // the larger pages are covered above; keep the test fast
    std::vector<uint16_t> px = makeImage(80, 60);
    std::vector<uint8_t> bits = makeBits(40, 10);
    p.image = {px.data(), false, 80, 60, 0, 0, 80, 60, dpi / 4, dpi / 4, dpi * 3, dpi * 2};
    p.bitmaps[0] = {bits.data(), 40, 10, 1, dpi / 4, dpi * 5, 0x66, 0x66, 0x66};
    p.bitmapCount = 1;
    std::string name = "colour" + std::to_string(dpi);
    roundTrip(name.c_str(), p);
    p.grayscale = true;
    name = "grey" + std::to_string(dpi);
    roundTrip(name.c_str(), p);
  }

  if (failures) {
    printf("%d check(s) failed\n", failures);
    return 1;
  }
  printf("All PWG raster tests passed\n");
  return 0;
}
