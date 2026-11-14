// Decodes every fact sheet picture with lgfx_tjpgd, the decoder behind drawJpg on the stick, so a file
// it can't read shows up here. Copied from tests/timeline/test_decode.cpp for single <slug>.jpg files.
// The speed is this computer's: it says nothing about the stick except which pictures cost the most.
#include <dirent.h>

#include <chrono>
#include <string>
#include <vector>

extern "C" {
#include "lgfx_tjpgd.h"
}
#include "../../sketch/sketch_jan12a/dino_core.h"

static std::string readFile(const std::string& path) {
  std::string out;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return out;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
  fclose(f);
  return out;
}

struct Source {
  const uint8_t* data;
  uint32_t length, at;
  uint32_t pixels;
};

static uint32_t input(void* device, uint8_t* buffer, uint32_t count) {
  Source* s = (Source*)device;
  if (s->at + count > s->length) count = s->length - s->at;
  if (buffer) memcpy(buffer, s->data + s->at, count);
  s->at += count;
  return count;
}

static uint32_t output(void* device, void*, JRECT* r) {
  Source* s = (Source*)device;
  s->pixels += (r->right - r->left + 1) * (r->bottom - r->top + 1);
  return 1;
}

static bool endsWith(const std::string& s, const char* tail) {
  const size_t n = strlen(tail);
  return s.size() >= n && s.compare(s.size() - n, n, tail) == 0;
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const std::string dir = argv[1];
  DIR* d = opendir(dir.c_str());
  if (!d) {
    printf("FAIL %s does not exist\n", dir.c_str());
    return 1;
  }
  std::vector<std::string> slugs;
  while (dirent* e = readdir(d)) {
    const std::string name = e->d_name;
    if (endsWith(name, ".txt") && name.rfind("index-", 0) != 0) slugs.push_back(name.substr(0, name.size() - 4));
  }
  closedir(d);
  int decoded = 0, failed = 0, largest = 0;
  long bytes = 0;
  double slowest = 0, total = 0;
  for (const std::string& slug : slugs) {
    const std::string text = readFile(dir + "/" + slug + ".txt");
    std::vector<char> storage(text.begin(), text.end());
    storage.resize(text.size() + 1);
    DinoSheet s;
    if (!dinoParseSheet(storage.data(), text.size(), storage.size(), slug.c_str(), s)) {
      printf("FAIL can't parse %s.txt\n", slug.c_str());
      failed++;
      continue;
    }
    if (!s.hasImage) continue;
    const std::string jpeg = readFile(dir + "/" + slug + ".jpg");
    if (!dinoImageOk(s, (const uint8_t*)jpeg.data(), jpeg.size())) {
      printf("FAIL %s.jpg is missing or not the picture its sheet names\n", slug.c_str());
      failed++;
      continue;
    }
    Source src = {(const uint8_t*)jpeg.data(), s.imageLength, 0, 0};
    static uint8_t pool[3900];  // As much as draw_jpg gives it
    lgfxJdec jd;
    const auto start = std::chrono::steady_clock::now();
    JRESULT r = lgfx_jd_prepare(&jd, input, pool, sizeof(pool), &src);
    if (r == JDR_OK) r = lgfx_jd_decomp(&jd, output, 0);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (r != JDR_OK || jd.width != s.imageW || jd.height != s.imageH || src.pixels != (uint32_t)s.imageW * s.imageH) {
      printf("FAIL %s.jpg: result %d, %ux%u, %u pixels, the sheet says %ux%u\n", slug.c_str(), (int)r,
             (unsigned)jd.width, (unsigned)jd.height, (unsigned)src.pixels, (unsigned)s.imageW, (unsigned)s.imageH);
      failed++;
    } else {
      decoded++;
      total += ms;
      bytes += s.imageLength;
      if ((int)s.imageLength > largest) largest = (int)s.imageLength;
      if (ms > slowest) slowest = ms;
    }
  }
  printf("Stick's decoder read %d pictures, %d failed; average %.1f KB, largest %.1f KB; on this computer %.2f ms "
         "average, %.2f ms slowest\n",
         decoded, failed, decoded ? bytes / 1024.0 / decoded : 0, largest / 1024.0, decoded ? total / decoded : 0,
         slowest);
  return failed ? 1 : 0;
}
