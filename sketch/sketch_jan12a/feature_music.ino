// Music: real notes move past a fixed line and the note on the line sounds. See edu_feature.h.
// Notes come from the checked table in music_songs.h, never from the AI.

#include "music_core.h"
#include "music_songs.h"
#include "music_draw.h"

static const int MUSIC_SPEAKER_CHANNEL = 1;  // Reading aloud and the alert use channel 0

static MusicNote* musicNotes = nullptr;
static int musicNoteCount = 0;
static int musicTotalTicks = 0;
static int musicBarTicks = 0;
static int musicPickupTicks = 0;
static float musicTickMs = 0;
static bool musicGerman = true;
static bool musicOpen = false;
static MusicPlayback musicPlayback;
static int musicStoredVolume = 210;
static int musicVolumeLevel = MUSIC_VOLUME_START;
static float musicVolumeLabelSeconds = 0;  // How much longer the level's name stays on screen
static const float MUSIC_VOLUME_LABEL_SECONDS = 1.5f;

static void musicUnload() {
  free(musicNotes);
  musicNotes = nullptr;
  musicNoteCount = 0;
}

// Only a song in the table counts. Anything else is false, so a picture shows.
static bool musicFetch(const char* query) {
  musicUnload();
  const MusicSong* song = musicFindSong(query);
  if (!song) {
    Serial.printf("Music: no song '%s'\n", query);
    return false;
  }
  musicNotes = (MusicNote*)ps_malloc(sizeof(MusicNote) * MUSIC_MAX_NOTES);
  if (!musicNotes) return false;
  musicNoteCount = musicParseNotes(song->notes, musicNotes, MUSIC_MAX_NOTES, musicTotalTicks);
  if (musicNoteCount <= 0 || song->beatsPerMinute == 0) {
    musicUnload();
    return false;
  }
  musicBarTicks = song->barTicks;
  musicPickupTicks = song->pickupTicks;
  musicTickMs = musicMsPerTick(song->beatsPerMinute);
  Preferences prefs;
  prefs.begin("settings", true);
  musicGerman = prefs.getString("language", "DE") != "EN";
  prefs.end();
  musicVolumeLevel = MUSIC_VOLUME_START;  // Going to the text and back keeps the level; a new song doesn't
  modelLabel = musicGerman ? song->nameDe : song->nameEn;
  modelSourceName = "";
  return true;
}

static bool musicBegin(M5Canvas& c) {
  (void)c;
  if (!musicNotes || musicNoteCount <= 0) return false;
  musicPlaybackReset(musicPlayback);
  M5.Speaker.begin();
  Preferences prefs;
  prefs.begin("settings", true);
  musicStoredVolume = prefs.getInt("volume", 210);
  prefs.end();
  M5.Speaker.setVolume(musicVolumeAt(musicStoredVolume, musicVolumeLevel));
  musicVolumeLabelSeconds = 0;
  musicOpen = true;
  return true;
}

static void musicEnd() {
  M5.Speaker.stop(MUSIC_SPEAKER_CHANNEL);
  // The level is only for this song: speech and alarms go back to the stored volume
  if (musicOpen) M5.Speaker.setVolume(musicStoredVolume);
  musicOpen = false;
}

static void musicButtonB() {
  musicTogglePause(musicPlayback);
  if (musicPlayback.paused) M5.Speaker.stop(MUSIC_SPEAKER_CHANNEL);
}

// Button C: the next volume level. Holding it goes back to the text (modelButtonCHold).
static bool musicButtonC() {
  musicVolumeLevel = musicNextVolumeLevel(musicVolumeLevel);
  M5.Speaker.setVolume(musicVolumeAt(musicStoredVolume, musicVolumeLevel));
  musicVolumeLabelSeconds = MUSIC_VOLUME_LABEL_SECONDS;
  Serial.printf("Music: volume level %s, speaker %d\n", musicVolumeName(musicVolumeLevel, false), M5.Speaker.getVolume());
  return true;
}

static bool musicKeepAwake() {
  return musicOpen && !musicPlayback.paused;
}

static void musicFrame(M5Canvas& c, float dt) {
  if (!musicNotes || musicNoteCount <= 0) return;
  uint8_t midi = 0;
  uint32_t toneMs = 0;
  // Band and sound run on the same clock, and each tone ends by itself: nothing here waits
  if (musicAdvance(musicPlayback, musicNotes, musicNoteCount, musicTickMs, dt, midi, toneMs)) {
    M5.Speaker.begin();  // Sleep switches it off; returns at once if it's running
    M5.Speaker.tone(musicFrequencyHz(midi), toneMs, MUSIC_SPEAKER_CHANNEL, true);
  }

  c.fillSprite(TFT_BLACK);
  const MusicBand band = {musicNotes,       musicNoteCount,      musicBarTicks,
                          musicPickupTicks, musicPlayback.note, musicPlayback.tickInNote};
  musicDrawBand(c, band);

  // Note names under the notes. A sixteenth leaves no room for its name.
  c.setFont(&fonts::DejaVu9);
  c.setTextSize(1);
  c.setTextDatum(textdatum_t::middle_center);
  c.setTextColor(TFT_LIGHTGREY);
  musicForEachVisible(band, [&](int i, int x) {
    if (i != musicPlayback.note && musicNotes[i].ticks >= 2 && x > MUSIC_CLEF_WIDTH) {
      c.drawString(musicNoteName(musicNotes[i], musicGerman), x, MUSIC_NAME_Y);
    }
  });
  // The one that's sounding, big, under the line
  const char* now = musicNoteName(musicNotes[musicPlayback.note], musicGerman);
  if (now[0]) {
    c.setFreeFont(FontManager::getFont12pt());
    const int width = c.textWidth(now);
    c.fillRect(MUSIC_PLAYHEAD_X - width / 2 - 3, MUSIC_NAME_Y - 11, width + 6, 22, TFT_BLACK);
    c.setTextColor(MUSIC_COLOR_NOW);
    c.drawString(now, MUSIC_PLAYHEAD_X, MUSIC_NAME_Y);
  }

  if (musicPlayback.paused) {
    c.fillRect(WIDTH - 20, 3, 5, 15, TFT_WHITE);
    c.fillRect(WIDTH - 11, 3, 5, 15, TFT_WHITE);
  }
  // The level's name after a press. Silent stays up, or it would look broken.
  if (musicVolumeLabelSeconds > 0) musicVolumeLabelSeconds -= dt;
  if (musicVolumeLabelSeconds > 0 || musicVolumeLevel == MUSIC_VOLUME_SILENT) {
    const char* level = musicVolumeName(musicVolumeLevel, musicGerman);
    c.setFont(&fonts::DejaVu12);
    c.setTextDatum(textdatum_t::top_right);
    const int right = WIDTH - 26;
    c.fillRect(right - c.textWidth(level) - 3, 2, c.textWidth(level) + 6, 16, TFT_BLACK);
    c.setTextColor(MUSIC_COLOR_NOW);
    c.drawString(level, right, 4);
  }
  eduDrawFooter(c, 0);
}

const EduFeature FEATURE_MUSIC = {
    "music", "",
    MUSIC_PROMPT_RULE,
    false, musicFetch, musicUnload, musicBegin, musicFrame, musicEnd, musicButtonB, musicButtonC, musicKeepAwake};
