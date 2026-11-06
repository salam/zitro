// Music band: draws the staff and the notes. Takes any canvas with the usual drawing calls, so the
// host tests can run the same code. Text is left to the caller, which has the fonts.
#pragma once

#include "music_core.h"

static const int MUSIC_BAND_WIDTH = 240;
static const int MUSIC_PLAYHEAD_X = 64;
static const int MUSIC_PX_PER_TICK = 10;   // A quarter is 40 pixels; a sixteenth just clears the next head
static const int MUSIC_STAFF_BOTTOM_Y = 68;  // E4, the bottom line
static const int MUSIC_STEP_PX = 5;          // A line every 2 steps: 10 pixels apart
static const int MUSIC_CLEF_WIDTH = 26;      // Notes pass behind the clef
static const int MUSIC_STEM_PX = 28;
static const int MUSIC_NAME_Y = 104;  // Middle of the row of note names

static const uint16_t MUSIC_COLOR_BLACK = 0x0000;
static const uint16_t MUSIC_COLOR_NOTE = 0xFFFF;
static const uint16_t MUSIC_COLOR_PLAYED = 0x8410;
static const uint16_t MUSIC_COLOR_STAFF = 0x7BEF;
static const uint16_t MUSIC_COLOR_NOW = 0xF5A0;  // Citrus, #f2b705

struct MusicBand {
  const MusicNote* notes;
  int count;
  int barTicks;
  int pickupTicks;
  int note;          // At the playhead
  float tickInNote;
};

inline int musicStaffY(int staffStep) {
  return MUSIC_STAFF_BOTTOM_Y - staffStep * MUSIC_STEP_PX;
}

// Calls visit(noteIndex, x) for every note on screen, x being where its head sits. Walks out from the
// playhead in both directions and wraps at the ends, so the band never ends, however short the song.
template <class Visit>
void musicForEachVisible(const MusicBand& b, Visit visit) {
  if (!b.notes || b.count <= 0 || b.note < 0 || b.note >= b.count) return;
  const float margin = 24;
  float x = MUSIC_PLAYHEAD_X - b.tickInNote * MUSIC_PX_PER_TICK;
  int i = b.note;
  float rightX = x;
  while (rightX < MUSIC_BAND_WIDTH + margin) {
    visit(i, (int)lroundf(rightX));
    rightX += b.notes[i].ticks * MUSIC_PX_PER_TICK;
    i = (i + 1) % b.count;
  }
  i = b.note;
  float leftX = x;
  while (leftX > -margin) {
    i = (i + b.count - 1) % b.count;
    leftX -= b.notes[i].ticks * MUSIC_PX_PER_TICK;
    visit(i, (int)lroundf(leftX));
  }
}

template <class C>
void musicDrawFlagged(C& c, int stemX, int tipY, bool stemUp, int flags, uint16_t color) {
  const int dir = stemUp ? 1 : -1;
  for (int f = 0; f < flags; f++) {
    const int y = tipY + dir * f * 6;
    c.drawLine(stemX, y, stemX + 6, y + dir * 7, color);
    c.drawLine(stemX + 1, y, stemX + 7, y + dir * 7, color);
    c.drawLine(stemX + 6, y + dir * 7, stemX + 5, y + dir * 11, color);
  }
}

template <class C>
void musicDrawAccidental(C& c, int x, int y, int accidental, uint16_t color) {
  if (accidental > 0) {
    c.drawLine(x - 2, y - 6, x - 2, y + 6, color);
    c.drawLine(x + 1, y - 7, x + 1, y + 5, color);
    c.drawLine(x - 4, y - 1, x + 3, y - 3, color);
    c.drawLine(x - 4, y + 3, x + 3, y + 1, color);
  } else if (accidental < 0) {
    c.drawLine(x - 2, y - 8, x - 2, y + 4, color);
    c.drawLine(x - 2, y + 4, x + 2, y + 1, color);
    c.drawLine(x + 2, y + 1, x + 2, y - 1, color);
    c.drawLine(x + 2, y - 1, x - 2, y - 2, color);
  }
}

template <class C>
void musicDrawRest(C& c, int x, int ticks, uint16_t color) {
  const int middle = musicStaffY(4);
  if (ticks >= 16) {
    c.fillRect(x - 6, middle - 10, 12, 5, color);  // Hangs from the fourth line
  } else if (ticks >= 8) {
    c.fillRect(x - 6, middle - 5, 12, 5, color);  // Sits on the middle line
  } else if (ticks >= 4) {
    c.drawLine(x - 2, middle - 12, x + 3, middle - 5, color);
    c.drawLine(x - 1, middle - 12, x + 4, middle - 5, color);
    c.drawLine(x + 3, middle - 5, x - 2, middle + 1, color);
    c.drawLine(x + 4, middle - 5, x - 1, middle + 1, color);
    c.drawLine(x - 2, middle + 1, x + 3, middle + 8, color);
    c.drawLine(x - 1, middle + 1, x + 3, middle + 8, color);
    c.drawLine(x + 3, middle + 8, x - 2, middle + 7, color);
  } else {
    c.fillCircle(x - 2, middle - 4, 2, color);
    c.drawLine(x - 2, middle - 2, x + 4, middle - 5, color);
    c.drawLine(x + 4, middle - 5, x, middle + 8, color);
    if (ticks < 2) c.fillCircle(x - 3, middle + 2, 2, color);
  }
}

template <class C>
void musicDrawNote(C& c, const MusicNote& n, int x, const MusicNote* next, uint16_t color) {
  if (n.midi == 0) {
    musicDrawRest(c, x, n.ticks, color);
    return;
  }
  const int y = musicStaffY(n.staffStep);
  // Ledger lines, for notes above or below the five lines
  for (int step = -2; step >= n.staffStep; step -= 2) c.drawFastHLine(x - 8, musicStaffY(step), 17, MUSIC_COLOR_NOTE);
  for (int step = 10; step <= n.staffStep; step += 2) c.drawFastHLine(x - 8, musicStaffY(step), 17, MUSIC_COLOR_NOTE);

  c.fillEllipse(x, y, 5, 4, color);
  if (n.ticks >= 8) c.fillEllipse(x, y, 3, 2, MUSIC_COLOR_BLACK);  // Half and whole notes are hollow
  if (n.ticks < 16) {
    const bool stemUp = n.staffStep < 4;  // Above the middle line the stem goes down, as printed
    const int stemX = stemUp ? x + 5 : x - 5;
    const int tipY = stemUp ? y - MUSIC_STEM_PX : y + MUSIC_STEM_PX;
    c.drawLine(stemX, y, stemX, tipY, color);
    const int flags = n.ticks < 2 ? 2 : (n.ticks < 4 ? 1 : 0);
    musicDrawFlagged(c, stemX, tipY, stemUp, flags, color);
  }
  if (n.ticks == 3 || n.ticks == 6 || n.ticks == 12) {
    // A dot beside the head; if the head sits on a line, the dot goes in the space above
    c.fillCircle(x + 9, (n.staffStep % 2 == 0) ? y - 3 : y, 2, color);
  }
  if (n.accidental) musicDrawAccidental(c, x - 11, y, n.accidental, color);
  if (n.tied && next) {
    const int endX = x + n.ticks * MUSIC_PX_PER_TICK;
    const int side = n.staffStep < 4 ? 1 : -1;  // Away from the stem
    const int arcY = y + side * 6;
    c.drawLine(x + 3, arcY, x + 10, arcY + side * 3, color);
    c.drawLine(x + 10, arcY + side * 3, endX - 10, arcY + side * 3, color);
    c.drawLine(endX - 10, arcY + side * 3, endX - 3, arcY, color);
  }
}

// One treble clef, curling around the G line, in pixels from the left edge. Drawn as a thick line.
template <class C>
void musicDrawClef(C& c) {
  static const int8_t POINTS[][2] = {{14, 62}, {10, 60}, {9, 56},  {12, 52}, {17, 52}, {20, 56}, {20, 62}, {16, 67},
                                     {10, 68}, {5, 64},  {4, 57},  {8, 49},  {13, 42}, {16, 36}, {17, 28}, {15, 21},
                                     {13, 24}, {12, 32}, {13, 42}, {15, 56}, {17, 70}, {17, 76}, {14, 80}, {10, 79},
                                     {9, 76},  {12, 74}};
  const int count = sizeof(POINTS) / sizeof(POINTS[0]);
  for (int i = 0; i + 1 < count; i++) {
    c.drawLine(POINTS[i][0], POINTS[i][1], POINTS[i + 1][0], POINTS[i + 1][1], MUSIC_COLOR_NOTE);
    c.drawLine(POINTS[i][0] + 1, POINTS[i][1], POINTS[i + 1][0] + 1, POINTS[i + 1][1], MUSIC_COLOR_NOTE);
  }
}

// Bar lines, and a double line where the song starts over. Every bar starts on a note (tested for all
// songs), so the notes say where they go: just before the head, and before its sharp or flat if it has one.
template <class C>
void musicDrawBar(C& c, const MusicBand& b, int i, int x) {
  const MusicNote& n = b.notes[i];
  const int top = musicStaffY(8), height = musicStaffY(0) - top + 1;
  const int barX = x - (n.accidental ? 17 : 9);
  if (i == 0) {
    c.drawFastVLine(barX - 3, top, height, MUSIC_COLOR_STAFF);
    c.fillRect(barX, top, 2, height, MUSIC_COLOR_NOTE);
  } else if (b.barTicks > 0 && (n.startTick - b.pickupTicks) % b.barTicks == 0) {
    c.drawFastVLine(barX, top, height, MUSIC_COLOR_STAFF);
  }
}

// Everything but the text
template <class C>
void musicDrawBand(C& c, const MusicBand& b) {
  if (!b.notes || b.count <= 0 || b.note < 0 || b.note >= b.count) return;
  for (int step = 0; step <= 8; step += 2) c.drawFastHLine(0, musicStaffY(step), MUSIC_BAND_WIDTH, MUSIC_COLOR_STAFF);
  musicForEachVisible(b, [&](int i, int x) { musicDrawBar(c, b, i, x); });
  const int current = b.note;
  // A note and what it's tied to count as one: both light up while it sounds
  const int partner = b.notes[current].tied ? current + 1 : (b.notes[(current + b.count - 1) % b.count].tied ? current - 1 : current);
  musicForEachVisible(b, [&](int i, int x) {
    const bool now = i == current || i == partner;
    const bool played = x < MUSIC_PLAYHEAD_X && !now;
    const MusicNote* next = i + 1 < b.count ? &b.notes[i + 1] : nullptr;
    musicDrawNote(c, b.notes[i], x, next, now ? MUSIC_COLOR_NOW : (played ? MUSIC_COLOR_PLAYED : MUSIC_COLOR_NOTE));
  });
  c.fillRect(0, 0, MUSIC_CLEF_WIDTH, MUSIC_NAME_Y - 12, MUSIC_COLOR_BLACK);
  for (int step = 0; step <= 8; step += 2) c.drawFastHLine(0, musicStaffY(step), MUSIC_CLEF_WIDTH, MUSIC_COLOR_STAFF);
  musicDrawClef(c);
  c.drawFastVLine(MUSIC_PLAYHEAD_X, 8, MUSIC_NAME_Y - 18, MUSIC_COLOR_NOW);
}
