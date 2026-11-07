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
static const int MUSIC_VOLUME_TAB_RESERVE = 46;

static void musicUnload() {
  free(musicNotes);
  musicNotes = nullptr;
  musicNoteCount = 0;
}

static MusicLookup musicLastLookup = MUSIC_LOOKUP_UNKNOWN;

static bool musicDeviceGerman() {
  Preferences prefs;
  prefs.begin("settings", true);
  const bool german = prefs.getString("language", "DE") != "EN";
  prefs.end();
  return german;
}

// What Zitro says when no song was found: the reason is the one musicFetch last stored
String musicMissAnswerForDevice() {
  char text[200];
  musicMissAnswer(musicLastLookup, musicDeviceGerman(), (unsigned)millis(), text, sizeof(text));
  return String(text);
}

static void musicLog(const char* query, MusicLookup result) {
  musicLastLookup = result;
  const char* names[] = {"found", "unknown", "offline"};
  Serial.printf("Music: lookup %s '%s'\n", names[result], query);
}

// A built-in song first, then the server's list. Anything else is false, so a picture shows.
static bool musicFetch(const char* query) {
  musicUnload();
  musicNotes = (MusicNote*)ps_malloc(sizeof(MusicNote) * MUSIC_MAX_NOTES);
  if (!musicNotes) {
    musicLog(query, MUSIC_LOOKUP_UNKNOWN);
    return false;
  }
  musicGerman = musicDeviceGerman();
  MusicLookup result = MUSIC_LOOKUP_UNKNOWN;
  const MusicSong* song = musicFindBuiltIn(query);
  if (song) {
    musicNoteCount = musicParseNotes(song->notes, musicNotes, MUSIC_MAX_NOTES, musicTotalTicks);
    if (musicNoteCount > 0 && song->beatsPerMinute != 0) {
      musicBarTicks = song->barTicks;
      musicPickupTicks = song->pickupTicks;
      musicTickMs = musicMsPerTick(song->beatsPerMinute);
      modelLabel = musicGerman ? song->nameDe : song->nameEn;
      modelSourceName = "";
      result = MUSIC_LOOKUP_FOUND;
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    result = MUSIC_LOOKUP_OFFLINE;
  } else {
    const unsigned long deadline = millis() + EDU_LOOKUP_DEADLINE_MS;
    char* body = (char*)ps_malloc(MUSIC_INDEX_LIMIT + 1);  // +1: musicParseSongFile ends the text in place
    size_t length = 0;
    MusicIndexEntry entry;
    if (!body || !eduHttpGet(EDU_DATA_BASE_URL "music/index.txt", deadline, body, MUSIC_INDEX_LIMIT, length)) {
      result = MUSIC_LOOKUP_OFFLINE;
    } else if (const MusicIndexResult found = musicResolve(body, length, query, entry); found == MUSIC_INDEX_BAD) {
      result = MUSIC_LOOKUP_OFFLINE;  // An error page or a damaged list is as good as no list
    } else if (found == MUSIC_INDEX_MATCH) {
      // musicResolve only passes ids made of safe characters, so this can't leave the folder
      int bpm = 0, barTicks = 0, pickupTicks = 0;
      const char* notes = nullptr;
      if (eduHttpGet(String(EDU_DATA_BASE_URL "music/") + entry.id + ".txt", deadline, body, MUSIC_FILE_LIMIT, length) &&
          musicParseSongFile(body, length, entry.id, bpm, barTicks, pickupTicks, notes)) {
        musicNoteCount = musicParseNotes(notes, musicNotes, MUSIC_MAX_NOTES, musicTotalTicks);
        if (musicNoteCount > 0 && musicSongIsSound(musicNotes, musicNoteCount, musicTotalTicks, barTicks, pickupTicks, bpm)) {
          musicBarTicks = barTicks;
          musicPickupTicks = pickupTicks;
          musicTickMs = musicMsPerTick(bpm);
          modelLabel = musicGerman ? entry.de : entry.en;
          modelSourceName = entry.source;
          result = MUSIC_LOOKUP_FOUND;
        }
      }
    }
    free(body);
  }
  musicLog(query, result);
  if (result != MUSIC_LOOKUP_FOUND) {
    musicUnload();
    return false;
  }
  musicVolumeLevel = MUSIC_VOLUME_START;  // Going to the text and back keeps the level; a new song doesn't
  return true;
}

static bool musicBegin(M5Canvas& c) {
  (void)c;
  if (!musicNotes || musicNoteCount <= 0) return false;
  musicPlaybackReset(musicPlayback);
  Preferences prefs;
  prefs.begin("settings", true);
  musicStoredVolume = prefs.getInt("volume", 210);
  prefs.end();
  M5.Speaker.setVolume(musicVolumeAt(musicStoredVolume, musicVolumeLevel));
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
  if (!musicVolumeSounds(musicVolumeLevel)) M5.Speaker.stop(MUSIC_SPEAKER_CHANNEL);
  Serial.printf("Music: volume level %d, speaker %d\n", musicVolumeLevel, M5.Speaker.getVolume());
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
  if (musicAdvance(musicPlayback, musicNotes, musicNoteCount, musicTickMs, dt, midi, toneMs) &&
      musicVolumeSounds(musicVolumeLevel)) {
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
  const int rotation = M5.Display.getRotation();
  // Button C sits at the bottom right in rotation 1, so a long song name must stop before its tab
  eduDrawFooter(c, rotation == 1 ? MUSIC_VOLUME_TAB_RESERVE : 0);
  drawButtonHints(c, rotation, HINT_NONE, HINT_NONE, HINT_SPEAKER + musicVolumeLevel);
}

const EduFeature FEATURE_MUSIC = {
    "music", "",
    MUSIC_PROMPT_RULE,
    false, musicFetch, musicUnload, musicBegin, musicFrame, musicEnd, musicButtonB, musicButtonC, musicKeepAwake};
