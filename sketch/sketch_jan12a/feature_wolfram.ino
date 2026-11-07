// Wolfram|Alpha results as sharp text and real formulas: the LLM API's text, typeset for this screen.
// The AI only says "this is a calculation"; every number comes from Wolfram, unchanged. When there's nothing
// to read in the answer, fetch() returns false and the Simple API picture takes over, as before.
// See edu_feature.h and docs/features/wolfram.md. Function signatures use plain types only: prototypes of
// .ino functions land before wolfram_layout.h is known.

#include "wolfram_core.h"
#include "wolfram_formula.h"
#include "wolfram_layout.h"

static const int WOLFRAM_MAX_CHARS = 900;  // Far less and the API answers 501 for questions it can do
static const size_t WOLFRAM_PLOT_FILE_LIMIT = 64 * 1024;  // Real plots are about 30 KB
static const uint32_t WOLFRAM_PLOT_PIXEL_LIMIT = 700000;  // Real ones are 903x446. Each pixel takes 2 bytes of PSRAM
static const long WOLFRAM_PLOT_MIN_TIME_MS = 2500;  // Less left than this: the answer is more use now than a plot later
static const int WOLFRAM_PAGE_NUMBER_ROOM = 32;

static WolframDoc* wolframDoc = nullptr;        // From fetch until unload
static WolframLayout* wolframLayout = nullptr;  // While the view is open
static uint16_t* wolframPlot = nullptr;         // From fetch until unload; bytes swapped, as a sprite stores them
static int wolframPlotWidth = 0, wolframPlotHeight = 0;
static uint8_t wolframPage = 0;
static bool wolframRedraw = true;
static String wolframQuery;
static M5Canvas* wolframMeasureCanvas = nullptr;
static uint8_t wolframCanvasFont = 255;

static void wolframUnload() {
  free(wolframDoc);
  wolframDoc = nullptr;
  free(wolframPlot);
  wolframPlot = nullptr;
  wolframPlotWidth = wolframPlotHeight = 0;
  wolframPage = 0;
}

// GET with what the Wolfram API needs and eduHttpGet doesn't do: the key as a header, never in the address.
// True only for HTTP 200 with a complete body inside the deadline. The key is never logged.
static bool wolframHttpGet(const String& url, bool sendKey, unsigned long deadline, char* buffer, size_t capacity,
                           size_t& bodyLength) {
  bodyLength = 0;
  const long remaining = (long)(deadline - millis());
  if (remaining <= 500) return false;
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(max(1L, remaining / 1000));
  HTTPClient http;
  http.setConnectTimeout(remaining);
  http.setTimeout(remaining);
  http.setUserAgent("Zitro");
  http.useHTTP10(true);  // No chunked replies, so the body can be read directly
  if (!http.begin(client, url)) return false;
  if (sendKey) http.addHeader("Authorization", "Bearer " WOLFRAM_APPID);
  lastActivityTime = millis();
  const int code = http.GET();
  bool ok = false;
  if (code == 200) {
    const int expected = http.getSize();  // -1 if unknown: then the end of the connection ends it
    WiFiClient* stream = http.getStreamPtr();
    bool overflow = false;
    while ((long)(deadline - millis()) > 0) {
      const int available = stream->available();
      if (available > 0) {
        if (bodyLength + available > capacity) {
          overflow = true;
          break;
        }
        const int got = stream->read((uint8_t*)buffer + bodyLength, available);
        if (got > 0) bodyLength += got;
      } else if (!stream->connected()) {
        break;
      } else {
        delay(5);
      }
      if (expected >= 0 && (int)bodyLength >= expected) break;
    }
    const bool inTime = (long)(deadline - millis()) > 0;
    ok = !overflow && inTime && bodyLength > 0 && (expected < 0 || (int)bodyLength == expected);
  } else {
    // 501: Wolfram couldn't read the question. 403: bad key. Either way the picture gets its turn.
    Serial.printf("Wolfram LLM API: HTTP %d\n", code);
  }
  http.end();
  lastActivityTime = millis();
  return ok;
}

// Loads the plot and shrinks it to the screen by averaging, so its thin lines stay lines. Any trouble: no plot,
// and the answer stands without it.
static void wolframLoadPlot(unsigned long deadline) {
  if (wolframDoc->plotUrl.len == 0 || (long)(deadline - millis()) < WOLFRAM_PLOT_MIN_TIME_MS) return;
  const String url = String(wolframDoc->text + wolframDoc->plotUrl.start).substring(0, wolframDoc->plotUrl.len);
  uint8_t* file = (uint8_t*)ps_malloc(WOLFRAM_PLOT_FILE_LIMIT);
  if (!file) return;
  size_t length = 0;
  uint32_t width = 0, height = 0;
  if (wolframHttpGet(url, false, deadline, (char*)file, WOLFRAM_PLOT_FILE_LIMIT, length) &&
      wolframPngSize(file, length, width, height) && (uint64_t)width * height <= WOLFRAM_PLOT_PIXEL_LIMIT) {
    M5Canvas full(&M5.Display);
    full.setPsram(true);
    full.setColorDepth(16);
    if (full.createSprite(width, height)) {
      if (full.drawPng(file, length, 0, 0)) {
        int fitWidth = 0, fitHeight = 0;
        wolframFitSize(width, height, WIDTH, HEIGHT - EDU_FOOTER_HEIGHT, fitWidth, fitHeight);
        wolframPlot = (uint16_t*)ps_malloc((size_t)fitWidth * fitHeight * sizeof(uint16_t));
        if (wolframPlot) {
          wolframShrink565((const uint16_t*)full.getBuffer(), width, height, wolframPlot, fitWidth, fitHeight);
          wolframPlotWidth = fitWidth;
          wolframPlotHeight = fitHeight;
        }
      }
      full.deleteSprite();
    }
  }
  free(file);
  Serial.printf("Wolfram plot: %s, %u x %u, %u bytes\n", wolframPlot ? "ready" : "none", (unsigned)width,
                (unsigned)height, (unsigned)length);
}

// Looks the English question up. The label shows what Wolfram understood, so a misreading is visible.
static bool wolframFetch(const char* query) {
  wolframUnload();
  if (strlen(WOLFRAM_APPID) == 0) return false;
  if (WiFi.status() != WL_CONNECTED && !reconnectWiFiNow()) return false;
  const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
  Preferences prefs;
  prefs.begin("settings", true);
  const String language = prefs.getString("language", "DE");
  prefs.end();

  char* body = (char*)ps_malloc(WOLFRAM_BODY_CAPACITY);
  wolframDoc = (WolframDoc*)ps_malloc(sizeof(WolframDoc));
  bool ok = body && wolframDoc;
  size_t length = 0;
  if (ok) {
    const String url = "https://www.wolframalpha.com/api/v1/llm-api?input=" + eduUrlEncode(query) +
                       "&maxchars=" + String(WOLFRAM_MAX_CHARS) + "&units=metric";
    ok = wolframHttpGet(url, true, deadline, body, WOLFRAM_BODY_CAPACITY, length) &&
         wolframParse(body, length, language != "EN", *wolframDoc);
  }
  free(body);
  if (!ok) {
    Serial.printf("Wolfram '%s': no readable answer, %u bytes\n", query, (unsigned)length);
    return false;
  }
  wolframQuery = query;
  const WolframSpan label = wolframDoc->label;
  if (label.len > 0 && wolframSpanCanShow(wolframDoc->text, label)) {
    // A character only drawn elsewhere, like the approx sign, isn't in the bottom line's font: name the source alone
    bool fontHasIt = true;
    for (uint16_t i = 0; i < label.len; i++) {
      if ((uint8_t)wolframDoc->text[label.start + i] >= 0xE0) fontHasIt = false;
    }
    if (fontHasIt) modelLabel = String(wolframDoc->text + label.start).substring(0, label.len);
  }
  if (modelLabel.length() == 0) {
    modelLabel = modelSourceName;  // The bottom line always says Wolfram|Alpha, and only once
    modelSourceName = "";
  }
  wolframLoadPlot(deadline);
  Serial.printf("Wolfram '%s': %u sections, label '%s'\n", query, wolframDoc->sectionCount, modelLabel.c_str());
  return true;
}

static void wolframSetFont(M5Canvas& c, uint8_t font) {
  if (font == wolframCanvasFont) return;
  wolframCanvasFont = font;
  if (font == WOLFRAM_FONT_SMALL) {
    c.setFont(&fonts::DejaVu9);
  } else {
    c.setFreeFont(font == WOLFRAM_FONT_14 ? FontManager::getFont16pt() : FontManager::getFont12pt());
  }
  c.setTextSize(1);
}

static int wolframMeasureText(uint8_t font, const char* text, int length) {
  char piece[256];
  if (length >= (int)sizeof(piece)) length = sizeof(piece) - 1;
  memcpy(piece, text, length);
  piece[length] = 0;
  wolframSetFont(*wolframMeasureCanvas, font);
  return wolframMeasureCanvas->textWidth(piece);
}

static void wolframEnd() {
  free(wolframLayout);
  wolframLayout = nullptr;
  wolframMeasureCanvas = nullptr;
}

static bool wolframBegin(M5Canvas& c) {
  if (!wolframDoc) return false;
  wolframLayout = (WolframLayout*)ps_malloc(sizeof(WolframLayout));
  WolframScratch* scratch = (WolframScratch*)ps_malloc(sizeof(WolframScratch));
  bool ok = wolframLayout && scratch;
  if (ok) {
    wolframMeasureCanvas = &c;
    wolframCanvasFont = 255;
    ok = wolframLayoutBuild(*wolframDoc, wolframPlot != nullptr, wolframMeasureText, *wolframLayout, *scratch);
  }
  free(scratch);
  if (!ok) {
    wolframEnd();
    return false;
  }
  if (wolframPage >= wolframLayout->pageCount) wolframPage = 0;
  wolframRedraw = true;
  return true;
}

static void wolframDrawOps(M5Canvas& c, uint16_t first, uint16_t count) {
  const uint16_t colors[3] = {TFT_WHITE, TFT_LIGHTGREY, c.color565(0xF2, 0xB7, 0x05)};
  char piece[256];
  for (uint16_t i = 0; i < count; i++) {
    const WolframOp& op = wolframLayout->ops[first + i];
    const uint16_t color = colors[op.color];
    if (op.type == WOLFRAM_OP_LINE) {
      c.drawLine(op.x, op.y, (int16_t)op.a, (int16_t)op.b, color);
      continue;
    }
    const size_t length = op.b < sizeof(piece) ? op.b : sizeof(piece) - 1;
    memcpy(piece, wolframDoc->text + op.a, length);
    piece[length] = 0;
    wolframSetFont(c, op.font);
    c.setTextColor(color);
    c.drawString(piece, op.x, op.y);
  }
}

// One page, drawn once: nothing here moves, so the sprite keeps it until a button changes the page
static void wolframFrame(M5Canvas& c, float dt) {
  (void)dt;
  if (modelViewRedraw) wolframRedraw = true;  // The button tabs went away
  if (!wolframRedraw || !wolframLayout || !wolframDoc || wolframPage >= wolframLayout->pageCount) return;
  wolframRedraw = false;
  const WolframPage& page = wolframLayout->pages[wolframPage];
  c.fillSprite(TFT_BLACK);
  wolframCanvasFont = 255;
  c.setTextDatum(textdatum_t::baseline_left);
  if (page.plot && wolframPlot) {
    c.pushImage((WIDTH - wolframPlotWidth) / 2, (HEIGHT - EDU_FOOTER_HEIGHT - wolframPlotHeight) / 2, wolframPlotWidth,
                wolframPlotHeight, (const lgfx::swap565_t*)wolframPlot);
  } else {
    wolframDrawOps(c, page.titleFirst, page.titleCount);
    wolframDrawOps(c, page.noteFirst, page.noteCount);
    wolframDrawOps(c, page.first, page.count);
  }
  const bool several = wolframLayout->pageCount > 1;
  eduDrawFooter(c, several ? WOLFRAM_PAGE_NUMBER_ROOM : 0);
  if (several) {
    char number[8];
    snprintf(number, sizeof(number), "%u/%u", wolframPage + 1, wolframLayout->pageCount);
    c.setFont(&fonts::DejaVu9);
    c.setTextSize(1);
    c.setTextColor(TFT_LIGHTGREY);
    c.setTextDatum(textdatum_t::baseline_right);
    c.drawString(number, WIDTH - 2, HEIGHT - 4);
  }
}

// Button B: one page back
static void wolframButtonB() {
  if (wolframLayout && wolframPage > 0) {
    wolframPage--;
    wolframRedraw = true;
  }
}

// Button C: the next page. After the last one, back to the text, and from there back to the start.
static bool wolframButtonC() {
  if (!wolframLayout) return false;
  if (wolframPage + 1 >= wolframLayout->pageCount) {
    wolframPage = 0;
    return false;
  }
  wolframPage++;
  wolframRedraw = true;
  return true;
}

// B: one page back, except on the first. C: next page, or the written answer after the last.
static void wolframHints(int& kindB, int& kindC) {
  kindB = wolframPage > 0 ? HINT_BACK : HINT_NONE;
  kindC = (wolframLayout && wolframPage + 1 < wolframLayout->pageCount) ? HINT_NEXT : HINT_TEXT;
}

// The question has been put into English already. False: no text result, the caller shows the picture.
// Longer questions than a model reference holds take the picture too: they couldn't come back after sleep.
bool wolframLookUp(const String& englishQuery) {
  ModelRef ref;
  const String text = "wolfram:" + englishQuery;
  return parseModelRef(text.c_str(), ref) && fetchModel(ref);
}

// Printing needs a picture. Holding Button B on a text result loads Wolfram's own, as Zitro always did for these,
// and it replaces the text result. If that fails, the text result stays as it was. False then.
bool wolframSwitchToPicture() {
  if (!hasModel || activeFeature != &FEATURE_WOLFRAM) return false;
  const bool wasInView = currentDisplayMode == SHOW_MODEL;
  const String query = wolframQuery;
  leaveModelView();  // TLS needs the memory
  drawScreen("Calculating...");
  if (audioBuffer) {
    free(audioBuffer);
    audioBuffer = nullptr;
  }
  if (!fetchWolframSimpleImage(query)) {
    Serial.println("Wolfram picture for printing: failed");
    drawPrintStatus("No picture", PRINT_BADGE_FAIL);
    delay(2000);
    if (wasInView) {
      modelViewShow();
    } else {
      drawScreen(response);
    }
    return false;
  }
  eduUnloadActive();
  hasModel = false;
  modelRefText = "";
  hasImage = true;
  isWolframImage = true;
  currentImageUrl = "";
  wolframScrollOffset = 0;
  imageZoomMode = CONTAIN;
  currentDisplayMode = SHOW_IMAGE;
  updateGyroPower();
  displayImage(true);
  return true;
}

const EduFeature FEATURE_WOLFRAM = {"wolfram",
                                    "Wolfram|Alpha",
                                    nullptr,  // The AI never asks for this: is_wolfram does, as before
                                    false,
                                    wolframFetch,
                                    wolframUnload,
                                    wolframBegin,
                                    wolframFrame,
                                    wolframEnd,
                                    wolframButtonB,
                                    wolframButtonC,
                                    nullptr,
                                    nullptr,
                                    wolframHints};
