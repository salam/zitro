// Decodes every prepared picture with lgfx_tjpgd, the decoder behind drawJpg on the stick, so a file
// it can't read shows up here. The speed is this computer's: it says nothing about the stick except
// which pictures cost the most.
#include <dirent.h>

#include <chrono>
#include <string>
#include <vector>

extern "C" {
#include "lgfx_tjpgd.h"
}
#include "../../sketch/sketch_jan12a/timeline_core.h"

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

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  const std::string dir = argv[1];
  int decoded = 0, failed = 0;
  double slowest = 0, total = 0;
  for (const char* id : TIMELINE_TOPICS) {
    std::string text = readFile(dir + "/" + id + ".txt");
    const std::string bundle = readFile(dir + "/" + id + ".jpgs");
    std::vector<char> s(text.begin(), text.end());
    s.resize(text.size() + 1);
    static Timeline t;
    if (!timelineParse(s.data(), text.size(), s.size(), id, t)) {
      printf("FAIL can't parse %s\n", id);
      failed++;
      continue;
    }
    timelineCheckImages(t, (const uint8_t*)bundle.data(), bundle.size());
    for (int i = 0; i < t.eventCount; i++) {
      const TimelineEvent& e = t.events[i];
      if (!e.hasImage) continue;
      Source src = {(const uint8_t*)bundle.data() + e.imageOffset, e.imageLength, 0, 0};
      static uint8_t pool[3900];  // As much as draw_jpg gives it
      lgfxJdec jd;
      const auto start = std::chrono::steady_clock::now();
      JRESULT r = lgfx_jd_prepare(&jd, input, pool, sizeof(pool), &src);
      if (r == JDR_OK) r = lgfx_jd_decomp(&jd, output, 0);
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
      if (r != JDR_OK || jd.width != e.imageW || jd.height != e.imageH || src.pixels != (uint32_t)e.imageW * e.imageH) {
        printf("FAIL %s %s: result %d, %ux%u, %u pixels\n", id, e.qid, (int)r, (unsigned)jd.width, (unsigned)jd.height,
               (unsigned)src.pixels);
        failed++;
      } else {
        decoded++;
        total += ms;
        if (ms > slowest) slowest = ms;
      }
    }
  }
  printf("Stick's decoder read %d pictures, %d failed; on this computer %.2f ms average, %.2f ms slowest\n", decoded, failed,
         decoded ? total / decoded : 0, slowest);
  return failed ? 1 : 0;
}
