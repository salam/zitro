// Function graphs: the AI names the expression, every point is computed here. See edu_feature.h.
// Turning the stick moves along the curve. Button B zooms (default handler, modelZoomStep),
// Button C goes back to the text.

#include "graph_core.h"

// Half the visible width of x, by modelZoomStep: normal, closer, far. Back at normal it also recentres.
static const double GRAPH_HALF_WIDTHS[3] = {5, 1, 20};
static const float GRAPH_COLUMNS_PER_DEGREE = 3.0f;  // A quarter turn crosses the screen more than once
static const float GRAPH_TILT_SIGN = 1.0f;  // Set from the molecule view's turning sign; flip if it runs backwards
static const unsigned long GRAPH_FRAME_BUDGET_US = 15000;  // Computing per frame; the rest waits for the next
static const int GRAPH_DOT_COLUMN = GRAPH_COLUMNS / 2;
static const int GRAPH_MAX_X_TICKS = 5;
static const int GRAPH_MAX_Y_TICKS = 4;
static const int GRAPH_TICK_LIMIT = 12;  // Safety net for the tick loops

static GraphProgram graphProgram;
static GraphSamples* graphSamples = nullptr;
static GraphYView graphYView = {-1, 1, -1, 1, false};
static double graphStartCenter = 0;
static double graphCenter = 0;
static uint8_t graphAppliedZoom = 0;
static float graphPanCarry = 0;  // Part of a column not moved yet, so slow turning still moves

static bool graphFetch(const char* query) {
  if (!graphCompile(query, graphProgram) ||
      !graphFindStart(graphProgram, GRAPH_HALF_WIDTHS[0], graphStartCenter)) {
    Serial.printf("Graph '%s': not available\n", query);
    return false;
  }
  graphCenter = graphStartCenter;
  // What was understood, not what was sent, so a misreading is visible on screen
  modelLabel = graphProgram.label;
  Serial.printf("Graph '%s': shown as '%s', centre %.2f\n", query, graphProgram.label, graphStartCenter);
  return true;
}

static bool graphBegin(M5Canvas& c) {
  (void)c;
  graphSamples = (GraphSamples*)ps_malloc(sizeof(GraphSamples));
  if (!graphSamples) return false;
  graphAppliedZoom = modelZoomStep;
  graphSetWindow(*graphSamples, graphCenter, GRAPH_HALF_WIDTHS[graphAppliedZoom]);
  graphYView.ready = false;
  graphPanCarry = 0;
  return true;
}

static void graphEnd() {
  if (!graphSamples) return;
  graphCenter = graphCenterX(*graphSamples);  // Back from the text: still in the same place
  free(graphSamples);
  graphSamples = nullptr;
}

static void graphDrawGridAndAxes(M5Canvas& c, double ymin, double ymax, int plotHeight) {
  const GraphSamples& s = *graphSamples;
  const uint16_t gridColor = c.color565(0x28, 0x28, 0x28);
  const uint16_t axisColor = c.color565(0x90, 0x90, 0x90);
  const double xmin = graphColumnX(s, 0), xmax = graphColumnX(s, GRAPH_COLUMNS - 1);
  const double xstep = graphNiceStep(xmax - xmin, GRAPH_MAX_X_TICKS);
  const double ystep = graphNiceStep(ymax - ymin, GRAPH_MAX_Y_TICKS);
  const long xfirst = lround(ceil(xmin / xstep));
  const long yfirst = lround(ceil(ymin / ystep));

  // Where 0 is out of sight, the axis stays at the edge, so the numbers always sit on a line
  const int zeroColumn = (int)lround(-xmin / s.unitsPerColumn);
  const int axisColumn = zeroColumn < 0 ? 0 : (zeroColumn >= GRAPH_COLUMNS ? GRAPH_COLUMNS - 1 : zeroColumn);
  const int zeroRow = graphPixelY(0, ymin, ymax, plotHeight);
  const int axisRow = zeroRow < 0 ? 0 : (zeroRow >= plotHeight ? plotHeight - 1 : zeroRow);

  for (int n = 0; n < GRAPH_TICK_LIMIT; n++) {
    const double x = (xfirst + n) * xstep;
    if (x > xmax) break;
    c.drawFastVLine((int)lround((x - xmin) / s.unitsPerColumn), 0, plotHeight, gridColor);
  }
  for (int n = 0; n < GRAPH_TICK_LIMIT; n++) {
    const double y = (yfirst + n) * ystep;
    if (y > ymax) break;
    c.drawFastHLine(0, graphPixelY(y, ymin, ymax, plotHeight), GRAPH_COLUMNS, gridColor);
  }
  c.drawFastVLine(axisColumn, 0, plotHeight, axisColor);
  c.drawFastHLine(0, axisRow, GRAPH_COLUMNS, axisColor);

  char text[24];
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::top_left);
  c.setTextColor(TFT_LIGHTGREY);
  const int labelHeight = 10;
  const int xLabelRow = axisRow + 3 + labelHeight <= plotHeight ? axisRow + 3 : axisRow - labelHeight - 1;
  for (int n = 0; n < GRAPH_TICK_LIMIT; n++) {
    const long k = xfirst + n;
    const double x = k * xstep;
    if (x > xmax) break;
    const int column = (int)lround((x - xmin) / s.unitsPerColumn);
    c.drawFastVLine(column, axisRow - 2, 5, axisColor);
    graphFormatTick(k, xstep, text, sizeof(text));
    const int width = c.textWidth(text);
    // The 0 sits beside the y axis, not on it
    int left = k == 0 ? column + 3 : column - width / 2;
    if (left < 1) left = 1;
    if (left + width >= GRAPH_COLUMNS) left = GRAPH_COLUMNS - 1 - width;
    c.drawString(text, left, xLabelRow);
  }
  for (int n = 0; n < GRAPH_TICK_LIMIT; n++) {
    const long k = yfirst + n;
    const double y = k * ystep;
    if (y > ymax) break;
    const int row = graphPixelY(y, ymin, ymax, plotHeight);
    c.drawFastHLine(axisColumn - 2, row, 5, axisColor);
    if (k == 0) continue;  // Already there from the x axis
    graphFormatTick(k, ystep, text, sizeof(text));
    const int width = c.textWidth(text);
    const int left = axisColumn + 4 + width < GRAPH_COLUMNS ? axisColumn + 4 : axisColumn - 4 - width;
    int top = row - labelHeight / 2;
    if (top < 0) top = 0;
    if (top + labelHeight > plotHeight) top = plotHeight - labelHeight;
    c.drawString(text, left, top);
  }
}

static void graphDrawCurve(M5Canvas& c, double ymin, double ymax, int plotHeight, unsigned long frameStart) {
  GraphSamples& s = *graphSamples;
  const uint16_t citrus = c.color565(0xF2, 0xB7, 0x05);
  for (int i = 0; i + 1 < GRAPH_COLUMNS; i++) {
    int row0 = 0, row1 = 0;
    const bool mayClassify = micros() - frameStart < GRAPH_FRAME_BUDGET_US;
    if (!graphSegmentRows(graphProgram, s, i, ymin, ymax, plotHeight, mayClassify, row0, row1)) continue;
    c.drawLine(i, row0, i + 1, row1, citrus);
    // Two pixels wide, sideways to the direction of the line
    if (abs(row1 - row0) > 1) {
      c.drawLine(i + 1, row0, i + 2, row1, citrus);
    } else {
      c.drawLine(i, row0 + 1, i + 1, row1 + 1, citrus);
    }
  }
}

// The dot in the middle and its exact values at the top
static void graphDrawDot(M5Canvas& c, double ymin, double ymax, int plotHeight) {
  const GraphSamples& s = *graphSamples;
  const double x = graphColumnX(s, GRAPH_DOT_COLUMN);
  const double y = graphEval(graphProgram, x);
  if (std::isfinite(y)) {
    const int row = graphPixelY(y, ymin, ymax, plotHeight);
    if (row >= 0 && row < plotHeight) {
      c.fillCircle(GRAPH_DOT_COLUMN, row, 4, TFT_BLACK);
      c.fillCircle(GRAPH_DOT_COLUMN, row, 3, TFT_WHITE);
    }
  }
  char xText[16], yText[16], line[40];
  graphFormatValue(x, s.unitsPerColumn * GRAPH_COLUMNS, xText, sizeof(xText));
  graphFormatValue(y, ymax - ymin, yText, sizeof(yText));
  snprintf(line, sizeof(line), "x=%s  y=%s", xText, yText);
  c.setFreeFont(FontManager::getFont12pt());
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::baseline_left);
  c.fillRect(0, 0, c.textWidth(line) + 5, 17, TFT_BLACK);
  c.setTextColor(TFT_WHITE);
  c.drawString(line, 2, 13);
}

static void graphFrame(M5Canvas& c, float dt) {
  GraphSamples& s = *graphSamples;
  const unsigned long frameStart = micros();
  const int plotHeight = HEIGHT - EDU_FOOTER_HEIGHT;

  if (modelZoomStep != graphAppliedZoom) {
    graphAppliedZoom = modelZoomStep;
    // Back at normal zoom is also the way home
    graphSetWindow(s, graphAppliedZoom == 0 ? graphStartCenter : graphCenterX(s), GRAPH_HALF_WIDTHS[graphAppliedZoom]);
    graphPanCarry = 0;
  }

  graphPanCarry += GRAPH_TILT_SIGN * graphTurnRate(modelGyroDps[1], modelGyroDps[2]) * GRAPH_COLUMNS_PER_DEGREE * dt;
  const long wholeColumns = (long)graphPanCarry;
  if (wholeColumns != 0) {
    graphPan(s, wholeColumns);
    graphPanCarry -= wholeColumns;
  }

  bool changed = false;
  while (micros() - frameStart < GRAPH_FRAME_BUDGET_US && graphComputeNext(graphProgram, s)) changed = true;
  if (changed) {
    double fitMin, fitMax;
    if (graphFitY(s, fitMin, fitMax)) graphYViewFollow(graphYView, fitMin, fitMax);
  }
  graphYViewEase(graphYView, dt);
  const double ymin = graphYView.min, ymax = graphYView.max;

  c.fillSprite(TFT_BLACK);
  graphDrawGridAndAxes(c, ymin, ymax, plotHeight);
  graphDrawCurve(c, ymin, ymax, plotHeight, frameStart);
  graphDrawDot(c, ymin, ymax, plotHeight);
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_GRAPH = {
    "graph", "Zitro",
    nullptr,  // Set once the view works on the stick and the main prompt allows it
    false, graphFetch, nullptr, graphBegin, graphFrame, graphEnd, nullptr, nullptr, nullptr};
