// Plays animated GIFs on the display, on top of the first frame displayImage() has drawn.
//
// Started by displayImage(), advanced from loop(), and stopped as soon as anything else draws
// the screen (captureCanvas() is called before every full-screen push). The tick never counts as
// activity, so display sleep and light sleep happen as usual.
//
// Stays a still first frame when: the GIF has one frame, was cut off by the download limit, has
// more than GIF_ANIM_MAX_PIXELS, or memory runs out.

#include "gif_decode.h"

// About 350 x 350. Measured on the stick: 320 x 320 takes 130 ms per frame, 400 x 400 takes 180 ms and
// 500 x 500 takes 280 ms. Buttons are only read between frames, so slower frames can miss a quick tap.
#define GIF_ANIM_MAX_PIXELS 120000

static GifAnimator gifAnim;
static bool gifAnimActive = false;
static bool gifAnimDrawingFrame = false;  // our own captureCanvas() call must not stop us
static const uint8_t* gifAnimData = nullptr;
static int gifAnimLen = 0;
static float gifAnimScale = 1.0f;
static unsigned long gifAnimDueAt = 0;

// Numbers for the serial test command
static uint32_t gifAnimFramesShown = 0;
static uint32_t gifAnimSlowestMs = 0;
static uint32_t gifAnimTotalMs = 0;
static unsigned long gifAnimStartedAt = 0;

static void* gifAnimAlloc(size_t n) { return ps_malloc(n); }

void gifAnimStop() {
  if (!gifAnimActive) return;
  gifAnimActive = false;
  gifAnim.end();
  Serial.printf("GIF: stopped after %u frames, %lu ms\n", (unsigned)gifAnimFramesShown, millis() - gifAnimStartedAt);
}

// Called by captureCanvas(): another screen is about to replace the animation
void gifAnimOnScreenDraw() {
  if (!gifAnimDrawingFrame) gifAnimStop();
}

// Shows the current picture with displayImage()'s scale and scroll position
static void gifAnimRender() {
  M5Canvas canvas(&M5.Display);
  if (!canvas.createSprite(WIDTH, HEIGHT)) return;  // next frame tries again
  canvas.fillSprite(TFT_BLACK);

  const int srcW = gifAnim.width(), srcH = gifAnim.height();
  const int scaledW = min(WIDTH, (int)(srcW * gifAnimScale + 0.5f));
  const int scaledH = (int)(srcH * gifAnimScale + 0.5f);
  const uint16_t* src = gifAnim.pixels();  // same byte order as the sprite
  uint16_t* dst = (uint16_t*)canvas.getBuffer();

  int16_t xmap[WIDTH];
  for (int x = 0; x < scaledW; x++) {
    xmap[x] = (int16_t)min((int)(x / gifAnimScale), srcW - 1);
  }
  for (int dy = 0; dy < HEIGHT; dy++) {
    int y = dy + wolframScrollOffset;
    if (y >= scaledH) break;
    const uint16_t* row = src + (size_t)min((int)(y / gifAnimScale), srcH - 1) * srcW;
    uint16_t* out = dst + (size_t)dy * WIDTH;
    for (int x = 0; x < scaledW; x++) {
      out[x] = row[xmap[x]];
    }
  }

  drawBatteryIndicator(canvas);
  gifAnimDrawingFrame = true;
  captureCanvas(canvas);
  gifAnimDrawingFrame = false;
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Called by displayImage() instead of decoding the first frame again. While this image is playing,
// a redraw (scrolling, zoom, turning the stick, waking up) keeps the current frame.
bool gifAnimResume(float scale) {
  if (!gifAnimActive || gifAnimData != imageBuffer || gifAnimLen != imageBufferSize) return false;
  gifAnimScale = scale;
  gifAnimRender();
  return true;
}

// Called by displayImage() after it has drawn the first frame.
void gifAnimStart(float scale) {
  gifAnimStop();
  unsigned long t0 = millis();
  if (!gifAnim.begin(imageBuffer, imageBufferSize, GIF_ANIM_MAX_PIXELS, true, gifAnimAlloc)) {
    const GifInfo& info = gifAnim.info();
    Serial.printf("GIF: stays still (%dx%d, %d frames, %s)\n", info.width, info.height, info.frames,
                  info.complete ? "complete" : "cut off");
    return;
  }
  if (!gifAnim.nextFrame()) {  // the picture already on screen
    gifAnim.end();
    return;
  }
  gifAnimData = imageBuffer;
  gifAnimLen = imageBufferSize;
  gifAnimScale = scale;
  gifAnimDueAt = t0 + gifAnim.delayMs();
  gifAnimFramesShown = 1;
  gifAnimSlowestMs = 0;
  gifAnimTotalMs = 0;
  gifAnimStartedAt = t0;
  gifAnimActive = true;
  Serial.printf("GIF: playing %dx%d, %d frames, first frame took %lu ms, free PSRAM %u\n", gifAnim.width(),
                gifAnim.height(), gifAnim.frameCount(), millis() - t0, (unsigned)ESP.getFreePsram());
}

// Called at the top of loop()
void gifAnimTick() {
  if (!gifAnimActive) return;
  if (!hasImage || imageBuffer != gifAnimData || imageBufferSize != gifAnimLen) {
    gifAnimStop();  // the picture was replaced or freed: the frames would read stale memory
    return;
  }
  // Paused, not stopped: waking up continues where it was
  if (currentDisplayMode != SHOW_IMAGE || displaySleeping || currentPowerState != POWER_ACTIVE ||
      wakeFromLightSleep || inPrinterSelectionMode || timerAlertActive || inTimerListMode) {
    return;
  }
  unsigned long now = millis();
  if ((long)(now - gifAnimDueAt) < 0) return;

  if (!gifAnim.nextFrame()) {
    gifAnimStop();  // played to the end (the last picture stays) or damaged data
    return;
  }
  gifAnimRender();

  unsigned long done = millis();
  uint32_t spent = done - now;
  gifAnimFramesShown++;
  gifAnimTotalMs += spent;
  if (spent > gifAnimSlowestMs) gifAnimSlowestMs = spent;
  gifAnimDueAt += gifAnim.delayMs();  // keeps the GIF's pace, even with uneven decoding
  if ((long)(done - gifAnimDueAt) > 0) gifAnimDueAt = done;  // too slow for this GIF: plays slower
}

void gifAnimPrintStats() {
  Serial.printf("@@GIF %s frame %d of %d, shown %u, decode+draw avg %u ms max %u ms, %lu ms running, free PSRAM %u heap %u\n",
                gifAnimActive ? "playing" : "idle", gifAnimActive ? gifAnim.frameIndex() : -1,
                gifAnimActive ? gifAnim.frameCount() : 0, (unsigned)gifAnimFramesShown,
                (unsigned)(gifAnimFramesShown > 1 ? gifAnimTotalMs / (gifAnimFramesShown - 1) : 0),
                (unsigned)gifAnimSlowestMs, millis() - gifAnimStartedAt, (unsigned)ESP.getFreePsram(),
                (unsigned)ESP.getFreeHeap());
}
