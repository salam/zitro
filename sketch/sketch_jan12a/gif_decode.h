// GIF decoding and frame compositing, portable C++ without Arduino dependencies.
//
// gifScan() reads a file's structure without decoding it. GifAnimator keeps the picture a viewer
// should see at the GIF's own size in an RGB565 buffer and advances it frame by frame: offset
// partial frames, transparency, disposal methods, local palettes, interlacing and loop counts.
// Transparent background shows as black, like the stick's screen.
//
// Used by the sketch (gif_anim.ino, drawGifToCanvas) and by tests/gif on the host.
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---------- LZW ----------

struct GifBitStream {
  const uint8_t* data;
  int len;
  int pos;
  int blockRemaining;
  uint32_t bitBuffer;
  int bitCount;
};

static inline bool gifReadDataByte(GifBitStream* stream, uint8_t* out) {
  while (stream->blockRemaining == 0) {
    if (stream->pos >= stream->len) {
      return false;
    }
    stream->blockRemaining = stream->data[stream->pos++];
    if (stream->blockRemaining == 0) {
      return false;  // block terminator: end of the image data
    }
  }
  if (stream->pos >= stream->len) {
    return false;
  }
  *out = stream->data[stream->pos++];
  stream->blockRemaining--;
  return true;
}

static inline bool gifReadCode(GifBitStream* stream, int codeSize, int* outCode) {
  while (stream->bitCount < codeSize) {
    uint8_t byte = 0;
    if (!gifReadDataByte(stream, &byte)) {
      return false;
    }
    stream->bitBuffer |= ((uint32_t)byte) << stream->bitCount;
    stream->bitCount += 8;
  }
  *outCode = stream->bitBuffer & ((1 << codeSize) - 1);
  stream->bitBuffer >>= codeSize;
  stream->bitCount -= codeSize;
  return true;
}

// Decodes one frame's image data into pixels (width x height palette indices, row by row).
// Pixels the data doesn't cover stay 0. Returns false if there was no image data at all.
static inline bool gifDecodeInto(const uint8_t* data, int len, int startPos, uint8_t lzwMinCodeSize,
                                 uint16_t width, uint16_t height, bool interlaced, uint8_t* pixels) {
  const int pixelCount = (int)width * (int)height;
  if (pixelCount <= 0 || lzwMinCodeSize < 1 || lzwMinCodeSize > 11) {
    return false;
  }
  memset(pixels, 0, pixelCount);

  static uint16_t prefix[4096];
  static uint8_t suffix[4096];
  static uint8_t stack[4097];

  GifBitStream stream = {};
  stream.data = data;
  stream.len = len;
  stream.pos = startPos;

  const int clearCode = 1 << lzwMinCodeSize;
  const int endCode = clearCode + 1;
  int nextCode = endCode + 1;
  int codeSize = lzwMinCodeSize + 1;
  for (int i = 0; i < clearCode; i++) {
    prefix[i] = 0;
    suffix[i] = (uint8_t)i;
  }

  int oldCode = -1;
  int first = 0;
  int outCount = 0;
  int stackSize = 0;
  int x = 0, y = 0, pass = 0;
  static const int interlaceOffsets[4] = {0, 4, 2, 1};
  static const int interlaceSteps[4] = {8, 8, 4, 2};

  auto writePixel = [&](uint8_t idx) {
    if (outCount >= pixelCount) {
      return;
    }
    if (interlaced) {
      if (y < (int)height) {
        pixels[y * width + x] = idx;
      }
      x++;
      if (x >= (int)width) {
        x = 0;
        y += interlaceSteps[pass];
        while (y >= (int)height && pass < 3) {
          pass++;
          y = interlaceOffsets[pass];
        }
      }
    } else {
      pixels[outCount] = idx;
    }
    outCount++;
  };

  int code = 0;
  while (outCount < pixelCount && gifReadCode(&stream, codeSize, &code)) {
    if (code == clearCode) {
      codeSize = lzwMinCodeSize + 1;
      nextCode = endCode + 1;
      oldCode = -1;
      continue;
    }
    if (code == endCode) {
      break;
    }
    if (oldCode == -1) {
      if (code >= clearCode) {
        break;  // damaged data
      }
      writePixel((uint8_t)code);
      first = code;
      oldCode = code;
      continue;
    }
    if (code > nextCode) {
      break;  // damaged data
    }

    int inCode = code;
    if (code == nextCode) {
      stack[stackSize++] = (uint8_t)first;
      code = oldCode;
    }
    while (code >= clearCode && stackSize < 4096) {
      stack[stackSize++] = suffix[code];
      code = prefix[code];
    }
    first = code;
    stack[stackSize++] = (uint8_t)first;
    while (stackSize) {
      writePixel(stack[--stackSize]);
    }

    if (nextCode < 4096) {
      prefix[nextCode] = (uint16_t)oldCode;
      suffix[nextCode] = (uint8_t)first;
      nextCode++;
      if (nextCode == (1 << codeSize) && codeSize < 12) {
        codeSize++;
      }
    }
    oldCode = inCode;
  }
  return outCount > 0;
}

// Allocating form, for the single-frame path. Returns the pixels in *outPixels; free() them.
static inline bool decodeGifPixels(const uint8_t* data, int len, int startPos, uint8_t lzwMinCodeSize,
                                   uint16_t width, uint16_t height, bool interlaced, uint8_t** outPixels) {
  const int pixelCount = (int)width * (int)height;
  if (pixelCount <= 0) {
    return false;
  }
  uint8_t* pixels = (uint8_t*)malloc(pixelCount);
  if (!pixels) {
    return false;
  }
  if (!gifDecodeInto(data, len, startPos, lzwMinCodeSize, width, height, interlaced, pixels)) {
    free(pixels);
    return false;
  }
  *outPixels = pixels;
  return true;
}

// ---------- File structure ----------

// Skips data sub-blocks from pos up to and including the terminator. Returns -1 if the file ends first.
static inline int gifSkipSubBlocks(const uint8_t* data, int len, int pos) {
  while (pos < len) {
    int size = data[pos++];
    if (size == 0) {
      return pos;
    }
    pos += size;
  }
  return -1;
}

struct GifInfo {
  bool valid = false;        // a GIF header with a size
  bool complete = false;     // reached the trailer; false for files cut off by a download limit
  int width = 0, height = 0;
  int frames = 0;            // frames whose image data ended properly
  int maxFramePixels = 0;    // largest frame rectangle
  bool usesRestore = false;  // some frame needs "restore to previous"
  int loops = -1;            // -1: no loop block, plays once; 0: forever; n: n extra plays
  int firstBlock = 0;        // offset of the first block after the header and global palette
};

static inline GifInfo gifScan(const uint8_t* data, int len) {
  GifInfo info;
  if (!data || len < 13 || memcmp(data, "GIF", 3) != 0) {
    return info;
  }
  info.width = data[6] | (data[7] << 8);
  info.height = data[8] | (data[9] << 8);
  int pos = 13;
  if (data[10] & 0x80) {
    pos += 3 << ((data[10] & 0x07) + 1);
  }
  if (pos > len || info.width <= 0 || info.height <= 0) {
    return info;
  }
  info.valid = true;
  info.firstBlock = pos;

  int nextDisposal = 0;
  while (pos < len) {
    uint8_t sep = data[pos++];
    if (sep == 0x3B) {
      info.complete = true;
      return info;
    }
    if (sep == 0x21) {
      if (pos >= len) return info;
      uint8_t label = data[pos++];
      if (label == 0xF9 && pos + 5 < len && data[pos] == 4) {
        nextDisposal = (data[pos + 1] >> 2) & 0x07;
      } else if (label == 0xFF && pos + 16 < len && data[pos] == 11 && memcmp(data + pos + 1, "NETSCAPE2.0", 11) == 0 &&
                 data[pos + 12] == 3 && data[pos + 13] == 1) {
        info.loops = data[pos + 14] | (data[pos + 15] << 8);
      }
      pos = gifSkipSubBlocks(data, len, pos);
      if (pos < 0) return info;
    } else if (sep == 0x2C) {
      if (pos + 9 > len) return info;
      int fw = data[pos + 4] | (data[pos + 5] << 8);
      int fh = data[pos + 6] | (data[pos + 7] << 8);
      uint8_t packed = data[pos + 8];
      pos += 9;
      if (packed & 0x80) {
        pos += 3 << ((packed & 0x07) + 1);
      }
      pos += 1;  // LZW minimum code size
      if (pos > len) return info;
      pos = gifSkipSubBlocks(data, len, pos);
      if (pos < 0) return info;
      info.frames++;
      if (fw * fh > info.maxFramePixels) info.maxFramePixels = fw * fh;
      if (nextDisposal == 3) info.usesRestore = true;
      nextDisposal = 0;
    } else {
      return info;  // not a block: damaged or cut off
    }
  }
  return info;
}

// ---------- Compositor ----------

#define GIF_DEFAULT_DELAY_MS 100

class GifAnimator {
public:
  typedef void* (*AllocFn)(size_t);

  ~GifAnimator() { end(); }

  // Prepares playback. swapBytes stores pixels byte-swapped, as LovyanGFX sprites hold them.
  // False if the file isn't a complete animation within maxPixels, or memory runs out.
  bool begin(const uint8_t* data, int len, int maxPixels, bool swapBytes, AllocFn alloc = malloc) {
    end();
    info_ = gifScan(data, len);
    if (!info_.valid || !info_.complete || info_.frames < 2) return false;
    if ((long)info_.width * info_.height > maxPixels || info_.maxFramePixels > maxPixels) return false;
    data_ = data;
    len_ = len;
    swap_ = swapBytes;
    size_t area = (size_t)info_.width * info_.height;
    canvas_ = (uint16_t*)alloc(area * sizeof(uint16_t));
    indices_ = (uint8_t*)alloc(info_.maxFramePixels);
    if (info_.usesRestore) backup_ = (uint16_t*)alloc(area * sizeof(uint16_t));
    if (!canvas_ || !indices_ || (info_.usesRestore && !backup_)) {
      end();
      return false;
    }
    if (data[10] & 0x80) {
      globalPaletteCount_ = 2 << (data[10] & 0x07);
      for (int i = 0; i < globalPaletteCount_; i++) {
        globalPalette_[i] = color(data + 13 + i * 3);
      }
    }
    playsLeft_ = info_.loops < 0 ? 1 : (info_.loops == 0 ? 0 : info_.loops + 1);  // 0: forever
    restart();
    return true;
  }

  void end() {
    free(canvas_);
    free(indices_);
    free(backup_);
    canvas_ = backup_ = nullptr;
    indices_ = nullptr;
    data_ = nullptr;
    globalPaletteCount_ = 0;
  }

  // Composites the next frame. Returns false when playback has ended (the last picture stays)
  // or the data is damaged.
  bool nextFrame() {
    if (!canvas_) return false;
    disposePrevious();

    int transparent = -1, disposal = 0;
    uint32_t delayMs = GIF_DEFAULT_DELAY_MS;
    bool wrapped = false;
    while (pos_ < len_) {
      uint8_t sep = data_[pos_++];
      if (sep == 0x3B) {
        if (wrapped) return false;  // a loop without frames: damaged
        if (playsLeft_ > 0 && --playsLeft_ == 0) return false;
        restart();
        wrapped = true;
      } else if (sep == 0x21) {
        if (pos_ >= len_) return false;
        uint8_t label = data_[pos_++];
        if (label == 0xF9 && pos_ + 5 < len_ && data_[pos_] == 4) {
          uint8_t packed = data_[pos_ + 1];
          disposal = (packed >> 2) & 0x07;
          int cs = data_[pos_ + 2] | (data_[pos_ + 3] << 8);
          delayMs = cs < 2 ? GIF_DEFAULT_DELAY_MS : (uint32_t)cs * 10;  // like browsers
          transparent = (packed & 0x01) ? data_[pos_ + 4] : -1;
        }
        pos_ = gifSkipSubBlocks(data_, len_, pos_);
        if (pos_ < 0) return false;
      } else if (sep == 0x2C) {
        if (!drawFrame(transparent, disposal)) return false;
        delayMs_ = delayMs;
        frameIndex_++;
        return true;
      } else {
        return false;
      }
    }
    return false;
  }

  const uint16_t* pixels() const { return canvas_; }
  int width() const { return info_.width; }
  int height() const { return info_.height; }
  int frameCount() const { return info_.frames; }
  int frameIndex() const { return frameIndex_; }  // of the current picture
  uint32_t delayMs() const { return delayMs_; }   // how long to show it
  const GifInfo& info() const { return info_; }

private:
  uint16_t color(const uint8_t* rgb) const {
    uint16_t v = (uint16_t)(((rgb[0] & 0xF8) << 8) | ((rgb[1] & 0xFC) << 3) | (rgb[2] >> 3));
    return swap_ ? (uint16_t)((v >> 8) | (v << 8)) : v;
  }

  void restart() {
    memset(canvas_, 0, (size_t)info_.width * info_.height * sizeof(uint16_t));
    pos_ = info_.firstBlock;
    frameIndex_ = -1;
    prevDisposal_ = 0;
  }

  void disposePrevious() {
    if (prevDisposal_ != 2 && prevDisposal_ != 3) return;
    for (int y = 0; y < prevH_; y++) {
      uint16_t* row = canvas_ + (size_t)(prevY_ + y) * info_.width + prevX_;
      if (prevDisposal_ == 2) {
        memset(row, 0, prevW_ * sizeof(uint16_t));
      } else {
        memcpy(row, backup_ + (size_t)(prevY_ + y) * info_.width + prevX_, prevW_ * sizeof(uint16_t));
      }
    }
    prevDisposal_ = 0;
  }

  bool drawFrame(int transparent, int disposal) {
    if (pos_ + 9 > len_) return false;
    const uint8_t* d = data_ + pos_;
    int left = d[0] | (d[1] << 8), top = d[2] | (d[3] << 8);
    int fw = d[4] | (d[5] << 8), fh = d[6] | (d[7] << 8);
    uint8_t packed = d[8];
    pos_ += 9;

    int paletteCount = globalPaletteCount_;
    uint16_t* palette = globalPalette_;
    if (packed & 0x80) {
      paletteCount = 2 << (packed & 0x07);
      if (pos_ + paletteCount * 3 > len_) return false;
      for (int i = 0; i < paletteCount; i++) {
        localPalette_[i] = color(data_ + pos_ + i * 3);
      }
      palette = localPalette_;
      pos_ += paletteCount * 3;
    }
    if (paletteCount == 0 || pos_ >= len_) return false;

    uint8_t lzwMin = data_[pos_++];
    if (fw <= 0 || fh <= 0 || fw * fh > info_.maxFramePixels) return false;
    if (!gifDecodeInto(data_, len_, pos_, lzwMin, fw, fh, packed & 0x40, indices_)) return false;
    pos_ = gifSkipSubBlocks(data_, len_, pos_);
    if (pos_ < 0) return false;

    // The part of the frame on the picture
    int visW = left >= info_.width ? 0 : (left + fw > info_.width ? info_.width - left : fw);
    int visH = top >= info_.height ? 0 : (top + fh > info_.height ? info_.height - top : fh);

    if (disposal == 3 && backup_) {
      memcpy(backup_, canvas_, (size_t)info_.width * info_.height * sizeof(uint16_t));
    }
    for (int y = 0; y < visH; y++) {
      const uint8_t* src = indices_ + (size_t)y * fw;
      uint16_t* dst = canvas_ + (size_t)(top + y) * info_.width + left;
      for (int x = 0; x < visW; x++) {
        int idx = src[x];
        if (idx == transparent) continue;       // keeps what is underneath
        dst[x] = idx < paletteCount ? palette[idx] : 0;
      }
    }
    prevDisposal_ = disposal;
    prevX_ = left;
    prevY_ = top;
    prevW_ = visW;
    prevH_ = visH;
    return true;
  }

  const uint8_t* data_ = nullptr;
  int len_ = 0;
  int pos_ = 0;
  bool swap_ = false;
  GifInfo info_;
  uint16_t* canvas_ = nullptr;
  uint16_t* backup_ = nullptr;
  uint8_t* indices_ = nullptr;
  uint16_t globalPalette_[256];
  uint16_t localPalette_[256];
  int globalPaletteCount_ = 0;
  int playsLeft_ = 0;
  int frameIndex_ = -1;
  uint32_t delayMs_ = GIF_DEFAULT_DELAY_MS;
  int prevDisposal_ = 0, prevX_ = 0, prevY_ = 0, prevW_ = 0, prevH_ = 0;
};
