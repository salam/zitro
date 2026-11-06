// Print sizes: the size menu shown after picking a printer, and the A4 page the stick renders for it.
//
// Flow: hold B on an image -> printer list -> A -> size list -> A prints
//       (Zoom opens a percentage picker first). B goes one step back, C steps through the choices.
//
// Every print is a full A4 page rendered on the stick as PWG raster (see pwg_raster.h), so it can
// carry a footnote with the image source and "Printed with https://zitro.spot".
// The stick asks the printer which resolutions and colour types it takes and tries them in turn
// when a page is rejected. If none works, the printer gets the image file as before (no layout,
// no footnote). Printers without PWG raster get that directly.

#include "pwg_raster.h"

#define PRINT_LAYOUT_FIT        0  // whole picture, as large as fits
#define PRINT_LAYOUT_FILL       1  // fills the page, edges cropped
#define PRINT_LAYOUT_HALF_TEXT  2  // picture in the top half, answer text below
#define PRINT_LAYOUT_HALF       3  // picture in the top half only
#define PRINT_LAYOUT_ZOOM       4  // Fit size times a percentage
#define PRINT_LAYOUT_COUNT      5

static const char* const PRINT_LAYOUT_LABELS[PRINT_LAYOUT_COUNT] = {
  "Fit to page", "Fill page", "Half + text", "Half page", "Zoom"
};
static const int PRINT_ZOOM_STEPS[] = {10, 25, 33, 50, 66, 75, 100, 125, 150, 200};
static const int PRINT_ZOOM_STEP_COUNT = sizeof(PRINT_ZOOM_STEPS) / sizeof(PRINT_ZOOM_STEPS[0]);

int printLayout = PRINT_LAYOUT_FIT;
int printZoomIndex = 3;  // 50 %
static bool printPrefsLoaded = false;

static void loadPrintPrefs() {
  if (printPrefsLoaded) return;
  Preferences p;
  p.begin("print", true);
  printLayout = clampInt(p.getInt("layout", PRINT_LAYOUT_FIT), 0, PRINT_LAYOUT_COUNT - 1);
  printZoomIndex = clampInt(p.getInt("zoom", 3), 0, PRINT_ZOOM_STEP_COUNT - 1);
  p.end();
  printPrefsLoaded = true;
}

static void savePrintPrefs() {
  Preferences p;
  p.begin("print", false);
  p.putInt("layout", printLayout);
  p.putInt("zoom", printZoomIndex);
  p.end();
}

static String printLayoutLabel(int layout) {
  if (layout == PRINT_LAYOUT_ZOOM) return "Zoom " + String(PRINT_ZOOM_STEPS[printZoomIndex]) + "%";
  return PRINT_LAYOUT_LABELS[layout];
}

// ---------- Menu state ----------

// Called when the printer list opens
void printMenuReset() {
  loadPrintPrefs();
  printMenuStep = PRINT_STEP_PRINTER;
}

// Button C: next printer, size or zoom value
void printMenuNext() {
  if (printMenuStep == PRINT_STEP_PRINTER) {
    if (printerCount > 0) selectedPrinterIndex = (selectedPrinterIndex + 1) % printerCount;
  } else if (printMenuStep == PRINT_STEP_SIZE) {
    printLayout = (printLayout + 1) % PRINT_LAYOUT_COUNT;
  } else {
    printZoomIndex = (printZoomIndex + 1) % PRINT_ZOOM_STEP_COUNT;
  }
}

// Button A: next step. Returns true when the page should print now.
bool printMenuConfirm() {
  if (printMenuStep == PRINT_STEP_PRINTER) {
    printMenuStep = PRINT_STEP_SIZE;
    return false;
  }
  if (printMenuStep == PRINT_STEP_SIZE && printLayout == PRINT_LAYOUT_ZOOM) {
    printMenuStep = PRINT_STEP_ZOOM;
    return false;
  }
  savePrintPrefs();
  printMenuStep = PRINT_STEP_PRINTER;
  return true;
}

// Button B: one step back. Returns false when already at the printer list (= leave printing).
bool printMenuBack() {
  if (printMenuStep == PRINT_STEP_ZOOM) {
    printMenuStep = PRINT_STEP_SIZE;
    return true;
  }
  if (printMenuStep == PRINT_STEP_SIZE) {
    printMenuStep = PRINT_STEP_PRINTER;
    return true;
  }
  return false;
}

// ---------- Menu screens ----------

// Small A4 page pictogram showing where the picture goes. (x, y) top-left, w x h page size.
static void drawLayoutPictogram(M5Canvas &canvas, int layout, int x, int y, int w, int h,
                                uint16_t paper, uint16_t ink, uint16_t picture, int zoomPercent) {
  canvas.fillRect(x, y, w, h, paper);
  canvas.drawRect(x, y, w, h, ink);
  int m = w >= 30 ? 3 : 2;  // page margin
  int ix = x + m, iy = y + m, iw = w - 2 * m, ih = h - 2 * m;
  // The picture in the pictograms is landscape 4:3
  auto fitRect = [&](int ax, int ay, int aw, int ah, int percent) {
    int rw = aw, rh = aw * 3 / 4;
    if (rh > ah) { rh = ah; rw = ah * 4 / 3; }
    rw = rw * percent / 100;
    rh = rh * percent / 100;
    int cw = rw < aw ? rw : aw;
    int chh = rh < ah ? rh : ah;
    if (cw < 2) cw = 2;
    if (chh < 2) chh = 2;
    canvas.fillRect(ax + (aw - cw) / 2, ay + (ah - chh) / 2, cw, chh, picture);
  };
  int footH = h >= 40 ? 2 : 1;
  int footY = y + h - m - footH;
  canvas.fillRect(ix + iw / 4, footY, iw / 2, footH, ink);  // footnote
  ih -= footH + 1;
  switch (layout) {
    case PRINT_LAYOUT_FIT:
      fitRect(ix, iy, iw, ih, 100);
      break;
    case PRINT_LAYOUT_FILL:
      canvas.fillRect(ix, iy, iw, ih, picture);
      break;
    case PRINT_LAYOUT_HALF_TEXT:
    case PRINT_LAYOUT_HALF: {
      int halfH = h / 2 - m;
      fitRect(ix, iy, iw, halfH, 100);
      if (layout == PRINT_LAYOUT_HALF_TEXT) {
        int lineGap = h >= 40 ? 4 : 3;
        int ty = y + h / 2 + 1;
        for (int i = 0; i < 3 && ty < footY - 2; i++, ty += lineGap) {
          int lw = (i == 2) ? iw * 3 / 5 : iw;
          canvas.fillRect(ix, ty, lw, 1, ink);
        }
      }
      break;
    }
    case PRINT_LAYOUT_ZOOM:
      fitRect(ix, iy, iw, ih, zoomPercent);
      break;
  }
}

// Landscape: list of the five sizes, the selected one on a white bar
static void drawSizeListLandscape(M5Canvas &canvas, int rotation) {
  bool buttonsLeft = (rotation == 3);
  int cw = canvas.width();
  int ch = canvas.height();
  canvas.setFreeFont(FontManager::getFont12pt());
  canvas.setTextDatum(ML_DATUM);

  const int rowH = 30;
  const int maxRows = 3;
  int listX = buttonsLeft ? 22 : 10;
  int listW = cw - 32;
  int listTop = (ch - maxRows * rowH) / 2;

  int first = printLayout - 1;
  if (first > PRINT_LAYOUT_COUNT - maxRows) first = PRINT_LAYOUT_COUNT - maxRows;
  if (first < 0) first = 0;

  for (int r = 0; r < maxRows; r++) {
    int i = first + r;
    int rowY = listTop + r * rowH;
    bool selected = (i == printLayout);
    if (selected) canvas.fillRoundRect(listX, rowY + 2, listW, rowH - 4, 7, TFT_WHITE);

    // Page pictogram, 16 x 22
    int iconX = listX + 10;
    int iconY = rowY + (rowH - 22) / 2;
    drawLayoutPictogram(canvas, i, iconX, iconY, 16, 22,
                        selected ? TFT_WHITE : TFT_BLACK,
                        selected ? TFT_BLACK : (uint16_t)0xBDF7,
                        selected ? PRINT_GREY : (uint16_t)0x8410,
                        PRINT_ZOOM_STEPS[printZoomIndex]);

    int textX = iconX + 16 + 11;
    canvas.setTextColor(selected ? TFT_BLACK : TFT_WHITE);
    canvas.drawString(fitToWidth(canvas, printLayoutLabel(i), listX + listW - 8 - textX), textX, rowY + rowH / 2);
  }

  // Scrollbar: five sizes, three rows
  int trackX = buttonsLeft ? cw - 6 : 3;
  int trackH = maxRows * rowH - 4;
  int thumbH = trackH * maxRows / PRINT_LAYOUT_COUNT;
  int thumbY = listTop + 2 + (trackH - thumbH) * first / (PRINT_LAYOUT_COUNT - maxRows);
  canvas.fillRoundRect(trackX, listTop + 2, 3, trackH, 1, PRINT_DARK_GREY);
  canvas.fillRoundRect(trackX, thumbY, 3, thumbH, 1, TFT_WHITE);
}

// Portrait: one size per white card with a large page pictogram, dots show which of five
static void drawSizeCardPortrait(M5Canvas &canvas, int rotation, bool zoomPicker) {
  int cw = canvas.width();
  int ch = canvas.height();
  bool aOnTop = (rotation == 2);

  const int cardX = 18;
  const int cardW = cw - 2 * cardX;
  const int cardH = 150;
  int cardY = (ch - cardH) / 2;
  canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

  const int pageW = 46, pageH = 65;
  int pageY = cardY + 14;
  drawLayoutPictogram(canvas, printLayout, (cw - pageW) / 2, pageY, pageW, pageH,
                      TFT_WHITE, TFT_BLACK, PRINT_GREY, PRINT_ZOOM_STEPS[printZoomIndex]);

  canvas.setTextColor(TFT_BLACK);
  canvas.setTextDatum(TC_DATUM);
  int textY = pageY + pageH + 12;
  if (zoomPicker) {
    canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawString(String(PRINT_ZOOM_STEPS[printZoomIndex]) + "%", cw / 2, textY);
  } else {
    canvas.setFreeFont(FontManager::getFont12pt());
    String lines[2];
    int n = wrapToLines(canvas, PRINT_LAYOUT_LABELS[printLayout], cardW - 10, lines, 2);
    int lineH = canvas.fontHeight() + 2;
    for (int i = 0; i < n; i++) canvas.drawString(lines[i], cw / 2, textY + i * lineH);
  }

  if (!zoomPicker) {
    const int spacing = 11;
    int dotsY = aOnTop ? ch - 26 : 26;
    int startX = cw / 2 - (PRINT_LAYOUT_COUNT - 1) * spacing / 2;
    for (int i = 0; i < PRINT_LAYOUT_COUNT; i++) {
      int dx = startX + i * spacing;
      if (i == printLayout) {
        canvas.fillCircle(dx, dotsY, 3, TFT_WHITE);
      } else {
        canvas.drawCircle(dx, dotsY, 3, PRINT_GREY);
      }
    }
  }
}

// Landscape zoom picker: page pictogram on a white card, big percentage next to it
static void drawZoomPickerLandscape(M5Canvas &canvas, int rotation) {
  bool buttonsLeft = (rotation == 3);
  int cw = canvas.width();
  int ch = canvas.height();
  int cardX = buttonsLeft ? 22 : 10;
  int cardW = cw - 32;
  int cardY = 18, cardH = ch - 36;
  canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

  const int pageW = 52, pageH = 74;
  int pageX = cardX + 16;
  int pageY = cardY + (cardH - pageH) / 2;
  drawLayoutPictogram(canvas, PRINT_LAYOUT_ZOOM, pageX, pageY, pageW, pageH,
                      TFT_WHITE, TFT_BLACK, PRINT_GREY, PRINT_ZOOM_STEPS[printZoomIndex]);

  int textX = pageX + pageW + (cardX + cardW - pageX - pageW) / 2;
  canvas.setTextColor(TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFreeFont(FontManager::getFont12pt());
  canvas.drawString("Zoom", textX, ch / 2 - 18);
  canvas.setFont(&fonts::FreeSansBold18pt7b);
  canvas.drawString(String(PRINT_ZOOM_STEPS[printZoomIndex]) + "%", textX, ch / 2 + 12);
}

// Size list or zoom picker in the orientation the stick is held (called by displayPrinterSelection)
void drawPrintSizeMenu() {
  int rotation = printUiRotation();
  M5.Display.setRotation(rotation);

  M5Canvas canvas(&M5.Display);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_BLACK);

  bool zoomPicker = (printMenuStep == PRINT_STEP_ZOOM);
  bool portrait = isPortraitRotation(rotation);
  if (portrait) {
    drawSizeCardPortrait(canvas, rotation, zoomPicker);
  } else if (zoomPicker) {
    drawZoomPickerLandscape(canvas, rotation);
  } else {
    drawSizeListLandscape(canvas, rotation);
  }
  // A: print (or open the zoom picker), B: back, C: next
  drawButtonHints(canvas, rotation, HINT_OK, HINT_CANCEL, HINT_NEXT);

  if (!portrait) drawBatteryIndicator(canvas);
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// ---------- Page content ----------

// "Image: ..." credit for the current picture, in the device language
static String printImageCredit(bool german) {
  String prefix = german ? "Bild: " : "Image: ";
  if (isWolframImage) return prefix + "Wolfram|Alpha";
  if (currentImageUrl.length() == 0) return prefix + (german ? "KI-generiert" : "AI-generated");

  // Wikipedia/Wikimedia: link the file's description page, which names author and licence.
  // .../wikipedia/<site>/thumb/a/ab/<file>/330px-<file>  or  .../wikipedia/<site>/a/ab/<file>
  String url = currentImageUrl;
  int wp = url.indexOf("/wikipedia/");
  if (url.indexOf("upload.wikimedia.org") >= 0 && wp >= 0) {
    int siteStart = wp + 11;
    int siteEnd = url.indexOf('/', siteStart);
    String site = siteEnd > 0 ? url.substring(siteStart, siteEnd) : "";
    String path = siteEnd > 0 ? url.substring(siteEnd + 1) : "";
    bool thumb = path.startsWith("thumb/");
    if (thumb) path = path.substring(6);
    // path is now a/ab/<file>[/<size>px-<file>]
    int s1 = path.indexOf('/');
    int s2 = s1 >= 0 ? path.indexOf('/', s1 + 1) : -1;
    if (s2 > 0) {
      String file = path.substring(s2 + 1);
      int s3 = file.indexOf('/');
      if (s3 > 0) file = file.substring(0, s3);
      String host = (site == "commons") ? "commons.wikimedia.org" : site + ".wikipedia.org";
      String name = (site == "commons") ? "Wikimedia Commons" : "Wikipedia";
      return prefix + name + ", https://" + host + "/wiki/File:" + file;
    }
  }
  return prefix + url;
}

#define PRINT_SITE_URL "https://zitro.spot"
#define PRINT_NBSP "\xC2\xA0"

// gap: non-breaking spaces reserved for the lemon, always stays attached to the URL
static String printFootnote(const String &gap) {
  Preferences sp;
  sp.begin("settings", true);
  bool german = sp.getString("language", "DE") != "EN";
  sp.end();
  return printImageCredit(german) + "  \xC2\xB7  " + (german ? "Gedruckt mit " : "Printed with ") +
         gap + PRINT_SITE_URL;
}

// The on-screen answer is wrapped at 25 characters; join it back into running text
static String printBodyText() {
  String t = response;
  t.replace("\r", "");
  t.replace("\n", " ");
  // Punctuation the print font lacks (it would print as a box)
  t.replace("\xE2\x80\x93", "-");    // en dash
  t.replace("\xE2\x80\x94", "-");    // em dash
  t.replace("\xE2\x80\x98", "'");    // curly quotes
  t.replace("\xE2\x80\x99", "'");
  t.replace("\xE2\x80\x9C", "\"");
  t.replace("\xE2\x80\x9D", "\"");
  t.replace("\xE2\x80\x9E", "\"");
  t.replace("\xE2\x80\xA6", "...");  // ellipsis
  while (t.indexOf("  ") >= 0) t.replace("  ", " ");
  t.trim();
  return t;
}

static void freeTextBitmap(PwgBitmap &b) {
  if (b.bits) free((void*)b.bits);
  b.bits = nullptr;
}

// Copies a 1-bit sprite's top-left w x h into a packed bitmap (set bit = non-black pixel)
static bool packCanvasBits(M5Canvas &canvas, int w, int h, PwgBitmap &out) {
  int stride = (w + 7) / 8;
  uint8_t* bits = (uint8_t*)ps_calloc(stride * h, 1);
  if (!bits) return false;
  for (int y = 0; y < h; y++) {
    uint8_t* row = bits + y * stride;
    for (int x = 0; x < w; x++) {
      if (canvas.readPixel(x, y) != 0) row[x >> 3] |= 0x80 >> (x & 7);
    }
  }
  out.bits = bits;
  out.width = w;
  out.height = h;
  return true;
}

// Word-wraps text in a GFX font and renders it 1 bit per pixel at font size (scaled up on the page).
// Words longer than a line are broken between characters. Returns false when memory runs out.
// Fills bits, width and height of out; the caller sets scale, position and colour.
// underline (optional): the last occurrence of this text gets underlined, and its start is
// returned in underlineX / underlineLine (bitmap pixels / line number).
static bool renderTextBitmap(const String &text, const GFXfont* font, int maxWidth, int maxLines,
                             PwgBitmap &out, const char* underline, int* underlineX, int* underlineLine) {
  M5Canvas measure;
  measure.setFreeFont(font);

  String lines[24];
  if (maxLines > 24) maxLines = 24;
  int count = 0;
  String line;
  int pos = 0;
  int len = text.length();
  while (pos < len && count < maxLines) {
    int sp = text.indexOf(' ', pos);
    if (sp < 0) sp = len;
    String word = text.substring(pos, sp);
    pos = sp + 1;
    String candidate = line.length() ? line + " " + word : word;
    if (measure.textWidth(candidate) <= maxWidth) {
      line = candidate;
      continue;
    }
    if (line.length()) {
      lines[count++] = line;
      line = "";
      if (count >= maxLines) break;
    }
    // Word alone is too wide: break it at character boundaries (never inside a UTF-8 sequence)
    while (measure.textWidth(word) > maxWidth && count < maxLines) {
      int cut = word.length() - 1;
      while (cut > 1 && (measure.textWidth(word.substring(0, cut)) > maxWidth ||
                         ((uint8_t)word[cut] & 0xC0) == 0x80)) {
        cut--;
      }
      lines[count++] = word.substring(0, cut);
      word = word.substring(cut);
    }
    line = word;
  }
  if (line.length() && count < maxLines) lines[count++] = line;
  if (count == 0) return true;

  int lineH = font->yAdvance;
  M5Canvas canvas;
  canvas.setPsram(true);
  canvas.setColorDepth(1);
  if (!canvas.createSprite(maxWidth, count * lineH)) return false;
  canvas.fillSprite(TFT_BLACK);
  canvas.setFreeFont(font);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(TL_DATUM);
  for (int i = 0; i < count; i++) canvas.drawString(lines[i], 0, i * lineH);
  if (underline) {
    for (int i = count - 1; i >= 0; i--) {
      int idx = lines[i].lastIndexOf(underline);
      if (idx < 0) continue;
      int ux = idx > 0 ? measure.textWidth(lines[i].substring(0, idx)) : 0;
      canvas.drawFastHLine(ux, i * lineH + lineH - 3, measure.textWidth(underline), TFT_WHITE);
      if (underlineX) *underlineX = ux;
      if (underlineLine) *underlineLine = i;
      break;
    }
  }

  // Trim to the widest line to keep the bitmap small
  int usedW = 1;
  for (int i = 0; i < count; i++) {
    int w = measure.textWidth(lines[i]) + 2;
    if (w > usedW) usedW = w;
  }
  if (usedW > maxWidth) usedW = maxWidth;

  bool packed = packCanvasBits(canvas, usedW, count * lineH, out);
  canvas.deleteSprite();
  return packed;
}

// The lemon from the home screen (drawLemon), one 1-bit layer per colour.
// k: page pixels per screen pixel. Spans 21k x 15k, drawn with cx = 10k, cy = 9k.
static bool renderLemonLayer(bool leaf, int k, PwgBitmap &out) {
  const int cx = 10 * k, cy = 9 * k;
  M5Canvas c;
  c.setPsram(true);
  c.setColorDepth(1);
  if (!c.createSprite(21 * k, 15 * k)) return false;
  c.fillSprite(TFT_BLACK);
  if (leaf) {
    c.fillTriangle(cx + k, cy - 5 * k, cx + 8 * k, cy - 9 * k, cx + 6 * k, cy - 4 * k, TFT_WHITE);
  } else {
    c.fillEllipse(cx, cy, 8 * k, 5 * k, TFT_WHITE);
    c.fillTriangle(cx - 10 * k, cy, cx - 7 * k, cy - 2 * k, cx - 7 * k, cy + 2 * k, TFT_WHITE);
    c.fillTriangle(cx + 10 * k, cy, cx + 7 * k, cy - 2 * k, cx + 7 * k, cy + 2 * k, TFT_WHITE);
  }
  bool packed = packCanvasBits(c, 21 * k, 15 * k, out);
  c.deleteSprite();
  return packed;
}

// Decodes imageBuffer into an RGB565 PSRAM sprite, scaled down when it would not fit.
static bool decodePrintImage(M5Canvas &img) {
  bool isJPEG = imageBufferSize >= 2 && imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8;
  bool isPNG = imageBufferSize >= 8 && imageBuffer[0] == 0x89 && imageBuffer[1] == 'P';
  bool isGIF = imageBufferSize >= 6 && imageBuffer[0] == 'G' && imageBuffer[1] == 'I' && imageBuffer[2] == 'F';
  int w = 0, h = 0;
  bool dims = false;
  if (isJPEG) {
    normalizeGrayscaleJpegSampling(imageBuffer, imageBufferSize);
    dims = getJpegDimensions(imageBuffer, imageBufferSize, w, h);
  } else if (isPNG) {
    dims = getPngDimensions(imageBuffer, imageBufferSize, w, h);
  } else if (isGIF) {
    dims = getGifDimensions(imageBuffer, imageBufferSize, w, h);
  }
  if (!dims || w <= 0 || h <= 0) {
    Serial.println("Print: unknown image format or size");
    return false;
  }

  // Leave room for the line buffers, text and the network stack
  long budget = (long)ESP.getMaxAllocPsram() - 300 * 1024;
  if (budget > 1400L * 1024) budget = 1400L * 1024;
  float scale = 1.0f;
  if ((long)w * h * 2 > budget) scale = sqrtf((float)budget / ((float)w * h * 2));
  if (w * scale > 1024) scale = 1024.0f / w;
  if (h * scale > 1024) scale = 1024.0f / h;
  int sw = (int)(w * scale);
  int sh = (int)(h * scale);
  if (sw < 1 || sh < 1) return false;
  Serial.printf("Print: decoding %dx%d image at %dx%d (budget %ld bytes)\n", w, h, sw, sh, budget);

  img.setPsram(true);
  img.setColorDepth(16);
  if (!img.createSprite(sw, sh)) {
    Serial.println("Print: not enough memory for the image");
    return false;
  }
  img.fillSprite(TFT_WHITE);  // transparent PNG areas print white
  bool ok;
  if (isJPEG) {
    ok = img.drawJpg(imageBuffer, imageBufferSize, 0, 0, sw, sh, 0, 0, scale, scale);
  } else if (isPNG) {
    ok = img.drawPng(imageBuffer, imageBufferSize, 0, 0, sw, sh, 0, 0, scale, scale);
  } else {
    ok = drawGifToCanvas(img, imageBuffer, imageBufferSize, 0, 0, scale);
  }
  if (!ok) {
    Serial.println("Print: image decode failed");
    img.deleteSprite();
  }
  return ok;
}

// Puts the picture centred in an area at the given scale (page pixels per image pixel),
// cropping it evenly when it is larger than the area.
static void placePrintImage(PwgImage &im, int ax, int ay, int aw, int ah, float scale) {
  int fullW = (int)(im.width * scale + 0.5f);
  int fullH = (int)(im.height * scale + 0.5f);
  im.dstW = max(1, min(aw, fullW));
  im.dstH = max(1, min(ah, fullH));
  im.cropW = clampInt((int)(im.dstW / scale + 0.5f), 1, im.width);
  im.cropH = clampInt((int)(im.dstH / scale + 0.5f), 1, im.height);
  im.cropX = (im.width - im.cropW) / 2;
  im.cropY = (im.height - im.cropH) / 2;
  im.dstX = ax + (aw - im.dstW) / 2;
  im.dstY = ay + (ah - im.dstH) / 2;
}

// ---------- Sending ----------

// Streams the page to the printer socket in 4 KB blocks. The printer stops reading while its
// spool is full, so a stalled write only fails after a long quiet period.
class PwgClientSink : public PwgSink {
public:
  WiFiClient &client;
  size_t total = 0;
  static const size_t BUF_SIZE = 4096;
  uint8_t *buf;  // on the heap: the loop task stack is only 8 KB
  size_t used = 0;
  explicit PwgClientSink(WiFiClient &c) : client(c), buf((uint8_t *)malloc(BUF_SIZE)) {}
  ~PwgClientSink() { free(buf); }

  bool flush() {
    size_t off = 0;
    unsigned long lastProgress = millis();
    while (off < used) {
      size_t n = client.write(buf + off, used - off);
      if (n > 0) {
        off += n;
        lastProgress = millis();
        lastActivityTime = millis();  // keep the stick awake while the printer pulls data
      } else {
        if (!client.connected() || millis() - lastProgress > 90000) return false;
        delay(20);
      }
    }
    used = 0;
    return true;
  }

  bool write(const uint8_t *data, size_t len) override {
    if (!buf) return false;
    total += len;
    while (len > 0) {
      size_t n = min(len, BUF_SIZE - used);
      memcpy(buf + used, data, n);
      used += n;
      data += n;
      len -= n;
      if (used == BUF_SIZE && !flush()) return false;
    }
    return true;
  }
};

static void* printPsramAlloc(size_t n) { return ps_malloc(n); }

// Reads an HTTP reply and stores up to cap bytes of its body, undoing chunked encoding
// (the Canon TS5100 answers chunked). Skips "100 Continue". Returns the bytes stored.
static int readHttpReply(WiFiClient &client, uint8_t *buf, int cap, unsigned long timeoutMs, int &httpStatus) {
  String head;
  bool inBody = false, chunked = false;
  long remaining = -1;  // Content-Length, -1: until the connection closes
  long chunkLeft = 0;
  String sizeLine;
  int stored = 0;
  httpStatus = 0;
  unsigned long deadline = millis() + timeoutMs;
  while (millis() < deadline && stored < cap) {
    if (!client.available()) {
      if (!client.connected()) break;
      delay(5);
      continue;
    }
    char c = client.read();
    if (!inBody) {
      head += c;
      if (!head.endsWith("\r\n\r\n")) continue;
      httpStatus = head.length() > 12 ? head.substring(9, 12).toInt() : 0;
      if (httpStatus == 100) {
        head = "";
        continue;
      }
      inBody = true;
      String lower = head;
      lower.toLowerCase();
      chunked = lower.indexOf("transfer-encoding: chunked") >= 0;
      int cl = lower.indexOf("content-length:");
      if (!chunked && cl >= 0) {
        remaining = lower.substring(cl + 15).toInt();
        if (remaining == 0) break;
      }
    } else if (chunked) {
      if (chunkLeft > 0) {
        buf[stored++] = (uint8_t)c;
        chunkLeft--;
      } else if (c == '\n') {
        sizeLine.trim();
        if (sizeLine.length() > 0) {  // otherwise: the line end after a chunk
          chunkLeft = strtol(sizeLine.c_str(), nullptr, 16);
          if (chunkLeft == 0) break;  // last chunk
        }
        sizeLine = "";
      } else {
        sizeLine += c;
      }
    } else {
      buf[stored++] = (uint8_t)c;
      if (remaining > 0 && --remaining == 0) break;
    }
  }
  return stored;
}

// Asks the printer which raster pages it takes. Leaves caps empty when it can't tell.
static void queryPwgCaps(PrinterInfo &printer, PwgCaps &caps) {
  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";
  uint8_t ipp[400];
  uint8_t* ptr = ipp;
  *ptr++ = 0x01; *ptr++ = 0x01;  // IPP 1.1
  *ptr++ = 0x00; *ptr++ = 0x0B;  // Get-Printer-Attributes
  *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x01;  // request id
  *ptr++ = 0x01;  // operation attributes
  addIPPAttribute(ptr, 0x47, "attributes-charset", "utf-8");
  addIPPAttribute(ptr, 0x48, "attributes-natural-language", "en");
  addIPPAttribute(ptr, 0x45, "printer-uri", printerUri.c_str());
  addIPPAttribute(ptr, 0x44, "requested-attributes", "pwg-raster-document-resolution-supported");
  addIPPAttribute(ptr, 0x44, "", "pwg-raster-document-type-supported");
  *ptr++ = 0x03;
  int ippLen = ptr - ipp;

  WiFiClient client;
  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("Print: capability query could not connect");
    return;
  }
  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + ":" + String(printer.port) + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String(ippLen) + "\r\n\r\n");
  client.write(ipp, ippLen);

  const int cap = 4096;
  uint8_t* reply = (uint8_t*)malloc(cap);
  if (!reply) {
    client.stop();
    return;
  }
  int httpStatus = 0;
  int len = readHttpReply(client, reply, cap, 8000, httpStatus);
  client.stop();
  bool parsed = httpStatus == 200 && pwgParseCaps(reply, len, caps);
  free(reply);
  Serial.printf("Print: capabilities HTTP %d, %d bytes, %s, %d resolutions, types %s%s%s\n", httpStatus, len,
                parsed ? "parsed" : "not understood", caps.dpiCount, caps.typesKnown ? "" : "unknown ",
                caps.srgb8 ? "srgb_8 " : "", caps.sgray8 ? "sgray_8" : "");
}

// Outcome of one page sent to the printer
#define PWG_SEND_OK       0
#define PWG_SEND_REJECTED 1  // the printer refused this page: another resolution or type may work
#define PWG_SEND_FAILED   2  // not about the page (unreachable, busy, no answer): don't retry

// A rejection means no job exists, so sending another variant can't print twice.
static int classifyPrintReply(int httpStatus, int ippStatus) {
  if (httpStatus == 200 && ippStatus >= 0 && ippStatus < 0x0100) return PWG_SEND_OK;
  if (httpStatus >= 400 && httpStatus < 500) return PWG_SEND_REJECTED;
  if (httpStatus != 200 || ippStatus < 0) return PWG_SEND_FAILED;  // no clear answer: it may be printing
  bool notAllowed = ippStatus >= 0x0401 && ippStatus <= 0x0403;    // forbidden, needs a login
  bool badRequest = ippStatus >= 0x0400 && ippStatus < 0x0500;     // client errors: page or attributes
  bool unsupported = ippStatus == 0x0501;                          // operation not supported
  return !notAllowed && (badRequest || unsupported) ? PWG_SEND_REJECTED : PWG_SEND_FAILED;
}

// Sends one PWG raster page with Content-Length (the Canon TS5100 stops on chunked uploads).
static int sendPwgPrintJob(PrinterInfo &printer, const PwgPage &page) {
  PwgCountingSink counter;
  unsigned long t0 = millis();
  if (!pwgWritePage(counter, page, true, printPsramAlloc)) {
    Serial.println("Print: page encoding failed (memory)");
    return PWG_SEND_REJECTED;  // a smaller page may fit
  }
  Serial.printf("Print: page is %u bytes (counted in %lu ms)\n", (unsigned)counter.total, millis() - t0);

  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";
  uint8_t ipp[512];
  uint8_t* ptr = ipp;
  *ptr++ = 0x01; *ptr++ = 0x01;  // IPP 1.1
  *ptr++ = 0x00; *ptr++ = 0x02;  // Print-Job
  *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x00; *ptr++ = 0x01;  // request id
  *ptr++ = 0x01;  // operation attributes
  addIPPAttribute(ptr, 0x47, "attributes-charset", "utf-8");
  addIPPAttribute(ptr, 0x48, "attributes-natural-language", "en");
  addIPPAttribute(ptr, 0x45, "printer-uri", printerUri.c_str());
  addIPPAttribute(ptr, 0x42, "requesting-user-name", "zitro");
  addIPPAttribute(ptr, 0x42, "job-name", "Zitro print");
  addIPPAttribute(ptr, 0x49, "document-format", "image/pwg-raster");
  *ptr++ = 0x02;  // job attributes
  addIPPAttribute(ptr, 0x44, "media", page.sizeName);
  *ptr++ = 0x03;  // end
  int ippLen = ptr - ipp;

  WiFiClient client;
  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("Print: connection failed");
    return PWG_SEND_FAILED;
  }
  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + ":" + String(printer.port) + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String((unsigned long)(ippLen + counter.total)) + "\r\n");
  client.print("\r\n");

  PwgClientSink sink(client);
  bool sent = sink.write(ipp, ippLen) && pwgWritePage(sink, page, true, printPsramAlloc) && sink.flush();
  Serial.printf("Print: sent %u of %u bytes in %lu ms\n", (unsigned)sink.total,
                (unsigned)(ippLen + counter.total), millis() - t0);
  int httpStatus = 0;
  uint8_t reply[8];
  if (!sent || sink.total != ippLen + counter.total) {
    // A printer that dislikes the page header may answer and hang up mid-upload
    bool hungUp = !client.connected();
    int got = hungUp ? readHttpReply(client, reply, sizeof(reply), 500, httpStatus) : 0;
    client.stop();
    Serial.printf("Print: upload cut off (%s, HTTP %d, %d reply bytes)\n", hungUp ? "printer hung up" : "stalled",
                  httpStatus, got);
    return hungUp ? PWG_SEND_REJECTED : PWG_SEND_FAILED;
  }

  // The IPP status code is in the first 4 bytes of the reply
  int got = readHttpReply(client, reply, 4, 60000, httpStatus);
  client.stop();
  int ippStatus = got >= 4 ? (reply[2] << 8) | reply[3] : -1;
  Serial.printf("Print: HTTP %d, IPP status 0x%04x\n", httpStatus, ippStatus);
  return classifyPrintReply(httpStatus, ippStatus);
}

// Renders the chosen size as one A4 page at this resolution and type, and sends it.
static int printPageAttempt(PrinterInfo &printer, M5Canvas &img, const PwgAttempt &attempt) {
  PwgPage page;
  pwgSetA4(page, attempt.dpi);
  page.grayscale = attempt.grayscale;
  const int W = page.width, H = page.height;
  const int px10mm = attempt.dpi * 100 / 254;  // 236 pixels = 10 mm at 600 dpi
  const int margin = px10mm;            // 10 mm top and sides
  const int bottom = px10mm * 14 / 10;  // 14 mm: some media need 12.7 mm at the bottom
  const int gap = px10mm * 3 / 10;      // 3 mm
  // Text keeps its size on paper: font pixels are scaled up with the resolution (down to 1:1)
  const int textScale = max(1, attempt.dpi / 300);

  page.image.pixels = (const uint16_t*)img.getBuffer();
  page.image.byteSwapped = true;  // LovyanGFX keeps 16-bit sprites in swapped byte order
  page.image.width = img.width();
  page.image.height = img.height();

  // Footnote, small and grey: image credit, "Printed with", lemon, underlined https://zitro.spot
  PwgBitmap foot, body, lemon, leaf;
  const int footScale = textScale;
  const GFXfont* footFont = FontManager::getFont12pt();  // 10 pt glyphs, 2.0 mm lines on paper
  const int lemonK = max(1, attempt.dpi / 200);  // page pixels per screen pixel of the lemon
  const int lemonW = 21 * lemonK, lemonH = 15 * lemonK;
  const int lemonPad = attempt.dpi / 50;  // page pixels between lemon and URL
  String lemonGap;
  {
    M5Canvas m;
    m.setFreeFont(footFont);
    int spaceW = m.textWidth(PRINT_NBSP);
    if (spaceW < 1) spaceW = 5;
    int need = (lemonW + lemonPad + footScale - 1) / footScale;
    for (int i = 0; i * spaceW < need; i++) lemonGap += PRINT_NBSP;
  }
  int urlX = 0, urlLine = 0;
  bool ok = renderTextBitmap(printFootnote(lemonGap), footFont, (W - 2 * margin) / footScale, 3, foot,
                             PRINT_SITE_URL, &urlX, &urlLine);
  int footY = H - bottom - foot.height * footScale;
  int contentBottom = footY - gap * 2;
  if (ok && foot.bits) {
    PwgBitmap &b = page.bitmaps[page.bitmapCount++];
    b = foot;
    b.scale = footScale;
    b.x = margin;
    b.y = footY;
    b.r = b.g = b.b = 0x66;

    // Lemon in the gap left of the URL, centred on that line
    int lineH = footFont->yAdvance * footScale;
    int lemonX = margin + urlX * footScale - lemonW - lemonPad;
    int lemonY = footY + urlLine * lineH + (lineH - lemonH) / 2;
    ok = renderLemonLayer(false, lemonK, lemon) && renderLemonLayer(true, lemonK, leaf);
    if (ok) {
      PwgBitmap &bodyLayer = page.bitmaps[page.bitmapCount++];
      bodyLayer = lemon;
      bodyLayer.x = lemonX;
      bodyLayer.y = lemonY;
      bodyLayer.r = 0xF2; bodyLayer.g = 0xB7; bodyLayer.b = 0x05;  // citrus, as on the home screen
      PwgBitmap &leafLayer = page.bitmaps[page.bitmapCount++];
      leafLayer = leaf;
      leafLayer.x = lemonX;
      leafLayer.y = lemonY;
      leafLayer.r = 0x4A; leafLayer.g = 0x82; leafLayer.b = 0x31;
    }
  }

  int areaX = margin, areaY = margin, areaW = W - 2 * margin, areaH = contentBottom - margin;
  int halfH = H / 2 - gap - margin;
  float fitScale = min((float)areaW / page.image.width, (float)areaH / page.image.height);
  float fillScale = max((float)areaW / page.image.width, (float)areaH / page.image.height);
  float halfScale = min((float)areaW / page.image.width, (float)halfH / page.image.height);

  switch (printLayout) {
    case PRINT_LAYOUT_FILL:
      placePrintImage(page.image, areaX, areaY, areaW, areaH, fillScale);
      break;
    case PRINT_LAYOUT_HALF_TEXT:
    case PRINT_LAYOUT_HALF:
      placePrintImage(page.image, areaX, areaY, areaW, halfH, halfScale);
      break;
    case PRINT_LAYOUT_ZOOM:
      placePrintImage(page.image, areaX, areaY, areaW, areaH,
                      fitScale * PRINT_ZOOM_STEPS[printZoomIndex] / 100.0f);
      break;
    default:
      placePrintImage(page.image, areaX, areaY, areaW, areaH, fitScale);
      break;
  }

  // Answer text in the lower half, starting just below the middle
  if (ok && printLayout == PRINT_LAYOUT_HALF_TEXT) {
    const int bodyScale = textScale;
    const GFXfont* bodyFont = FontManager::getFont16pt();  // 14 pt glyphs, 2.7 mm lines on paper
    int bodyY = H / 2 + gap;
    int maxLines = (contentBottom - bodyY) / (bodyFont->yAdvance * bodyScale);
    String text = printBodyText();
    if (text.length() > 0 && maxLines > 0) {
      ok = renderTextBitmap(text, bodyFont, (W - 2 * margin) / bodyScale, maxLines, body, nullptr, nullptr, nullptr);
      if (ok && body.bits) {
        PwgBitmap &b = page.bitmaps[page.bitmapCount++];
        b = body;
        b.scale = bodyScale;
        b.x = margin;
        b.y = bodyY;
      }
    }
  }

  Serial.printf("Print: %d dpi %s, layout %d, image %dx%d -> %dx%d at (%d,%d), crop %dx%d+%d+%d\n", attempt.dpi,
                attempt.grayscale ? "grey" : "colour", printLayout, page.image.width, page.image.height,
                page.image.dstW, page.image.dstH, page.image.dstX, page.image.dstY, page.image.cropW,
                page.image.cropH, page.image.cropX, page.image.cropY);

  int result = PWG_SEND_REJECTED;  // out of memory: a smaller page may fit
  if (ok) {
    result = sendPwgPrintJob(printer, page);
  } else {
    Serial.println("Print: not enough memory for the text");
  }
  freeTextBitmap(foot);
  freeTextBitmap(body);
  freeTextBitmap(lemon);
  freeTextBitmap(leaf);
  return result;
}

// Prints the chosen size, trying what the printer offers until one page is accepted.
int printComposedPage(PrinterInfo &printer) {
  loadPrintPrefs();

  PwgCaps caps;
  queryPwgCaps(printer, caps);
  PwgAttempt attempts[4];
  int count = pwgPlanAttempts(caps, attempts, 4);
  if (count == 0) {
    Serial.println("Print: the printer offers no raster page we can make");
    return PRINT_COMPOSE_UNAVAILABLE;
  }

  M5Canvas img;
  if (!decodePrintImage(img)) return PRINT_COMPOSE_UNAVAILABLE;

  int result = PRINT_COMPOSE_UNAVAILABLE;  // all rejected: the image file is the last resort
  for (int i = 0; i < count; i++) {
    int sent = printPageAttempt(printer, img, attempts[i]);
    if (sent == PWG_SEND_OK) {
      result = PRINT_COMPOSE_OK;
      break;
    }
    if (sent == PWG_SEND_FAILED) {
      result = PRINT_COMPOSE_FAILED;
      break;
    }
    Serial.printf("Print: %d dpi %s rejected\n", attempts[i].dpi, attempts[i].grayscale ? "grey" : "colour");
    lastActivityTime = millis();
  }
  img.deleteSprite();
  return result;
}

// ---------- Serial test commands (development) ----------
//   printtext <text>                        set the answer text
//   printimg <url>                          download a picture to print
//   printtest <layout> <zoomIndex> <ip>:<port>  print the current picture like the print menu does
//   printtest caps <ip>:<port>              show what a printer offers, prints nothing
//   printtest show [offset] [cover]         show the current picture, optionally scrolled or zoomed
//   printtest gif [stop]                    GIF player numbers: frame, speed, memory; or stop it
//   printtest wolfram <question>            fetch a Wolfram|Alpha picture
// Lets a computer pose as the printer to check pages without paper (see tests/pwg/fake_printer.py).
void runPrintTestCommand(const String &line) {
  if (line.startsWith("printtext ")) {  // answer text for "Half + text"
    response = wordWrap(line.substring(10), 25);
    Serial.println("@@PRINTTEXT OK");
    return;
  }
  if (line.startsWith("printimg ")) {
    String url = line.substring(9);
    url.trim();
    hasImage = downloadImage(url);
    currentImageUrl = hasImage ? url : "";
    isWolframImage = false;
    Serial.printf("@@PRINTIMG %s\n", hasImage ? "OK" : "FAIL");
    return;
  }
  String args = line.substring(10);
  args.trim();
  if (args == "show" || args.startsWith("show ")) {
    currentDisplayMode = SHOW_IMAGE;
    imageZoomMode = args.endsWith("cover") ? COVER : CONTAIN;
    wolframScrollOffset = args.length() > 5 ? args.substring(5).toInt() : 0;
    resetToActiveState();  // like a real redraw from turning the stick
    displayImage(currentImageUrl.length() == 0);
    Serial.printf("@@PRINTTEST show OK, scroll %d of %d\n", wolframScrollOffset, wolframMaxScrollOffset);
    return;
  }
  if (args == "gif" || args == "gif stop") {
    if (args == "gif stop") gifAnimStop();
    gifAnimPrintStats();
    return;
  }
  if (args.startsWith("wolfram ")) {
    hasImage = fetchWolframAlphaImage(args.substring(8));
    isWolframImage = hasImage;
    currentImageUrl = "";
    Serial.printf("@@PRINTTEST wolfram %s, %d bytes\n", hasImage ? "OK" : "FAIL", imageBufferSize);
    return;
  }
  if (args.startsWith("caps ")) {
    int colon = args.lastIndexOf(':');
    PrinterInfo p;
    p.name = "test";
    p.ip.fromString(args.substring(5, colon));
    p.port = args.substring(colon + 1).toInt();
    PwgCaps caps;
    queryPwgCaps(p, caps);
    PwgAttempt attempts[4];
    int count = pwgPlanAttempts(caps, attempts, 4);
    String plan;
    for (int i = 0; i < count; i++) {
      plan += String(attempts[i].dpi) + (attempts[i].grayscale ? " grey, " : " colour, ");
    }
    Serial.println("@@PRINTTEST caps plan: " + plan);
    return;
  }
  // printtest <layout> <zoomIndex> <ip>:<port>
  int s1 = args.indexOf(' ');
  int s2 = s1 > 0 ? args.indexOf(' ', s1 + 1) : -1;
  int colon = args.lastIndexOf(':');
  if (s1 < 0 || s2 < 0 || colon < s2) {
    Serial.println("@@PRINTTEST usage: printtest <layout> <zoomIndex> <ip>:<port>");
    return;
  }
  if (!hasImage || !imageBuffer) {
    Serial.println("@@PRINTTEST FAIL no image");
    return;
  }
  loadPrintPrefs();
  int savedLayout = printLayout, savedZoom = printZoomIndex;
  printLayout = clampInt(args.substring(0, s1).toInt(), 0, PRINT_LAYOUT_COUNT - 1);
  printZoomIndex = clampInt(args.substring(s1 + 1, s2).toInt(), 0, PRINT_ZOOM_STEP_COUNT - 1);
  // Through printImage(), like the menu: format check, capabilities, retries, last resort
  PrinterInfo savedFirst = discoveredPrinters[0];
  int savedCount = printerCount, savedSelected = selectedPrinterIndex;
  discoveredPrinters[0].name = "test";
  discoveredPrinters[0].ip.fromString(args.substring(s2 + 1, colon));
  discoveredPrinters[0].port = args.substring(colon + 1).toInt();
  printerCount = 1;
  selectedPrinterIndex = 0;
  unsigned long t0 = millis();
  bool printed = printImage();
  Serial.printf("@@PRINTTEST %s in %lu ms, free PSRAM %u\n", printed ? "OK" : "FAILED", millis() - t0,
                (unsigned)ESP.getFreePsram());
  discoveredPrinters[0] = savedFirst;
  printerCount = savedCount;
  selectedPrinterIndex = savedSelected;
  printLayout = savedLayout;
  printZoomIndex = savedZoom;
}
