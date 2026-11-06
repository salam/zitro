// Host tool for tests/gif/run.py: plays a GIF through GifAnimator and writes every picture.
//   gif_dump <file.gif> <out.raw> <maxPixels> <frameLimit>
// stdout: "scan <valid> <complete> <w> <h> <frames> <loops>", "begin <0|1>", then "frame <i> <delayMs>".
// out.raw holds one little-endian RGB565 picture per frame line.
#include <stdio.h>

#include <vector>

#include "../../sketch/sketch_jan12a/gif_decode.h"

int main(int argc, char **argv) {
  if (argc < 5) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  std::vector<uint8_t> data;
  uint8_t chunk[65536];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) data.insert(data.end(), chunk, chunk + n);
  fclose(f);

  GifInfo info = gifScan(data.data(), (int)data.size());
  printf("scan %d %d %d %d %d %d\n", info.valid, info.complete, info.width, info.height, info.frames, info.loops);

  GifAnimator anim;
  bool ok = anim.begin(data.data(), (int)data.size(), atoi(argv[3]), false);
  printf("begin %d\n", ok);
  if (!ok) return 0;
  FILE *out = fopen(argv[2], "wb");
  int limit = atoi(argv[4]);
  for (int i = 0; i < limit && anim.nextFrame(); i++) {
    printf("frame %d %u\n", anim.frameIndex(), anim.delayMs());
    fwrite(anim.pixels(), sizeof(uint16_t), (size_t)anim.width() * anim.height(), out);
  }
  fclose(out);
  return 0;
}
