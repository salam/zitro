struct GifBitStream;
struct PrinterInfo;
struct WikiResult;
struct GPTResponse;
struct ChatResult;
struct AudioEndpoint;

// Enum declarations must be at the top for Arduino function prototype generation
enum Orientation { LANDSCAPE, PORTRAIT };
enum DisplayMode { SHOW_TEXT, SHOW_IMAGE, SHOW_MODEL };
enum ImageZoomMode { CONTAIN, COVER };
enum ImageFetchResult { IMAGE_FETCH_OK, IMAGE_FETCH_TOO_LARGE, IMAGE_FETCH_NEEDS_DOWNSAMPLE, IMAGE_FETCH_ERROR };

#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// AI providers (OpenAI, Claude, Infomaniak, OpenAI-compatible, Zitro relay) and conversation memory
#if __has_include("local_secrets.h")
#include "local_secrets.h"
#endif
#ifndef WOLFRAM_APPID
#define WOLFRAM_APPID ""  // Without one, Wolfram questions fail and Zitro says so. See local_secrets.h.example
#endif
#include "ai_provider.h"
#include "model_core.h"
#include "edu_feature.h"
#include "button_rules.h"
#include <SPIFFS.h>

// Optional: Enable JPEG conversion for GIF/PNG images when printing
// Requires JPEGENC library: https://github.com/bitbank2/JPEGENC
// Uncomment the next line and install library to enable
#define ENABLE_JPEG_CONVERSION
#include <JPEGENC.h>

// Alert sound for timer alarms
#include "alert_sound_data.h"
#include "splash_jingle_data.h"

// Launch logo
#include "logo_data.h"
#include "setup_fonts.h"
#include "wifi_qr_data.h"
#include "pwg_raster.h"  // A4 print pages (print_layout.ino)
#include "gif_decode.h"  // GIF decoding (drawGifToCanvas, gif_anim.ino)

// Custom fonts with European language support (German umlauts, Greek, Cyrillic, Hebrew)
#include "fonts/PrimaryFont10pt.h"
#include "fonts/PrimaryFont14pt.h"

// FontManager class for smart UTF-8 font rendering
class FontManager {
private:
  static const GFXfont* font12pt;  // Actually 10pt now
  static const GFXfont* font16pt;  // Actually 14pt now

public:
  static void init() {
    font12pt = &Roboto_Regular10pt8b;
    font16pt = &Roboto_Regular14pt8b;
    Serial.println("FontManager: Custom fonts loaded (10pt & 14pt, European + Greek + Cyrillic + Hebrew)");
  }

  static const GFXfont* getFont12pt() { return font12pt; }  // Returns 10pt
  static const GFXfont* getFont16pt() { return font16pt; }  // Returns 14pt

  // UTF-8 decoder supporting 1-3 byte sequences
  static uint16_t decodeUTF8Character(const String& text, int& index) {
    if (index >= text.length()) return 0;

    uint8_t byte1 = text[index];

    // 1-byte sequence (ASCII 0x00-0x7F)
    if ((byte1 & 0x80) == 0) {
      index++;
      return byte1;
    }

    // 2-byte sequence (0x80-0x7FF)
    if ((byte1 & 0xE0) == 0xC0 && index + 1 < text.length()) {
      uint16_t codepoint = ((byte1 & 0x1F) << 6) | (text[index + 1] & 0x3F);
      index += 2;
      return codepoint;
    }

    // 3-byte sequence (0x800-0xFFFF)
    if ((byte1 & 0xF0) == 0xE0 && index + 2 < text.length()) {
      uint16_t codepoint = ((byte1 & 0x0F) << 12) |
                           ((text[index + 1] & 0x3F) << 6) |
                           (text[index + 2] & 0x3F);
      index += 3;
      return codepoint;
    }

    // Invalid sequence - skip byte
    index++;
    return 0;
  }

  // Check if character is supported by primary fonts (0x20-0x5FF)
  static bool isSupportedCharacter(uint16_t codepoint) {
    return (codepoint >= 0x20 && codepoint <= 0x5FF);
  }
};

// Static member initialization
const GFXfont* FontManager::font12pt = nullptr;
const GFXfont* FontManager::font16pt = nullptr;

static const int WIDTH  = 240;
static const int HEIGHT = 135;

// API keys live in Preferences ("settings" namespace) and are set on the setup page.

// Audio settings (defaults, overridden by preferences at boot)
static const int DEFAULT_SAMPLE_RATE = 8000;
static const int DEFAULT_RECORD_SECONDS = 5;
static int activeSampleRate = DEFAULT_SAMPLE_RATE;
static int activeRecordSeconds = DEFAULT_RECORD_SECONDS;
static int activeRecordSamples = DEFAULT_SAMPLE_RATE * DEFAULT_RECORD_SECONDS;
static int16_t* audioBuffer = nullptr;

// WiFi setup
Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
String savedSSID;
String savedPass;
bool apMode = false;

// Multi-AP support
#define MAX_WIFI_NETWORKS 5
struct WiFiCredential {
  String ssid;
  String password;
};
WiFiCredential wifiNetworks[MAX_WIFI_NETWORKS];
int wifiNetworkCount = 0;
unsigned long lastConnectionCheck = 0;
#define CONNECTION_CHECK_INTERVAL 30000  // Check every 30 seconds

// Wikipedia rate limiting
unsigned long wikipediaBlockedUntil = 0;  // Timestamp when Wikipedia requests can resume

// Home screen key: drawScreen() draws this text as the home screen
// ("[lemon] Press [M5] button / to ask a question / [shake] Shake for random fact.")
const String MAIN_QUESTION = "Press BUTTON\nto ask a question\n\nShake for random fact.";
String response = MAIN_QUESTION;

// Display sleep management
unsigned long lastActivityTime = 0;
bool displaySleeping = false;

// Power management - sleep timeouts
// Defaults; loadSleepTimeouts() replaces them with the dispSleep/lightSleep/deepSleep settings
unsigned long DISPLAY_SLEEP_TIMEOUT = 30000;      // 30 seconds - display off
unsigned long LIGHT_SLEEP_TIMEOUT = 90000;        // 90 seconds - enter light sleep (WiFi off, CPU paused)
unsigned long DEEP_SLEEP_TIMEOUT = 180000;        // 180 seconds (3 min) - enter deep sleep

// GPIO pins for M5StickC Plus 2 buttons
#define BUTTON_A_GPIO GPIO_NUM_37
#define BUTTON_B_GPIO GPIO_NUM_39

// Preferences keys for deep sleep state persistence
#define PREFS_NAMESPACE "powerstate"
#define PREFS_KEY_RESPONSE "response"
#define PREFS_KEY_IMAGE_URL "imageUrl"
#define PREFS_KEY_MODEL_REF "modelRef"
#define PREFS_KEY_IS_WOLFRAM "isWolfram"
#define PREFS_KEY_HAS_IMAGE "hasImage"
#define PREFS_KEY_DISPLAY_MODE "dispMode"
#define PREFS_KEY_VALID "valid"
#define PREFS_KEY_PARENTAL_LOCK "parentLock"

// Power management state
enum PowerState {
  POWER_ACTIVE,
  POWER_DISPLAY_SLEEP,
  POWER_LIGHT_SLEEP,
  POWER_DEEP_SLEEP
};

PowerState currentPowerState = POWER_ACTIVE;
bool wakeFromLightSleep = false;
bool wakeFromDeepSleep = false;
unsigned long timeWokeFromSleep = 0;  // Track when we woke from sleep
const unsigned long WAKE_COOLDOWN = 2000;  // 2 second cooldown after wake before allowing recording
const unsigned long MULTI_TAP_WINDOW = 500;  // A second tap within this time while recording cancels

// Parental control / child safety lock
bool parentalLockEnabled = false;

// Image storage
uint8_t* imageBuffer = nullptr;
int imageBufferSize = 0;
String currentImageUrl = "";
bool hasImage = false;
// 3D view (model_view.ino, model_source.ino). modelRefText is what deep sleep saves.
bool hasModel = false;
String modelRefText;
String modelLabel;
String modelSourceName;
ModelMolecule* currentMolecule = nullptr;
Mat3 modelRotation;
uint8_t modelZoomStep = 0;
bool modelForceNoMemory = false;  // Serial "modelnomem 1": tests the out-of-memory path
String serialTestQuestion;  // Serial "ask <question>": stands in for the recording
String modelPubChemHost = "pubchem.ncbi.nlm.nih.gov";  // Serial "modelhost": tests a dead server
bool isWolframImage = false;  // Flag to track if current image is from Wolfram Alpha
int wolframScrollOffset = 0;  // Vertical scroll offset for Wolfram images
int wolframMaxScrollOffset = 0;

// TTS audio storage for replay
int16_t* lastAudioBuffer = nullptr;
int lastAudioSampleCount = 0;
bool hasAudioForCurrentText = false;  // Track if current text has spoken audio
int gyroPowerState = -1;            // Gyroscope power: -1 unknown (after Imu.begin), 0 off, 1 on
unsigned long gyroEnabledAt = 0;

// Orientation tracking for display toggle (enums defined at top of file)
struct OrientationState {
  Orientation current;
  unsigned long lastChangeTime;
  int debounceMs;
};
OrientationState orientationState = {LANDSCAPE, 0, 500};
DisplayMode currentDisplayMode = SHOW_TEXT;
ImageZoomMode imageZoomMode = CONTAIN;

// Printer discovery and printing
#define MAX_PRINTERS 10
struct PrinterInfo {
  String name;
  String hostname;
  IPAddress ip;
  uint16_t port;
  String serviceType;  // ipp, ipps, etc.
};
PrinterInfo discoveredPrinters[MAX_PRINTERS];
int printerCount = 0;
int selectedPrinterIndex = 0;
bool inPrinterSelectionMode = false;
bool isPrinting = false;
// Steps of the print menu while inPrinterSelectionMode (print_layout.ino)
#define PRINT_STEP_PRINTER 0  // pick a printer
#define PRINT_STEP_SIZE    1  // pick a page size
#define PRINT_STEP_ZOOM    2  // pick the zoom percentage
int printMenuStep = PRINT_STEP_PRINTER;
// Results of printComposedPage()
#define PRINT_COMPOSE_OK          0
#define PRINT_COMPOSE_FAILED      1
#define PRINT_COMPOSE_UNAVAILABLE 2  // could not build the page: send the image file instead

// Triple-click detection for Button C to enter settings
unsigned long btnCLastClickTime = 0;
int btnCClickCount = 0;
const unsigned long TRIPLE_CLICK_TIMEOUT = 500;  // 500ms window for triple-click

// Timer/Reminder system
#define MAX_TIMERS 5
struct TimerEntry {
  bool active;
  time_t fireTime;     // Unix epoch seconds
  String label;        // What to show when timer fires
};
TimerEntry timers[MAX_TIMERS];
int timerCount = 0;
// Last "timer set" confirmation: whenever this response text is drawn, it shows as a card
String timerSetResponse;
String timerSetLabel;
String timerSetWhen;
bool inTimerListMode = false;
int timerListIndex = 0;
bool timerAlertActive = false;
int timerAlertIndex = -1;
// Rotation the "timer set" card was last drawn in (it turns with the stick, portrait included)
int timerSetDrawnRotation = -1;
// A response shown with a large icon in front (volume change): drawn that way on every redraw
String iconResponse;
String iconResponseText;
const uint8_t* iconResponseIcon = nullptr;
unsigned long lastTimerCheck = 0;

// Wikipedia result structure
struct WikiResult {
  bool found;
  String imageUrl;
  String title;
};

// GPT response structure

// Surprise response structure (defined in surprise_data.ino)
struct SurpriseResponse {
  uint8_t instructionIndex;  // 0-3
  uint8_t questionIndex;     // 0-4
  const char* answerDE;      // German answer text
  const char* answerEN;      // English answer text
  const char* wikiQuery;     // Wikipedia search query
};

// Forward declarations for functions in surprise_data.ino
SurpriseResponse getSurpriseResponse(int index);
int getRandomSurpriseIndex(uint8_t instructionIdx, uint8_t questionIdx);
String readProgmemString(const char* progmemStr);

// Forward declarations for timer functions
int getNextTimerSeconds();

// Settings registry for voice-configurable settings
// Adding a new setting here automatically makes it voice-configurable
struct SettingDef {
  const char* key;          // Preferences key
  const char* type;         // "bool", "int", "long", "string"
  const char* nameDE;       // German display name
  const char* nameEN;       // English display name
  const char* promptHint;   // Compact description for GPT prompt
  int minVal;               // Min value (int/long), -1 if N/A
  int maxVal;               // Max value (int/long), -1 if N/A
  const char* allowedVals;  // Comma-separated allowed values for string/int enums, NULL if N/A
  bool needsRestart;        // Whether change needs restart for full effect
  long defVal;              // Default value (bool: 0/1, int, long in ms); string defaults use allowedVals' first entry
};

static const SettingDef settingsRegistry[] = {
  {"volume",      "int",    "Lautstaerke",       "Volume",           "Speaker volume",             0, 255,     NULL,         false, 210},
  {"language",    "string", "Sprache",            "Language",         "Device language",           -1, -1,      "DE,EN",      false, 0},
  {"recordSec",   "int",    "Aufnahmedauer",      "Recording time",   "Recording seconds",          1, 30,      NULL,         false, DEFAULT_RECORD_SECONDS},
  {"sampleRate",  "int",    "Audioqualitaet",     "Audio quality",    "Sample rate Hz",            -1, -1,      "8000,16000", false, DEFAULT_SAMPLE_RATE},
  {"dispSleep",   "long",   "Display-Schlaf",     "Display sleep",    "Display sleep seconds",      5, 300,     NULL,         false, 30000},
  {"lightSleep",  "long",   "Leichter Schlaf",    "Light sleep",      "Light sleep seconds",       10, 600,     NULL,         false, 90000},
  {"deepSleep",   "long",   "Tiefer Schlaf",      "Deep sleep",       "Deep sleep seconds",        30, 1800,    NULL,         false, 180000},
  {"searchImg",   "bool",   "Bildersuche",        "Image search",     "Wikipedia image search",    -1, -1,      NULL,         false, 1},
  {"useWolfram",  "bool",   "Wolfram Alpha",      "Wolfram Alpha",    "Wolfram for math",          -1, -1,      NULL,         false, 1},
  {"localQuotes", "bool",   "Lokale Zitate",      "Local quotes",     "Use local CSV quotes",      -1, -1,      NULL,         false, 1},
  {"parentLock",  "bool",   "Kindersicherung",    "Parental lock",    "Parental lock 20-06h",      -1, -1,      NULL,         false, 0},
  {"shakeRandom", "bool",   "Schuetteln",         "Shake fact",       "Shake for random fact",     -1, -1,      NULL,         false, 1},
  {"tripleAudio", "bool",   "Doppelklick Audio",  "Double-click audio","Double-click for audio",   -1, -1,      NULL,         false, 1},
  {"autoAudio",   "bool",   "Auto-Audio",         "Auto audio",       "Auto-play audio requests",  -1, -1,      NULL,         false, 0},
  {"moveZoom",    "bool",   "Roll-Zoom",          "Roll zoom",        "Roll gesture for zoom",     -1, -1,      NULL,         false, 1},
  {"moveToggle",  "bool",   "Dreh-Umschalten",    "Rotate toggle",    "Rotation to toggle view",   -1, -1,      NULL,         false, 1},
  {"memory",      "bool",   "Gespraechsgedaechtnis", "Conversation memory", "Remember last 10 minutes", -1, -1, NULL,       false, 1},
};
static const int SETTINGS_REGISTRY_COUNT = sizeof(settingsRegistry) / sizeof(settingsRegistry[0]);

// Icon bitmaps are now in icons.ino (auto-generated from PNG files)
// Forward declarations for icons (defined in icons.ino)
extern const uint8_t micIcon[60];
extern const uint8_t transcribeIcon[60];
extern const uint8_t thinkingIcon[60];
extern const uint8_t voiceIcon[60];
extern const uint8_t rotateIcon[60];
extern const uint8_t nightIcon[60];
extern const uint8_t generateIcon[60];
extern const uint8_t printIcon[60];
extern const uint8_t timerIcon[60];
extern const uint8_t bellIcon[60];
extern const uint8_t shakeIcon[60];
extern const uint8_t volumeIcon[60];
extern const uint8_t micBrokenIcon[60];
#define MICICON_WIDTH 20
#define MICICON_HEIGHT 20
#define TRANSCRIBEICON_WIDTH 20
#define TRANSCRIBEICON_HEIGHT 20
#define THINKINGICON_WIDTH 20
#define THINKINGICON_HEIGHT 20
#define VOICEICON_WIDTH 20
#define VOICEICON_HEIGHT 20
#define ROTATEICON_WIDTH 20
#define ROTATEICON_HEIGHT 20
#define NIGHTICON_WIDTH 20
#define NIGHTICON_HEIGHT 20
#define GENERATEICON_WIDTH 20
#define GENERATEICON_HEIGHT 20
#define PRINTICON_WIDTH 20
#define PRINTICON_HEIGHT 20
#define TIMERICON_WIDTH 20
#define TIMERICON_HEIGHT 20
#define BELLICON_WIDTH 20
#define BELLICON_HEIGHT 20
#define SHAKEICON_WIDTH 20
#define SHAKEICON_HEIGHT 20
#define VOLUMEICON_WIDTH 20
#define VOLUMEICON_HEIGHT 20
#define MICBROKENICON_WIDTH 20
#define MICBROKENICON_HEIGHT 20

// Forward declarations for utility and image functions
int getLandscapeRotation();
void displayImage(bool isGeneratedImage = false);
void discardButtonA(unsigned long quietMs);
bool inWakeCooldown();
void displayPrinterSelection();
void drawPrintSizeMenu();
void printMenuReset();
void printMenuNext();
bool printMenuConfirm();
bool printMenuBack();
int printComposedPage(PrinterInfo &printer);
void drawTimerSet(const String &label, const String &when);
String formatTimerDuration(int minutes);
Orientation detectOrientation();
void redrawPreviousResult();
void loadSleepTimeouts();
void powerDownPeripherals();
void enterDeepSleep();
void captureCanvas(M5Canvas &canvas);
void gifAnimStart(float scale);
bool gifAnimResume(float scale);
void gifAnimTick();
void drawSetupScreen(bool openingSettings = false);
void drawHomeScreen();
void drawIconTextScreen(const String &text, const uint8_t* iconData);
int heldRotation();
void handleSerialCommands();
bool saveImageToFlash();
bool restoreImageFromFlash();
void handleDeepSleepWake();
bool modelViewBegin();
void modelViewEnd();
bool modelViewActive();
void modelViewRender(float dt);
bool modelViewSpriteInPsram();
void modelBench(int atoms);
bool fetchModel(const ModelRef& ref);
void eduUnloadActive();
bool modelViewShow();
void modelButtonB();
bool modelButtonC();
void drawModelHint(M5Canvas& c);
void modelViewTick();
void leaveModelView();
bool wolframLookUp(const String& englishQuery);
bool wolframSwitchToPicture();
bool fetchWolframSimpleImage(const String &englishQuery);
void modelGyroNoise();
extern bool modelResetRotateToggle;
void setGyroEnabled(bool on);
void updateGyroPower();
bool gyroReady();
void sendSetupFont(const unsigned char* data, unsigned int len);
bool downloadImage(const String &url);
bool generateImageWithFallback(const String &prompt);

// Setup page (WiFi, AI provider, device settings) in zitro.spot style
const char* setupPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>zitro.spot setup</title>
  <link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='15' fill='%23f4efe6'/%3E%3Cpath d='M16 20h22L18 44h22' fill='none' stroke='%232b2721' stroke-width='8' stroke-linecap='round' stroke-linejoin='round'/%3E%3Ccircle cx='49' cy='41' r='5' fill='%23f2b705'/%3E%3C/svg%3E">
  <style>
    @font-face { font-family: "Sniglet"; font-weight: 400; font-display: swap; src: url(/f/sniglet-400.woff2) format("woff2"); }
    @font-face { font-family: "Sniglet"; font-weight: 800; font-display: swap; src: url(/f/sniglet-800.woff2) format("woff2"); }
    :root { --paper:#f4efe6; --paper-deep:#ece5d8; --card:#fbf8f1; --ink:#2b2721; --ink-soft:#6b6459;
            --line:rgba(43,39,33,.12); --citrus:#f2b705; --citrus-soft:#fbedc2; --citrus-deep:#b98a00;
            --leaf:#6f9c4f; --danger:#b4432f; }
    * { box-sizing: border-box; }
    body { font-family: "Instrument Sans", -apple-system, "Helvetica Neue", "Segoe UI", Arial, sans-serif;
           background-color: var(--paper); color: var(--ink); padding: 20px 16px 40px; margin: 0; line-height: 1.55;
           -webkit-font-smoothing: antialiased;
           background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='140' height='140'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='140' height='140' filter='url(%23n)' opacity='0.028'/%3E%3C/svg%3E"); }
    .container { max-width: 520px; margin: 0 auto; }
    .wordmark { font-family: "Sniglet", "Arial Rounded MT Bold", "Comic Sans MS", sans-serif; font-weight: 800;
                font-size: 34px; text-align: center; margin: 10px 0 6px; line-height: 1.1; }
    .wordmark .dot { color: var(--citrus); }
    .wordmark .tld { color: var(--ink-soft); font-weight: 400; }
    .tagline { text-align: center; color: var(--citrus-deep); font-family: "Fragment Mono", ui-monospace, "SF Mono", Menlo, monospace;
               font-size: 12px; letter-spacing: .12em; text-transform: uppercase; margin: 0 0 22px; }
    h2 { font-family: "Sniglet", "Arial Rounded MT Bold", sans-serif; font-weight: 400; font-size: 23px; line-height: 1.15;
         margin: 26px 0 6px; }
    .info { color: var(--ink-soft); font-size: 14px; margin-bottom: 12px; }
    .note { background: var(--citrus-soft); border-radius: 14px; padding: 10px 14px; font-size: 14px; margin: 12px 0; }
    .ok { background: #e9f1de; }
    input, select { width: 100%; padding: 12px 14px; margin: 6px 0; border-radius: 12px; border: 1px solid var(--line);
                    background: #fff; color: var(--ink); font-size: 16px; }
    input:focus, select:focus { outline: 2px solid var(--citrus); border-color: transparent; }
    input[type=submit], .btn { background: var(--citrus); color: var(--ink); cursor: pointer; font-weight: 600; border: none;
                               padding: 14px 32px; display: block; width: auto; min-width: 190px; margin: 16px auto 4px; font-size: 16px; font-family: inherit;
                               border-radius: 55% 45% 57% 43% / 56% 48% 52% 44%; transform: rotate(-0.6deg);
                               box-shadow: 0 1px 2px rgba(43,39,33,.05), 0 12px 32px -16px rgba(43,39,33,.18);
                               transition: transform .2s cubic-bezier(.22,1,.36,1), box-shadow .2s; }
    input[type=submit]:hover, .btn:hover { background: #f7c62a; transform: rotate(-0.6deg) translateY(-1px);
                                           box-shadow: 0 2px 4px rgba(43,39,33,.06), 0 20px 44px -18px rgba(43,39,33,.26); }
    .btn-accent { background: var(--citrus); color: var(--ink); }
    label { display: block; color: var(--ink-soft); font-size: 14px; margin-top: 12px; }
    .checkbox-group { display: flex; align-items: center; margin: 12px 0; }
    .checkbox-group input[type=checkbox] { width: 20px; height: 20px; margin: 0 10px 0 0; accent-color: var(--ink); }
    .checkbox-group label { margin: 0; color: var(--ink); }
    .card { background: var(--card); padding: 20px; border-radius: 28px; border: 1px solid var(--line);
            box-shadow: 0 1px 2px rgba(43,39,33,.05), 0 12px 32px -16px rgba(43,39,33,.18); margin-top: 8px; }
    .network-item { background: #fff; padding: 10px 12px; margin: 8px 0; border-radius: 14px; border: 1px solid var(--line);
                    display: flex; justify-content: space-between; align-items: center; }
    .network-name { flex-grow: 1; font-size: 16px; }
    .network-order { color: var(--ink-soft); margin-right: 10px; font-size: 14px; }
    .network-actions { display: flex; gap: 6px; }
    .btn-small { padding: 6px 12px; font-size: 14px; background: var(--paper-deep); color: var(--ink); border: none;
                 border-radius: 999px; cursor: pointer; }
    .btn-delete { background: #f3d9d3; color: var(--danger); }
    .loading { color: var(--ink-soft); font-style: italic; }
    .hidden { display: none; }
    .tabs { display: flex; gap: 4px; background: var(--paper-deep); padding: 4px; border-radius: 999px; margin-bottom: 16px; }
    .tab { background: transparent; color: var(--ink-soft); padding: 10px 8px; border: none; cursor: pointer; font-size: 15px;
           font-weight: 600; flex: 1; border-radius: 999px; }
    .tab.active { background: var(--card); color: var(--ink); box-shadow: 0 1px 3px rgba(43,39,33,.15); }
    .tab-content { display: none; }
    .tab-content.active { display: block; }
    a { color: var(--ink); }
    .key-state { font-size: 13px; color: var(--leaf); }
    .tutorial-overlay { position: fixed; inset: 0; background: rgba(43,39,33,.55); z-index: 1000; display: none;
                        align-items: center; justify-content: center; padding: 20px; }
    .tutorial-overlay.active { display: flex; }
    .tutorial-content { background: var(--card); padding: 28px; border-radius: 28px; max-width: 400px; width: 100%;
                        box-shadow: 0 2px 4px rgba(43,39,33,.06), 0 20px 44px -18px rgba(43,39,33,.26); }
    .tutorial-title { font-family: "Sniglet", "Arial Rounded MT Bold", sans-serif; font-weight: 400; font-size: 28px; line-height: 1.15; margin-bottom: 10px; }
    .tutorial-text { color: var(--ink-soft); font-size: 16px; margin-bottom: 12px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="wordmark" id="pageTitle">zitro<span class="dot">.</span><span class="tld">spot</span></div>
    <p class="tagline" id="tagline">Ask out loud. Then look up.</p>

    <div class="tabs">
      <button class="tab active" id="tabWifi" onclick="switchTab('wifi')">WiFi</button>
      <button class="tab" id="tabAi" onclick="switchTab('ai')">AI</button>
      <button class="tab" id="tabSettings" onclick="switchTab('settings')">Settings</button>
    </div>

    <!-- WiFi Tab -->
    <div class="tab-content active" id="contentWifi">
      <h2 id="storedNetworksTitle">Stored networks</h2>
      <div class="info" id="storedNetworksInfo">Networks are tried in order. Use ↑↓ to reorder.</div>
      <div id="networkList"><div class="loading" id="loadingText">Loading...</div></div>

      <h2 id="addNetworkTitle">Add a network</h2>
      <div class="info" id="addNetworkInfo">Up to 5 WiFi networks.</div>
      <form action="/save" method="POST" class="card">
        <input type="text" name="ssid" id="ssidInput" placeholder="WiFi name" required>
        <input type="password" name="pass" id="passInput" placeholder="WiFi password" required>
        <input type="submit" id="addNetworkBtn" value="Add network">
      </form>
    </div>

    <!-- AI Tab -->
    <div class="tab-content" id="contentAi">
      <h2 id="aiTitle">Which AI should answer?</h2>
      <div class="info" id="aiInfo">Pick a provider and paste its key. Keys stay on the device and are never shown again.</div>
      <div class="note ok hidden" id="aiReadyNote"></div>
      <form id="aiForm" class="card">
        <label for="provider" id="labelProvider">Provider</label>
        <select id="provider" name="provider">
          <option value="zitro" disabled>Zitro subscription (coming soon)</option>
          <option value="openai">OpenAI (ChatGPT)</option>
          <option value="anthropic">Anthropic (Claude)</option>
          <option value="infomaniak">Infomaniak AI (Switzerland)</option>
          <option value="custom">Other (OpenAI-compatible)</option>
        </select>

        <div class="provider-fields" data-provider="zitro">
          <label for="zitroKey" id="labelZitroKey">Zitro key</label>
          <input type="password" id="zitroKey" placeholder="zk_..." autocomplete="off">
          <div class="info" id="zitroInfo">One key for answers, voice and pictures. Billed monthly, fair use.</div>
        </div>

        <div class="provider-fields" data-provider="openai">
          <label for="oaModel" id="labelOaModel">Model</label>
          <input id="oaModel" list="oaModels" placeholder="gpt-6-luna">
          <datalist id="oaModels">
            <option value="gpt-6-luna"><option value="gpt-6.1-terra"><option value="gpt-6-astra"><option value="gpt-5.5">
          </datalist>
        </div>

        <div class="provider-fields" data-provider="anthropic">
          <label for="claudeKey" id="labelClaudeKey">Claude API key</label>
          <input type="password" id="claudeKey" placeholder="sk-ant-..." autocomplete="off">
          <div class="info"><a href="https://platform.claude.com/settings/keys" target="_blank">platform.claude.com</a></div>
          <label for="claudeModel" id="labelClaudeModel">Model</label>
          <input id="claudeModel" list="claudeModels" placeholder="claude-opus-5-5">
          <datalist id="claudeModels">
            <option value="claude-opus-5-5"><option value="claude-sonnet-5-5"><option value="claude-haiku-5-5">
          </datalist>
        </div>

        <div class="provider-fields" data-provider="infomaniak">
          <label for="ikKey" id="labelIkKey">Infomaniak API token</label>
          <input type="password" id="ikKey" placeholder="Token with ai-tools scope" autocomplete="off">
          <label for="ikProduct" id="labelIkProduct">AI Tools product ID</label>
          <input id="ikProduct" inputmode="numeric" placeholder="123456">
          <label for="ikModel" id="labelIkModel">Model</label>
          <input id="ikModel" list="ikModels" placeholder="qwen3">
          <datalist id="ikModels">
            <option value="qwen3"><option value="mistral3"><option value="llama3"><option value="gemma3n">
          </datalist>
          <div class="info"><a href="https://manager.infomaniak.com" target="_blank">manager.infomaniak.com</a> → AI Tools</div>
        </div>

        <div class="provider-fields" data-provider="custom">
          <label for="customUrl" id="labelCustomUrl">API base URL</label>
          <input id="customUrl" placeholder="https://api.mistral.ai/v1">
          <label for="customKey" id="labelCustomKey">API key</label>
          <input type="password" id="customKey" autocomplete="off">
          <label for="customModel" id="labelCustomModel">Model</label>
          <input id="customModel" placeholder="mistral-small-latest">
        </div>

        <div id="openaiKeyBox">
          <label for="apiKey" id="labelApiKey">OpenAI API key</label>
          <input type="password" id="apiKey" placeholder="sk-..." autocomplete="off">
          <div class="info" id="speechInfo">Needed for voice input, read-aloud and pictures.</div>
          <div class="info"><a href="https://platform.openai.com/api-keys" target="_blank">platform.openai.com</a></div>
        </div>

        <button type="submit" class="btn btn-accent" id="saveAiBtn">Save AI settings</button>
      </form>
    </div>

    <!-- Settings Tab -->
    <div class="tab-content" id="contentSettings">
      <h2 id="deviceSettingsTitle">Device settings</h2>
      <form id="settingsForm" class="card">
        <div class="checkbox-group">
          <input type="checkbox" id="memory" name="memory" checked>
          <label for="memory" id="labelMemory">Remember the last 10 minutes (follow-up questions)</label>
        </div>

        <label for="language" id="labelLanguage">Language</label>
        <select id="language" name="language">
          <option value="DE">Deutsch (DE)</option>
          <option value="EN">English (EN)</option>
        </select>

        <label for="recordSec" id="labelRecordSec">Recording duration (seconds)</label>
        <input type="number" id="recordSec" min="1" max="30" value="5" required>

        <label for="sampleRate" id="labelSampleRate">Audio quality</label>
        <select id="sampleRate">
          <option value="8000" id="optSampleRate8k">Standard (8 kHz), faster</option>
          <option value="16000" id="optSampleRate16k">High (16 kHz), best quality</option>
        </select>

        <label for="volume" id="labelVolume">Volume (0-255)</label>
        <input type="number" id="volume" min="0" max="255" value="210" required>

        <label for="dispSleep" id="labelDispSleep">Display sleep (seconds)</label>
        <input type="number" id="dispSleep" min="5" max="300" value="30" required>
        <label for="lightSleep" id="labelLightSleep">Light sleep (seconds)</label>
        <input type="number" id="lightSleep" min="10" max="600" value="90" required>
        <label for="deepSleep" id="labelDeepSleep">Deep sleep (seconds)</label>
        <input type="number" id="deepSleep" min="30" max="1800" value="180" required>

        <div class="checkbox-group"><input type="checkbox" id="searchImg" checked><label for="searchImg" id="labelSearchImg">Wikipedia images</label></div>
        <div class="checkbox-group"><input type="checkbox" id="useWolfram" checked><label for="useWolfram" id="labelUseWolfram">Wolfram Alpha math</label></div>
        <div class="checkbox-group"><input type="checkbox" id="localQuotes" checked><label for="localQuotes" id="labelLocalQuotes">Local quotes</label></div>
        <div class="checkbox-group"><input type="checkbox" id="parentLock"><label for="parentLock" id="labelParentLock">Parental lock (8pm-6am)</label></div>
        <div class="checkbox-group"><input type="checkbox" id="shakeRandom" checked><label for="shakeRandom" id="labelShakeRandom">Shake for random fact</label></div>
        <div class="checkbox-group"><input type="checkbox" id="tripleAudio" checked><label for="tripleAudio" id="labelTripleAudio">Double-click for audio</label></div>
        <div class="checkbox-group"><input type="checkbox" id="autoAudio"><label for="autoAudio" id="labelAutoAudio">Auto-audio for music/stories</label></div>
        <div class="checkbox-group"><input type="checkbox" id="moveZoom" checked><label for="moveZoom" id="labelMoveZoom">Roll to zoom</label></div>
        <div class="checkbox-group"><input type="checkbox" id="moveToggle" checked><label for="moveToggle" id="labelMoveToggle">Rotate to toggle</label></div>

        <button type="submit" class="btn" id="saveSettingsBtn">Save settings</button>
      </form>
    </div>
  </div>

  <!-- First-run welcome -->
  <div class="tutorial-overlay" id="tutorialOverlay">
    <div class="tutorial-content">
      <div class="tutorial-title" id="tutorialTitle1">Hi, I'm Zitro.</div>
      <div class="tutorial-text" id="tutorialText1">Two quick steps: choose which AI answers your questions, then connect me to your WiFi.</div>
      <button class="btn btn-accent" onclick="startTutorial()" id="tutorialBtn1">Let's start</button>
    </div>
  </div>

  <script>
    var networks = [];
    var currentLang = 'EN';
    var firstRun = false;
    var BOOL_KEYS = ['memory','searchImg','useWolfram','localQuotes','parentLock','shakeRandom','tripleAudio','autoAudio','moveZoom','moveToggle'];
    var SECRET_KEYS = ['apiKey','claudeKey','ikKey','customKey','zitroKey'];
    var TEXT_KEYS = ['oaModel','claudeModel','ikModel','ikProduct','customUrl','customModel'];

    var t = {
      EN: {
        tagline: 'Ask out loud. Then look up.',
        tabWifi: 'WiFi', tabAi: 'AI', tabSettings: 'Settings',
        storedNetworksTitle: 'Stored networks', storedNetworksInfo: 'Networks are tried in order. Use ↑↓ to reorder.',
        addNetworkTitle: 'Add a network', addNetworkInfo: 'Up to 5 WiFi networks.',
        ssidInput: 'WiFi name', passInput: 'WiFi password', addNetworkBtn: 'Add network',
        aiTitle: 'Which AI should answer?',
        aiInfo: 'Pick a provider and paste its key. Keys stay on the device and are never shown again.',
        labelProvider: 'Provider', labelZitroKey: 'Zitro key',
        zitroInfo: 'One key for answers, voice and pictures. Billed monthly, fair use.',
        labelOaModel: 'Model', labelClaudeKey: 'Claude API key', labelClaudeModel: 'Model',
        labelIkKey: 'Infomaniak API token', labelIkProduct: 'AI Tools product ID', labelIkModel: 'Model',
        labelCustomUrl: 'API base URL', labelCustomKey: 'API key', labelCustomModel: 'Model',
        labelApiKey: 'OpenAI API key', labelApiKeySpeech: 'OpenAI API key (for voice)',
        speechInfo: 'Needed for voice input, read-aloud and pictures.',
        speechInfoOther: 'This provider cannot hear or speak, so voice input, read-aloud and pictures still use OpenAI.',
        saveAiBtn: 'Save AI settings', keySaved: 'saved', keyKeep: 'Saved key {k} - leave empty to keep',
        aiReady: 'Ready: answers and voice are set up.', aiNoSpeech: 'Answers are set up, but voice needs an OpenAI key.',
        deviceSettingsTitle: 'Device settings',
        labelMemory: 'Remember the last 10 minutes (follow-up questions)',
        labelLanguage: 'Language', labelRecordSec: 'Recording duration (seconds)', labelSampleRate: 'Audio quality',
        optSampleRate8k: 'Standard (8 kHz), faster', optSampleRate16k: 'High (16 kHz), best quality',
        labelVolume: 'Volume (0-255)', labelDispSleep: 'Display sleep (seconds)',
        labelLightSleep: 'Light sleep (seconds)', labelDeepSleep: 'Deep sleep (seconds)',
        labelSearchImg: 'Wikipedia images', labelUseWolfram: 'Wolfram Alpha math', labelLocalQuotes: 'Local quotes',
        labelParentLock: 'Parental lock (8pm-6am)', labelShakeRandom: 'Shake for random fact',
        labelTripleAudio: 'Double-click for audio', labelAutoAudio: 'Auto-audio for music/stories',
        labelMoveZoom: 'Roll to zoom', labelMoveToggle: 'Rotate to toggle', saveSettingsBtn: 'Save settings',
        tutorialTitle1: "Hi, I'm Zitro.",
        tutorialText1: 'Two quick steps: choose which AI answers your questions, then connect me to your WiFi.',
        tutorialBtn1: "Let's start",
        loadingText: 'Loading...', noNetworks: 'No networks stored yet.',
        saved: 'Saved!', aiSavedNext: 'Saved! Now add your WiFi network.', saveFailed: 'Could not save. Try again.',
        needKey: 'Please paste a key first.',
        restartNow: 'Saved! Restart Zitro now to start asking questions?', restarting: 'Zitro is restarting...'
      },
      DE: {
        tagline: 'Laut fragen. Dann wieder aufschauen.',
        tabWifi: 'WLAN', tabAi: 'KI', tabSettings: 'Einstellungen',
        storedNetworksTitle: 'Gespeicherte Netzwerke', storedNetworksInfo: 'Netzwerke werden der Reihe nach versucht. Mit ↑↓ ordnen.',
        addNetworkTitle: 'Netzwerk hinzufügen', addNetworkInfo: 'Bis zu 5 WLAN-Netzwerke.',
        ssidInput: 'WLAN-Name', passInput: 'WLAN-Passwort', addNetworkBtn: 'Hinzufügen',
        aiTitle: 'Welche KI soll antworten?',
        aiInfo: 'Anbieter wählen und Schlüssel einfügen. Schlüssel bleiben auf dem Gerät und werden nie wieder angezeigt.',
        labelProvider: 'Anbieter', labelZitroKey: 'Zitro-Schlüssel',
        zitroInfo: 'Ein Schlüssel für Antworten, Sprache und Bilder. Monatlich, faire Nutzung.',
        labelOaModel: 'Modell', labelClaudeKey: 'Claude API-Schlüssel', labelClaudeModel: 'Modell',
        labelIkKey: 'Infomaniak API-Token', labelIkProduct: 'AI-Tools Produkt-ID', labelIkModel: 'Modell',
        labelCustomUrl: 'API-Basis-URL', labelCustomKey: 'API-Schlüssel', labelCustomModel: 'Modell',
        labelApiKey: 'OpenAI API-Schlüssel', labelApiKeySpeech: 'OpenAI API-Schlüssel (für Sprache)',
        speechInfo: 'Für Spracheingabe, Vorlesen und Bilder.',
        speechInfoOther: 'Dieser Anbieter kann nicht hören oder sprechen. Spracheingabe, Vorlesen und Bilder laufen weiter über OpenAI.',
        saveAiBtn: 'KI-Einstellungen speichern', keySaved: 'gespeichert', keyKeep: 'Gespeicherter Schlüssel {k} - leer lassen zum Behalten',
        aiReady: 'Bereit: Antworten und Sprache sind eingerichtet.', aiNoSpeech: 'Antworten sind eingerichtet, Sprache braucht einen OpenAI-Schlüssel.',
        deviceSettingsTitle: 'Geräteeinstellungen',
        labelMemory: 'Die letzten 10 Minuten merken (Folgefragen)',
        labelLanguage: 'Sprache', labelRecordSec: 'Aufnahmedauer (Sekunden)', labelSampleRate: 'Audioqualität',
        optSampleRate8k: 'Standard (8 kHz), schneller', optSampleRate16k: 'Hoch (16 kHz), beste Qualität',
        labelVolume: 'Lautstärke (0-255)', labelDispSleep: 'Display-Ruhemodus (Sek.)',
        labelLightSleep: 'Leichter Ruhemodus (Sek.)', labelDeepSleep: 'Tiefer Ruhemodus (Sek.)',
        labelSearchImg: 'Wikipedia-Bilder', labelUseWolfram: 'Wolfram Alpha Mathematik', labelLocalQuotes: 'Lokale Zitate',
        labelParentLock: 'Kindersicherung (20-6 Uhr)', labelShakeRandom: 'Schütteln für Fakten',
        labelTripleAudio: 'Doppelklick für Audio', labelAutoAudio: 'Auto-Audio für Musik/Geschichten',
        labelMoveZoom: 'Rollen zum Zoomen', labelMoveToggle: 'Drehen zum Umschalten', saveSettingsBtn: 'Speichern',
        tutorialTitle1: 'Hallo, ich bin Zitro.',
        tutorialText1: 'Zwei kurze Schritte: Wähle, welche KI antwortet, und verbinde mich dann mit deinem WLAN.',
        tutorialBtn1: 'Los geht\'s',
        loadingText: 'Lädt...', noNetworks: 'Noch keine Netzwerke gespeichert.',
        saved: 'Gespeichert!', aiSavedNext: 'Gespeichert! Jetzt noch dein WLAN hinzufügen.', saveFailed: 'Speichern fehlgeschlagen.',
        needKey: 'Bitte zuerst einen Schlüssel einfügen.',
        restartNow: 'Gespeichert! Zitro jetzt neu starten und loslegen?', restarting: 'Zitro startet neu...'
      }
    };

    var $ = function(id) { return document.getElementById(id); };

    var detectLanguage = function() {
      var lang = (navigator.language || '').toLowerCase();
      return lang.indexOf('de') === 0 ? 'DE' : 'EN';
    };

    var applyTranslations = function() {
      var tr = t[currentLang];
      Object.keys(tr).forEach(function(key) {
        var el = $(key);
        if (!el) return;
        if (el.tagName === 'INPUT' && el.type === 'submit') el.value = tr[key];
        else if (el.tagName === 'INPUT') el.placeholder = tr[key];
        else el.textContent = tr[key];
      });
    };

    var switchTab = function(tab) {
      var tabs = {wifi:['tabWifi','contentWifi'], ai:['tabAi','contentAi'], settings:['tabSettings','contentSettings']};
      document.querySelectorAll('.tab').forEach(function(el){ el.classList.remove('active'); });
      document.querySelectorAll('.tab-content').forEach(function(el){ el.classList.remove('active'); });
      $(tabs[tab][0]).classList.add('active');
      $(tabs[tab][1]).classList.add('active');
    };

    // Show only the fields of the chosen provider. Claude, Infomaniak and custom
    // providers have no speech or image API, so they still need an OpenAI key.
    var updateProviderFields = function() {
      var p = $('provider').value;
      document.querySelectorAll('.provider-fields').forEach(function(el) {
        el.classList.toggle('hidden', el.getAttribute('data-provider') !== p);
      });
      var tr = t[currentLang];
      $('openaiKeyBox').classList.toggle('hidden', p === 'zitro');
      $('labelApiKey').textContent = p === 'openai' ? tr.labelApiKey : tr.labelApiKeySpeech;
      $('speechInfo').textContent = p === 'openai' ? tr.speechInfo : tr.speechInfoOther;
    };

    var startTutorial = function() {
      $('tutorialOverlay').classList.remove('active');
      switchTab('ai');
    };

    var post = function(params) {
      return fetch('/savesettings', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: params.toString()
      }).then(function(r) { if (!r.ok) throw new Error('save'); });
    };

    var loadSettings = function() {
      fetch('/settings')
        .then(function(r) { return r.json(); })
        .then(function(data) {
          var tr = t[currentLang];
          $('provider').value = data.provider || 'openai';
          TEXT_KEYS.forEach(function(k) { $(k).value = data[k] || ''; });
          SECRET_KEYS.forEach(function(k) {
            var hint = data.keys && data.keys[k];
            $(k).value = '';
            if (hint) $(k).placeholder = tr.keyKeep.replace('{k}', hint === 'set' ? '' : hint);
          });
          $('recordSec').value = data.recordSec || 5;
          $('sampleRate').value = data.sampleRate || 8000;
          $('language').value = data.language || 'DE';
          $('volume').value = data.volume != null ? data.volume : 210;
          $('dispSleep').value = (data.dispSleep || 30000) / 1000;
          $('lightSleep').value = (data.lightSleep || 90000) / 1000;
          $('deepSleep').value = (data.deepSleep || 180000) / 1000;
          BOOL_KEYS.forEach(function(k) { if (k in data) $(k).checked = data[k] === true; });
          updateProviderFields();

          var note = $('aiReadyNote');
          if (data.aiReady) {
            note.textContent = data.speechReady ? tr.aiReady : tr.aiNoSpeech;
            note.classList.remove('hidden');
            note.classList.toggle('ok', !!data.speechReady);
          }
          if (!data.aiReady) {
            firstRun = true;
            $('tutorialOverlay').classList.add('active');
          }
        })
        .catch(function() { updateProviderFields(); $('tutorialOverlay').classList.add('active'); });
    };

    $('provider').addEventListener('change', updateProviderFields);

    $('aiForm').addEventListener('submit', function(e) {
      e.preventDefault();
      var p = $('provider').value;
      var params = new URLSearchParams();
      params.append('provider', p);
      TEXT_KEYS.forEach(function(k) { params.append(k, $(k).value.trim()); });
      SECRET_KEYS.forEach(function(k) { if ($(k).value.trim()) params.append(k, $(k).value.trim()); });
      var mainKey = {zitro:'zitroKey', openai:'apiKey', anthropic:'claudeKey', infomaniak:'ikKey', custom:'customKey'}[p];
      if (firstRun && !$(mainKey).value.trim()) { alert(t[currentLang].needKey); return; }
      post(params).then(function() {
        if (firstRun && networks.length === 0) {
          alert(t[currentLang].aiSavedNext);
          switchTab('wifi');
        } else if (networks.length > 0 && confirm(t[currentLang].restartNow)) {
          fetch('/restart', {method: 'POST'});
          alert(t[currentLang].restarting);
          return;
        } else {
          alert(t[currentLang].saved);
        }
        loadSettings();
      }).catch(function() { alert(t[currentLang].saveFailed); });
    });

    $('settingsForm').addEventListener('submit', function(e) {
      e.preventDefault();
      var params = new URLSearchParams();
      ['recordSec','sampleRate','language','volume'].forEach(function(k) { params.append(k, $(k).value); });
      ['dispSleep','lightSleep','deepSleep'].forEach(function(k) { params.append(k, $(k).value * 1000); });
      BOOL_KEYS.forEach(function(k) { params.append(k, $(k).checked ? '1' : '0'); });
      post(params)
        .then(function() { alert(t[currentLang].saved); })
        .catch(function() { alert(t[currentLang].saveFailed); });
    });

    var loadNetworks = function() {
      fetch('/networks')
        .then(function(r) { return r.json(); })
        .then(function(data) { networks = data; renderNetworks(); })
        .catch(function() { networks = []; renderNetworks(); });
    };

    var renderNetworks = function() {
      var list = $('networkList');
      list.textContent = '';
      if (networks.length === 0) {
        var info = document.createElement('div');
        info.className = 'info';
        info.textContent = t[currentLang].noNetworks;
        list.appendChild(info);
        return;
      }
      networks.forEach(function(net, idx) {
        var item = document.createElement('div');
        item.className = 'network-item';
        var order = document.createElement('span');
        order.className = 'network-order';
        order.textContent = (idx + 1) + '.';
        var name = document.createElement('span');
        name.className = 'network-name';
        name.textContent = net.ssid;
        var actions = document.createElement('div');
        actions.className = 'network-actions';
        if (idx > 0) actions.appendChild(smallButton('↑', '', function() { move(idx, -1); }));
        if (idx < networks.length - 1) actions.appendChild(smallButton('↓', '', function() { move(idx, 1); }));
        actions.appendChild(smallButton('×', ' btn-delete', function() { deleteNetwork(idx); }));
        item.appendChild(order);
        item.appendChild(name);
        item.appendChild(actions);
        list.appendChild(item);
      });
    };

    var smallButton = function(label, extraClass, onClick) {
      var b = document.createElement('button');
      b.className = 'btn-small' + extraClass;
      b.textContent = label;
      b.addEventListener('click', onClick);
      return b;
    };

    var move = function(idx, dir) {
      var other = idx + dir;
      var tmp = networks[idx];
      networks[idx] = networks[other];
      networks[other] = tmp;
      var order = networks.map(function(n) { return n.index; }).join(',');
      fetch('/reorder', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'order=' + order
      }).then(function() {
        networks = networks.map(function(n, i) { return {ssid: n.ssid, index: i}; });
        renderNetworks();
      });
    };

    var deleteNetwork = function(idx) {
      if (!confirm('Delete "' + networks[idx].ssid + '"?')) return;
      fetch('/delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'index=' + networks[idx].index
      }).then(function() {
        networks.splice(idx, 1);
        networks = networks.map(function(n, i) { return {ssid: n.ssid, index: i}; });
        renderNetworks();
      });
    };

    currentLang = detectLanguage();
    applyTranslations();
    loadSettings();
    loadNetworks();
  </script>
</body>
</html>
)rawliteral";

const char* successPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    body { font-family: -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; background: #f4efe6; color: #2b2721;
           padding: 40px 20px; text-align: center; }
    @font-face { font-family: "Sniglet"; font-weight: 400; src: url(/f/sniglet-400.woff2) format("woff2"); }
    h1 { font-family: "Sniglet", "Arial Rounded MT Bold", sans-serif; font-weight: 400; font-size: 34px; line-height: 1.15; }
    h1 span { color: #f2b705; }
    .info { color: #6b6459; font-size: 14px; margin-top: 20px; }
  </style>
</head>
<body>
  <h1>Network added<span>.</span></h1>
  <p>Zitro is restarting...</p>
  <div class="info">
    Zitro connects to any known network automatically.
    You can add more networks by opening this setup page again.
  </div>
</body>
</html>
)rawliteral";

void drawBatteryIndicator(M5Canvas &canvas) {
  // Get battery level (0-100%)
  int batteryLevel = M5.Power.getBatteryLevel();

  // Only show battery indicator when below 35%
  if (batteryLevel >= 35) {
    return;
  }

  // Battery indicator position (top right corner)
  int startX = WIDTH - 25;
  int startY = 2;
  int rectSize = 5;
  int spacing = 2;

  // Draw 3 rectangles
  for (int i = 0; i < 3; i++) {
    int x = startX + i * (rectSize + spacing);

    // Determine if this bar should be filled based on battery level
    bool filled = false;
    if (i == 0 && batteryLevel >= 33) filled = true;
    if (i == 1 && batteryLevel >= 66) filled = true;
    if (i == 2 && batteryLevel >= 99) filled = true;

    if (filled) {
      canvas.fillRect(x, startY, rectSize, rectSize, TFT_WHITE);
    } else {
      canvas.drawRect(x, startY, rectSize, rectSize, TFT_WHITE);
    }
  }
}

void drawIcon(M5Canvas &canvas, const uint8_t* iconData, int iconWidth, int iconHeight, int x, int y, uint8_t brightness = 255) {
  // Draw a bitmap icon at specified position with optional brightness/alpha
  for (int py = 0; py < iconHeight; py++) {
    for (int px = 0; px < iconWidth; px++) {
      int bytesPerRow = (iconWidth + 7) / 8;
      int byteIndex = py * bytesPerRow + (px / 8);
      int bitIndex = 7 - (px % 8);
      uint8_t byte = pgm_read_byte(&iconData[byteIndex]);
      if (byte & (1 << bitIndex)) {
        // Create grayscale color based on brightness
        uint16_t color = canvas.color565(brightness, brightness, brightness);
        canvas.drawPixel(x + px, y + py, color);
      }
    }
  }
}

void drawIconScaled(M5Canvas &canvas, const uint8_t* iconData, int iconWidth, int iconHeight,
                    int x, int y, int scale, uint16_t color);

// scale: 1 = native 20px icon, 2 = large icon (same size as the printer screens)
void drawScreenWithIcon(const String &text, const uint8_t* iconData, int iconWidth, int iconHeight, int scale = 1) {
  M5.Display.setRotation(getLandscapeRotation());
  // Draw screen with pulsating icon effect (3 frames for simple animation)
  for (int pulse = 0; pulse < 3; pulse++) {
    M5Canvas canvas(&M5.Display);
    canvas.createSprite(WIDTH, HEIGHT);
    canvas.fillSprite(TFT_BLACK);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.setFreeFont(FontManager::getFont16pt());  // Use larger custom font with umlaut support

    // Calculate brightness for pulsation
    uint8_t brightness;
    if (pulse == 0) brightness = 180;      // Dim
    else if (pulse == 1) brightness = 255; // Bright
    else brightness = 220;                  // Medium

    // Calculate total width of icon + gap + text for centering
    int drawW = iconWidth * scale;
    int drawH = iconHeight * scale;
    int gap = (scale > 1) ? 16 : 10;  // same gap as the printer screens when large
    int textWidth = canvas.textWidth(text);
    int totalWidth = drawW + gap + textWidth;
    int startX = (WIDTH - totalWidth) / 2;
    if (startX < 6) startX = 6;

    int iconX = startX;
    int iconY = HEIGHT / 2 - drawH / 2;
    drawIconScaled(canvas, iconData, iconWidth, iconHeight, iconX, iconY, scale,
                   canvas.color565(brightness, brightness, brightness));

    int textX = startX + drawW + gap;
    canvas.setTextDatum(ML_DATUM);  // Middle-left alignment for text
    canvas.drawString(text, textX, HEIGHT / 2);

    drawBatteryIndicator(canvas);
    captureCanvas(canvas);
    canvas.pushSprite(0, 0);
    canvas.deleteSprite();

    delay(150); // Hold each frame for 150ms
  }
}

void drawVoicePlaybackIndicator() {
  // Draw pulsating voice icon at top left to indicate audio is playing
  static unsigned long lastPulseTime = 0;
  static float pulsePhase = 0.0;

  unsigned long currentTime = millis();
  if (currentTime - lastPulseTime > 30) {
    lastPulseTime = currentTime;
    pulsePhase += 0.15; // Faster pulse for voice
    if (pulsePhase > 6.28) pulsePhase = 0;
  }

  uint8_t brightness = 128 + (uint8_t)(127.0 * sin(pulsePhase));

  M5Canvas canvas(&M5.Display);
  canvas.createSprite(VOICEICON_WIDTH + 4, VOICEICON_HEIGHT + 4);
  canvas.fillSprite(TFT_BLACK);
  drawIcon(canvas, voiceIcon, VOICEICON_WIDTH, VOICEICON_HEIGHT, 2, 2, brightness);
  canvas.pushSprite(2, 2);  // Top left corner
  canvas.deleteSprite();
}

void clearVoicePlaybackIndicator() {
  // Clear the voice playback indicator by drawing a black rectangle over it
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(VOICEICON_WIDTH + 4, VOICEICON_HEIGHT + 4);
  canvas.fillSprite(TFT_BLACK);
  canvas.pushSprite(2, 2);  // Top left corner
  canvas.deleteSprite();
}

// Helper function to wrap text to fit screen width
String wrapText(M5Canvas& canvas, const String& text, int maxWidth) {
  String result = "";
  String currentLine = "";
  int currentWidth = 0;
  int spaceWidth = canvas.textWidth(" ");

  // Process text word by word
  int wordStart = 0;
  for (int i = 0; i <= text.length(); i++) {
    // Check for word boundary (space, newline, or end of text)
    if (i == text.length() || text[i] == ' ' || text[i] == '\n') {
      if (i > wordStart) {
        String word = text.substring(wordStart, i);
        int wordWidth = canvas.textWidth(word);

        // Check if adding this word would exceed line width
        if (currentWidth + wordWidth > maxWidth && currentLine.length() > 0) {
          // Start new line
          result += currentLine + "\n";
          currentLine = word;
          currentWidth = wordWidth;
        } else {
          // Add word to current line
          if (currentLine.length() > 0) {
            currentLine += " ";
            currentWidth += spaceWidth;
          }
          currentLine += word;
          currentWidth += wordWidth;
        }
      }

      // Handle explicit newlines
      if (i < text.length() && text[i] == '\n') {
        result += currentLine + "\n";
        currentLine = "";
        currentWidth = 0;
      }

      wordStart = i + 1;
    }
  }

  // Add remaining text
  if (currentLine.length() > 0) {
    result += currentLine;
  }

  return result;
}

// Vertical layout for wrapped text. Blank lines (paragraph breaks) take half a line, and when
// the text is too tall for maxHeight the line spacing is tightened step by step so it still fits.
struct TextLayout {
  int lineHeight;
  int blankHeight;
  int totalHeight;
};

TextLayout layoutWrappedText(M5Canvas& canvas, const String& wrappedText, int maxHeight) {
  int textLines = 0;
  int blankLines = 0;
  int lineStart = 0;
  for (unsigned int i = 0; i <= wrappedText.length(); i++) {
    if (i == wrappedText.length() || wrappedText[i] == '\n') {
      if ((int)i > lineStart) textLines++; else blankLines++;
      lineStart = i + 1;
    }
  }

  const int fontHeight = canvas.fontHeight();
  const int normalLineHeight = fontHeight + 4;
  const int densestLineHeight = fontHeight - 3;  // Tight, but glyphs still don't touch

  TextLayout layout;
  for (int lh = normalLineHeight; lh >= densestLineHeight; lh--) {
    layout.lineHeight = lh;
    layout.blankHeight = lh / 2;
    layout.totalHeight = textLines * lh + blankLines * layout.blankHeight;
    if (layout.totalHeight <= maxHeight) break;
  }
  return layout;
}

// Draw wrapped text centered horizontally, starting at topY, using a layout from layoutWrappedText()
void drawWrappedText(M5Canvas& canvas, const String& wrappedText, const TextLayout& layout, int topY) {
  int y = topY;
  int lineStart = 0;
  for (unsigned int i = 0; i <= wrappedText.length(); i++) {
    if (i == wrappedText.length() || wrappedText[i] == '\n') {
      if ((int)i > lineStart) {
        canvas.drawString(wrappedText.substring(lineStart, i), WIDTH / 2, y + layout.lineHeight / 2);
        y += layout.lineHeight;
      } else {
        y += layout.blankHeight;
      }
      lineStart = i + 1;
    }
  }
}

void drawScreen(const String &text) {
  // A timer confirmation keeps its card look on every redraw (wake, rotation, ...)
  if (timerSetResponse.length() > 0 && text == timerSetResponse) {
    drawTimerSet(timerSetLabel, timerSetWhen);
    return;
  }
  if (text == MAIN_QUESTION) {
    drawHomeScreen();
    return;
  }
  if (iconResponse.length() > 0 && text == iconResponse && iconResponseIcon) {
    drawIconTextScreen(iconResponseText, iconResponseIcon);
    return;
  }
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFreeFont(FontManager::getFont12pt());  // Use custom font with umlaut support
  canvas.setCursor(0, 0);

  // Wrap text to fit screen width (leave margins for battery and icons)
  String wrappedText = wrapText(canvas, text, WIDTH - 10);

  // Center vertically; long texts get slightly denser line spacing to stay on screen
  TextLayout layout = layoutWrappedText(canvas, wrappedText, HEIGHT - 4);
  drawWrappedText(canvas, wrappedText, layout, (HEIGHT - layout.totalHeight) / 2);

  // Draw battery indicator
  drawBatteryIndicator(canvas);

  // Draw rotate icon at bottom right if image is available and we're showing text
  if (hasImage && currentDisplayMode == SHOW_TEXT) {
    int iconX = WIDTH - ROTATEICON_WIDTH - 3;
    int iconY = HEIGHT - ROTATEICON_HEIGHT - 3;
    drawIcon(canvas, rotateIcon, ROTATEICON_WIDTH, ROTATEICON_HEIGHT, iconX, iconY, 200);
  } else if (hasModel && currentDisplayMode == SHOW_TEXT) {
    drawModelHint(canvas);
  }

  captureCanvas(canvas);

  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

static const uint16_t CITRUS = 0xF5A0;  // zitro.spot citrus #f2b705 (RGB565)

// Small lemon for branding: citrus body with pointed ends and a leaf
void drawLemon(M5Canvas &canvas, int cx, int cy) {
  canvas.fillEllipse(cx, cy, 8, 5, CITRUS);
  canvas.fillTriangle(cx - 10, cy, cx - 7, cy - 2, cx - 7, cy + 2, CITRUS);
  canvas.fillTriangle(cx + 10, cy, cx + 7, cy - 2, cx + 7, cy + 2, CITRUS);
  canvas.fillTriangle(cx + 1, cy - 5, cx + 8, cy - 9, cx + 6, cy - 4, 0x4C06);  // leaf
}

// Keycap like the stick's front button: rounded face with a 2 px grey side showing at the
// bottom and right, label in a smaller font. x: left edge, cy: vertical center.
// Returns the keycap width.
int drawKeycap(M5Canvas &canvas, const String &label, int x, int cy, bool measureOnly = false) {
  canvas.setFont(&fonts::DejaVu12);
  const int padX = 5;
  const int side = 2;  // visible side of the key
  int faceW = canvas.textWidth(label) + 2 * padX;
  int faceH = canvas.fontHeight() + 4;
  int w = faceW + side;
  if (!measureOnly) {
    int y = cy - (faceH + side) / 2;
    canvas.fillRoundRect(x + side, y + side, faceW, faceH, 4, 0x7BEF);  // side of the key, mid grey
    canvas.fillRoundRect(x, y, faceW, faceH, 4, 0x2104);                // key face, dark grey
    canvas.drawRoundRect(x, y, faceW, faceH, 4, TFT_WHITE);
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString(label, x + faceW / 2, y + faceH / 2);
  }
  canvas.setFreeFont(FontManager::getFont12pt());
  return w;
}

// Home screen: "[lemon] Press [M5] button", "to ask a question", then the shake hint with its icon
void drawHomeScreen() {
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setFreeFont(FontManager::getFont12pt());

  const int lh = canvas.fontHeight() + 4;
  int blockH = 2 * lh + lh / 2 + lh;
  int y = (HEIGHT - blockH) / 2;

  // Lemon, then "Press [M5] button" on the same line
  const int lemonW = 21;  // drawLemon() spans cx - 10 .. cx + 10
  const int lemonGap = 7;
  String press = "Press ";
  String after = " button";
  int pressW = canvas.textWidth(press);
  int afterW = canvas.textWidth(after);
  int capW = drawKeycap(canvas, "M5", 0, 0, true);
  int x = (WIDTH - (lemonW + lemonGap + pressW + capW + afterW)) / 2;
  int cy = y + lh / 2;
  drawLemon(canvas, x + lemonW / 2, cy);
  x += lemonW + lemonGap;
  canvas.setTextDatum(ML_DATUM);
  canvas.drawString(press, x, cy);
  x += pressW;
  drawKeycap(canvas, "M5", x, cy);
  x += capW;
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(ML_DATUM);
  canvas.drawString(after, x, cy);
  y += lh;

  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("to ask a question", WIDTH / 2, y + lh / 2);
  y += lh + lh / 2;

  // Shake icon + hint
  String shake = "Shake for random fact.";
  const int gap = 6;
  int rowW = SHAKEICON_WIDTH + gap + canvas.textWidth(shake);
  x = (WIDTH - rowW) / 2;
  if (x < 2) x = 2;
  cy = y + lh / 2;
  drawIconScaled(canvas, shakeIcon, SHAKEICON_WIDTH, SHAKEICON_HEIGHT, x, cy - SHAKEICON_HEIGHT / 2, 1, TFT_WHITE);
  canvas.setTextDatum(ML_DATUM);
  canvas.drawString(shake, x + SHAKEICON_WIDTH + gap, cy);

  drawBatteryIndicator(canvas);
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Large icon (2x) with word-wrapped text to its right, the pair centered on screen
void drawIconTextScreen(const String &text, const uint8_t* iconData) {
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setFreeFont(FontManager::getFont12pt());

  const int scale = 2;
  const int iconW = 20 * scale;
  const int gap = 14;
  String flat = text;
  flat.replace('\n', ' ');
  String wrapped = wrapText(canvas, flat, WIDTH - 12 - iconW - gap);

  int textW = 0;
  int lineStart = 0;
  for (unsigned int i = 0; i <= wrapped.length(); i++) {
    if (i == wrapped.length() || wrapped[i] == '\n') {
      int w = canvas.textWidth(wrapped.substring(lineStart, i));
      if (w > textW) textW = w;
      lineStart = i + 1;
    }
  }
  TextLayout layout = layoutWrappedText(canvas, wrapped, HEIGHT - 4);

  int x = (WIDTH - (iconW + gap + textW)) / 2;
  if (x < 6) x = 6;
  drawIconScaled(canvas, iconData, 20, 20, x, HEIGHT / 2 - iconW / 2, scale, TFT_WHITE);

  canvas.setTextDatum(ML_DATUM);
  int y = (HEIGHT - layout.totalHeight) / 2;
  lineStart = 0;
  for (unsigned int i = 0; i <= wrapped.length(); i++) {
    if (i == wrapped.length() || wrapped[i] == '\n') {
      canvas.drawString(wrapped.substring(lineStart, i), x + iconW + gap, y + layout.lineHeight / 2);
      y += layout.lineHeight;
      lineStart = i + 1;
    }
  }

  drawBatteryIndicator(canvas);
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Draw screen with spinner animation
void drawScreenWithSpinner(const String &text, int spinnerFrame) {
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(MC_DATUM);
  canvas.setFreeFont(FontManager::getFont12pt());  // Use custom font with umlaut support
  canvas.setCursor(0, 0);

  // Wrap text to fit screen width
  String wrappedText = wrapText(canvas, text, WIDTH - 10);

  // Draw text centered, shifted up to leave room for the spinner
  TextLayout layout = layoutWrappedText(canvas, wrappedText, HEIGHT - 34);
  int topY = (HEIGHT - layout.totalHeight) / 2 - 15;
  drawWrappedText(canvas, wrappedText, layout, topY);

  // Draw spinner below text
  const char* spinner[] = {"|", "/", "-", "\\"};
  canvas.setTextFont(2);  // Use built-in font for spinner
  canvas.setTextSize(2.0);
  canvas.drawString(spinner[spinnerFrame % 4], WIDTH / 2, topY + layout.totalHeight + layout.lineHeight / 2 + 15);

  // Draw battery indicator
  drawBatteryIndicator(canvas);

  captureCanvas(canvas);

  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

void drawProgress(int secondsRemaining) {
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);

  // Draw microphone icon at 2x (same size as the printer screens).
  // The bitmap has ~5px empty margin left/right, so the visible mic ends at x~38,
  // clear of the progress boxes starting at x=50.
  const int micScale = 2;
  int micX = 8;
  int micY = HEIGHT / 2 - (MICICON_HEIGHT * micScale) / 2;
  drawIconScaled(canvas, micIcon, MICICON_WIDTH, MICICON_HEIGHT, micX, micY, micScale, TFT_WHITE);

  // Draw progress rectangles
  int rectWidth = 25;
  int rectHeight = 12;
  int spacing = 5;
  int startX = 50;
  int startY = HEIGHT / 2 - rectHeight / 2;

  int secondsRecorded = activeRecordSeconds - secondsRemaining;

  for (int i = 0; i < activeRecordSeconds; i++) {
    int x = startX + i * (rectWidth + spacing);
    if (i < secondsRecorded) {
      // Filled rectangle for recorded seconds
      canvas.fillRoundRect(x, startY, rectWidth, rectHeight, 3, TFT_WHITE);
    } else {
      // Empty rectangle for remaining seconds
      canvas.drawRoundRect(x, startY, rectWidth, rectHeight, 3, TFT_WHITE);
    }
  }

  // Draw battery indicator
  drawBatteryIndicator(canvas);

  captureCanvas(canvas);

  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// =============== WIFI SETUP ===============

// Load all stored WiFi credentials from Preferences
void loadWiFiCredentials() {
  prefs.begin("wifi", true);
  wifiNetworkCount = prefs.getInt("count", 0);

  Serial.println("\n========== LOADING WIFI CREDENTIALS ==========");
  Serial.printf("Stored networks: %d\n", wifiNetworkCount);

  for (int i = 0; i < wifiNetworkCount && i < MAX_WIFI_NETWORKS; i++) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    wifiNetworks[i].ssid = prefs.getString(ssidKey.c_str(), "");
    wifiNetworks[i].password = prefs.getString(passKey.c_str(), "");
    Serial.printf("  [%d] Key: %s, SSID: '%s', Password: %d chars\n",
                  i, ssidKey.c_str(), wifiNetworks[i].ssid.c_str(), wifiNetworks[i].password.length());
  }

  prefs.end();
  Serial.println("==============================================\n");
}

// Display list of stored SSIDs (for debugging/user info)
void listStoredNetworks() {
  Serial.println("\n========== STORED NETWORKS ==========");
  Serial.printf("Total: %d/%d\n", wifiNetworkCount, MAX_WIFI_NETWORKS);

  if (wifiNetworkCount == 0) {
    Serial.println("No networks stored");
  } else {
    for (int i = 0; i < wifiNetworkCount; i++) {
      Serial.printf("[%d] %s\n", i, wifiNetworks[i].ssid.c_str());
    }
  }
  Serial.println("=====================================\n");
}

// Save WiFi credentials to Preferences
void saveWiFiCredential(String ssid, String password) {
  prefs.begin("wifi", false);

  // Check if SSID already exists
  int existingIndex = -1;
  for (int i = 0; i < wifiNetworkCount; i++) {
    if (wifiNetworks[i].ssid == ssid) {
      existingIndex = i;
      break;
    }
  }

  if (existingIndex >= 0) {
    // Update existing credential
    Serial.printf("Updating existing network at index %d\n", existingIndex);
    wifiNetworks[existingIndex].password = password;
    String passKey = "pass_" + String(existingIndex);
    prefs.putString(passKey.c_str(), password);
  } else {
    // Add new credential
    if (wifiNetworkCount < MAX_WIFI_NETWORKS) {
      Serial.printf("Adding new network at index %d\n", wifiNetworkCount);
      wifiNetworks[wifiNetworkCount].ssid = ssid;
      wifiNetworks[wifiNetworkCount].password = password;

      String ssidKey = "ssid_" + String(wifiNetworkCount);
      String passKey = "pass_" + String(wifiNetworkCount);
      prefs.putString(ssidKey.c_str(), ssid);
      prefs.putString(passKey.c_str(), password);

      wifiNetworkCount++;
      prefs.putInt("count", wifiNetworkCount);
    } else {
      // Replace oldest (first) credential
      Serial.println("Max networks reached, replacing oldest");

      // Shift all networks down
      for (int i = 0; i < MAX_WIFI_NETWORKS - 1; i++) {
        wifiNetworks[i] = wifiNetworks[i + 1];
        String ssidKey = "ssid_" + String(i);
        String passKey = "pass_" + String(i);
        prefs.putString(ssidKey.c_str(), wifiNetworks[i].ssid);
        prefs.putString(passKey.c_str(), wifiNetworks[i].password);
      }

      // Add new network at the end
      wifiNetworks[MAX_WIFI_NETWORKS - 1].ssid = ssid;
      wifiNetworks[MAX_WIFI_NETWORKS - 1].password = password;
      String ssidKey = "ssid_" + String(MAX_WIFI_NETWORKS - 1);
      String passKey = "pass_" + String(MAX_WIFI_NETWORKS - 1);
      prefs.putString(ssidKey.c_str(), ssid);
      prefs.putString(passKey.c_str(), password);
    }
  }

  prefs.end();
  Serial.println("Credential saved");
}

void handleRoot() {
  Serial.println("Serving setup page");
  server.send(200, "text/html", setupPage);
}

void handleCaptivePortal() {
  // Redirect all unknown requests to the setup page
  // This ensures captive portal detection works on iOS/Android/Windows
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleListNetworks() {
  Serial.println("Serving network list");

  // Load current networks
  if (wifiNetworkCount == 0) {
    loadWiFiCredentials();
  }

  // Build JSON response
  String json = "[";
  for (int i = 0; i < wifiNetworkCount; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"index\":" + String(i) + ",";
    json += "\"ssid\":\"" + wifiNetworks[i].ssid + "\"";
    json += "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleReorder() {
  Serial.println("Handling network reorder");

  // Get the order parameter (comma-separated indices)
  String orderStr = server.arg("order");
  Serial.println("New order: " + orderStr);

  // Parse the order
  int newOrder[MAX_WIFI_NETWORKS];
  int orderCount = 0;
  int startIdx = 0;

  for (int i = 0; i <= orderStr.length(); i++) {
    if (i == orderStr.length() || orderStr.charAt(i) == ',') {
      if (i > startIdx) {
        String numStr = orderStr.substring(startIdx, i);
        newOrder[orderCount++] = numStr.toInt();
      }
      startIdx = i + 1;
    }
  }

  if (orderCount != wifiNetworkCount) {
    server.send(400, "text/plain", "Invalid order");
    return;
  }

  // Create temporary copy of networks
  WiFiCredential tempNetworks[MAX_WIFI_NETWORKS];
  for (int i = 0; i < wifiNetworkCount; i++) {
    tempNetworks[i] = wifiNetworks[i];
  }

  // Reorder based on new indices
  for (int i = 0; i < wifiNetworkCount; i++) {
    wifiNetworks[i] = tempNetworks[newOrder[i]];
  }

  // Save reordered networks to Preferences
  prefs.begin("wifi", false);
  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    prefs.putString(ssidKey.c_str(), wifiNetworks[i].ssid);
    prefs.putString(passKey.c_str(), wifiNetworks[i].password);
    Serial.printf("Saved [%d]: %s\n", i, wifiNetworks[i].ssid.c_str());
  }
  prefs.end();

  Serial.println("Networks reordered successfully");
  server.send(200, "text/plain", "OK");
}

void handleDelete() {
  Serial.println("Handling network deletion");

  int indexToDelete = server.arg("index").toInt();
  Serial.printf("Deleting network at index %d\n", indexToDelete);

  if (indexToDelete < 0 || indexToDelete >= wifiNetworkCount) {
    server.send(400, "text/plain", "Invalid index");
    return;
  }

  // Shift networks down to fill the gap
  for (int i = indexToDelete; i < wifiNetworkCount - 1; i++) {
    wifiNetworks[i] = wifiNetworks[i + 1];
  }

  wifiNetworkCount--;

  // Save updated networks to Preferences
  prefs.begin("wifi", false);

  // Update count
  prefs.putInt("count", wifiNetworkCount);

  // Save remaining networks
  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    prefs.putString(ssidKey.c_str(), wifiNetworks[i].ssid);
    prefs.putString(passKey.c_str(), wifiNetworks[i].password);
  }

  // Clear the last slot
  String lastSsidKey = "ssid_" + String(wifiNetworkCount);
  String lastPassKey = "pass_" + String(wifiNetworkCount);
  prefs.remove(lastSsidKey.c_str());
  prefs.remove(lastPassKey.c_str());

  prefs.end();

  Serial.println("Network deleted successfully");
  server.send(200, "text/plain", "OK");
}

void handleRestart() {
  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  Serial.println("Received credentials:");
  Serial.println("  SSID: " + ssid);
  Serial.println("  Pass: " + String(pass.length()) + " chars");

  // Save using new multi-credential system
  saveWiFiCredential(ssid, pass);

  Serial.println("Credentials saved to Preferences");

  server.send(200, "text/html", successPage);

  delay(2000);
  ESP.restart();
}

// Secret API keys stored in Preferences ("settings" namespace). Never sent back to the browser.
static const char* const SECRET_SETTING_KEYS[] = {"apiKey", "claudeKey", "ikKey", "customKey", "zitroKey"};
// Plain-text AI settings editable on the setup page
static const char* const AI_TEXT_SETTING_KEYS[] = {"oaModel", "claudeModel", "ikModel", "ikProduct", "customUrl", "customModel"};

void handleGetSettings() {
  Serial.println("Serving settings JSON");
  Preferences settingsPrefs;
  settingsPrefs.begin("settings", true);

  JsonDocument doc;
  for (int i = 0; i < SETTINGS_REGISTRY_COUNT; i++) {
    const SettingDef& def = settingsRegistry[i];
    if (strcmp(def.type, "bool") == 0)      doc[def.key] = settingsPrefs.getBool(def.key, def.defVal != 0);
    else if (strcmp(def.type, "int") == 0)  doc[def.key] = settingsPrefs.getInt(def.key, (int)def.defVal);
    else if (strcmp(def.type, "long") == 0) doc[def.key] = settingsPrefs.getLong(def.key, def.defVal);
    else                                    doc[def.key] = settingsPrefs.getString(def.key, "DE");
  }

  doc["provider"] = aiProvider();
  for (const char* key : AI_TEXT_SETTING_KEYS) {
    doc[key] = settingsPrefs.getString(key, "");
  }
  // Only reveal whether a key exists plus its last 4 characters
  for (const char* key : SECRET_SETTING_KEYS) {
    String secret = settingsPrefs.getString(key, "");
    doc["keys"][key] = secret.length() > 8 ? "..." + secret.substring(secret.length() - 4) : (secret.length() > 0 ? "set" : "");
  }
  settingsPrefs.end();

  doc["aiReady"] = aiChatConfigured();
  doc["speechReady"] = audioEndpoint().available;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSaveSettings() {
  Serial.println("Saving settings");
  Preferences settingsPrefs;
  settingsPrefs.begin("settings", false);

  // Registry settings, validated against their ranges
  for (int i = 0; i < SETTINGS_REGISTRY_COUNT; i++) {
    const SettingDef& def = settingsRegistry[i];
    if (!server.hasArg(def.key)) continue;
    String value = server.arg(def.key);

    if (strcmp(def.type, "bool") == 0) {
      bool on = (value == "1" || value == "true" || value == "on");
      settingsPrefs.putBool(def.key, on);
      if (strcmp(def.key, "memory") == 0 && !on) memoryClear();
    } else if (strcmp(def.type, "int") == 0) {
      int v = value.toInt();
      if (def.allowedVals) {
        if (("," + String(def.allowedVals) + ",").indexOf("," + String(v) + ",") < 0) continue;
      } else {
        v = constrain(v, def.minVal, def.maxVal);
      }
      settingsPrefs.putInt(def.key, v);
    } else if (strcmp(def.type, "long") == 0) {
      // Setup page sends milliseconds; ranges are defined in seconds
      long v = constrain(value.toInt(), (long)def.minVal * 1000, (long)def.maxVal * 1000);
      settingsPrefs.putLong(def.key, v);
    } else {
      value.toUpperCase();
      if (def.allowedVals && ("," + String(def.allowedVals) + ",").indexOf("," + value + ",") < 0) continue;
      if (strcmp(def.key, "language") == 0 && settingsPrefs.getString("language", "DE") != value) memoryClear();
      settingsPrefs.putString(def.key, value);
    }
  }

  if (server.hasArg("provider")) {
    String provider = server.arg("provider");
    if (provider == AI_PROVIDER_ZITRO || provider == AI_PROVIDER_OPENAI || provider == AI_PROVIDER_ANTHROPIC ||
        provider == AI_PROVIDER_INFOMANIAK || provider == AI_PROVIDER_CUSTOM) {
      settingsPrefs.putString("provider", provider);
    }
  }

  for (const char* key : AI_TEXT_SETTING_KEYS) {
    if (!server.hasArg(key)) continue;
    String value = server.arg(key);
    value.trim();
    if (strcmp(key, "customUrl") == 0 && value.length() > 0 && !value.startsWith("https://")) continue;
    if (value.length() <= 200) settingsPrefs.putString(key, value);
  }

  // Empty key fields mean "keep the stored key"
  for (const char* key : SECRET_SETTING_KEYS) {
    if (!server.hasArg(key)) continue;
    String value = server.arg(key);
    value.trim();
    if (value.length() >= 8 && value.length() <= 300 && value.indexOf(' ') < 0) {
      settingsPrefs.putString(key, value);
      Serial.printf("%s saved\n", key);
    }
  }

  settingsPrefs.end();
  loadSleepTimeouts();
  server.send(200, "text/plain", "OK");
}

// Move successfully connected network to the front of the list
void moveNetworkToFront(int connectedIndex) {
  if (connectedIndex <= 0 || connectedIndex >= wifiNetworkCount) {
    return;  // Already at front or invalid index
  }

  Serial.printf("Moving network at index %d to front\n", connectedIndex);

  // Save the connected network
  WiFiCredential connectedNetwork = wifiNetworks[connectedIndex];

  // Shift all networks from 0 to connectedIndex-1 down by one position
  for (int i = connectedIndex; i > 0; i--) {
    wifiNetworks[i] = wifiNetworks[i - 1];
  }

  // Put connected network at the front
  wifiNetworks[0] = connectedNetwork;

  // Save the new order to Preferences
  Preferences prefs;
  prefs.begin("wifi", false);
  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssidKey = "ssid_" + String(i);
    String passKey = "pass_" + String(i);
    prefs.putString(ssidKey.c_str(), wifiNetworks[i].ssid);
    prefs.putString(passKey.c_str(), wifiNetworks[i].password);
    Serial.printf("Saved [%d]: %s\n", i, wifiNetworks[i].ssid.c_str());
  }
  prefs.end();

  Serial.println("Network moved to front and saved");
}

// Fonts for the setup page (no internet in setup mode, so they come from flash)
void sendSetupFont(const unsigned char* data, unsigned int len) {
  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.send_P(200, "font/woff2", (const char*)data, len);
}

void startAPMode() {
  modelViewEnd();  // Setup mode needs the memory
  if (currentDisplayMode == SHOW_MODEL) {
    currentDisplayMode = SHOW_TEXT;
    modelResetRotateToggle = true;
  }
  Serial.println("\n========== STARTING AP MODE ==========");

  apMode = true;

  static const String AP_NAME = "zitro";

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/networks", HTTP_GET, handleListNetworks);
  server.on("/reorder", HTTP_POST, handleReorder);
  server.on("/delete", HTTP_POST, handleDelete);
  server.on("/settings", HTTP_GET, handleGetSettings);
  server.on("/savesettings", HTTP_POST, handleSaveSettings);
  server.on("/restart", HTTP_POST, handleRestart);
  server.on("/f/sniglet-400.woff2", HTTP_GET, []() { sendSetupFont(sniglet_400_woff2, sniglet_400_woff2_len); });
  server.on("/f/sniglet-800.woff2", HTTP_GET, []() { sendSetupFont(sniglet_800_woff2, sniglet_800_woff2_len); });
  server.onNotFound(handleCaptivePortal);
  server.begin();

  // Start mDNS so users can browse to http://zitro.local
  MDNS.begin("zitro");
  MDNS.addService("http", "tcp", 80);

  // Start DNS server for captive portal - redirect all domains to AP IP
  dnsServer.start(DNS_PORT, "*", ip);

  Serial.println("Web server started");
  Serial.println("DNS server started (captive portal)");
  Serial.println("=======================================\n");

  drawSetupScreen();
}

// Setup-mode screen: join instructions on the left, WiFi QR code on the right.
// openingSettings: shown right after a triple-press or voice request, while the hotspot starts.
void drawSetupScreen(bool openingSettings) {
  static const String AP_NAME = "zitro";
  M5.Display.setRotation(getLandscapeRotation());
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(WIDTH, HEIGHT);
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextDatum(TL_DATUM); // Top-left alignment
  canvas.setTextFont(2);
  canvas.setTextSize(1);

  // Draw text on the left side (one extra line when opening settings, so lines sit closer)
  int textX = 5;
  int textY = openingSettings ? 4 : 10;
  int lineHeight = openingSettings ? 16 : 18;

  if (openingSettings) {
    canvas.drawString("Opening settings...", textX, textY);
    textY += lineHeight;
    canvas.drawString("Connect to:", textX, textY);
  } else {
    canvas.drawString("SETUP: Connect to:", textX, textY);
  }
  textY += lineHeight;
  canvas.drawString(AP_NAME, textX, textY);
  textY += lineHeight;
  canvas.drawString("and open:", textX, textY);
  textY += lineHeight;
  // The address reads as a link: underlined
  const char* setupUrl = "zitro.local";
  canvas.drawString(setupUrl, textX, textY);
  canvas.drawFastHLine(textX, textY + 15, canvas.textWidth(setupUrl), TFT_WHITE);
  textY += lineHeight * 1.25;

  if (wifiNetworkCount > 0) {
    canvas.drawString("Then press BUTTON", textX, textY);
    textY += lineHeight;
    canvas.drawString("to retry.", textX, textY);
  }

  // Draw QR code on the right side
  // QR code is 93x93 pixels
  int qrX = WIDTH - 93 - 5; // 5px margin from right
  int qrY = (HEIGHT - 93) / 2; // Vertically centered
  canvas.drawPng(wifi_qr_png, wifi_qr_png_len, qrX, qrY);

  captureCanvas(canvas);

  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

bool connectToWiFi() {
  // Load all credentials from storage
  loadWiFiCredentials();

  Serial.println("\n========== WIFI CONNECTION ==========");

  // Show stored networks
  listStoredNetworks();

  if (wifiNetworkCount == 0) {
    Serial.println("No credentials stored");
    Serial.println("======================================\n");
    return false;
  }

  // Ensure WiFi is fully stopped before starting
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // Disable auto-reconnect for manual control
  WiFi.persistent(false);         // Disable NVS storage
  delay(100);

  // Try connecting to each known network
  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssid = wifiNetworks[i].ssid;
    String pass = wifiNetworks[i].password;

    if (ssid.length() == 0) {
      Serial.printf("Skipping empty SSID at index %d\n", i);
      continue;
    }

    Serial.printf("\nAttempting connection [%d/%d]:\n", i + 1, wifiNetworkCount);
    Serial.printf("  SSID: '%s'\n", ssid.c_str());
    Serial.printf("  Password: %d chars\n", pass.length());
    Serial.printf("  Using wifiNetworks[%d]\n", i);

    drawScreen("Connecting to\n" + ssid + "...\n(" + String(i + 1) + "/" + String(wifiNetworkCount) + ")");

    // Disconnect from any previous connection attempt
    if (i > 0) {
      Serial.println("Disconnecting from previous attempt...");
      WiFi.disconnect(false);
      delay(500);  // Wait for disconnect to complete
    }

    // Start new connection attempt
    Serial.println("Calling WiFi.begin()...");
    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 5) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      // Move this network to front of list for faster future connections
      moveNetworkToFront(i);

      // Store as currently connected
      savedSSID = ssid;
      savedPass = pass;

      // Configure NTP time for Switzerland (CET/CEST, UTC+1/UTC+2)
      configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
      Serial.println("Syncing time with NTP...");

      Serial.println("======================================\n");
      return true;
    }

    Serial.println(" Failed");
  }

  Serial.println("\nFailed to connect to any known network");
  Serial.println("======================================\n");
  return false;
}

// Check WiFi connection and reconnect if needed
void checkWiFiConnection() {
  // Skip if in AP mode or if not enough time has passed
  if (apMode || millis() - lastConnectionCheck < CONNECTION_CHECK_INTERVAL) {
    return;
  }

  lastConnectionCheck = millis();

  // Check if still connected
  if (WiFi.status() == WL_CONNECTED) {
    return;  // All good
  }

  Serial.println("\n========== WIFI DISCONNECTED ==========");
  Serial.println("Attempting to reconnect...");

  // Try to reconnect to any known network
  if (wifiNetworkCount == 0) {
    loadWiFiCredentials();
  }

  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssid = wifiNetworks[i].ssid;
    String pass = wifiNetworks[i].password;

    if (ssid.length() == 0) continue;

    Serial.printf("Trying network [%d/%d]: %s\n", i + 1, wifiNetworkCount, ssid.c_str());

    // Disconnect between attempts
    if (i > 0) {
      WiFi.disconnect(false);
      delay(500);
    }

    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      // Move this network to front of list for faster future connections
      moveNetworkToFront(i);

      savedSSID = ssid;
      savedPass = pass;

      Serial.println("=======================================\n");
      return;
    }

    Serial.println(" Failed");
  }

  Serial.println("Could not reconnect to any network");
  Serial.println("=======================================\n");
}

// Force immediate WiFi reconnection (for use when HTTP requests fail)
bool reconnectWiFiNow() {
  Serial.println("\n========== FORCING WIFI RECONNECTION ==========");

  // Load credentials if not already loaded
  if (wifiNetworkCount == 0) {
    loadWiFiCredentials();
  }

  if (wifiNetworkCount == 0) {
    Serial.println("No networks stored, cannot reconnect");
    Serial.println("===============================================\n");
    return false;
  }

  // Disconnect and reconnect
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  delay(100);

  // Try all known networks
  for (int i = 0; i < wifiNetworkCount; i++) {
    String ssid = wifiNetworks[i].ssid;
    String pass = wifiNetworks[i].password;

    if (ssid.length() == 0) continue;

    Serial.printf("Trying network [%d/%d]: %s\n", i + 1, wifiNetworkCount, ssid.c_str());
    drawScreen("Reconnecting to\n" + ssid + "...");

    // Disconnect between attempts
    if (i > 0) {
      WiFi.disconnect(false);
      delay(500);
    }

    WiFi.begin(ssid.c_str(), pass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      // Move this network to front of list for faster future connections
      moveNetworkToFront(i);

      savedSSID = ssid;
      savedPass = pass;

      Serial.println("===============================================\n");
      return true;
    }

    Serial.println(" Failed");
  }

  Serial.println("Could not reconnect to any network");
  Serial.println("===============================================\n");
  return false;
}

void resetCredentials() {
  Serial.println("\n========== RESETTING CREDENTIALS ==========");

  drawScreen("Resetting...");

  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  Serial.println("Credentials cleared, restarting...");
  Serial.println("============================================\n");

  delay(1000);
  ESP.restart();
}

// =============== POWER MANAGEMENT - STATE PERSISTENCE ===============

// Save current state to Preferences before deep sleep
// Pictures without an address survive deep sleep in flash. Formats the partition on first use.
static const char* SAVED_IMAGE_PATH = "/lastimage";
static const int SAVED_IMAGE_LIMIT = 512 * 1024;

bool saveImageToFlash() {
  if (!imageBuffer || imageBufferSize <= 0 || imageBufferSize > SAVED_IMAGE_LIMIT) return false;
  if (!SPIFFS.begin(true)) {
    Serial.println("Saved image: flash storage not available");
    return false;
  }
  File file = SPIFFS.open(SAVED_IMAGE_PATH, FILE_WRITE);
  const size_t written = file ? file.write(imageBuffer, imageBufferSize) : 0;
  if (file) file.close();
  SPIFFS.end();
  Serial.printf("Saved image: %u of %d bytes\n", (unsigned)written, imageBufferSize);
  return written == (size_t)imageBufferSize;
}

bool restoreImageFromFlash() {
  if (!SPIFFS.begin(true)) return false;
  bool ok = false;
  File file = SPIFFS.open(SAVED_IMAGE_PATH, FILE_READ);
  if (file) {
    const size_t size = file.size();
    uint8_t* buffer = (size > 0 && size <= (size_t)SAVED_IMAGE_LIMIT) ? (uint8_t*)ps_malloc(size) : nullptr;
    if (buffer && file.read(buffer, size) == size) {
      free(imageBuffer);
      imageBuffer = buffer;
      imageBufferSize = (int)size;
      ok = true;
    } else {
      free(buffer);
    }
    file.close();
    SPIFFS.remove(SAVED_IMAGE_PATH);  // Back in memory; the next deep sleep saves it again
  }
  SPIFFS.end();
  Serial.printf("Restored saved image: %s, %d bytes\n", ok ? "ok" : "failed", ok ? imageBufferSize : 0);
  return ok;
}

void saveStateToPreferences() {
  Serial.println("\n========== SAVING STATE ==========");

  prefs.begin(PREFS_NAMESPACE, false);

  // Save response text
  prefs.putString(PREFS_KEY_RESPONSE, response);
  Serial.printf("Saved response: %d chars\n", response.length());

  // Save image URL
  prefs.putString(PREFS_KEY_IMAGE_URL, currentImageUrl);
  prefs.putString(PREFS_KEY_MODEL_REF, hasModel ? modelRefText : String(""));
  prefs.putBool(PREFS_KEY_IS_WOLFRAM, isWolframImage);
  // Wolfram and AI-drawn pictures have no address to download again from
  if (hasImage && currentImageUrl.length() == 0 && !saveImageToFlash()) hasImage = false;
  Serial.printf("Saved image URL: %s\n", currentImageUrl.c_str());

  // Save boolean flags
  prefs.putBool(PREFS_KEY_HAS_IMAGE, hasImage);
  prefs.putInt(PREFS_KEY_DISPLAY_MODE, (int)currentDisplayMode);
  prefs.putBool(PREFS_KEY_PARENTAL_LOCK, parentalLockEnabled);

  // Mark state as valid
  prefs.putBool(PREFS_KEY_VALID, true);

  prefs.end();
  Serial.println("State saved successfully");
  Serial.println("==================================\n");
}

// Restore state from Preferences after deep sleep wake
bool restoreStateFromPreferences() {
  Serial.println("\n========== RESTORING STATE ==========");

  prefs.begin(PREFS_NAMESPACE, true);

  // Check if valid state exists
  bool hasValidState = prefs.getBool(PREFS_KEY_VALID, false);

  if (!hasValidState) {
    Serial.println("No valid saved state found");
    prefs.end();
    return false;
  }

  // Restore response
  response = prefs.getString(PREFS_KEY_RESPONSE, "");
  Serial.printf("Restored response: %d chars\n", response.length());

  // Restore image URL
  currentImageUrl = prefs.getString(PREFS_KEY_IMAGE_URL, "");
  modelRefText = prefs.getString(PREFS_KEY_MODEL_REF, "");
  isWolframImage = prefs.getBool(PREFS_KEY_IS_WOLFRAM, false);
  Serial.printf("Restored image URL: %s\n", currentImageUrl.c_str());

  // Restore flags
  hasImage = prefs.getBool(PREFS_KEY_HAS_IMAGE, false);
  currentDisplayMode = (DisplayMode)prefs.getInt(PREFS_KEY_DISPLAY_MODE, SHOW_TEXT);
  parentalLockEnabled = prefs.getBool(PREFS_KEY_PARENTAL_LOCK, false);

  prefs.end();

  Serial.println("State restored successfully");
  Serial.println("====================================\n");

  return true;
}

// Clear saved state (called after successful restoration)
void clearSavedState() {
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putBool(PREFS_KEY_VALID, false);
  prefs.end();
}

// =============== POWER MANAGEMENT - SLEEP FUNCTIONS ===============

// Configure GPIO wake-up sources for both buttons
void configureGPIOWakeup() {
  Serial.println("Configuring GPIO wake-up sources...");

  // Configure Button A (GPIO37) as wake source - wake on LOW (button pressed)
  esp_sleep_enable_ext0_wakeup(BUTTON_A_GPIO, 0);

  Serial.println("GPIO wake-up configured for Button A (GPIO37)");
}

// Enter light sleep mode
void enterLightSleep() {
  Serial.println("\n========== ENTERING LIGHT SLEEP ==========");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  currentPowerState = POWER_LIGHT_SLEEP;

  // Power down WiFi to save power
  Serial.println("Disconnecting WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Configure wake-up source
  configureGPIOWakeup();

  // Wake on a timer: either the next reminder, or (no reminders) the moment deep sleep is
  // due. The inactivity check in loop() can't run while the CPU is paused, so without this
  // wake-up the device used to stay in light sleep forever and never reach deep sleep.
  int secondsUntilNext = getNextTimerSeconds();
  if (secondsUntilNext >= 0) {
    uint64_t wakeupUs = (uint64_t)(secondsUntilNext + 1) * 1000000ULL;  // +1s margin
    esp_sleep_enable_timer_wakeup(wakeupUs);
    Serial.printf("Timer wakeup set for %d seconds\n", secondsUntilNext + 1);
  } else {
    unsigned long inactive = millis() - lastActivityTime;
    unsigned long untilDeep = inactive < DEEP_SLEEP_TIMEOUT ? DEEP_SLEEP_TIMEOUT - inactive : 0;
    if (untilDeep < 1000) untilDeep = 1000;
    esp_sleep_enable_timer_wakeup((uint64_t)untilDeep * 1000ULL);
    Serial.printf("Deep sleep due in %lu seconds\n", untilDeep / 1000);
  }

  powerDownPeripherals();

  Serial.println("Entering light sleep mode...");
  Serial.println("Will wake on Button A press or timer");
  Serial.println("==========================================\n");

  // Enter light sleep
  esp_light_sleep_start();

  // Code resumes here after wake-up
  Serial.println("\n========== WOKE FROM LIGHT SLEEP ==========");

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wake-up: Button A (GPIO37)");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wake-up: External GPIO");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      if (timerCount == 0) {
        // No reminder pending: this was the deep sleep deadline. Stay dark and go deeper.
        Serial.println("Wake-up: deep sleep deadline");
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
        enterDeepSleep();
      }
      Serial.println("Wake-up: Timer alarm!");
      break;
    default:
      Serial.printf("Wake-up: Other reason (%d)\n", wakeup_reason);
      break;
  }

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  M5.Imu.begin();  // Wake the motion sensor (sleeping since powerDownPeripherals)
  gyroPowerState = -1;
  setGyroEnabled(false);

  // Wake display immediately after button press
  Serial.println("Waking display...");
  M5.Display.wakeup();
  displaySleeping = false;

  // Show waking up message immediately
  drawScreen("Waking up...");

  wakeFromLightSleep = true;
  currentPowerState = POWER_ACTIVE;
  lastActivityTime = millis();
  timeWokeFromSleep = millis();  // Track when we woke up

  // Clear button state to prevent wake-up button press from triggering recording
  // M5.update() will read and clear any pending button events from the wake-up press
  M5.update();

  Serial.println("Button state cleared (wake-up press consumed)");
  Serial.println("===========================================\n");
}

// Enter deep sleep mode
void enterDeepSleep() {
  Serial.println("\n========== ENTERING DEEP SLEEP ==========");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Save current state to Preferences
  saveStateToPreferences();

  // Power down WiFi
  Serial.println("Disconnecting WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Configure wake-up source
  configureGPIOWakeup();

  powerDownPeripherals();

  // The Plus2 stays powered only while GPIO4 (HOLD) is high. Latch it through deep sleep,
  // otherwise the board can drop its own power and Button A can no longer wake it.
  // GPIO4 is an RTC pad, so gpio_hold_en alone keeps it through deep sleep. (The global
  // gpio_deep_sleep_hold_en() would also latch buzzer and LCD lines.)
  gpio_hold_en(GPIO_NUM_4);

  Serial.println("Entering deep sleep mode...");
  Serial.println("Will wake on Button A press (device will reboot)");
  Serial.println("=========================================\n");

  delay(100); // Allow serial output to complete

  // Enter deep sleep - device will reboot on wake
  esp_deep_sleep_start();

  // Code never reaches here
}

// Handle wake-up after light sleep
void handleLightSleepWake() {
  Serial.println("Handling light sleep wake-up...");

  // Reconnect WiFi
  Serial.println("Reconnecting WiFi...");

  if (wifiNetworkCount == 0) {
    loadWiFiCredentials();
  }

  // Ensure WiFi is fully stopped before starting
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);  // Disable auto-reconnect to prevent race condition
  WiFi.persistent(false);         // Disable NVS storage
  delay(200);
  bool connected = false;

  // Try to reconnect to known networks (quick attempt)
  for (int i = 0; i < wifiNetworkCount && !connected; i++) {
    String ssid = wifiNetworks[i].ssid;

    // Show reconnection status on display
    drawScreen("Reconnecting to\n" + ssid + "...");
    Serial.printf("Attempting to reconnect to: %s\n", ssid.c_str());

    WiFi.begin(ssid.c_str(), wifiNetworks[i].password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.println("WiFi reconnected");

      // Move this network to front of list for faster future connections
      moveNetworkToFront(i);
    }
  }

  if (!connected) {
    Serial.println("Warning: Could not reconnect to WiFi");
  }

  // Display is already awake (woken in enterLightSleep immediately after GPIO wake)
  // Just redraw previous content
  if (inPrinterSelectionMode) {
    displayPrinterSelection();
  } else if (response.length() > 0 && !response.startsWith("Press A")) {
    if (currentDisplayMode == SHOW_TEXT) {
      drawScreen(response);
    } else if (currentDisplayMode == SHOW_IMAGE && hasImage && imageBuffer) {
      // AI-generated images have empty currentImageUrl
      bool isGenerated = (currentImageUrl.length() == 0);
      displayImage(isGenerated);
    } else if (currentDisplayMode == SHOW_MODEL && hasModel) {
      modelViewShow();
    } else {
      drawScreen(response);
    }
  } else {
    drawScreen(MAIN_QUESTION);
  }

  // Drop the wake-up press. It was read in enterLightSleep and released during the WiFi
  // reconnect; left alone, M5Unified reports it as a single click ~1 s later, which
  // used to start a recording.
  discardButtonA(0);

  // Reset cooldown timer AFTER WiFi reconnection so the 2-second window
  // starts now (not before the multi-second WiFi reconnection delay)
  timeWokeFromSleep = millis();

  // Clear flag so handleLightSleepWake() only runs once per wake cycle.
  // The cooldown check in button handlers still uses timeWokeFromSleep.
  wakeFromLightSleep = false;
}

// Handle wake-up after deep sleep (called in setup)
void handleDeepSleepWake() {
  Serial.println("\n========== DEEP SLEEP WAKE-UP ==========");

  // Start the wake cooldown even if there is nothing to restore
  timeWokeFromSleep = millis();

  // Try to restore state
  bool stateRestored = restoreStateFromPreferences();

  if (!stateRestored) {
    Serial.println("No state to restore");
    return;
  }

  // Display restored text immediately
  if (response.length() > 0) {
    drawScreen(response);
    Serial.println("Displayed restored response");
  }

  // If we had an image, try to re-download it
  if (hasImage && currentImageUrl.length() > 0) {
    Serial.println("Attempting to re-download image...");
    drawScreen("Restoring image...");

    bool imageDownloaded = downloadImage(currentImageUrl);

    if (imageDownloaded) {
      Serial.println("Image restored successfully");
      hasImage = true;

      // If we were showing image before sleep, show it now
      if (currentDisplayMode == SHOW_IMAGE) {
        // AI-generated images have empty currentImageUrl
        bool isGenerated = (currentImageUrl.length() == 0);
        displayImage(isGenerated);
      } else {
        drawScreen(response);
      }
    } else {
      Serial.println("Failed to restore image, showing text only");
      hasImage = false;
      currentDisplayMode = SHOW_TEXT;
      drawScreen(response);
    }
  }

  // A picture without an address comes back from flash; without it there is no picture
  if (hasImage && currentImageUrl.length() == 0) {
    if (restoreImageFromFlash()) {
      if (currentDisplayMode == SHOW_IMAGE) displayImage(true);
    } else {
      hasImage = false;
      if (currentDisplayMode == SHOW_IMAGE) {
        currentDisplayMode = SHOW_TEXT;
        drawScreen(response);
      }
    }
  } else if (!hasImage && currentDisplayMode == SHOW_IMAGE) {
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
  }

  // A 3D model is fetched again, like a picture
  if (modelRefText.length() > 0) {
    drawScreen("Restoring...");
    ModelRef ref;
    const bool wasShowingModel = currentDisplayMode == SHOW_MODEL;
    if (parseModelRef(modelRefText.c_str(), ref) && fetchModel(ref)) {
      if (wasShowingModel) {
        modelViewShow();
      } else {
        drawScreen(response);
      }
    } else {
      Serial.println("Failed to restore 3D model, showing text only");
      currentDisplayMode = SHOW_TEXT;
      drawScreen(response);
    }
  } else if (currentDisplayMode == SHOW_MODEL) {
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
  }

  // Clear saved state after successful restoration
  clearSavedState();

  // Set wake time to enable cooldown protection
  timeWokeFromSleep = millis();
  Serial.printf("Deep sleep wake time set: %lu\n", timeWokeFromSleep);

  // Keep wakeFromDeepSleep true to enable cooldown check in button handler
  // It will be cleared after cooldown expires

  Serial.println("========================================\n");
}

// Check and transition between power states based on inactivity
void managePowerState() {
  unsigned long inactiveTime = millis() - lastActivityTime;

  // State transition logic
  switch (currentPowerState) {
    case POWER_ACTIVE:
      // Check if we should enter display sleep
      if (!displaySleeping && inactiveTime >= DISPLAY_SLEEP_TIMEOUT) {
        Serial.println("Transitioning: ACTIVE → DISPLAY_SLEEP");
        M5.Display.sleep();
        displaySleeping = true;
        currentPowerState = POWER_DISPLAY_SLEEP;
      }
      break;

    case POWER_DISPLAY_SLEEP:
      // Check if we should enter light sleep
      if (inactiveTime >= LIGHT_SLEEP_TIMEOUT) {
        Serial.println("Transitioning: DISPLAY_SLEEP → LIGHT_SLEEP");
        enterLightSleep();
        // After wake, we're back in ACTIVE state (set in enterLightSleep)
      }
      break;

    case POWER_LIGHT_SLEEP:
      // This state is only active during sleep, never in loop
      break;

    case POWER_DEEP_SLEEP:
      // This state is terminal - device will reboot
      break;
  }

  // Special case: If we've been inactive for too long, go to deep sleep
  // This handles progression from ACTIVE or DISPLAY_SLEEP directly to DEEP_SLEEP
  if (currentPowerState != POWER_DEEP_SLEEP && inactiveTime >= DEEP_SLEEP_TIMEOUT) {
    if (timerCount > 0) {
      // Don't enter deep sleep with active timers - use light sleep instead
      Serial.println("Skipping deep sleep: active timers present");
      if (currentPowerState == POWER_DISPLAY_SLEEP || currentPowerState == POWER_ACTIVE) {
        enterLightSleep();  // Will wake on timer or button
      }
    } else {
      Serial.println("Transitioning to DEEP_SLEEP (timeout exceeded)");
      enterDeepSleep();
    }
  }
}

// Reset to active state (call when user interacts)
void resetToActiveState() {
  lastActivityTime = millis();

  if (currentPowerState != POWER_ACTIVE) {
    Serial.printf("Resetting to ACTIVE state from %d\n", currentPowerState);

    // Wake display if sleeping
    if (displaySleeping) {
      M5.Display.wakeup();
      displaySleeping = false;
    }

    currentPowerState = POWER_ACTIVE;
  }
}

// Read the sleep timeouts (ms) from settings, keeping display <= light <= deep
void loadSleepTimeouts() {
  Preferences sp;
  sp.begin("settings", true);
  DISPLAY_SLEEP_TIMEOUT = sp.getLong("dispSleep", 30000);
  LIGHT_SLEEP_TIMEOUT = sp.getLong("lightSleep", 90000);
  DEEP_SLEEP_TIMEOUT = sp.getLong("deepSleep", 180000);
  sp.end();
  if (LIGHT_SLEEP_TIMEOUT < DISPLAY_SLEEP_TIMEOUT) LIGHT_SLEEP_TIMEOUT = DISPLAY_SLEEP_TIMEOUT;
  if (DEEP_SLEEP_TIMEOUT < LIGHT_SLEEP_TIMEOUT) DEEP_SLEEP_TIMEOUT = LIGHT_SLEEP_TIMEOUT;
  Serial.printf("Sleep timeouts: display %lus, light %lus, deep %lus\n",
    DISPLAY_SLEEP_TIMEOUT / 1000, LIGHT_SLEEP_TIMEOUT / 1000, DEEP_SLEEP_TIMEOUT / 1000);
}

// The MPU6886 gyroscope draws most of the motion sensor's current, but only the roll gesture
// (zoom in image view) needs it. Shake, auto-flip and rotate-to-toggle use the accelerometer.
// So the gyro runs only while an image or equation is on screen.
void setGyroEnabled(bool on) {
  if (gyroPowerState == (on ? 1 : 0)) return;
  // PWR_MGMT_2 (0x6C): bits 2..0 put the gyro X/Y/Z axes in standby, accelerometer stays on
  M5.In_I2C.writeRegister8(0x68, 0x6C, on ? 0x00 : 0x07, 400000);
  gyroPowerState = on ? 1 : 0;
  if (on) gyroEnabledAt = millis();
  Serial.printf("Gyroscope %s\n", on ? "on (image view)" : "off");
}

void updateGyroPower() {
  setGyroEnabled(!displaySleeping && ((currentDisplayMode == SHOW_IMAGE && hasImage) ||
                                      (currentDisplayMode == SHOW_MODEL && hasModel)));
}

// The gyro needs ~35 ms to settle after power-up; ignore its readings until then
bool gyroReady() {
  return gyroPowerState == 1 && millis() - gyroEnabledAt > 150;
}

// Switch off what keeps drawing current while asleep: display controller and backlight,
// motion sensor (MPU6886), microphone and speaker. The StickC Plus2 has no power chip,
// so none of this happens by itself.
void powerDownPeripherals() {
  M5.Mic.end();
  M5.Speaker.end();
  M5.Imu.sleep();
  M5.Display.sleep();   // ST7789 sleep-in plus backlight off
  M5.Display.waitDisplay();
  displaySleeping = true;
}

// Forget any pending Button A press or click (for example the press that woke the device),
// so it can't start a recording later. Waits until the button is released and has stayed
// released for quietMs (bounded to 5 s), then clears M5Unified's pending click count.
void discardButtonA(unsigned long quietMs) {
  unsigned long start = millis();
  unsigned long lastActive = millis();
  while (millis() - start < 5000) {
    M5.update();
    if (M5.BtnA.isPressed()) {
      lastActive = millis();
    } else if (millis() - lastActive >= quietMs) {
      break;
    }
    delay(10);
  }
  // M5Unified resets its click counter when leaving the "decide click count" state
  M5.BtnA.setState(millis(), m5::Button_Class::state_decide_click_count);
  M5.BtnA.setState(millis(), m5::Button_Class::state_nochange);
}

// True for WAKE_COOLDOWN ms after waking from any sleep stage
bool inWakeCooldown() {
  return timeWokeFromSleep != 0 && millis() - timeWokeFromSleep < WAKE_COOLDOWN;
}

// Show the last answer (text or image) again, or the home screen if there is none
void redrawPreviousResult() {
  if (response.length() > 0 && !response.startsWith("Press BUTTON")) {
    if (currentDisplayMode == SHOW_IMAGE && hasImage && imageBuffer) {
      displayImage(currentImageUrl.length() == 0);
    } else if (currentDisplayMode == SHOW_MODEL && hasModel) {
      modelViewShow();
    } else {
      drawScreen(response);
    }
  } else {
    drawScreen(MAIN_QUESTION);
  }
}

// =============== AUDIO ===============

// Records into audioBuffer. During recording, one tap restarts it; two quick taps cancel it
// (cancelled is set and false is returned). Returns false with cancelled=false on mic error.
bool recordAudio(bool &cancelled) {
  bool restartRecording = false;
  cancelled = false;

  do {
    restartRecording = false;
    Serial.println("\n========== RECORDING ==========");

    if (!audioBuffer) {
      Serial.printf("Allocating buffer: %d samples, %d bytes\n", activeRecordSamples, activeRecordSamples * sizeof(int16_t));
      audioBuffer = (int16_t*)malloc(activeRecordSamples * sizeof(int16_t));
      if (!audioBuffer) {
        Serial.println("ERROR: Failed to allocate audio buffer!");
        return false;
      }
      Serial.println("Buffer allocated OK");
    } else {
      Serial.println("Using existing buffer");
    }

    Serial.println("Starting mic...");
    M5.Mic.begin();

    int samplesPerSecond = activeSampleRate;
    for (int sec = 0; sec < activeRecordSeconds; sec++) {
      Serial.printf("Recording second %d/%d...\n", sec + 1, activeRecordSeconds);
      drawProgress(activeRecordSeconds - sec);
      M5.Mic.record(&audioBuffer[sec * samplesPerSecond], samplesPerSecond, activeSampleRate);
      while (M5.Mic.isRecording()) {
        M5.update();  // Update button states

        // Button A during recording: one tap restarts, a second quick tap cancels
        if (M5.BtnA.wasPressed()) {
          M5.Mic.end();
          drawScreen("Restarting...");
          unsigned long tapTime = millis();
          bool secondTap = false;
          while (millis() - tapTime < MULTI_TAP_WINDOW) {
            M5.update();
            if (M5.BtnA.wasPressed()) {
              secondTap = true;
              break;
            }
            delay(10);
          }
          if (secondTap) {
            Serial.println("\n*** BUTTON A TAPPED REPEATEDLY - RECORDING CANCELLED ***");
            discardButtonA(800);  // Swallow any further taps of the burst
            cancelled = true;
            return false;
          }
          Serial.println("\n*** BUTTON A PRESSED - RESTARTING RECORDING ***");
          restartRecording = true;
          break;  // Break out of the while loop
        }

        delay(10);
      }

      // If restart was triggered, break out of the for loop too
      if (restartRecording) {
        break;
      }
    }

    // Only complete recording if we didn't restart
    if (!restartRecording) {
      M5.Mic.end();
      Serial.println("Recording complete");

      int16_t minVal = 32767, maxVal = -32768;
      int64_t sum = 0;
      for (int i = 0; i < activeRecordSamples; i++) {
        if (audioBuffer[i] < minVal) minVal = audioBuffer[i];
        if (audioBuffer[i] > maxVal) maxVal = audioBuffer[i];
        sum += abs(audioBuffer[i]);
      }
      Serial.printf("Audio stats: min=%d, max=%d, avg=%lld\n", minVal, maxVal, sum / activeRecordSamples);
      Serial.println("================================\n");
    }
  } while (restartRecording);  // Loop back if restart was triggered

  // A restart tap released near the end would otherwise fire as a new click later
  discardButtonA(0);
  return true;
}

void createWavHeader(uint8_t* header, int dataSize) {
  int fileSize = dataSize + 36;
  int byteRate = activeSampleRate * 1 * 16 / 8;
  int blockAlign = 1 * 16 / 8;

  memcpy(header, "RIFF", 4);
  header[4] = fileSize & 0xFF;
  header[5] = (fileSize >> 8) & 0xFF;
  header[6] = (fileSize >> 16) & 0xFF;
  header[7] = (fileSize >> 24) & 0xFF;
  memcpy(header + 8, "WAVE", 4);
  memcpy(header + 12, "fmt ", 4);
  header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
  header[20] = 1; header[21] = 0;
  header[22] = 1; header[23] = 0;
  header[24] = activeSampleRate & 0xFF;
  header[25] = (activeSampleRate >> 8) & 0xFF;
  header[26] = (activeSampleRate >> 16) & 0xFF;
  header[27] = (activeSampleRate >> 24) & 0xFF;
  header[28] = byteRate & 0xFF;
  header[29] = (byteRate >> 8) & 0xFF;
  header[30] = (byteRate >> 16) & 0xFF;
  header[31] = (byteRate >> 24) & 0xFF;
  header[32] = blockAlign; header[33] = 0;
  header[34] = 16; header[35] = 0;
  memcpy(header + 36, "data", 4);
  header[40] = dataSize & 0xFF;
  header[41] = (dataSize >> 8) & 0xFF;
  header[42] = (dataSize >> 16) & 0xFF;
  header[43] = (dataSize >> 24) & 0xFF;
}

String transcribeAudio() {
  Serial.println("\n========== TRANSCRIBING ==========");
  
  AudioEndpoint ep = audioEndpoint();
  if (!ep.available) {
    Serial.println("ERROR: No speech key configured");
    return "No speech key";
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);

  Serial.println("Connecting to " + ep.host + "...");
  if (!client.connect(ep.host.c_str(), 443)) {
    Serial.println("ERROR: Connection failed!");
    return "Connection failed";
  }
  Serial.println("Connected");

  int audioDataSize = activeRecordSamples * sizeof(int16_t);
  uint8_t wavHeader[44];
  createWavHeader(wavHeader, audioDataSize);

  String boundary = "----ESP32Boundary";
  
  String bodyStart = "--" + boundary + "\r\n";
  bodyStart += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
  bodyStart += "Content-Type: audio/wav\r\n\r\n";
  
  String bodyEnd = "\r\n--" + boundary + "\r\n";
  bodyEnd += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
  bodyEnd += STT_MODEL "\r\n";
  bodyEnd += "--" + boundary + "--\r\n";

  int contentLength = bodyStart.length() + 44 + audioDataSize + bodyEnd.length();
  Serial.printf("Content length: %d bytes (audio: %d bytes)\n", contentLength, audioDataSize);

  Serial.println("Sending request headers...");
  client.print("POST /v1/audio/transcriptions HTTP/1.1\r\n");
  client.print("Host: " + ep.host + "\r\n");
  client.print("Authorization: Bearer " + ep.key + "\r\n");
  client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  client.print("Content-Length: " + String(contentLength) + "\r\n");
  client.print("Connection: close\r\n\r\n");

  Serial.println("Sending audio data...");
  client.print(bodyStart);
  client.write(wavHeader, 44);
  
  int chunkSize = 4096;
  uint8_t* ptr = (uint8_t*)audioBuffer;
  int remaining = audioDataSize;
  int sent = 0;
  while (remaining > 0) {
    int toSend = min(chunkSize, remaining);
    client.write(ptr, toSend);
    ptr += toSend;
    remaining -= toSend;
    sent += toSend;
    if (sent % 16384 == 0) {
      Serial.printf("  Sent %d / %d bytes\n", sent, audioDataSize);
    }
    delay(1);
  }
  
  client.print(bodyEnd);
  Serial.println("Request sent, waiting for response...");

  unsigned long timeout = millis();
  while (!client.available()) {
    if (millis() - timeout > 60000) {
      Serial.println("ERROR: Timeout waiting for response!");
      client.stop();
      return "Timeout";
    }
    delay(100);
  }

  Serial.println("Response received, reading headers...");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println("  " + line);
    if (line == "\r") break;
  }

  String response = client.readString();
  client.stop();

  Serial.println("Whisper response body:");
  Serial.println(response);

  int jsonStart = response.indexOf('{');
  if (jsonStart < 0) {
    Serial.println("ERROR: No JSON in transcription response!");
    return "No transcription";
  }
  JsonDocument doc;
  if (deserializeJson(doc, response.substring(jsonStart))) {
    Serial.println("ERROR: Parse error!");
    return "Parse error";
  }
  if (!doc["error"].isNull()) {
    Serial.println("ERROR: " + String(doc["error"]["message"] | "transcription error"));
    return "No transcription";
  }
  String result = doc["text"] | "";
  result.trim();

  Serial.println("Transcription: " + result);
  Serial.println("==================================\n");
  
  return result;
}

// Normalize string for comparison: lowercase and remove punctuation/spaces
String normalizeString(const String& str) {
  String normalized = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    // Convert to lowercase and skip punctuation/spaces
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= 128 && c <= 255)) {  // Include extended ASCII for umlauts (ä, ö, ü, ß)
      if (c >= 'A' && c <= 'Z') {
        c = c + 32;  // Convert uppercase to lowercase
      }
      normalized += String(c);  // Properly append as String
    }
  }
  return normalized;
}

// Check if current time is within restricted hours (8pm-6am)
bool isRestrictedHours() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false; // If time not synced, don't restrict
  }

  int hour = timeinfo.tm_hour;
  // Restricted between 20:00 (8pm) and 06:00 (6am)
  return (hour >= 20 || hour < 6);
}

String getCurrentDateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return ""; // Time not synced yet
  }

  // German day and month names
  const char* days[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  const char* months[] = {"Januar", "Februar", "Maerz", "April", "Mai", "Juni",
                          "Juli", "August", "September", "Oktober", "November", "Dezember"};

  char buffer[150];
  // Format: "Es ist heute der dddd, dd.mm.yyyy. hh:mm:ss."
  snprintf(buffer, sizeof(buffer), "Es ist heute der %s, %02d.%02d.%d. %02d:%02d:%02d.",
           days[timeinfo.tm_wday],
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           timeinfo.tm_year + 1900,
           timeinfo.tm_hour,
           timeinfo.tm_min,
           timeinfo.tm_sec);

  return String(buffer);
}

// ========== PRINTER DISCOVERY AND PRINTING ==========

void discoverPrinters() {
  Serial.println("\n========== DISCOVERING PRINTERS ==========");

  printerCount = 0;

  // Initialize mDNS if not already done
  if (!MDNS.begin("ai-stick")) {
    Serial.println("Failed to start mDNS");
    return;
  }

  // Try multiple service types that printers might advertise
  const char* serviceTypes[] = {"ipp", "ipps", "printer"};
  const int serviceTypeCount = 3;

  for (int serviceIdx = 0; serviceIdx < serviceTypeCount && printerCount < MAX_PRINTERS; serviceIdx++) {
    Serial.printf("Querying service: _%s._tcp...\n", serviceTypes[serviceIdx]);

    int n = MDNS.queryService(serviceTypes[serviceIdx], "tcp");

    if (n == 0) {
      Serial.println("  No services found");
      continue;
    }

    Serial.printf("  Found %d service(s)\n", n);

    for (int i = 0; i < n && printerCount < MAX_PRINTERS; i++) {
      // ESP32 mDNS library uses different method names
      IPAddress ip = MDNS.address(i);
      String hostname = MDNS.hostname(i);
      uint16_t port = MDNS.port(i);

      // Check if this printer is already in our list (avoid duplicates)
      bool isDuplicate = false;
      for (int j = 0; j < printerCount; j++) {
        if (discoveredPrinters[j].ip == ip) {
          isDuplicate = true;
          break;
        }
      }

      if (!isDuplicate) {
        // Show the printer's own label: the Bonjour service name ("Canon TS5100 series"),
        // else its make and model (TXT "ty"), else the hostname ("16B50F...")
        String label = MDNS.instanceName(i);
        label.trim();
        if (label.length() == 0 && MDNS.hasTxt(i, "ty")) {
          label = MDNS.txt(i, "ty");
          label.trim();
        }
        if (label.length() == 0) label = hostname;
        discoveredPrinters[printerCount].name = label;
        discoveredPrinters[printerCount].hostname = hostname;
        discoveredPrinters[printerCount].ip = ip;
        discoveredPrinters[printerCount].port = port;
        discoveredPrinters[printerCount].serviceType = String(serviceTypes[serviceIdx]);

        Serial.printf("  Added printer #%d: %s (%s:%d)\n",
                      printerCount + 1,
                      discoveredPrinters[printerCount].name.c_str(),
                      discoveredPrinters[printerCount].ip.toString().c_str(),
                      discoveredPrinters[printerCount].port);

        printerCount++;
      }
    }
  }

  Serial.printf("Total unique printers discovered: %d\n", printerCount);
  Serial.printf("Free heap after discovery: %d bytes\n", ESP.getFreeHeap());
}

// ---------- Print screens ----------
// Small icon-based screens instead of explanatory text. They follow how the stick is held,
// in landscape and in portrait. The button hints are tabs at the screen edge right next to
// the physical button:
//   rotation 3 (landscape, buttons left):  A left-middle,   C top-left,     B bottom-center
//   rotation 1 (landscape, buttons right): A right-middle,  C bottom-right, B top-center
//   rotation 0 (portrait, A at the bottom): A bottom-center, C left-low,     B right-middle
//   rotation 2 (portrait, A on top):        A top-center,    C right-high,   B left-middle

#define PRINT_BADGE_NONE 0
#define PRINT_BADGE_OK   1
#define PRINT_BADGE_FAIL 2

// Symbols for the button tabs (print and timer screens)
#define HINT_NONE   -1
#define HINT_NEXT    0  // triangle: next item
#define HINT_CANCEL  1  // cross
#define HINT_OK      2  // check mark
#define HINT_DELETE  3  // trash can
#define PRINT_HINT_NEXT   HINT_NEXT    // Button C: next printer
#define PRINT_HINT_CANCEL HINT_CANCEL  // Button B: cancel
#define PRINT_HINT_PRINT  HINT_OK      // Button A: print

#define EDGE_TOP    0
#define EDGE_BOTTOM 1
#define EDGE_LEFT   2
#define EDGE_RIGHT  3

static const uint16_t PRINT_GREY = 0x7BEF;       // mid grey (RGB565)
static const uint16_t PRINT_DARK_GREY = 0x2104;  // very dark grey

// Screenshot tour can force a rotation (-1: follow the motion sensor)
int printUiForcedRotation = -1;

// Portrait rotation from the accelerometer: 0 = Button A at the bottom, 2 = A on top
int getPortraitRotation() {
  M5.Imu.update();
  float ay = M5.Imu.getImuData().accel.y;
  return (ay > 0) ? 0 : 2;
}

// Rotation for the print screens: portrait when the stick is held upright, else landscape
int printUiRotation() {
  if (printUiForcedRotation >= 0) return printUiForcedRotation;
  if (detectOrientation() == PORTRAIT) return getPortraitRotation();
  return getLandscapeRotation();
}

bool isPortraitRotation(int rotation) {
  return rotation == 0 || rotation == 2;
}

// 1-bit icon drawn at an integer scale, in any color
void drawIconScaled(M5Canvas &canvas, const uint8_t* iconData, int iconWidth, int iconHeight,
                    int x, int y, int scale, uint16_t color) {
  int bytesPerRow = (iconWidth + 7) / 8;
  for (int py = 0; py < iconHeight; py++) {
    for (int px = 0; px < iconWidth; px++) {
      uint8_t b = pgm_read_byte(&iconData[py * bytesPerRow + (px / 8)]);
      if (b & (1 << (7 - (px % 8)))) {
        canvas.fillRect(x + px * scale, y + py * scale, scale, scale, color);
      }
    }
  }
}

void drawCheckMark(M5Canvas &canvas, int cx, int cy, int size, uint16_t color) {
  float s = size / 2.0f;
  canvas.drawWideLine(cx - s, cy, cx - s / 3, cy + s * 0.65f, 1.5f, color);
  canvas.drawWideLine(cx - s / 3, cy + s * 0.65f, cx + s, cy - s * 0.7f, 1.5f, color);
}

void drawCrossMark(M5Canvas &canvas, int cx, int cy, int size, uint16_t color) {
  float s = size / 2.0f;
  canvas.drawWideLine(cx - s, cy - s, cx + s, cy + s, 1.5f, color);
  canvas.drawWideLine(cx - s, cy + s, cx + s, cy - s, 1.5f, color);
}

void drawTrashMark(M5Canvas &canvas, int cx, int cy, uint16_t color) {
  canvas.fillRect(cx - 2, cy - 6, 5, 2, color);   // handle
  canvas.fillRect(cx - 5, cy - 4, 11, 2, color);  // lid
  canvas.drawRect(cx - 4, cy - 1, 9, 7, color);   // can
  canvas.drawFastVLine(cx - 1, cy + 1, 4, color);
  canvas.drawFastVLine(cx + 1, cy + 1, 4, color);
}

// A tab that sticks out of the screen edge, with a symbol in its visible part.
// pos: center along that edge. filled: main action (Button A). invert: screen is white.
// The "next" triangle points down on top/bottom tabs and right on side tabs.
void drawPrintHint(M5Canvas &canvas, int edge, int pos, int kind, bool filled = false, bool invert = false) {
  const int len = 34;      // along the edge
  const int depth = 15;    // visible part
  const int hidden = 8;    // part beyond the screen edge (rounds only the inner corners)
  int cw = canvas.width();
  int ch = canvas.height();
  bool sideEdge = (edge == EDGE_LEFT || edge == EDGE_RIGHT);
  int x, y, w, h;
  if (!sideEdge) {
    w = len; h = depth + hidden; x = pos - len / 2;
    y = (edge == EDGE_TOP) ? -hidden : ch - depth;
  } else {
    w = depth + hidden; h = len; y = pos - len / 2;
    x = (edge == EDGE_LEFT) ? -hidden : cw - depth;
  }

  uint16_t fg = invert ? TFT_BLACK : TFT_WHITE;
  uint16_t bg = invert ? TFT_WHITE : TFT_BLACK;
  uint16_t symbolColor = filled ? bg : fg;
  if (filled) {
    canvas.fillRoundRect(x, y, w, h, 6, fg);
  } else {
    canvas.fillRoundRect(x, y, w, h, 6, invert ? (uint16_t)0xDEFB : PRINT_DARK_GREY);
    canvas.drawRoundRect(x, y, w, h, 6, PRINT_GREY);
  }

  // Center of the visible part
  int cx, cy;
  if (!sideEdge) {
    cx = pos;
    cy = (edge == EDGE_TOP) ? depth / 2 : ch - depth / 2 - 1;
  } else {
    cy = pos;
    cx = (edge == EDGE_LEFT) ? depth / 2 : cw - depth / 2 - 1;
  }

  if (kind == HINT_NEXT) {
    if (sideEdge) {
      canvas.fillTriangle(cx - 3, cy - 5, cx - 3, cy + 5, cx + 3, cy, symbolColor);
    } else {
      canvas.fillTriangle(cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 3, symbolColor);
    }
  } else if (kind == HINT_CANCEL) {
    drawCrossMark(canvas, cx, cy, 8, symbolColor);
  } else if (kind == HINT_DELETE) {
    drawTrashMark(canvas, cx, cy, symbolColor);
  } else {
    drawCheckMark(canvas, cx, cy, 9, symbolColor);
  }
}

// Place a tab next to each physical button for this rotation (HINT_NONE: no tab).
// Button A's tab is filled: it is the main action.
void drawButtonHints(M5Canvas &canvas, int rotation, int kindA, int kindB, int kindC, bool invert = false) {
  int cw = canvas.width();
  int ch = canvas.height();
  int edgeA, posA, edgeB, posB, edgeC, posC;
  switch (rotation) {
    case 3:  // landscape, buttons left
      edgeA = EDGE_LEFT;   posA = ch / 2;  edgeC = EDGE_TOP;    posC = 24;      edgeB = EDGE_BOTTOM; posB = cw / 2;
      break;
    case 1:  // landscape, buttons right
      edgeA = EDGE_RIGHT;  posA = ch / 2;  edgeC = EDGE_BOTTOM; posC = cw - 24; edgeB = EDGE_TOP;    posB = cw / 2;
      break;
    case 0:  // portrait, A at the bottom
      edgeA = EDGE_BOTTOM; posA = cw / 2;  edgeC = EDGE_LEFT;   posC = ch - 24; edgeB = EDGE_RIGHT;  posB = ch / 2;
      break;
    default:  // 2: portrait, A on top
      edgeA = EDGE_TOP;    posA = cw / 2;  edgeC = EDGE_RIGHT;  posC = 24;      edgeB = EDGE_LEFT;   posB = ch / 2;
      break;
  }
  if (kindA != HINT_NONE) drawPrintHint(canvas, edgeA, posA, kindA, true, invert);
  if (kindB != HINT_NONE) drawPrintHint(canvas, edgeB, posB, kindB, false, invert);
  if (kindC != HINT_NONE) drawPrintHint(canvas, edgeC, posC, kindC, false, invert);
}

// Print screens: A prints, B cancels, C goes to the next printer
void drawPrintButtonHints(M5Canvas &canvas, int rotation, bool showNext) {
  drawButtonHints(canvas, rotation, HINT_OK, HINT_CANCEL, showNext ? HINT_NEXT : HINT_NONE);
}

// Printer names come from Bonjour, sometimes a hostname like "Canon_TS5100_series.local"
String printerDisplayName(const String &raw) {
  String name = raw;
  if (name.endsWith(".local")) name = name.substring(0, name.length() - 6);
  name.replace('_', ' ');
  name.replace('-', ' ');
  name.trim();
  if (name.length() == 0) name = "Printer";
  return name;
}

String fitToWidth(M5Canvas &canvas, const String &text, int maxWidth) {
  if (canvas.textWidth(text) <= maxWidth) return text;
  String cut = text;
  while (cut.length() > 1 && canvas.textWidth(cut + "...") > maxWidth) {
    cut.remove(cut.length() - 1);
  }
  cut.trim();
  return cut + "...";
}

// Word-wrap into at most maxLines lines; the last line is cut with "..." if needed.
// Returns the number of lines written to lines[].
int wrapToLines(M5Canvas &canvas, const String &text, int maxWidth, String lines[], int maxLines) {
  int count = 0;
  String rest = text;
  rest.trim();
  while (rest.length() > 0 && count < maxLines) {
    if (count == maxLines - 1 || canvas.textWidth(rest) <= maxWidth) {
      lines[count++] = fitToWidth(canvas, rest, maxWidth);
      break;
    }
    // Longest run of whole words that fits
    int cut = -1;
    for (int i = 1; i <= (int)rest.length(); i++) {
      if (i == (int)rest.length() || rest[i] == ' ') {
        if (canvas.textWidth(rest.substring(0, i)) <= maxWidth) cut = i;
        else break;
      }
    }
    if (cut <= 0) {
      // One word longer than the line: cut it
      lines[count++] = fitToWidth(canvas, rest, maxWidth);
      break;
    }
    lines[count++] = rest.substring(0, cut);
    rest = rest.substring(cut);
    rest.trim();
  }
  return count;
}

// Landscape printer list: one row per printer, printer icon in front, selected row highlighted
void drawPrinterListLandscape(M5Canvas &canvas, int rotation) {
  bool buttonsLeft = (rotation == 3);
  int cw = canvas.width();
  int ch = canvas.height();
  canvas.setFreeFont(FontManager::getFont12pt());
  canvas.setTextDatum(ML_DATUM);

  // Room for the A tab on the button side and the scrollbar on the other side
  const int rowH = 30;
  const int maxRows = 3;
  int listX = buttonsLeft ? 22 : 10;
  int listW = cw - 32;

  int rowsShown = printerCount < maxRows ? printerCount : maxRows;
  int listTop = (ch - rowsShown * rowH) / 2;

  // Keep the selected printer visible, in the middle row when possible
  int first = selectedPrinterIndex - 1;
  if (first > printerCount - maxRows) first = printerCount - maxRows;
  if (first < 0) first = 0;

  for (int r = 0; r < rowsShown; r++) {
    int i = first + r;
    int rowY = listTop + r * rowH;
    bool selected = (i == selectedPrinterIndex);

    if (selected) {
      canvas.fillRoundRect(listX, rowY + 2, listW, rowH - 4, 7, TFT_WHITE);
    }
    int iconX = listX + 8;
    int iconY = rowY + (rowH - PRINTICON_HEIGHT) / 2;
    drawIconScaled(canvas, printIcon, PRINTICON_WIDTH, PRINTICON_HEIGHT, iconX, iconY, 1,
                   selected ? TFT_BLACK : (uint16_t)0xBDF7);

    int textX = iconX + PRINTICON_WIDTH + 9;
    String name = fitToWidth(canvas, printerDisplayName(discoveredPrinters[i].name),
                             listX + listW - 8 - textX);
    canvas.setTextColor(selected ? TFT_BLACK : TFT_WHITE);
    canvas.drawString(name, textX, rowY + rowH / 2);
  }

  // Scrollbar when not all printers fit
  if (printerCount > maxRows) {
    int trackX = buttonsLeft ? cw - 6 : 3;
    int trackH = rowsShown * rowH - 4;
    int thumbH = trackH * maxRows / printerCount;
    int thumbY = listTop + 2 + (trackH - thumbH) * first / (printerCount - maxRows);
    canvas.fillRoundRect(trackX, listTop + 2, 3, trackH, 1, PRINT_DARK_GREY);
    canvas.fillRoundRect(trackX, thumbY, 3, thumbH, 1, TFT_WHITE);
  }
}

// Portrait printer "card": the selected printer large in the middle, dots show which one of how many
void drawPrinterCardPortrait(M5Canvas &canvas, int rotation) {
  int cw = canvas.width();
  int ch = canvas.height();
  bool aOnTop = (rotation == 2);

  // Card between the side tabs
  const int cardX = 18;
  const int cardW = cw - 2 * cardX;
  const int cardH = 136;
  int cardY = (ch - cardH) / 2;
  canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

  const int scale = 2;
  int iconW = PRINTICON_WIDTH * scale;
  int iconY = cardY + 14;
  drawIconScaled(canvas, printIcon, PRINTICON_WIDTH, PRINTICON_HEIGHT,
                 (cw - iconW) / 2, iconY, scale, TFT_BLACK);

  canvas.setFreeFont(FontManager::getFont12pt());
  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(TFT_BLACK);
  String lines[3];
  int n = wrapToLines(canvas, printerDisplayName(discoveredPrinters[selectedPrinterIndex].name),
                      cardW - 10, lines, 3);
  int lineH = canvas.fontHeight() + 2;
  int textTop = iconY + PRINTICON_HEIGHT * scale + 10;
  int freeH = cardY + cardH - 8 - textTop;
  int y = textTop + (freeH - n * lineH) / 2;
  for (int i = 0; i < n; i++) {
    canvas.drawString(lines[i], cw / 2, y + i * lineH);
  }

  // Position dots on the side without the A tab
  if (printerCount > 1) {
    const int spacing = 11;
    int dotsY = aOnTop ? ch - 26 : 26;
    int startX = cw / 2 - (printerCount - 1) * spacing / 2;
    for (int i = 0; i < printerCount; i++) {
      int dx = startX + i * spacing;
      if (i == selectedPrinterIndex) {
        canvas.fillCircle(dx, dotsY, 3, TFT_WHITE);
      } else {
        canvas.drawCircle(dx, dotsY, 3, PRINT_GREY);
      }
    }
  }
}

// Print menu screen in the orientation the stick is held: printer list, or the size menu after it
void displayPrinterSelection() {
  if (printMenuStep != PRINT_STEP_PRINTER) {
    drawPrintSizeMenu();
    return;
  }
  int rotation = printUiRotation();
  M5.Display.setRotation(rotation);

  M5Canvas canvas(&M5.Display);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  canvas.fillSprite(TFT_BLACK);

  if (isPortraitRotation(rotation)) {
    drawPrinterCardPortrait(canvas, rotation);
  } else {
    drawPrinterListLandscape(canvas, rotation);
  }
  drawPrintButtonHints(canvas, rotation, printerCount > 1);

  if (!isPortraitRotation(rotation)) drawBatteryIndicator(canvas);  // drawn for 240 px width
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Status screen of the print flow: big printer icon, optional check/cross badge, short text
// (up to two lines, split at '\n'). Landscape: icon left of the text. Portrait: icon above.
void drawPrintStatus(const String &text, int badge) {
  int rotation = printUiRotation();
  bool portrait = isPortraitRotation(rotation);
  M5.Display.setRotation(rotation);
  M5Canvas canvas(&M5.Display);
  int cw = M5.Display.width();
  int ch = M5.Display.height();
  canvas.createSprite(cw, ch);
  canvas.fillSprite(TFT_BLACK);
  canvas.setFreeFont(portrait ? FontManager::getFont12pt() : FontManager::getFont16pt());
  canvas.setTextColor(TFT_WHITE);

  String line1 = text;
  String line2 = "";
  int nl = text.indexOf('\n');
  if (nl >= 0) {
    line1 = text.substring(0, nl);
    line2 = text.substring(nl + 1);
  }
  int lineCount = (line2.length() > 0) ? 2 : 1;
  int textW = canvas.textWidth(line1);
  if (lineCount == 2 && canvas.textWidth(line2) > textW) textW = canvas.textWidth(line2);
  int lh = canvas.fontHeight();

  const int scale = 2;
  int iconW = PRINTICON_WIDTH * scale;
  int iconH = PRINTICON_HEIGHT * scale;
  int iconX, iconY, textX, textY;
  if (portrait) {
    int gap = 16;
    int totalH = iconH + gap + lineCount * lh;
    iconX = (cw - iconW) / 2;
    iconY = (ch - totalH) / 2;
    textX = cw / 2;
    textY = iconY + iconH + gap;
    canvas.setTextDatum(TC_DATUM);
  } else {
    int gap = 16;
    iconX = (cw - (iconW + gap + textW)) / 2;
    if (iconX < 6) iconX = 6;
    iconY = ch / 2 - iconH / 2;
    textX = iconX + iconW + gap;
    textY = ch / 2 - lineCount * lh / 2;
    canvas.setTextDatum(TL_DATUM);
  }

  drawIconScaled(canvas, printIcon, PRINTICON_WIDTH, PRINTICON_HEIGHT, iconX, iconY, scale, TFT_WHITE);

  if (badge != PRINT_BADGE_NONE) {
    int bx = iconX + iconW - 2;
    int by = iconY + iconH - 4;
    canvas.fillCircle(bx, by, 11, TFT_BLACK);
    canvas.fillCircle(bx, by, 9, TFT_WHITE);
    if (badge == PRINT_BADGE_OK) {
      drawCheckMark(canvas, bx, by, 9, TFT_BLACK);
    } else {
      drawCrossMark(canvas, bx, by, 7, TFT_BLACK);
    }
  }

  canvas.drawString(line1, textX, textY);
  if (lineCount == 2) canvas.drawString(line2, textX, textY + lh);

  if (!portrait) drawBatteryIndicator(canvas);
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Create IPP request for Print-Job
void addIPPAttribute(uint8_t* &ptr, uint8_t tag, const char* name, const char* value) {
  *ptr++ = tag;

  // Name length and name
  uint16_t nameLen = strlen(name);
  *ptr++ = (nameLen >> 8) & 0xFF;
  *ptr++ = nameLen & 0xFF;
  memcpy(ptr, name, nameLen);
  ptr += nameLen;

  // Value length and value
  uint16_t valueLen = strlen(value);
  *ptr++ = (valueLen >> 8) & 0xFF;
  *ptr++ = valueLen & 0xFF;
  memcpy(ptr, value, valueLen);
  ptr += valueLen;
}

void addIPPAttributeInt(uint8_t* &ptr, uint8_t tag, const char* name, int32_t value) {
  *ptr++ = tag;

  // Name length and name
  uint16_t nameLen = strlen(name);
  *ptr++ = (nameLen >> 8) & 0xFF;
  *ptr++ = nameLen & 0xFF;
  memcpy(ptr, name, nameLen);
  ptr += nameLen;

  // Value length (4 bytes for int) and value
  *ptr++ = 0x00;
  *ptr++ = 0x04;
  *ptr++ = (value >> 24) & 0xFF;
  *ptr++ = (value >> 16) & 0xFF;
  *ptr++ = (value >> 8) & 0xFF;
  *ptr++ = value & 0xFF;
}

// Create URF (Uncompressed Raster Format) wrapper around JPEG image
// URF is Apple's AirPrint format - simpler than PWG-raster
// NOTE: Only JPEG format (0x01) is reliably supported in URF
// GIF (0x40) and PNG (0x41) format codes are non-standard and will be rejected by printers
// Returns size of URF data, or 0 on error
int createURFFromImage(uint8_t* imageData, int imageSize, uint8_t** urfData, uint8_t imageFormat) {
  // URF file structure:
  // - File header: "UNIRAST\0" (8 bytes)
  // - Page header (32 bytes)
  // - JPEG image data (as-is)
  //
  // imageFormat: Should always be 0x01 (JPEG)

  if (imageFormat != 0x01) {
    Serial.printf("WARNING: URF format code 0x%02X is non-standard and likely unsupported!\n", imageFormat);
    Serial.println("  URF officially only supports JPEG (0x01)");
  }

  Serial.printf("Creating URF wrapper for JPEG image (format code 0x%02X)...\n", imageFormat);

  // Allocate buffer for URF (header + image)
  // Page header is about 30 bytes, add some safety margin
  int urfSize = 8 + 32 + imageSize;
  *urfData = (uint8_t*)malloc(urfSize);

  if (*urfData == nullptr) {
    Serial.println("Failed to allocate URF buffer");
    return 0;
  }

  uint8_t* ptr = *urfData;

  // URF file header
  memcpy(ptr, "UNIRAST", 8);  // Includes null terminator
  ptr += 8;

  // Page header
  // Format: Each field is big-endian

  // BitsPerPixel (1 byte)
  // 0x01 = JPEG compressed (standard and widely supported)
  // Note: Other format codes like 0x40 (GIF) or 0x41 (PNG) are non-standard
  *ptr++ = imageFormat;

  // ColorSpace (1 byte) - 0x01 for sRGB
  *ptr++ = 0x01;

  // Duplex (1 byte) - 0x00 for simplex
  *ptr++ = 0x00;

  // Quality (1 byte) - 0x04 for default
  *ptr++ = 0x04;

  // MediaType (1 byte) - 0x00 for auto
  *ptr++ = 0x00;

  // Copies (1 byte) - 0x01
  *ptr++ = 0x01;

  // Width (4 bytes) - We'll use 1024 as default (Image generation size)
  uint32_t width = 1024;
  *ptr++ = (width >> 24) & 0xFF;
  *ptr++ = (width >> 16) & 0xFF;
  *ptr++ = (width >> 8) & 0xFF;
  *ptr++ = width & 0xFF;

  // Height (4 bytes) - We'll use 1024 as default
  uint32_t height = 1024;
  *ptr++ = (height >> 24) & 0xFF;
  *ptr++ = (height >> 16) & 0xFF;
  *ptr++ = (height >> 8) & 0xFF;
  *ptr++ = height & 0xFF;

  // HWResolutionX (4 bytes) - 300 DPI
  uint32_t dpi = 300;
  *ptr++ = (dpi >> 24) & 0xFF;
  *ptr++ = (dpi >> 16) & 0xFF;
  *ptr++ = (dpi >> 8) & 0xFF;
  *ptr++ = dpi & 0xFF;

  // HWResolutionY (4 bytes) - 300 DPI
  *ptr++ = (dpi >> 24) & 0xFF;
  *ptr++ = (dpi >> 16) & 0xFF;
  *ptr++ = (dpi >> 8) & 0xFF;
  *ptr++ = dpi & 0xFF;

  // Append image data
  memcpy(ptr, imageData, imageSize);
  ptr += imageSize;

  int actualSize = ptr - *urfData;
  Serial.printf("Created URF wrapper: %d bytes (header) + %d bytes (JPEG) = %d bytes total\n",
                actualSize - imageSize, imageSize, actualSize);

  return actualSize;
}

// Convert GIF or PNG image to JPEG format
// This is a STUB function - requires JPEGENC library to be implemented
// Returns: pointer to JPEG buffer (caller must free), or nullptr on failure
// jpegSize: output parameter for JPEG data size
uint8_t* convertToJPEG(uint8_t* imageData, int imageSize, bool isGIF, bool isPNG, int* jpegSize) {
  *jpegSize = 0;

#ifdef ENABLE_JPEG_CONVERSION
  Serial.println("Converting PNG to JPEG...");

  // Step 1: Decode PNG to RGB565
  M5Canvas tempCanvas(&M5.Display);
  tempCanvas.setColorDepth(16); // RGB565

  // Find PNG dimensions first by peeking at the header
  // PNG width/height are at bytes 16-23 (big-endian)
  if (imageSize < 24) {
    Serial.println("Image too small to be valid PNG");
    return nullptr;
  }

  int width = (imageData[16] << 24) | (imageData[17] << 16) | (imageData[18] << 8) | imageData[19];
  int height = (imageData[20] << 24) | (imageData[21] << 16) | (imageData[22] << 8) | imageData[23];

  Serial.printf("PNG dimensions: %dx%d\n", width, height);

  // Create sprite and decode PNG
  if (!tempCanvas.createSprite(width, height)) {
    Serial.println("Failed to create canvas for PNG decoding");
    return nullptr;
  }

  tempCanvas.drawPng(imageData, imageSize, 0, 0);
  uint16_t* decoded = (uint16_t*)tempCanvas.getBuffer();

  // Step 2: Convert RGB565 to RGB888 for JPEGENC
  int pixelCount = width * height;
  uint8_t* rgb888 = (uint8_t*)ps_malloc(pixelCount * 3);
  if (!rgb888) {
    Serial.println("Failed to allocate RGB888 buffer");
    tempCanvas.deleteSprite();
    return nullptr;
  }

  // RGB565 -> RGB888 conversion with R/B swap to compensate for BGR encoding
  for (int i = 0; i < pixelCount; i++) {
    uint16_t pixel = decoded[i];
    rgb888[i*3 + 0] = (pixel & 0x1F) << 3;          // Blue -> Red position (bits 4-0)
    rgb888[i*3 + 1] = ((pixel >> 5) & 0x3F) << 2;   // Green unchanged (bits 10-5)
    rgb888[i*3 + 2] = ((pixel >> 11) & 0x1F) << 3;  // Red -> Blue position (bits 15-11)
  }

  tempCanvas.deleteSprite();

  // Step 3: Encode as JPEG
  // Allocate output buffer (estimate: assume 1:10 compression for safety)
  int maxJpegSize = pixelCount * 3 / 10 + 1024; // Conservative estimate + header space
  uint8_t* jpegBuffer = (uint8_t*)malloc(maxJpegSize);
  if (!jpegBuffer) {
    Serial.println("Failed to allocate JPEG output buffer");
    free(rgb888);
    return nullptr;
  }

  JPEGENC encoder;
  int rc = encoder.open(jpegBuffer, maxJpegSize);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to initialize JPEG encoder: %d\n", rc);
    free(rgb888);
    free(jpegBuffer);
    return nullptr;
  }

  JPEGENCODE jpe;
  rc = encoder.encodeBegin(&jpe, width, height, JPEGE_PIXEL_RGB888, JPEGE_SUBSAMPLE_420, JPEGE_Q_HIGH);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to begin JPEG encoding: %d\n", rc);
    encoder.close();
    free(rgb888);
    free(jpegBuffer);
    return nullptr;
  }

  rc = encoder.addFrame(&jpe, rgb888, width * 3);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to encode JPEG frame: %d\n", rc);
    encoder.close();
    free(rgb888);
    free(jpegBuffer);
    return nullptr;
  }

  // close() returns the actual encoded size
  *jpegSize = encoder.close();
  free(rgb888);

  if (*jpegSize <= 0) {
    Serial.println("JPEG encoding produced no output");
    free(jpegBuffer);
    return nullptr;
  }

  // Validate JPEG magic bytes and show header for debugging
  if (*jpegSize < 2 || jpegBuffer[0] != 0xFF || jpegBuffer[1] != 0xD8) {
    Serial.printf("⚠ WARNING: Encoded data doesn't have JPEG magic bytes! Got: %02X %02X\n",
                  jpegBuffer[0], jpegBuffer[1]);
  } else {
    Serial.printf("✓ Valid JPEG header: FF D8\n");

    // Show first 32 bytes for debugging
    Serial.print("First 32 bytes: ");
    for (int i = 0; i < 32 && i < *jpegSize; i++) {
      Serial.printf("%02X ", jpegBuffer[i]);
    }
    Serial.println();
  }

  Serial.printf("✓ Converted PNG to JPEG: %d -> %d bytes\n", imageSize, *jpegSize);
  return jpegBuffer;

#else
  Serial.println("⚠ JPEG conversion not enabled (ENABLE_JPEG_CONVERSION not defined)");
  return nullptr;
#endif

  // IMPLEMENTATION OUTLINE (when library is added):
  /*
  // Step 1: Decode image to RGB pixels
  uint8_t* rgbPixels = nullptr;
  int width = 0, height = 0;

  if (isGIF) {
    // Use existing decodeGifPixels or similar function
    // rgbPixels = decodeGIF(imageData, imageSize, &width, &height);
  } else if (isPNG) {
    // Use PNG decoder (M5GFX or PNGdec library)
    // rgbPixels = decodePNG(imageData, imageSize, &width, &height);
  }

  if (rgbPixels == nullptr) {
    Serial.println("Failed to decode image to pixels");
    return nullptr;
  }

  // Step 2: Encode as JPEG using JPEGENC
  JPEGENC jpeg;
  jpeg.open(width, height, JPEG_PIXEL_RGB888);
  jpeg.encodeBegin(85);  // Quality 85

  // Encode scanlines
  for (int y = 0; y < height; y++) {
    jpeg.addMCUs(&rgbPixels[y * width * 3], width);
  }

  // Get JPEG buffer
  uint8_t* jpegBuffer = jpeg.close(jpegSize);

  free(rgbPixels);
  return jpegBuffer;
  */

  return nullptr;  // Not implemented yet
}

// Query printer capabilities (Get-Printer-Attributes)
bool getPrinterCapabilities(PrinterInfo& printer, String& supportedFormats) {
  Serial.println("\n========== QUERYING PRINTER CAPABILITIES ==========");
  Serial.printf("Printer: %s (%s:%d)\n",
                printer.name.c_str(),
                printer.ip.toString().c_str(),
                printer.port);

  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";

  // Create IPP Get-Printer-Attributes request
  uint8_t ippRequest[512];
  uint8_t* ptr = ippRequest;

  // IPP version (1.1)
  *ptr++ = 0x01;
  *ptr++ = 0x01;

  // Operation ID: Get-Printer-Attributes (0x000B)
  *ptr++ = 0x00;
  *ptr++ = 0x0B;

  // Request ID
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x01;

  // Operation attributes group tag
  *ptr++ = 0x01;

  // attributes-charset
  addIPPAttribute(ptr, 0x47, "attributes-charset", "utf-8");

  // attributes-natural-language
  addIPPAttribute(ptr, 0x48, "attributes-natural-language", "en");

  // printer-uri
  addIPPAttribute(ptr, 0x45, "printer-uri", printerUri.c_str());

  // requested-attributes (only ask for document-format-supported)
  addIPPAttribute(ptr, 0x44, "requested-attributes", "document-format-supported");

  // End of attributes
  *ptr++ = 0x03;

  int ippHeaderSize = ptr - ippRequest;

  // Connect to printer
  WiFiClient client;

  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("Connection failed!");
    return false;
  }

  // Send HTTP POST headers
  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String(ippHeaderSize) + "\r\n");
  client.print("\r\n");

  // Send IPP request
  client.write(ippRequest, ippHeaderSize);

  // Read response - handle HTTP 100 Continue properly
  String response = "";
  unsigned long timeout = millis() + 10000;  // 10 second timeout
  bool gotFinalResponse = false;
  unsigned long lastDataTime = millis();

  while (millis() < timeout) {
    // Wait for data
    while (client.available() == 0 && millis() < timeout) {
      // If we got a 200 OK response and no data for 500ms, we're probably done
      if (gotFinalResponse && (millis() - lastDataTime > 500)) {
        break;
      }
      delay(10);
    }

    // Check if we should exit
    if (millis() >= timeout ||
        (gotFinalResponse && !client.available() && (millis() - lastDataTime > 500))) {
      break;
    }

    // Read available data
    if (client.available()) {
      while (client.available()) {
        response += (char)client.read();
      }
      lastDataTime = millis();
    }

    // Check if we got a final response (not just 100 Continue)
    if (!gotFinalResponse) {
      if (response.indexOf("HTTP/1.1 200") >= 0 ||
          response.indexOf("HTTP/1.0 200") >= 0 ||
          response.indexOf("HTTP/1.1 4") >= 0 ||  // 4xx errors
          response.indexOf("HTTP/1.1 5") >= 0) {  // 5xx errors
        gotFinalResponse = true;
        // Don't break - keep reading until no more data
      }
    }

    // If we got "100 Continue", keep waiting for the real response
    if (response.indexOf("100 Continue") >= 0 && !gotFinalResponse) {
      Serial.println("Got '100 Continue', waiting for final capabilities response...");
      delay(100);  // Give printer time to send actual data
    }
  }

  client.stop();

  if (!gotFinalResponse && millis() >= timeout) {
    Serial.println("Timeout waiting for capabilities response!");
    return false;
  }

  Serial.println("Capabilities response received");
  Serial.printf("Response length: %d bytes\n", response.length());

  // Handle HTTP 100 Continue followed by 200 OK
  // Find the FINAL HTTP response (skip 100 Continue if present)
  int finalResponseStart = 0;
  if (response.indexOf("HTTP/1.1 100") >= 0 || response.indexOf("HTTP/1.0 100") >= 0) {
    Serial.println("Detected 100 Continue, looking for final response...");
    // Find the second HTTP response
    int secondHttp = response.indexOf("HTTP/1", 10);  // Start search after first "HTTP/1"
    if (secondHttp >= 0) {
      finalResponseStart = secondHttp;
      Serial.printf("Found final response at position: %d\n", finalResponseStart);
    }
  }

  // Find the end of HTTP headers (look for \r\n\r\n after final response start)
  int headerEnd = response.indexOf("\r\n\r\n", finalResponseStart);
  if (headerEnd == -1) {
    Serial.println("ERROR: Could not find end of HTTP headers");
    return false;
  }

  // Check if response is chunked (look in the final response headers only)
  String finalHeaders = response.substring(finalResponseStart, headerEnd);
  bool isChunked = (finalHeaders.indexOf("Transfer-Encoding: chunked") >= 0 ||
                    finalHeaders.indexOf("transfer-encoding: chunked") >= 0);

  // Get the body starting position
  int bodyStart = headerEnd + 4;  // Skip past \r\n\r\n

  Serial.printf("HTTP headers end at: %d, chunked=%s\n", headerEnd, isChunked ? "YES" : "NO");

  // Decode chunked data if needed
  String ippData = "";
  if (isChunked) {
    Serial.println("Decoding chunked transfer encoding...");
    int pos = bodyStart;
    while (pos < response.length()) {
      // Read chunk size (hex string until \r\n)
      int lineEnd = response.indexOf("\r\n", pos);
      if (lineEnd == -1) break;

      String chunkSizeStr = response.substring(pos, lineEnd);
      int chunkSize = strtol(chunkSizeStr.c_str(), NULL, 16);

      Serial.printf("  Chunk size: 0x%s = %d bytes\n", chunkSizeStr.c_str(), chunkSize);

      if (chunkSize == 0) break;  // Last chunk

      // Read chunk data
      pos = lineEnd + 2;  // Skip \r\n
      ippData += response.substring(pos, pos + chunkSize);
      pos += chunkSize + 2;  // Skip chunk data + trailing \r\n
    }
  } else {
    ippData = response.substring(bodyStart);
  }

  Serial.printf("IPP data size: %d bytes\n", ippData.length());

  // Parse supported formats from binary IPP data
  // IPP encodes strings with length prefixes, so indexOf() won't work due to null bytes
  supportedFormats = "";

  // Convert String to byte array for proper binary handling
  const uint8_t* ippBytes = (const uint8_t*)ippData.c_str();
  int ippDataLen = ippData.length();

  // Search for format strings in the binary IPP data
  for (int i = 0; i < ippDataLen - 10; i++) {
    // Look for mimeMediaType tag (0x49) followed by format strings
    if (ippBytes[i] == 0x49) {  // mimeMediaType tag
      // Skip name length (2 bytes) and name
      int nameLen = (ippBytes[i+1] << 8) | ippBytes[i+2];
      int valuePos = i + 3 + nameLen;

      if (valuePos + 2 < ippDataLen) {
        // Get value length
        int valueLen = (ippBytes[valuePos] << 8) | ippBytes[valuePos+1];
        valuePos += 2;

        if (valuePos + valueLen <= ippDataLen) {
          // Extract the format string
          String format = "";
          for (int j = 0; j < valueLen; j++) {
            format += (char)ippBytes[valuePos + j];
          }

          // Check which format it is
          if (format == "image/jpeg") {
            supportedFormats += "jpeg ";
            Serial.println("  ✓ Supports image/jpeg");
          } else if (format == "image/png") {
            supportedFormats += "png ";
            Serial.println("  ✓ Supports image/png");
          } else if (format == "image/gif") {
            supportedFormats += "gif ";
            Serial.println("  ✓ Supports image/gif");
          } else if (format == "application/pdf") {
            supportedFormats += "pdf ";
            Serial.println("  ✓ Supports application/pdf");
          } else if (format == "application/octet-stream") {
            supportedFormats += "octet-stream ";
            Serial.println("  ✓ Supports application/octet-stream");
          } else if (format == "image/pwg-raster") {
            supportedFormats += "pwg-raster ";
            Serial.println("  ✓ Supports image/pwg-raster");
          } else if (format == "image/urf") {
            supportedFormats += "urf ";
            Serial.println("  ✓ Supports image/urf");
          } else if (format == "application/postscript") {
            supportedFormats += "postscript ";
            Serial.println("  ✓ Supports application/postscript");
          }
        }
      }
    }
  }
  if (ippData.indexOf("application/vnd.hp-PCL") >= 0) {
    supportedFormats += "pcl ";
    Serial.println("  ✓ Supports application/vnd.hp-PCL");
  }

  if (supportedFormats.length() == 0) {
    Serial.println("  ⚠ No recognized formats found in IPP response");
    Serial.println("  Printing first 200 bytes of IPP data for debugging:");
    for (int i = 0; i < min(200, (int)ippData.length()); i++) {
      char c = ippData[i];
      if (c >= 32 && c <= 126) {
        Serial.print(c);
      } else {
        Serial.printf("[%02X]", (uint8_t)c);
      }
    }
    Serial.println();

    // Try octet-stream as last resort
    Serial.println("  Will try application/octet-stream as fallback");
    supportedFormats = "octet-stream ";
  }

  return true;
}

bool printImage() {
  if (!hasImage || imageBuffer == nullptr || imageBufferSize == 0) {
    Serial.println("No image to print");
    return false;
  }

  if (printerCount == 0 || selectedPrinterIndex >= printerCount) {
    Serial.println("No valid printer selected");
    return false;
  }

  PrinterInfo& printer = discoveredPrinters[selectedPrinterIndex];

  Serial.println("\n========== PRINTING IMAGE ==========");
  Serial.printf("Printer: %s (%s:%d)\n",
                printer.name.c_str(),
                printer.ip.toString().c_str(),
                printer.port);
  Serial.printf("Image size: %d bytes\n", imageBufferSize);

  // First, check printer capabilities
  String supportedFormats = "";
  bool capabilitiesQueried = getPrinterCapabilities(printer, supportedFormats);

  if (!capabilitiesQueried) {
    Serial.println("Warning: Could not query printer capabilities");
    supportedFormats = "";  // Don't assume anything
  }

  Serial.printf("Supported formats string: '%s'\n", supportedFormats.c_str());

  // Preferred: render the chosen page size with footnote as a PWG raster page
  if (supportedFormats.indexOf("pwg-raster") >= 0) {
    int result = printComposedPage(printer);
    if (result != PRINT_COMPOSE_UNAVAILABLE) return result == PRINT_COMPOSE_OK;
    Serial.println("Could not build the page, sending the image file instead");
  }

  // Determine best format to use
  String documentFormat = "";
  bool useURFWrapper = false;

  // Priority order: JPEG > URF wrapper (most reliable) > octet-stream fallback
  if (supportedFormats.indexOf("jpeg") >= 0) {
    documentFormat = "image/jpeg";
    Serial.println("✓ Selected format: image/jpeg");
  } else if (supportedFormats.indexOf("urf") >= 0) {
    // Use URF wrapper around JPEG
    documentFormat = "image/urf";
    useURFWrapper = true;
    Serial.println("✓ Selected format: image/urf (will wrap JPEG in URF container)");
  } else if (supportedFormats.indexOf("octet-stream") >= 0) {
    // Try octet-stream as fallback - some printers auto-detect JPEG
    documentFormat = "application/octet-stream";
    Serial.println("✓ Selected format: application/octet-stream (will send raw JPEG)");
    Serial.println("  Note: This may not work if printer can't auto-detect JPEG");
  } else if (supportedFormats.indexOf("pwg-raster") >= 0) {
    Serial.println("⚠ Printer only supports PWG-raster format");
    Serial.println("  PWG requires full decompression - trying URF wrapper instead");
    if (supportedFormats.indexOf("urf") >= 0) {
      documentFormat = "image/urf";
      useURFWrapper = true;
    } else {
      documentFormat = "application/octet-stream";
      Serial.println("  No URF support, trying application/octet-stream");
    }
  } else if (supportedFormats.indexOf("pdf") >= 0) {
    Serial.println("⚠ Printer supports PDF but we can't convert yet");
    Serial.println("  Trying application/octet-stream as fallback...");
    documentFormat = "application/octet-stream";
  } else {
    // No recognized formats, or query failed
    Serial.println("⚠ No recognized formats detected");
    Serial.println("  Trying application/octet-stream as last resort...");
    documentFormat = "application/octet-stream";
  }

  Serial.printf("Final document-format: %s\n", documentFormat.c_str());
  Serial.printf("Use URF wrapper: %s\n", useURFWrapper ? "YES" : "NO");

  // Build printer URI
  String printerUri = "ipp://" + printer.ip.toString() + ":" + String(printer.port) + "/ipp/print";

  // Create IPP Print-Job request
  uint8_t ippRequest[1024];
  uint8_t* ptr = ippRequest;

  // IPP version (1.1)
  *ptr++ = 0x01;
  *ptr++ = 0x01;

  // Operation ID: Print-Job (0x0002)
  *ptr++ = 0x00;
  *ptr++ = 0x02;

  // Request ID
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x00;
  *ptr++ = 0x01;

  // Operation attributes group tag
  *ptr++ = 0x01;

  // attributes-charset
  addIPPAttribute(ptr, 0x47, "attributes-charset", "utf-8");

  // attributes-natural-language
  addIPPAttribute(ptr, 0x48, "attributes-natural-language", "en");

  // printer-uri
  addIPPAttribute(ptr, 0x45, "printer-uri", printerUri.c_str());

  // requesting-user-name
  addIPPAttribute(ptr, 0x42, "requesting-user-name", "ai-stick");

  // job-name
  addIPPAttribute(ptr, 0x42, "job-name", "AI-Stick Print");

  // document-format (use detected format)
  addIPPAttribute(ptr, 0x49, "document-format", documentFormat.c_str());

  // End of attributes
  *ptr++ = 0x03;

  int ippHeaderSize = ptr - ippRequest;
  Serial.printf("IPP header size: %d bytes\n", ippHeaderSize);

  // Connect to printer
  WiFiClient client;

  Serial.printf("Connecting to printer at %s:%d...\n",
                printer.ip.toString().c_str(),
                printer.port);

  if (!client.connect(printer.ip, printer.port)) {
    Serial.println("Connection failed!");
    return false;
  }

  Serial.println("Connected! Sending HTTP POST...");

  // Detect image format
  bool isJPEG = (imageBufferSize >= 2 && imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8);
  bool isGIF = (imageBufferSize >= 6 &&
                imageBuffer[0] == 'G' && imageBuffer[1] == 'I' && imageBuffer[2] == 'F');
  bool isPNG = (imageBufferSize >= 8 &&
                imageBuffer[0] == 0x89 && imageBuffer[1] == 0x50 &&
                imageBuffer[2] == 0x4E && imageBuffer[3] == 0x47);

  Serial.printf("Image format detection: JPEG=%s, GIF=%s, PNG=%s\n",
                isJPEG ? "YES" : "NO",
                isGIF ? "YES" : "NO",
                isPNG ? "YES" : "NO");

  // Prepare image data (may be converted to JPEG below)
  uint8_t* dataToSend = imageBuffer;
  int dataSize = imageBufferSize;
  uint8_t* convertedBuffer = nullptr;

  // Check if document format matches actual image format
  // If we selected JPEG but have PNG/GIF, convert to JPEG
  if (documentFormat == "image/jpeg" && !isJPEG) {
    Serial.println("⚠ WARNING: Selected image/jpeg but image is not JPEG!");

    if (isPNG && supportedFormats.indexOf("png") >= 0) {
      Serial.println("  Image is PNG and printer supports it - switching to image/png");
      documentFormat = "image/png";
    } else if (isGIF && supportedFormats.indexOf("gif") >= 0) {
      Serial.println("  Image is GIF and printer supports it - switching to image/gif");
      documentFormat = "image/gif";
    } else if (isPNG || isGIF) {
      // Canon TS5151 doesn't support PNG/GIF and JPEGENC creates incompatible JPEGs
      Serial.println("  ⚠ Image is PNG/GIF but printer requires JPEG");
      Serial.println("  ⚠ JPEGENC library creates JPEGs that Canon cannot render");
      Serial.println("  See docs/PRINTER_FINDINGS.md for solutions");
      Serial.println("  Recommended: Use gpt-image-1.5 (creates JPEG) or external converter");
      return false;
    }
  }
  uint8_t* urfBuffer = nullptr;

  if (useURFWrapper) {
    // Only JPEG can be wrapped in URF reliably
    // GIF and PNG format codes (0x40, 0x41) are non-standard and not supported
    if (isJPEG) {
      Serial.println("Wrapping JPEG in URF format...");
      // Use dataToSend/dataSize which may already be converted JPEG
      dataSize = createURFFromImage(dataToSend, dataSize, &urfBuffer, 0x01);
      if (dataSize == 0) {
        Serial.println("Failed to create URF wrapper!");
        client.stop();
        if (convertedBuffer) free(convertedBuffer);
        return false;
      }
      dataToSend = urfBuffer;
    } else if (isGIF || isPNG) {
      Serial.println("⚠ Cannot wrap GIF/PNG in URF (unsupported format codes)");

#ifdef ENABLE_JPEG_CONVERSION
      // JPEG conversion enabled - convert GIF/PNG to JPEG, then wrap in URF
      Serial.println("  Strategy: Convert to JPEG and wrap in URF");

      int jpegSize = 0;
      uint8_t* jpegBuffer = convertToJPEG(imageBuffer, imageBufferSize, isGIF, isPNG, &jpegSize);

      if (jpegBuffer != nullptr && jpegSize > 0) {
        Serial.printf("✓ Converted to JPEG (%d bytes)\n", jpegSize);

        // Now wrap the JPEG in URF
        dataSize = createURFFromImage(jpegBuffer, jpegSize, &urfBuffer, 0x01);
        free(jpegBuffer);  // Free intermediate JPEG buffer

        if (dataSize == 0) {
          Serial.println("Failed to create URF wrapper after conversion!");
          client.stop();
          return false;
        }
        dataToSend = urfBuffer;
        // useURFWrapper remains true, documentFormat stays as image/urf
      } else {
        Serial.println("✗ JPEG conversion failed, falling back to octet-stream");
        useURFWrapper = false;
        documentFormat = "application/octet-stream";
      }
#else
      // JPEG conversion not enabled - send as raw octet-stream
      Serial.println("  Strategy: Send as octet-stream (printer auto-detect)");
      Serial.println("  Tip: Enable JPEG conversion if this fails (see top of sketch)");

      useURFWrapper = false;
      documentFormat = "application/octet-stream";
#endif
    } else {
      Serial.println("⚠ Unknown image format - trying octet-stream");
      useURFWrapper = false;
      documentFormat = "application/octet-stream";
    }
  }

  // Send HTTP POST headers
  client.print("POST /ipp/print HTTP/1.1\r\n");
  client.print("Host: " + printer.ip.toString() + "\r\n");
  client.print("Content-Type: application/ipp\r\n");
  client.print("Content-Length: " + String(ippHeaderSize + dataSize) + "\r\n");
  client.print("\r\n");

  // Send IPP request
  client.write(ippRequest, ippHeaderSize);

  // Send image data
  const char* dataType = useURFWrapper ? "URF-wrapped image" :
                        (isJPEG ? "JPEG" : isGIF ? "GIF" : isPNG ? "PNG" : "image");
  Serial.printf("Sending %s data (%d bytes)...\n", dataType, dataSize);

  // Debug: Show first few bytes being sent
  if (dataSize >= 4) {
    Serial.printf("First 4 bytes: %02X %02X %02X %02X\n",
                  dataToSend[0], dataToSend[1], dataToSend[2], dataToSend[3]);
  }

  client.write(dataToSend, dataSize);

  // Free URF buffer if allocated
  if (urfBuffer != nullptr) {
    free(urfBuffer);
    urfBuffer = nullptr;
  }

  // Wait for response
  String response = "";
  unsigned long timeout = millis() + 30000;  // 30 second timeout
  bool gotFinalResponse = false;

  while (millis() < timeout && !gotFinalResponse) {
    // Wait for data
    while (client.available() == 0 && millis() < timeout) {
      delay(10);
    }

    if (millis() >= timeout) {
      break;
    }

    // Read available data
    while (client.available()) {
      response += (char)client.read();
    }

    // Check if we got a final response (not just 100 Continue)
    if (response.indexOf("HTTP/1.1 200") >= 0 ||
        response.indexOf("HTTP/1.0 200") >= 0 ||
        response.indexOf("HTTP/1.1 4") >= 0 ||  // 4xx errors
        response.indexOf("HTTP/1.1 5") >= 0) {  // 5xx errors
      gotFinalResponse = true;
      break;
    }

    // If we got "100 Continue", keep waiting for the real response
    if (response.indexOf("100 Continue") >= 0) {
      Serial.println("Got '100 Continue', waiting for final response...");
      delay(100);  // Give printer time to process
      continue;
    }

    // If we have some response but not recognized, wait a bit more
    if (response.length() > 0) {
      delay(100);
    }
  }

  client.stop();

  if (!gotFinalResponse && millis() >= timeout) {
    Serial.println("Timeout waiting for final response!");
    Serial.println("Last response received:");
    Serial.println(response.substring(0, 500));
    return false;
  }

  Serial.println("Response received:");
  Serial.println(response.substring(0, 500));  // Print first 500 chars

  // Check if successful (look for HTTP 200)
  bool success = response.indexOf("HTTP/1.1 200") >= 0 || response.indexOf("HTTP/1.0 200") >= 0;

  if (success) {
    Serial.println("Print job submitted successfully!");
  } else {
    Serial.println("Print job may have failed - check response");
  }

  // Clean up converted buffer if we created one
  if (convertedBuffer) {
    free(convertedBuffer);
  }

  return success;
}

// Build a compact settings description for the GPT system prompt
// Reads current values from Preferences so GPT knows the current state
String buildSettingsPrompt() {
  Preferences sp;
  sp.begin("settings", true);

  String prompt = "Geraete-Einstellungen per Sprache aenderbar (key, Typ, Bereich, aktuell): ";
  for (int i = 0; i < SETTINGS_REGISTRY_COUNT; i++) {
    const SettingDef& s = settingsRegistry[i];
    if (i > 0) prompt += "; ";
    prompt += String(s.nameDE) + " '" + s.key + "' ";

    if (strcmp(s.type, "bool") == 0) {
      bool val = sp.getBool(s.key, s.defVal != 0);
      prompt += "(ein/aus, jetzt:" + String(val ? "ein" : "aus") + ")";
    } else if (strcmp(s.type, "int") == 0) {
      int val = sp.getInt(s.key, (int)s.defVal);
      if (s.allowedVals) {
        prompt += "(" + String(s.allowedVals) + ", jetzt:" + String(val) + ")";
      } else {
        prompt += "(" + String(s.minVal) + "-" + String(s.maxVal) + ", jetzt:" + String(val) + ")";
      }
    } else if (strcmp(s.type, "long") == 0) {
      long val = sp.getLong(s.key, s.defVal);
      // Display sleep timeouts in seconds for user-friendliness
      prompt += "(" + String(s.minVal) + "-" + String(s.maxVal) + " Sek, jetzt:" + String(val / 1000) + ")";
    } else if (strcmp(s.type, "string") == 0) {
      String val = sp.getString(s.key, "DE");
      prompt += "(" + String(s.allowedVals) + ", jetzt:" + val + ")";
    }
  }
  sp.end();
  return prompt;
}

// Apply a setting change from voice command via GPT
// Returns a confirmation message for display
String applySettingChange(const String& key, const String& value) {
  // Find the setting in the registry
  const SettingDef* setting = nullptr;
  for (int i = 0; i < SETTINGS_REGISTRY_COUNT; i++) {
    if (key == settingsRegistry[i].key) {
      setting = &settingsRegistry[i];
      break;
    }
  }

  if (!setting) {
    Serial.println("Setting not found: " + key);
    return "Einstellung nicht gefunden.";
  }

  Serial.printf("Applying setting: %s = %s (type: %s)\n", key.c_str(), value.c_str(), setting->type);

  Preferences sp;
  sp.begin("settings", false);

  if (strcmp(setting->type, "bool") == 0) {
    // Accept various boolean representations
    String v = value;
    v.toLowerCase();
    bool boolVal = (v == "true" || v == "1" || v == "ein" || v == "on" || v == "an" || v == "ja" || v == "yes");
    sp.putBool(setting->key, boolVal);
    sp.end();

    if (key == "memory" && !boolVal) {
      memoryClear();
    }

    // Hot-apply parental lock
    if (key == "parentLock") {
      parentalLockEnabled = boolVal;
      // Also save to powerstate namespace for consistency
      Preferences ps;
      ps.begin(PREFS_NAMESPACE, false);
      ps.putBool(PREFS_KEY_PARENTAL_LOCK, boolVal);
      ps.end();
    }

    String displayName = String(setting->nameDE);
    return displayName + (boolVal ? " aktiviert." : " deaktiviert.");

  } else if (strcmp(setting->type, "int") == 0) {
    int intVal = value.toInt();

    // Validate against allowed values if specified
    if (setting->allowedVals) {
      String allowed = String(setting->allowedVals);
      bool found = false;
      int startIdx = 0;
      while (startIdx < (int)allowed.length()) {
        int commaIdx = allowed.indexOf(',', startIdx);
        if (commaIdx < 0) commaIdx = allowed.length();
        String token = allowed.substring(startIdx, commaIdx);
        token.trim();
        if (token.toInt() == intVal) { found = true; break; }
        startIdx = commaIdx + 1;
      }
      if (!found) {
        sp.end();
        return "Ungueltig. Erlaubt: " + String(setting->allowedVals);
      }
    } else if (setting->minVal >= 0 && setting->maxVal >= 0) {
      if (intVal < setting->minVal) intVal = setting->minVal;
      if (intVal > setting->maxVal) intVal = setting->maxVal;
    }

    sp.putInt(setting->key, intVal);
    sp.end();

    // Hot-apply runtime globals
    if (key == "volume") {
      M5.Speaker.setVolume(intVal);
    } else if (key == "sampleRate") {
      activeSampleRate = intVal;
      activeRecordSamples = activeSampleRate * activeRecordSeconds;
    } else if (key == "recordSec") {
      activeRecordSeconds = intVal;
      activeRecordSamples = activeSampleRate * activeRecordSeconds;
    }

    return String(setting->nameDE) + " auf " + String(intVal) + " gesetzt.";

  } else if (strcmp(setting->type, "long") == 0) {
    // User speaks in seconds, we store in milliseconds
    long longVal = (long)value.toInt() * 1000;
    long minMs = (long)setting->minVal * 1000;
    long maxMs = (long)setting->maxVal * 1000;
    if (longVal < minMs) longVal = minMs;
    if (longVal > maxMs) longVal = maxMs;

    sp.putLong(setting->key, longVal);
    sp.end();
    loadSleepTimeouts();

    return String(setting->nameDE) + " auf " + String(longVal / 1000) + " Sekunden gesetzt.";

  } else if (strcmp(setting->type, "string") == 0) {
    // Validate against allowed values
    if (setting->allowedVals) {
      String allowed = String(setting->allowedVals);
      String upperVal = value;
      upperVal.toUpperCase();
      bool found = false;
      int startIdx = 0;
      while (startIdx < (int)allowed.length()) {
        int commaIdx = allowed.indexOf(',', startIdx);
        if (commaIdx < 0) commaIdx = allowed.length();
        String token = allowed.substring(startIdx, commaIdx);
        token.trim();
        if (token == upperVal) { found = true; break; }
        startIdx = commaIdx + 1;
      }
      if (!found) {
        sp.end();
        return "Ungueltig. Erlaubt: " + String(setting->allowedVals);
      }
      sp.putString(setting->key, upperVal);
      if (key == "language") {
        memoryClear();  // don't mix languages across remembered turns
      }
    } else {
      sp.putString(setting->key, value);
    }
    sp.end();

    return String(setting->nameDE) + " auf " + value + " gesetzt.";
  }

  sp.end();
  return "Einstellung gespeichert.";
}

GPTResponse askGPT(const String &question) {
  Serial.println("\n========== ASKING AI ==========");
  Serial.println("Question: " + question);

  GPTResponse gptResult = {"", "", false, false, false, false, false, "", "", false, 0, "", false, false, false};

  // Get current date/time for context
  String dateTimeContext = getCurrentDateTimeString();
  // Build dynamic settings description for the prompt
  String settingsDesc = buildSettingsPrompt();
  String systemPrompt = "Du bist ein Informationsassistent für ein 8-jähriges Kind. ";
  if (dateTimeContext.length() > 0) {
    systemPrompt += dateTimeContext + " ";
  }
  systemPrompt += "Antworte als JSON mit { \"answer\": \"\", \"image_query\":\"\", \"model\":\"\", \"create_image\": false, \"is_wolfram\": false, \"is_audio\": false, \"is_setting\": false, \"setting_key\": \"\", \"setting_value\": \"\", \"set_timer\": false, \"timer_minutes\": 0, \"timer_label\": \"\", \"list_timers\": false, \"clear_timer\": false, \"is_onboarding\": false }, wobei 'answer' eine direkte, sachliche, altersgerechte Antwort in der Sprache der Frage ist (max 20 Wörter), 'image_query' einen englischen kurzen Suchbegriff enthält, der bei Wikipedia-Suche einen Artikel-Treffer für das Thema liefert, besser ist ein einzelner Begriff oder der Name der Person, ohne Appendix wie Porträt, Geschichte, Übersicht, o.ä. Gut sind bspw. 'Space', schlecht ist 'Space sound'. Gut ist 'Classical music', schlecht ist 'music history'; 'create_image' auf true gesetzt wird wenn der Nutzer explizit ein Bild malen/zeichnen/erstellen will (z.B. 'male ein Bild von', 'zeichne', 'erstelle ein Bild'; nicht bei Funktionskurven wie 'Zeichne x^2', siehe GRAPHEN), und 'is_wolfram' auf true gesetzt wird wenn es eine mathematische Berechnung, Gleichung, Visualisierung ist oder wenn eines der Wörter 'Wolfram', 'Visualisiere', 'Berechne' erwähnt wird (ausser wenn 'model' gesetzt ist, siehe 3D-MODELLE), und 'is_audio' auf true gesetzt wird wenn der Nutzer nach Musik, Liedern, Gesang, Abspielen, Klängen, Geschichten erzählen, Singen oder ähnlichem fragt (z.B. 'Spiele ab', 'Sing mir', 'Erzähle mir', 'Play a song', 'Tell me a story'). WICHTIG: Wenn create_image true ist: (1) setze answer auf eine Bestätigung wie 'Ich male <transcribed user request> fuer dich!' oder 'Ich zeichne <transcribed user request>.' NICHT und niemals 'Ich kann keine Bilder erstellen', (2) image_query muss eine detaillierte englische Bildbeschreibung enthalten, die familienfreundlich, kindgerecht, ohne Text und für 8-Jährige geeignet ist. Wenn is_wolfram true ist, berechne nichts selbst, sage nur 'Ich berechne <User-Anfrage>...' oder 'Ich visualisiere <User-Anfrage>...' oder ähnlich. Immer familienfreundlich. Nie Rückfragen stellen. Nie gefährliche Aktivitäten vorschlagen. Beantworte Fragen zu aktuellen Amtsträger in der Art 'Aktuell weiss ich nicht. Im Jahr XYZ war es ABC.'. Antworten müssen in ASCII Text Plain sein, ohne Sonderzeichen, Symbole oder Emojis. Bereits bei leichten Unsicherheiten, gebe das zu. Dieses hier verwendete Gerät heisst 'Zitro'. Der Macher ist matthias.sala.ch. 3D-MODELLE: 'model' bleibt normalerweise leer. Setze 'model' auf 'molecule:<englischer Name des Stoffs>' NUR wenn der Nutzer nach dem Aufbau oder der Struktur eines einzelnen chemischen Stoffs oder Molekuels fragt oder ausdruecklich ein 3D-Modell davon will (z.B. 'Wie ist Koffein aufgebaut?', 'Wie sieht ein Wassermolekuel aus?', 'Zeig mir Aspirin in 3D', 'Visualisiere Koffein' ergibt 'molecule:caffeine'). Bei allgemeinen Fragen ('Was ist Wasser?', 'Was ist Gold?', 'Wie sieht das Matterhorn aus?') bleibt model leer. Erfinde nie Koordinaten, Kennungen oder Zahlen fuer ein Modell: du nennst nur den Namen, die Daten kommen aus einer Datenbank. Fuelle image_query auch dann wie immer. Wenn model gesetzt ist, ist is_wolfram false. Berechnungen ('Molmasse von Koffein', 'Integral von sin(x)') bleiben is_wolfram mit leerem model. Wenn create_image true ist, bleibt model leer. TIMER/ERINNERUNGEN: Wenn der Nutzer einen Timer, Wecker, Erinnerung oder Alert setzen will (z.B. 'erinnere mich in 90 Minuten an die Waschmaschine', 'Timer auf 5 Minuten', 'alert for tomorrow 5:30am', 'set pomodoro timer', 'Wecker in 2 Stunden'), setze set_timer auf true, timer_minutes auf die Anzahl Minuten ab jetzt (berechne aus der aktuellen Uhrzeit), timer_label auf eine kurze Beschreibung (z.B. 'Waschmaschine', 'Pomodoro', 'Aufwachen'). Pomodoro = 25 Minuten. Bei 'Liste Timer'/'list timers'/'welche Timer laufen'/'pending timers', setze list_timers auf true. Bei 'loesche Timer'/'cancel timers'/'Timer loeschen'/'clear timers', setze clear_timer auf true. Bei Timer-Befehlen alle anderen Felder (create_image, is_wolfram, is_audio, is_setting) auf false setzen. EINSTELLUNGEN: " + settingsDesc + " Wenn der Nutzer eine Einstellung aendern moechte (z.B. 'mach lauter', 'Lautstaerke auf 100', 'switch to English', 'Aufnahme 10 Sekunden', 'Kindersicherung ein', 'stelle Timer auf 60 Sekunden'), setze is_setting auf true, setting_key auf den Preferences-Key (z.B. 'volume'), setting_value auf den neuen Wert als String (bei bool: 'true'/'false', bei Schlaf-Zeiten: Sekunden als Zahl). Die answer soll die Aenderung bestaetigen. Alle anderen Felder auf false/leer setzen. ONBOARDING/SETUP: Wenn der Nutzer den Einrichtungsmodus starten will (z.B. 'Start onboarding mode', 'Onboarding starten', 'Setup starten', 'WLAN Einrichtung starten', 'Make wifi setup page available', 'Einstellungsseite oeffnen', 'Start AP mode', 'Access Point starten', 'open settings page'), setze is_onboarding auf true. Die answer soll bestaetigen z.B. 'Einrichtungsmodus wird gestartet...' oder 'Starting setup mode...'. Alle anderen Felder auf false/leer setzen. GEDAECHTNIS: Fruehere Nachrichten in diesem Gespraech stammen aus den letzten 10 Minuten; nutze sie fuer Folgefragen (z.B. 'und wie gross ist er?'), beantworte aber immer nur die neueste Frage.";
  for (int i = 0; i < EDU_FEATURE_COUNT; i++) {
    if (EDU_FEATURES[i]->promptRule) systemPrompt += String(" ") + EDU_FEATURES[i]->promptRule;
  }

  // Language: EN answers are requested explicitly (German is the prompt's default)
  String userText = question;
  {
    Preferences lp;
    lp.begin("settings", true);
    if (lp.getString("language", "DE") == "EN") userText += " Answer in English.";
    lp.end();
  }

  ChatResult chat = aiChat(systemPrompt, userText, 400, true, true);
  if (!chat.ok) {
    Serial.println("AI request failed: " + chat.error);
    if (chat.error == "Refusal") {
      gptResult.answer = "Dazu kann ich nichts sagen. Frag mich etwas anderes!";
    } else if (chat.error == "API key rejected") {
      // Point to the fix: the key is changed on the settings page
      gptResult.answer = "API key rejected.\nTriple-press the\nside button for\nsettings.";
    } else {
      gptResult.answer = chat.error;
    }
    return gptResult;
  }

  Serial.println("AI content: " + chat.content);

  parseAiAnswer(chat.content, gptResult);

  gptResult.valid = (gptResult.answer.length() > 0);

  if (!gptResult.valid) {
    Serial.println("WARNING: Failed to parse JSON.");
    gptResult.answer = "Das habe ich nicht verstanden. Versuche es nochmals.";
  }

  Serial.println("=================================\n");

  return gptResult;
}

String extractSearchTerms(const String &question, const String &answer) {
  // Strategy: Use the first few words of the question as search terms
  // Remove common question words
  String cleaned = question;
  cleaned.replace("what is", "");
  cleaned.replace("who is", "");
  cleaned.replace("where is", "");
  cleaned.replace("when is", "");
  cleaned.replace("how does", "");
  cleaned.replace("why is", "");
  cleaned.replace("?", "");
  cleaned.trim();

  // Take first 2-3 words as search term
  int firstSpace = cleaned.indexOf(' ');
  int secondSpace = cleaned.indexOf(' ', firstSpace + 1);

  String searchTerm;
  if (secondSpace > 0) {
    searchTerm = cleaned.substring(0, secondSpace);
  } else if (firstSpace > 0) {
    searchTerm = cleaned.substring(0, firstSpace);
  } else {
    searchTerm = cleaned;
  }

  Serial.println("Extracted search term: " + searchTerm);
  return searchTerm;
}

String translateToEnglishForWolfram(const String &query) {
  Serial.println("\n========== TRANSLATING FOR WOLFRAM ==========");
  Serial.println("Original query: " + query);

  String systemPrompt = "Translate the following query to English, making it suitable for a Wolfram Alpha query. Keep mathematical equations semantically the same. Queries to Wolfram Alpha are normally statements rather than directions. So remove the words such as 'Wolfram', 'Math' (and words sounding similar), and remove action words such as 'Calculate', 'Do', 'Create', etc. and instead just describe the wanted thing. E.g. good are '5 + 5 = ?' and 'Torus, radius = 5cm'. Output ONLY the translated query, nothing else.";

  ChatResult chat = aiChat(systemPrompt, query, 200, false, false);
  String translatedQuery = query;  // Default to original
  if (chat.ok) {
    translatedQuery = chat.content;
    translatedQuery.replace("\n", " ");
    translatedQuery.trim();
    Serial.println("Translated query: " + translatedQuery);
  } else {
    Serial.println("Translation failed: " + chat.error);
  }

  Serial.println("==============================================\n");
  return translatedQuery;
}

bool fetchWolframAlphaImage(const String &query) {
  // Translate query to English if needed
  return fetchWolframSimpleImage(translateToEnglishForWolfram(query));
}

// The Simple API's picture of a whole result page: what printing uses, and the answer when there's no text result
bool fetchWolframSimpleImage(const String &englishQuery) {
  Serial.println("\n========== FETCHING WOLFRAM ALPHA IMAGE ==========");

  lastActivityTime = millis();  // Prevent sleep during download

  Serial.println("Query for Wolfram: " + englishQuery);

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    if (!reconnectWiFiNow()) {
      Serial.println("Failed to reconnect WiFi");
      return false;
    }
  }

  // URL encode the query - more comprehensive encoding
  String encoded = "";
  for (int i = 0; i < englishQuery.length(); i++) {
    char c = englishQuery.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }

  String url = "https://api.wolframalpha.com/v1/simple?units=metric&fontsize=8&width=240&layout=labelbar&appid=" WOLFRAM_APPID "&i=" + encoded;

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  http.begin(client, url);
  http.setTimeout(30000);

  // Force HTTP/1.1 to avoid HTTP/2 framing issues
  http.useHTTP10(false);  // Use HTTP/1.1, not HTTP/1.0
  http.addHeader("Connection", "close");  // Ensure connection closes after response

  // Show spinner while waiting for response
  int spinnerFrame = 0;
  drawScreenWithSpinner("Calculating...", spinnerFrame);

  int httpCode = http.GET();
  Serial.printf("Wolfram HTTP response code: %d\n", httpCode);

  if (httpCode != 200) {
    Serial.printf("Failed to fetch Wolfram Alpha image, HTTP code: %d\n", httpCode);
    String responseBody = http.getString();
    Serial.println("Response body: " + responseBody.substring(0, 200));
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  Serial.printf("Wolfram image Content-Length: %d bytes\n", contentLength);

  // Check transfer encoding
  String transferEncoding = http.header("Transfer-Encoding");
  Serial.printf("Transfer-Encoding: %s\n", transferEncoding.c_str());

  // Free existing image buffer
  if (imageBuffer) {
    free(imageBuffer);
    imageBuffer = nullptr;
    imageBufferSize = 0;
  }

  // Stream until the connection closes; Content-Length is unreliable here.
  Serial.println("Streaming Wolfram image until connection closes...");

    WiFiClient* stream = http.getStreamPtr();
    const int CHUNK_SIZE = 4096;
    int bufferCapacity = CHUNK_SIZE;
    int bytesRead = 0;
    unsigned long lastDataTime = millis();
    int lastSpinnerBytes = 0;
    unsigned long lastSpinnerUpdate = millis();
    const unsigned long streamIdleTimeoutMs = 15000;

    // Allocate initial buffer
    imageBuffer = (uint8_t*)malloc(bufferCapacity);
    if (!imageBuffer) {
      Serial.println("Failed to allocate initial buffer");
      http.end();
      return false;
    }

    auto ensureCapacity = [&](int needed) -> bool {
      if (needed <= bufferCapacity) {
        return true;
      }
      int newCapacity = bufferCapacity;
      while (newCapacity < needed) {
        newCapacity += CHUNK_SIZE;
      }
      if (newCapacity > 200000) {
        Serial.printf("Image too large (>200KB), stopping at %d bytes\n", bytesRead);
        return false;
      }
      uint8_t* newBuffer = (uint8_t*)realloc(imageBuffer, newCapacity);
      if (!newBuffer) {
        Serial.println("Failed to grow buffer");
        return false;
      }
      imageBuffer = newBuffer;
      bufferCapacity = newCapacity;
      Serial.printf("Grew buffer to %d bytes\n", newCapacity);
      return true;
    };

    auto readLine = [&](char* line, size_t lineSize) -> int {
      int len = 0;
      while (true) {
        if (stream->available()) {
          int c = stream->read();
          if (c < 0) {
            continue;
          }
          lastDataTime = millis();
          if (c == '\n') {
            break;
          }
          if (c == '\r') {
            continue;
          }
          if (len < (int)lineSize - 1) {
            line[len++] = (char)c;
          }
        } else {
          if (millis() - lastDataTime > streamIdleTimeoutMs) {
            return -1;
          }
          delay(1);
        }
      }
      line[len] = '\0';
      return len;
    };

    auto parseChunkSize = [&](int &outSize) -> bool {
      char line[16];
      int len = 0;
      do {
        len = readLine(line, sizeof(line));
        if (len < 0) {
          return false;
        }
      } while (len == 0);

      for (int i = 0; i < len; i++) {
        if (line[i] == ';') {
          line[i] = '\0';
          break;
        }
      }

      char* endPtr = nullptr;
      unsigned long size = strtoul(line, &endPtr, 16);
      if (endPtr == line) {
        return false;
      }
      outSize = (int)size;
      return true;
    };

    // Detect chunked transfer by sniffing the first line for hex chunk size.
    const int PREFIX_MAX = 12;
    char prefix[PREFIX_MAX];
    int prefixLen = 0;
    bool sawNewline = false;
    while (prefixLen < PREFIX_MAX && (http.connected() || stream->available())) {
      if (stream->available()) {
        char c = (char)stream->read();
        prefix[prefixLen++] = c;
        lastDataTime = millis();
        if (c == '\n') {
          sawNewline = true;
          break;
        }
      } else {
        if (millis() - lastDataTime > streamIdleTimeoutMs) {
          Serial.println("ERROR: Wolfram response stream stalled");
          break;
        }
        delay(1);
      }
    }

    bool useChunked = false;
    int firstChunkSize = 0;
    if (sawNewline) {
      char line[16];
      int lineLen = 0;
      bool hexOnly = true;
      for (int i = 0; i < prefixLen; i++) {
        char c = prefix[i];
        if (c == '\n') {
          break;
        }
        if (c == '\r') {
          continue;
        }
        if (lineLen < (int)sizeof(line) - 1) {
          line[lineLen++] = c;
        }
      }
      line[lineLen] = '\0';
      if (lineLen > 0 && lineLen <= 8) {
        for (int i = 0; i < lineLen; i++) {
          char c = line[i];
          if (!((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F') ||
                c == ';')) {
            hexOnly = false;
            break;
          }
        }
        if (hexOnly) {
          char* endPtr = nullptr;
          unsigned long size = strtoul(line, &endPtr, 16);
          if (endPtr != line) {
            useChunked = true;
            firstChunkSize = (int)size;
          }
        }
      }
    }

    if (useChunked) {
      Serial.printf("Detected chunked transfer, first chunk size: %d bytes\n", firstChunkSize);
      int chunkSize = firstChunkSize;
      while (chunkSize > 0) {
        if (!ensureCapacity(bytesRead + chunkSize)) {
          break;
        }
        int remaining = chunkSize;
        while (remaining > 0) {
          size_t available = stream->available();
          if (available) {
            int toRead = min((int)available, remaining);
            int read = stream->readBytes(imageBuffer + bytesRead, toRead);
            bytesRead += read;
            remaining -= read;
            lastDataTime = millis();

            if (bytesRead > lastSpinnerBytes && millis() - lastSpinnerUpdate >= 200) {
              spinnerFrame++;
              drawScreenWithSpinner("Calculating...", spinnerFrame);
              lastSpinnerBytes = bytesRead;
              lastSpinnerUpdate = millis();
              lastActivityTime = lastSpinnerUpdate;  // Prevent sleep during download
            }
          } else {
            if (millis() - lastDataTime > streamIdleTimeoutMs) {
              Serial.println("ERROR: Wolfram response stream stalled");
              break;
            }
            delay(1);
          }
        }

        // Consume CRLF after chunk data
        while (true) {
          if (stream->available()) {
            char c = (char)stream->read();
            lastDataTime = millis();
            if (c == '\n') {
              break;
            }
          } else {
            if (millis() - lastDataTime > streamIdleTimeoutMs) {
              Serial.println("ERROR: Wolfram response stream stalled");
              break;
            }
            delay(1);
          }
        }

        if (!parseChunkSize(chunkSize)) {
          Serial.println("ERROR: Failed to parse chunk size");
          break;
        }
      }

      // Consume trailing headers after the 0-size chunk
      if (chunkSize == 0) {
        char trailer[32];
        while (true) {
          int len = readLine(trailer, sizeof(trailer));
          if (len < 0 || len == 0) {
            break;
          }
        }
      }
    } else {
      // Fallback: treat as raw stream until connection closes
      if (prefixLen > 0) {
        if (!ensureCapacity(bytesRead + prefixLen)) {
          free(imageBuffer);
          imageBuffer = nullptr;
          http.end();
          return false;
        }
        memcpy(imageBuffer + bytesRead, prefix, prefixLen);
        bytesRead += prefixLen;
      }

      while (http.connected() || stream->available()) {
        size_t available = stream->available();
        if (available) {
          if (!ensureCapacity(bytesRead + available)) {
            break;
          }
          int toRead = min((int)available, bufferCapacity - bytesRead);
          int read = stream->readBytes(imageBuffer + bytesRead, toRead);
          bytesRead += read;
          lastDataTime = millis();
          Serial.printf("Downloaded %d bytes (capacity: %d)\r", bytesRead, bufferCapacity);

          if (bytesRead > lastSpinnerBytes && millis() - lastSpinnerUpdate >= 200) {
            spinnerFrame++;
            drawScreenWithSpinner("Calculating...", spinnerFrame);
            lastSpinnerBytes = bytesRead;
            lastSpinnerUpdate = millis();
            lastActivityTime = lastSpinnerUpdate;  // Prevent sleep during download
          }
        } else {
          if (millis() - lastDataTime > streamIdleTimeoutMs) {
            Serial.println("ERROR: Wolfram response stream stalled");
            break;
          }
          delay(1);
        }
      }
    }

    http.end();
    Serial.println();

    if (millis() - lastDataTime > streamIdleTimeoutMs) {
      free(imageBuffer);
      imageBuffer = nullptr;
      return false;
    }

    if (bytesRead == 0) {
      Serial.println("No data received");
      free(imageBuffer);
      imageBuffer = nullptr;
      return false;
    }

    imageBufferSize = bytesRead;
    Serial.printf("Wolfram Alpha image downloaded successfully: %d bytes\n", imageBufferSize);
    Serial.println("==================================================\n");
    return true;
}

WikiResult searchWikipedia(const String &searchTerms) {
  Serial.println("\n========== WIKIPEDIA SEARCH ==========");
  Serial.println("Search terms: " + searchTerms);

  WikiResult result = {false, "", ""};

  // Check if Wikipedia is rate-limited
  unsigned long currentTime = millis();
  if (wikipediaBlockedUntil > 0 && currentTime < wikipediaBlockedUntil) {
    unsigned long remainingSeconds = (wikipediaBlockedUntil - currentTime) / 1000;
    Serial.printf("Wikipedia is rate-limited. Blocked for %lu more seconds.\n", remainingSeconds);
    return result;
  }

  // Check WiFi connection before making request
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    if (!reconnectWiFiNow()) {
      Serial.println("Failed to reconnect WiFi");
      return result;
    }
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  // URL encode search terms - replace spaces with underscores for REST API
  String encoded = searchTerms;
  encoded.replace(" ", "_");

  // Wikipedia REST API endpoint - cleaner and better rate limits
  String url = "https://en.wikipedia.org/api/rest_v1/page/summary/" + encoded;

  Serial.println("URL: " + url);
  http.begin(client, url);
  http.setTimeout(15000);
  http.addHeader("User-Agent", "AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");
  Serial.println("User-Agent: AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");

  int httpCode = http.GET();
  Serial.printf("HTTP response code: %d\n", httpCode);

  // Handle connection errors
  if (httpCode < 0) {
    Serial.printf("Connection error: %d\n", httpCode);
    Serial.println("Attempting WiFi reconnection...");
    http.end();

    if (reconnectWiFiNow()) {
      Serial.println("Retrying Wikipedia request...");
      http.begin(client, url);
      http.setTimeout(15000);
      http.addHeader("User-Agent", "AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");
      httpCode = http.GET();
      Serial.printf("Retry HTTP response code: %d\n", httpCode);
    }
  }

  if (httpCode == 429) {
    Serial.println("Wikipedia rate limit reached (HTTP 429)");

    // Check for Retry-After header
    String retryAfter = http.header("Retry-After");
    int delaySeconds = 60; // Default to 60 seconds if no header present

    if (retryAfter.length() > 0) {
      delaySeconds = retryAfter.toInt();
      if (delaySeconds <= 0) delaySeconds = 60; // Fallback if parsing fails
      Serial.printf("Retry-After header: %d seconds\n", delaySeconds);
    } else {
      Serial.println("No Retry-After header, using default 60 seconds");
    }

    // If delay is more than 5 seconds, don't retry now - block future requests
    if (delaySeconds > 5) {
      Serial.printf("Delay too long (%d seconds). Blocking Wikipedia requests.\n", delaySeconds);
      wikipediaBlockedUntil = millis() + (delaySeconds * 1000);
      http.end();
      return result;
    }

    // For delays <= 5 seconds, wait and retry once
    Serial.printf("Waiting %d seconds before retry...\n", delaySeconds);
    http.end();
    delay(delaySeconds * 1000);

    // Retry once after delay
    http.begin(client, url);
    http.setTimeout(15000);
    http.addHeader("User-Agent", "AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");
    httpCode = http.GET();
    Serial.printf("Retry after rate limit - HTTP response code: %d\n", httpCode);

    if (httpCode != 200) {
      Serial.println("Wikipedia API request failed after retry");
      http.end();
      return result;
    }
  } else if (httpCode == 404) {
    Serial.println("Wikipedia page not found (HTTP 404)");
    http.end();
    return result;
  } else if (httpCode != 200) {
    Serial.printf("Wikipedia API request failed with code: %d\n", httpCode);
    http.end();
    return result;
  }

  String response = http.getString();
  http.end();

  Serial.println("Wikipedia response:");
  Serial.println(response);

  // Check if this is a disambiguation page
  int typeIdx = response.indexOf("\"type\"");
  if (typeIdx > 0) {
    int typeStart = response.indexOf("\"", typeIdx + 7);
    int typeEnd = response.indexOf("\"", typeStart + 1);
    if (typeStart > 0 && typeEnd > 0) {
      String pageType = response.substring(typeStart + 1, typeEnd);
      if (pageType == "disambiguation") {
        Serial.println("Disambiguation page detected, fetching links...");

        // Get the page title for the parse API request
        int titleIdx = response.indexOf("\"title\"");
        String pageTitle = "";
        if (titleIdx > 0) {
          int titleStart = response.indexOf("\"", titleIdx + 8);
          int titleEnd = response.indexOf("\"", titleStart + 1);
          if (titleStart > 0 && titleEnd > 0) {
            pageTitle = response.substring(titleStart + 1, titleEnd);
            Serial.println("Page title: " + pageTitle);
          }
        }

        if (pageTitle.length() == 0) {
          Serial.println("Could not extract page title");
          Serial.println("======================================\n");
          return result;
        }

        // Use Wikipedia parse API to get links from disambiguation page
        String parseUrl = "https://en.wikipedia.org/w/api.php?action=parse&page=" + pageTitle + "&prop=links&format=json";
        parseUrl.replace(" ", "_");

        Serial.println("Fetching links from: " + parseUrl);
        http.begin(client, parseUrl);
        http.setTimeout(15000);
        http.addHeader("User-Agent", "AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");

        int parseCode = http.GET();
        Serial.printf("Parse API HTTP response code: %d\n", parseCode);

        if (parseCode == 200) {
          String parseResponse = http.getString();
          Serial.println("Parse response (first 500 chars):");
          Serial.println(parseResponse.substring(0, min(500, (int)parseResponse.length())));

          // Find first link with "exists":"" (existing page) and ns:0 (main namespace)
          int linksIdx = parseResponse.indexOf("\"links\"");
          if (linksIdx > 0) {
            int searchStart = linksIdx;
            String firstLink = "";

            // Search for first link with ns:0 and exists
            while (searchStart < parseResponse.length()) {
              int nsIdx = parseResponse.indexOf("\"ns\":", searchStart);
              if (nsIdx < 0) break;

              // Check if ns is 0 (main namespace)
              int nsValueStart = nsIdx + 5;
              int nsValueEnd = parseResponse.indexOf(",", nsValueStart);
              if (nsValueEnd < 0) nsValueEnd = parseResponse.indexOf("}", nsValueStart);

              String nsValue = parseResponse.substring(nsValueStart, nsValueEnd);
              nsValue.trim();

              if (nsValue == "0") {
                // Check if "exists" field is present (indicates existing page)
                int existsIdx = parseResponse.indexOf("\"exists\"", nsIdx);
                int nextLinkIdx = parseResponse.indexOf("{\"ns\":", nsIdx + 5);

                // Make sure exists is in the same link object
                if (existsIdx > 0 && (nextLinkIdx < 0 || existsIdx < nextLinkIdx)) {
                  // Extract the link title
                  int starIdx = parseResponse.indexOf("\"*\"", nsIdx);
                  if (starIdx > 0 && (nextLinkIdx < 0 || starIdx < nextLinkIdx)) {
                    int linkStart = parseResponse.indexOf("\"", starIdx + 3);
                    int linkEnd = parseResponse.indexOf("\"", linkStart + 1);
                    if (linkStart > 0 && linkEnd > 0) {
                      firstLink = parseResponse.substring(linkStart + 1, linkEnd);
                      Serial.println("Found first existing link: " + firstLink);
                      break;
                    }
                  }
                }
              }

              searchStart = nsIdx + 10; // Move past current link
            }

            if (firstLink.length() > 0) {
              // Retry with first link
              firstLink.replace(" ", "_");
              Serial.println("Retrying with first link...");
              String retryUrl = "https://en.wikipedia.org/api/rest_v1/page/summary/" + firstLink;

              http.end(); // Close parse API connection
              http.begin(client, retryUrl);
              http.setTimeout(15000);
              http.addHeader("User-Agent", "AIStick/1.0 (https://matthias.sala.ch/aistick; matthias@sala.ch)");

              int retryCode = http.GET();
              Serial.printf("Retry HTTP response code: %d\n", retryCode);

              if (retryCode == 200) {
                response = http.getString();
                Serial.println("Retry response (first 300 chars):");
                Serial.println(response.substring(0, min(300, (int)response.length())));
              } else {
                Serial.printf("Retry failed with code %d\n", retryCode);
                http.end();
                Serial.println("======================================\n");
                return result;
              }
              http.end();
            } else {
              Serial.println("No valid links found in disambiguation page");
              http.end();
              Serial.println("======================================\n");
              return result;
            }
          }
        } else {
          Serial.printf("Parse API failed with code %d\n", parseCode);
          http.end();
          Serial.println("======================================\n");
          return result;
        }
        http.end();
      }
    }
  }

  // Parse JSON for thumbnail URL (REST API has cleaner structure)
  int thumbIdx = response.indexOf("\"thumbnail\"");
  if (thumbIdx > 0) {
    int sourceIdx = response.indexOf("\"source\"", thumbIdx);
    if (sourceIdx > 0) {
      int urlStart = response.indexOf("\"", sourceIdx + 9);
      int urlEnd = response.indexOf("\"", urlStart + 1);
      if (urlStart > 0 && urlEnd > 0) {
        result.imageUrl = response.substring(urlStart + 1, urlEnd);
        // Unescape backslashes in URL (usually not needed in REST API but keep for safety)
        result.imageUrl.replace("\\/", "/");
        result.found = true;
        Serial.println("Found image: " + result.imageUrl);

        // Also extract title if available
        int titleIdx = response.indexOf("\"title\"");
        if (titleIdx > 0 && titleIdx < thumbIdx) {
          int titleStart = response.indexOf("\"", titleIdx + 8);
          int titleEnd = response.indexOf("\"", titleStart + 1);
          if (titleStart > 0 && titleEnd > 0) {
            result.title = response.substring(titleStart + 1, titleEnd);
            Serial.println("Page title: " + result.title);
          }
        }
      }
    }
  }

  if (!result.found) {
    Serial.println("No thumbnail found for this article");
  }

  Serial.println("======================================\n");
  return result;
}

bool downloadImage(const String &url) {
  Serial.println("\n========== DOWNLOADING IMAGE ==========");
  Serial.println("URL: " + url);
  Serial.printf("Free heap before download: %d bytes\n", ESP.getFreeHeap());

  // Check WiFi connection before making request
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    if (!reconnectWiFiNow()) {
      Serial.println("Failed to reconnect WiFi");
      return IMAGE_FETCH_ERROR;
    }
  }

  // Free existing buffer
  if (imageBuffer) {
    Serial.println("Freeing existing image buffer...");
    free(imageBuffer);
    imageBuffer = nullptr;
    imageBufferSize = 0;
  }

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("Starting HTTP request...");
  http.begin(client, url);
  http.setTimeout(30000);

  int httpCode = http.GET();
  Serial.printf("HTTP response code: %d\n", httpCode);

  // Handle connection errors
  if (httpCode < 0) {
    Serial.printf("Connection error: %d\n", httpCode);
    Serial.println("Attempting WiFi reconnection...");
    http.end();

    if (reconnectWiFiNow()) {
      Serial.println("Retrying image download...");
      http.begin(client, url);
      http.setTimeout(30000);
      httpCode = http.GET();
      Serial.printf("Retry HTTP response code: %d\n", httpCode);
    }
  }

  if (httpCode != 200) {
    Serial.printf("ERROR: Image download failed with code %d\n", httpCode);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  Serial.printf("Content-Length: %d bytes\n", contentLength);

  if (contentLength <= 0) {
    Serial.println("Content-Length unknown, streaming until connection closes...");

    WiFiClient* stream = http.getStreamPtr();
    const int CHUNK_SIZE = 4096;
    int bufferCapacity = CHUNK_SIZE;
    int bytesRead = 0;
    unsigned long lastDataTime = millis();
    const unsigned long streamIdleTimeoutMs = 15000;

    imageBuffer = (uint8_t*)malloc(bufferCapacity);
    if (!imageBuffer) {
      Serial.println("Failed to allocate initial buffer");
      http.end();
      return false;
    }

    auto ensureCapacity = [&](int needed) -> bool {
      if (needed <= bufferCapacity) {
        return true;
      }
      int newCapacity = bufferCapacity;
      while (newCapacity < needed) {
        newCapacity += CHUNK_SIZE;
      }
      if (newCapacity > 300000) {
        Serial.printf("Image too large (>300KB), stopping at %d bytes\n", bytesRead);
        return false;
      }
      uint8_t* newBuffer = (uint8_t*)realloc(imageBuffer, newCapacity);
      if (!newBuffer) {
        Serial.println("Failed to grow buffer");
        return false;
      }
      imageBuffer = newBuffer;
      bufferCapacity = newCapacity;
      return true;
    };

    auto readLine = [&](char* line, size_t lineSize) -> int {
      int len = 0;
      while (true) {
        if (stream->available()) {
          int c = stream->read();
          if (c < 0) {
            continue;
          }
          lastDataTime = millis();
          if (c == '\n') {
            break;
          }
          if (c == '\r') {
            continue;
          }
          if (len < (int)lineSize - 1) {
            line[len++] = (char)c;
          }
        } else {
          if (millis() - lastDataTime > streamIdleTimeoutMs) {
            return -1;
          }
          delay(1);
        }
      }
      line[len] = '\0';
      return len;
    };

    auto parseChunkSize = [&](int &outSize) -> bool {
      char line[16];
      int len = 0;
      do {
        len = readLine(line, sizeof(line));
        if (len < 0) {
          return false;
        }
      } while (len == 0);

      for (int i = 0; i < len; i++) {
        if (line[i] == ';') {
          line[i] = '\0';
          break;
        }
      }

      char* endPtr = nullptr;
      unsigned long size = strtoul(line, &endPtr, 16);
      if (endPtr == line) {
        return false;
      }
      outSize = (int)size;
      return true;
    };

    const int PREFIX_MAX = 12;
    char prefix[PREFIX_MAX];
    int prefixLen = 0;
    bool sawNewline = false;
    while (prefixLen < PREFIX_MAX && (http.connected() || stream->available())) {
      if (stream->available()) {
        char c = (char)stream->read();
        prefix[prefixLen++] = c;
        lastDataTime = millis();
        if (c == '\n') {
          sawNewline = true;
          break;
        }
      } else {
        if (millis() - lastDataTime > streamIdleTimeoutMs) {
          Serial.println("ERROR: Image response stream stalled");
          break;
        }
        delay(1);
      }
    }

    bool useChunked = false;
    int firstChunkSize = 0;
    if (sawNewline) {
      char line[16];
      int lineLen = 0;
      bool hexOnly = true;
      for (int i = 0; i < prefixLen; i++) {
        char c = prefix[i];
        if (c == '\n') {
          break;
        }
        if (c == '\r') {
          continue;
        }
        if (lineLen < (int)sizeof(line) - 1) {
          line[lineLen++] = c;
        }
      }
      line[lineLen] = '\0';
      if (lineLen > 0 && lineLen <= 8) {
        for (int i = 0; i < lineLen; i++) {
          char c = line[i];
          if (!((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F') ||
                c == ';')) {
            hexOnly = false;
            break;
          }
        }
        if (hexOnly) {
          char* endPtr = nullptr;
          unsigned long size = strtoul(line, &endPtr, 16);
          if (endPtr != line) {
            useChunked = true;
            firstChunkSize = (int)size;
          }
        }
      }
    }

    if (useChunked) {
      int chunkSize = firstChunkSize;
      while (chunkSize > 0) {
        if (!ensureCapacity(bytesRead + chunkSize)) {
          break;
        }
        int remaining = chunkSize;
        while (remaining > 0) {
          size_t available = stream->available();
          if (available) {
            int toRead = min((int)available, remaining);
            int read = stream->readBytes(imageBuffer + bytesRead, toRead);
            bytesRead += read;
            remaining -= read;
            lastDataTime = millis();
          } else {
            if (millis() - lastDataTime > streamIdleTimeoutMs) {
              Serial.println("ERROR: Image response stream stalled");
              break;
            }
            delay(1);
          }
        }

        while (true) {
          if (stream->available()) {
            char c = (char)stream->read();
            lastDataTime = millis();
            if (c == '\n') {
              break;
            }
          } else {
            if (millis() - lastDataTime > streamIdleTimeoutMs) {
              Serial.println("ERROR: Image response stream stalled");
              break;
            }
            delay(1);
          }
        }

        if (!parseChunkSize(chunkSize)) {
          break;
        }
      }

      if (chunkSize == 0) {
        char trailer[32];
        while (true) {
          int len = readLine(trailer, sizeof(trailer));
          if (len < 0 || len == 0) {
            break;
          }
        }
      }
    } else {
      if (prefixLen > 0) {
        if (!ensureCapacity(bytesRead + prefixLen)) {
          free(imageBuffer);
          imageBuffer = nullptr;
          http.end();
          return false;
        }
        memcpy(imageBuffer + bytesRead, prefix, prefixLen);
        bytesRead += prefixLen;
      }

      while (http.connected() || stream->available()) {
        size_t available = stream->available();
        if (available) {
          if (!ensureCapacity(bytesRead + available)) {
            break;
          }
          int toRead = min((int)available, bufferCapacity - bytesRead);
          int read = stream->readBytes(imageBuffer + bytesRead, toRead);
          bytesRead += read;
          lastDataTime = millis();
        } else {
          if (millis() - lastDataTime > streamIdleTimeoutMs) {
            Serial.println("ERROR: Image response stream stalled");
            break;
          }
          delay(1);
        }
      }
    }

    http.end();

    if (millis() - lastDataTime > streamIdleTimeoutMs) {
      free(imageBuffer);
      imageBuffer = nullptr;
      return false;
    }

    if (bytesRead == 0) {
      Serial.println("No data received");
      free(imageBuffer);
      imageBuffer = nullptr;
      return false;
    }

    imageBufferSize = bytesRead;
    Serial.printf("Download complete: %d bytes received\n", bytesRead);
    Serial.printf("First 4 bytes: %02X %02X %02X %02X\n",
                  imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
    Serial.printf("Free heap after download: %d bytes\n", ESP.getFreeHeap());
    Serial.println("SUCCESS: Image downloaded successfully");
    Serial.println("=======================================\n");
    return true;
  }

  // Allocate buffer for image
  Serial.printf("Allocating %d bytes for image...\n", contentLength);
  imageBuffer = (uint8_t*)malloc(contentLength);
  if (!imageBuffer) {
    Serial.printf("ERROR: Failed to allocate %d bytes! Free heap: %d\n",
                  contentLength, ESP.getFreeHeap());
    http.end();
    return false;
  }
  Serial.printf("Buffer allocated at: %p\n", imageBuffer);

  // Download image data
  WiFiClient* stream = http.getStreamPtr();
  int bytesRead = 0;
  unsigned long startTime = millis();

  Serial.println("Downloading image data...");
  while (http.connected() && bytesRead < contentLength) {
    size_t available = stream->available();
    if (available) {
      int toRead = min((int)available, contentLength - bytesRead);
      int read = stream->readBytes(imageBuffer + bytesRead, toRead);
      bytesRead += read;

      if (bytesRead % 1024 == 0 || bytesRead == contentLength) {
        Serial.printf("  Downloaded: %d / %d bytes (%.1f%%)\n",
                      bytesRead, contentLength, (bytesRead * 100.0) / contentLength);
      }
    }
    delay(1);

    // Timeout check
    if (millis() - startTime > 30000) {
      Serial.println("ERROR: Download timeout!");
      free(imageBuffer);
      imageBuffer = nullptr;
      http.end();
      return false;
    }
  }

  imageBufferSize = bytesRead;
  http.end();

  Serial.printf("Download complete: %d bytes received\n", bytesRead);
  Serial.printf("First 4 bytes: %02X %02X %02X %02X\n",
                imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
  Serial.printf("Free heap after download: %d bytes\n", ESP.getFreeHeap());

  bool success = (bytesRead == contentLength);
  if (success) {
    Serial.println("SUCCESS: Image downloaded successfully");
  } else {
    Serial.printf("ERROR: Incomplete download (%d != %d)\n", bytesRead, contentLength);
  }

  Serial.println("=======================================\n");
  return success;
}

// Base64 decoding function
bool decodeBase64Image(const String &base64Data) {
  Serial.println("\n========== DECODING BASE64 IMAGE ==========");
  Serial.printf("Base64 data length: %d\n", base64Data.length());

  // Free existing buffer
  if (imageBuffer) {
    Serial.println("Freeing existing image buffer...");
    free(imageBuffer);
    imageBuffer = nullptr;
    imageBufferSize = 0;
  }

  // Estimate decoded size (base64 is ~4/3 of original)
  int estimatedSize = (base64Data.length() * 3) / 4;
  Serial.printf("Estimated decoded size: %d bytes\n", estimatedSize);

  // Allocate buffer
  imageBuffer = (uint8_t*)malloc(estimatedSize);
  if (!imageBuffer) {
    Serial.printf("ERROR: Failed to allocate %d bytes!\n", estimatedSize);
    return false;
  }

  // Simple base64 decoder
  const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int bufferPos = 0;
  uint32_t bits = 0;
  int bitCount = 0;

  for (unsigned int i = 0; i < base64Data.length(); i++) {
    char c = base64Data[i];
    if (c == '=') break; // Padding

    const char* pos = strchr(base64Chars, c);
    if (!pos) continue; // Skip invalid characters

    bits = (bits << 6) | (pos - base64Chars);
    bitCount += 6;

    if (bitCount >= 8) {
      bitCount -= 8;
      imageBuffer[bufferPos++] = (bits >> bitCount) & 0xFF;
    }
  }

  imageBufferSize = bufferPos;
  Serial.printf("Decoded %d bytes\n", imageBufferSize);
  Serial.printf("First 4 bytes: %02X %02X %02X %02X\n",
                imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);
  Serial.println("==========================================\n");

  return imageBufferSize > 0;
}

static int clampInt(int value, int minVal, int maxVal) {
  if (value < minVal) return minVal;
  if (value > maxVal) return maxVal;
  return value;
}

static uint8_t expand5To8(uint8_t value) {
  return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t expand6To8(uint8_t value) {
  return (uint8_t)((value << 2) | (value >> 4));
}

static int computeScaledHeightForWidth(int imgWidth, int imgHeight, float scale) {
  if (imgWidth <= 0 || imgHeight <= 0) {
    return 0;
  }
  return (int)(imgHeight * scale + 0.5f);
}

static void computeScaledLayout(int imgWidth, int imgHeight, ImageZoomMode mode,
                                float &outScale, int &outScaledWidth, int &outScaledHeight,
                                int &outX, int &outY) {
  if (imgWidth <= 0 || imgHeight <= 0) {
    outScale = 1.0f;
    outScaledWidth = imgWidth;
    outScaledHeight = imgHeight;
    outX = 0;
    outY = 0;
    return;
  }
  float scaleX = (float)WIDTH / (float)imgWidth;
  float scaleY = (float)HEIGHT / (float)imgHeight;
  outScale = (mode == CONTAIN) ? min(scaleX, scaleY) : max(scaleX, scaleY);
  outScaledWidth = (int)(imgWidth * outScale + 0.5f);
  outScaledHeight = (int)(imgHeight * outScale + 0.5f);
  outX = (WIDTH - outScaledWidth) / 2;
  outY = (HEIGHT - outScaledHeight) / 2;
}

static void updateWolframScrollBounds(int scaledHeight) {
  wolframMaxScrollOffset = (scaledHeight > HEIGHT) ? (scaledHeight - HEIGHT) : 0;
  wolframScrollOffset = clampInt(wolframScrollOffset, 0, wolframMaxScrollOffset);
}

static bool getPngDimensions(const uint8_t* data, int len, int &outW, int &outH) {
  if (!data || len < 24) {
    return false;
  }
  const uint8_t pngSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (memcmp(data, pngSig, sizeof(pngSig)) != 0) {
    return false;
  }
  if (memcmp(data + 12, "IHDR", 4) != 0) {
    return false;
  }
  uint32_t w = ((uint32_t)data[16] << 24) | ((uint32_t)data[17] << 16) |
               ((uint32_t)data[18] << 8) | (uint32_t)data[19];
  uint32_t h = ((uint32_t)data[20] << 24) | ((uint32_t)data[21] << 16) |
               ((uint32_t)data[22] << 8) | (uint32_t)data[23];
  outW = (int)w;
  outH = (int)h;
  return outW > 0 && outH > 0;
}

static bool getGifDimensions(const uint8_t* data, int len, int &outW, int &outH) {
  if (!data || len < 10) {
    return false;
  }
  if (memcmp(data, "GIF", 3) != 0) {
    return false;
  }
  outW = data[6] | (data[7] << 8);
  outH = data[8] | (data[9] << 8);
  return outW > 0 && outH > 0;
}

static bool getJpegDimensions(const uint8_t* data, int len, int &outW, int &outH) {
  if (!data || len < 9) {
    return false;
  }
  for (int i = 0; i < len - 9; i++) {
    if (data[i] == 0xFF && (data[i + 1] == 0xC0 || data[i + 1] == 0xC2)) {
      outH = (data[i + 5] << 8) | data[i + 6];
      outW = (data[i + 7] << 8) | data[i + 8];
      return outW > 0 && outH > 0;
    }
  }
  return false;
}

// Grayscale JPEGs (1 component) sometimes declare a 2x2 sampling factor (e.g. the
// Wikimedia thumbnail of Thomas_Edison2.jpg). Per JPEG spec (T.81 A.2.2) a
// single-component scan is non-interleaved, so the MCU is always one 8x8 block and
// the factor is meaningless. M5GFX's TJpgDec honours it anyway (16x16 MCU), misreads
// the bitstream and drawJpg() fails. Resetting the factor to 1x1 is lossless.
// Walks the marker segments up to SOS; patches the buffer in place.
static void normalizeGrayscaleJpegSampling(uint8_t* data, int len) {
  if (!data || len < 4 || data[0] != 0xFF || data[1] != 0xD8) {
    return;
  }
  int pos = 2;
  while (pos + 4 <= len) {
    if (data[pos] != 0xFF) {
      return;  // Not at a marker - malformed, leave untouched
    }
    uint8_t marker = data[pos + 1];
    if (marker == 0xFF) {  // Fill byte
      pos++;
      continue;
    }
    if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {  // Standalone markers
      pos += 2;
      continue;
    }
    if (marker == 0xDA || marker == 0xD9) {  // SOS / EOI: no SOF0 before the scan
      return;
    }
    int segLen = (data[pos + 2] << 8) | data[pos + 3];
    if (segLen < 2 || pos + 2 + segLen > len) {
      return;
    }
    if (marker == 0xC0) {  // SOF0 (baseline)
      // Layout: len(2) precision(1) height(2) width(2) ncomp(1) [id, sampling, qt]...
      if (segLen >= 11 && data[pos + 9] == 1 && data[pos + 11] != 0x11) {
        Serial.printf("JPEG: grayscale with sampling 0x%02X, normalizing to 0x11\n", data[pos + 11]);
        data[pos + 11] = 0x11;
      }
      return;
    }
    pos += 2 + segLen;
  }
}

bool handleImageRetrieval(const GPTResponse &gptResponse, const String &originalQuestion = "") {
  // Check if this is a Wolfram Alpha query (math/calculation)
  if (gptResponse.isWolfram) {
    drawScreen("Calculating...");
    Serial.println("Wolfram Alpha query detected");

    // Free audio buffer to make room for image
    if (audioBuffer) {
      free(audioBuffer);
      audioBuffer = nullptr;
      Serial.printf("Freed audio buffer, heap now: %d bytes\n", ESP.getFreeHeap());
    }

    // The last answer's picture is gone either way, and a plot needs the room
    if (imageBuffer) {
      free(imageBuffer);
      imageBuffer = nullptr;
      imageBufferSize = 0;
    }

    const String englishQuery = translateToEnglishForWolfram(originalQuestion);

    // Sharp text and formulas if Wolfram's answer can be read (feature_wolfram.ino); it opens right away
    if (wolframLookUp(englishQuery)) {
      Serial.println("Wolfram Alpha text result ready");
      hasImage = false;
      isWolframImage = false;
      currentImageUrl = "";
      return true;
    }

    // Otherwise Wolfram's picture of the result page
    hasImage = fetchWolframSimpleImage(englishQuery);
    isWolframImage = true;
    wolframScrollOffset = 0;  // Reset scroll position
    currentImageUrl = "";

    if (!hasImage) {
      Serial.println("Failed to fetch Wolfram Alpha image");
      isWolframImage = false;
      return false;  // Return failure so caller can update response text
    } else {
      Serial.println("Wolfram Alpha image fetched successfully");
      return true;
    }
  }
  // Check if user wants to create an image with image generation
  else if (gptResponse.createImage && gptResponse.imageQuery.length() > 0) {
    drawScreenWithIcon("Creating image...", generateIcon, GENERATEICON_WIDTH, GENERATEICON_HEIGHT);
    Serial.println("User requested image creation");
    Serial.println("Prompt: " + gptResponse.imageQuery);

    // Free audio buffer to make room for image
    if (audioBuffer) {
      free(audioBuffer);
      audioBuffer = nullptr;
      Serial.printf("Freed audio buffer, heap now: %d bytes\n", ESP.getFreeHeap());
    }
    if (lastAudioBuffer) {
      free(lastAudioBuffer);
      lastAudioBuffer = nullptr;
      lastAudioSampleCount = 0;
      Serial.printf("Freed last audio buffer, heap now: %d bytes\n", ESP.getFreeHeap());
    }

    // Generate and decode image directly into imageBuffer
    hasImage = generateImageWithFallback(gptResponse.imageQuery);
    currentImageUrl = ""; // No URL for base64 DALL-E data
    isWolframImage = false;  // Not a Wolfram image
    wolframScrollOffset = 0;

    if (hasImage) {
      Serial.println("Image generated successfully");
      Serial.printf("Free heap after image: %d bytes\n", ESP.getFreeHeap());
    } else {
      Serial.println("Image generation failed");
    }
  }
  // Search Wikipedia for image using GPT's suggested query
  else if (gptResponse.imageQuery.length() > 0) {
    drawScreen("Finding related image...");
    Serial.println("Using GPT's image query: " + gptResponse.imageQuery);
    WikiResult wikiResult = searchWikipedia(gptResponse.imageQuery);

    if (wikiResult.found) {
      Serial.println("Wikipedia image found, downloading...");

      // Free audio buffer to make room for image
      if (audioBuffer) {
        free(audioBuffer);
        audioBuffer = nullptr;
        Serial.printf("Freed audio buffer, heap now: %d bytes\n", ESP.getFreeHeap());
      }

      hasImage = downloadImage(wikiResult.imageUrl);
      currentImageUrl = wikiResult.imageUrl;
      isWolframImage = false;  // Not a Wolfram image
      wolframScrollOffset = 0;

      Serial.printf("Free heap after image download: %d bytes\n", ESP.getFreeHeap());
    } else {
      Serial.println("No Wikipedia image found");
      hasImage = false;
      currentImageUrl = "";
      isWolframImage = false;
      wolframScrollOffset = 0;
    }
  } else {
    Serial.println("No image query provided by GPT");
    hasImage = false;
    currentImageUrl = "";
    isWolframImage = false;
    wolframScrollOffset = 0;
  }
  return true;  // Non-Wolfram paths always return true
}

static ImageFetchResult generateImageWithModel(const String &prompt, const String &model, const String &size, bool includeQuality, const String &outputFormat, bool requestUrl) {
  Serial.println("\n========== GENERATING IMAGE ==========");
  Serial.println("Model: " + model);
  Serial.println("Prompt: " + prompt);

  lastActivityTime = millis();  // Prevent sleep during image generation

  // Check WiFi connection before making request
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, attempting reconnection...");
    if (!reconnectWiFiNow()) {
      Serial.println("Failed to reconnect WiFi");
      return IMAGE_FETCH_ERROR;
    }
  }

  AudioEndpoint ep = audioEndpoint();
  if (!ep.available) {
    Serial.println("Image generation needs an OpenAI or Zitro key");
    return IMAGE_FETCH_ERROR;
  }
  String imageUrl = "https://" + ep.host + "/v1/images/generations";

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();

  Serial.println("Connecting to image API at " + ep.host + "...");
  http.begin(client, imageUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + ep.key);
  http.addHeader("Accept-Encoding", "identity");
  http.addHeader("Connection", "close");
  http.setTimeout(90000);

  // Enhance prompt with safety guidelines and aspect ratio instructions
  String safePrompt = prompt + ", family friendly, suitable for children, no text, cartoon style, colorful, educational. if not mentioned differently, place the subject front and center, fully visible.";

  // Escape the prompt for JSON
  String escaped = safePrompt;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", " ");

  String body =
    "{"
      "\"model\":\"" + model + "\","
      "\"prompt\":\"" + escaped + "\","
      "\"n\":1,"
      "\"size\":\"" + size + "\"";

  if (includeQuality) {
    body += ",\"quality\":\"low\"";
  }
  if (outputFormat.length() > 0) {
    body += ",\"output_format\":\"" + outputFormat + "\"";
  }
  if (requestUrl) {
    body += ",\"response_format\":\"url\"";
  }
  body += "}";

  Serial.println("Sending image generation request...");
  Serial.println("Request body: " + body);

  // Show spinner while waiting for response
  int spinnerFrame = 0;
  unsigned long lastSpinnerUpdate = millis();
  drawScreenWithSpinner("Creating image...", spinnerFrame);

  int httpCode = http.POST(body);
  Serial.printf("HTTP response code: %d\n", httpCode);

  // Handle connection errors
  if (httpCode < 0) {
    Serial.printf("Connection error: %d\n", httpCode);
    Serial.println("Attempting WiFi reconnection...");
    http.end();

    if (reconnectWiFiNow()) {
      Serial.println("Retrying image generation request...");
      http.begin(client, imageUrl);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", "Bearer " + ep.key);
      http.setTimeout(90000);
      httpCode = http.POST(body);
      Serial.printf("Retry HTTP response code: %d\n", httpCode);
    }

    if (httpCode < 0 || httpCode != 200) {
      http.end();
      return IMAGE_FETCH_ERROR;
    }
  }

  if (httpCode != 200) {
    String error = http.getString();
    Serial.println("Error response: " + error);
    http.end();
    return IMAGE_FETCH_ERROR;
  }

  // Update spinner - receiving response
  spinnerFrame++;
  drawScreenWithSpinner("Creating image...", spinnerFrame);

  // Get response stream and read base64 data directly
  WiFiClient* stream = http.getStreamPtr();
  int contentLength = http.getSize();
  Serial.printf("Image generation response Content-Length: %d bytes\n", contentLength);
  String contentEncoding = http.header("Content-Encoding");
  String contentType = http.header("Content-Type");
  Serial.printf("Image generation Content-Encoding: %s\n", contentEncoding.c_str());
  Serial.printf("Image generation Content-Type: %s\n", contentType.c_str());
  if (contentEncoding.length() > 0 && contentEncoding != "identity") {
    Serial.println("ERROR: Compressed response not supported for streaming parse");
    http.end();
    return IMAGE_FETCH_ERROR;
  }

  Serial.println("Reading image generation response stream...");
  Serial.printf("Free heap before parsing: %d bytes\n", ESP.getFreeHeap());

  String searchBuffer = "";
  String urlData = "";

  enum ParseState { FIND_KEY, FIND_COLON, FIND_QUOTE, READ_B64, READ_URL };
  ParseState state = FIND_KEY;
  bool readingB64 = false;
  bool foundB64Start = false;
  bool foundB64End = false;
  bool foundUrlStart = false;
  bool foundUrlEnd = false;
  lastSpinnerUpdate = millis();  // Reset timer for stream reading phase
  unsigned long lastDataTime = millis();
  int bytesRead = 0;
  int lastSpinnerBytes = 0;
  const unsigned long streamIdleTimeoutMs = 15000;

  const int markerBufferMax = 40;
  const char* b64Key = "\"b64_json\"";
  const char* imageDataKey = "\"image_data\"";
  const char* imageBase64Key = "\"image_base64\"";
  const char* urlKey = "\"url\"";
  const int b64KeyLen = (int)strlen(b64Key);
  const int imageDataKeyLen = (int)strlen(imageDataKey);
  const int imageBase64KeyLen = (int)strlen(imageBase64Key);
  const int urlKeyLen = (int)strlen(urlKey);
  int b64KeyPos = 0;
  int imageDataKeyPos = 0;
  int imageBase64KeyPos = 0;
  int urlKeyPos = 0;

  const int MAX_IMAGE_BYTES = 350000;  // Increased to allow full 1024x1024 JPEG for downsampling
  const int DOWNSAMPLE_THRESHOLD = 200000;  // Trigger downsampling above this size
  const int DECODE_CHUNK = 8192;
  int decodeCapacity = 0;
  int decodePos = 0;
  int quadIndex = 0;
  char quad[4];
  bool decodeFailed = false;
  bool decodeTooLarge = false;
  bool needsDownsampling = false;
  bool seenPadding = false;

  auto ensureDecodeCapacity = [&](int needed) -> bool {
    if (needed <= decodeCapacity) {
      return true;
    }
    if (needed > MAX_IMAGE_BYTES) {
      decodeTooLarge = true;
      return false;
    }
    // Mark for downsampling if exceeding threshold
    if (needed > DOWNSAMPLE_THRESHOLD) {
      needsDownsampling = true;
    }
    int newCapacity = decodeCapacity == 0 ? DECODE_CHUNK : decodeCapacity;
    while (newCapacity < needed) {
      newCapacity += DECODE_CHUNK;
    }
    if (newCapacity > MAX_IMAGE_BYTES) {
      newCapacity = MAX_IMAGE_BYTES;
    }
    if (newCapacity < needed) {
      decodeTooLarge = true;
      return false;
    }
    // Use PSRAM for large images that will be downsampled
    uint8_t* newBuffer;
    if (needsDownsampling && imageBuffer == nullptr) {
      // First allocation - use PSRAM for large images
      newBuffer = (uint8_t*)ps_malloc(newCapacity);
    } else if (needsDownsampling && decodeCapacity > 0) {
      // Reallocation - need to move from heap to PSRAM or grow PSRAM
      newBuffer = (uint8_t*)ps_realloc(imageBuffer, newCapacity);
    } else {
      // Small images stay in regular heap
      newBuffer = (uint8_t*)realloc(imageBuffer, newCapacity);
    }
    if (!newBuffer) {
      return false;
    }
    imageBuffer = newBuffer;
    decodeCapacity = newCapacity;
    return true;
  };

  auto b64Value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;
    return -1;
  };

  auto appendDecodedByte = [&](uint8_t b) -> bool {
    if (!ensureDecodeCapacity(decodePos + 1)) {
      return false;
    }
    imageBuffer[decodePos++] = b;
    return true;
  };

  auto decodeQuartet = [&]() -> bool {
    int v[4];
    for (int i = 0; i < 4; i++) {
      v[i] = b64Value(quad[i]);
      if (v[i] == -1) {
        return false;
      }
    }
    if (v[0] < 0 || v[1] < 0) {
      return false;
    }

    if (!appendDecodedByte((uint8_t)((v[0] << 2) | (v[1] >> 4)))) {
      return false;
    }

    if (v[2] == -2) {
      seenPadding = true;
      return true;
    }
    if (!appendDecodedByte((uint8_t)(((v[1] & 0x0F) << 4) | (v[2] >> 2)))) {
      return false;
    }

    if (v[3] == -2) {
      seenPadding = true;
      return true;
    }
    if (!appendDecodedByte((uint8_t)(((v[2] & 0x03) << 6) | v[3]))) {
      return false;
    }
    return true;
  };

  while (http.connected() || stream->available()) {
    if (stream->available()) {
      char c = stream->read();
      bytesRead++;
      lastDataTime = millis();
      lastActivityTime = lastDataTime;  // Prevent sleep during image generation

      // Update spinner only when data is arriving
      if (bytesRead != lastSpinnerBytes && millis() - lastSpinnerUpdate >= 200) {
        spinnerFrame++;
        drawScreenWithSpinner("Creating image...", spinnerFrame);
        lastSpinnerUpdate = millis();
        lastSpinnerBytes = bytesRead;
      }

      // Keep a short tail for debugging when parsing fails.
      searchBuffer += c;
      if (searchBuffer.length() > markerBufferMax) {
        searchBuffer = searchBuffer.substring(1);
      }

      if (state == FIND_KEY) {
        if (c == b64Key[b64KeyPos]) {
          b64KeyPos++;
        } else {
          b64KeyPos = (c == b64Key[0]) ? 1 : 0;
        }
        if (c == imageDataKey[imageDataKeyPos]) {
          imageDataKeyPos++;
        } else {
          imageDataKeyPos = (c == imageDataKey[0]) ? 1 : 0;
        }
        if (c == imageBase64Key[imageBase64KeyPos]) {
          imageBase64KeyPos++;
        } else {
          imageBase64KeyPos = (c == imageBase64Key[0]) ? 1 : 0;
        }
        if (c == urlKey[urlKeyPos]) {
          urlKeyPos++;
        } else {
          urlKeyPos = (c == urlKey[0]) ? 1 : 0;
        }

        if (b64KeyPos == b64KeyLen || imageDataKeyPos == imageDataKeyLen ||
            imageBase64KeyPos == imageBase64KeyLen) {
          readingB64 = true;
          foundB64Start = true;
          state = FIND_COLON;
          b64KeyPos = 0;
          imageDataKeyPos = 0;
          imageBase64KeyPos = 0;
          urlKeyPos = 0;
        } else if (urlKeyPos == urlKeyLen) {
          readingB64 = false;
          foundUrlStart = true;
          state = FIND_COLON;
          b64KeyPos = 0;
          imageDataKeyPos = 0;
          imageBase64KeyPos = 0;
          urlKeyPos = 0;
        }
      } else if (state == FIND_COLON) {
        if (c == ':') {
          state = FIND_QUOTE;
        }
      } else if (state == FIND_QUOTE) {
        if (c == '"') {
          state = readingB64 ? READ_B64 : READ_URL;
          if (readingB64) {
            if (imageBuffer) {
              free(imageBuffer);
              imageBuffer = nullptr;
              imageBufferSize = 0;
            }
            decodeCapacity = 0;
            decodePos = 0;
            quadIndex = 0;
            decodeFailed = false;
            seenPadding = false;
          }
        }
      } else if (state == READ_B64) {
        if (c == '"') {
          foundB64End = true;
          break;
        }
        if (seenPadding) {
          continue;
        }
        int val = b64Value(c);
        if (val < 0 && val != -2) {
          continue;
        }
        quad[quadIndex++] = c;
        if (quadIndex == 4) {
          if (!decodeQuartet()) {
            decodeFailed = true;
            break;
          }
          quadIndex = 0;
        }
      } else if (state == READ_URL) {
        if (c == '"') {
          foundUrlEnd = true;
          break;
        }
        urlData += c;
      }
    } else {
      if (millis() - lastDataTime > streamIdleTimeoutMs) {
        Serial.println("ERROR: Image generation response stream stalled");
        http.end();
        return IMAGE_FETCH_ERROR;
      }
      delay(1);
    }
  }

  http.end();
  Serial.printf("Total bytes read from stream: %d\n", bytesRead);

  if (decodeTooLarge) {
    Serial.println("ERROR: Image too large for available memory");
    if (imageBuffer) {
      free(imageBuffer);
      imageBuffer = nullptr;
      imageBufferSize = 0;
    }
    return IMAGE_FETCH_TOO_LARGE;
  }

  // Check if image needs downsampling (>200KB but <=350KB)
  if (needsDownsampling) {
    Serial.printf("Image size (%d bytes) exceeds threshold, will attempt downsampling\n", decodePos);
    imageBufferSize = decodePos;
    return IMAGE_FETCH_TOO_LARGE;  // Signal to try downsampling
  }

  if (!foundB64Start && !foundUrlStart) {
    Serial.println("ERROR: No 'b64_json' or 'url' field found in response!");
    Serial.printf("Read %d total bytes, search buffer ends with: '%s'\n", bytesRead, searchBuffer.substring(max(0, (int)searchBuffer.length() - 20)).c_str());
    return IMAGE_FETCH_ERROR;
  }

  if (foundB64Start) {
    if (!foundB64End) {
      Serial.println("ERROR: Base64 data incomplete!");
      if (imageBuffer) {
        free(imageBuffer);
        imageBuffer = nullptr;
        imageBufferSize = 0;
      }
      return IMAGE_FETCH_ERROR;
    }
    if (decodeFailed) {
      Serial.println("ERROR: Failed to decode base64 data (invalid input or out of memory)");
      if (imageBuffer) {
        free(imageBuffer);
        imageBuffer = nullptr;
        imageBufferSize = 0;
      }
      return IMAGE_FETCH_ERROR;
    }
    if (decodePos == 0) {
      Serial.println("ERROR: Decoded image data is empty!");
      if (imageBuffer) {
        free(imageBuffer);
        imageBuffer = nullptr;
        imageBufferSize = 0;
      }
      return IMAGE_FETCH_ERROR;
    }
    imageBufferSize = decodePos;
    Serial.printf("Decoded image size: %d bytes\n", imageBufferSize);
    Serial.printf("First 4 bytes: %02X %02X %02X %02X\n",
                  imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3]);

    Serial.println("Image generated and decoded successfully");
    Serial.println("==================================================\n");
    return IMAGE_FETCH_OK;
  }

  if (!foundUrlEnd) {
    Serial.println("ERROR: Image URL incomplete!");
    return IMAGE_FETCH_ERROR;
  }

  if (urlData.length() == 0) {
    Serial.println("ERROR: Image URL is empty!");
    return IMAGE_FETCH_ERROR;
  }

  urlData.replace("\\/", "/");
  urlData.replace("\\u0026", "&");
  urlData.replace("\\u003d", "=");
  urlData.replace("\\u003f", "?");
  Serial.println("Image URL: " + urlData);

  bool downloadSuccess = downloadImage(urlData);
  if (!downloadSuccess) {
    Serial.println("ERROR: Failed to download image from URL");
    return IMAGE_FETCH_ERROR;
  }

  Serial.println("Image generated and decoded successfully");
  Serial.println("==================================================\n");
  return IMAGE_FETCH_OK;
}

// Downsample RGB565 image by factor of 4 (e.g., 1024x1024 -> 256x256)
static uint16_t* downsampleRGB565(uint16_t* input, int inputWidth, int inputHeight,
                                   int* outputWidth, int* outputHeight) {
  *outputWidth = inputWidth / 4;
  *outputHeight = inputHeight / 4;
  size_t outputSize = (*outputWidth) * (*outputHeight) * sizeof(uint16_t);

  Serial.printf("Downsampling from %dx%d to %dx%d\n", inputWidth, inputHeight, *outputWidth, *outputHeight);

  uint16_t* output = (uint16_t*)ps_malloc(outputSize);
  if (!output) {
    Serial.println("Failed to allocate PSRAM for downsampled buffer");
    return nullptr;
  }

  // Sample every 4th pixel in both dimensions
  for (int y = 0; y < *outputHeight; y++) {
    for (int x = 0; x < *outputWidth; x++) {
      output[y * (*outputWidth) + x] = input[(y*4) * inputWidth + (x*4)];
    }
  }

  return output;
}

// Complete downsampling pipeline: JPEG → decode → downsample → re-encode
static ImageFetchResult downsampleLargeImage() {
  Serial.println("\n========== DOWNSAMPLING LARGE IMAGE ==========");
  Serial.printf("PSRAM total: %d bytes, free: %d bytes\n", ESP.getPsramSize(), ESP.getFreePsram());
  Serial.printf("Heap free: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("Input JPEG size: %d bytes\n", imageBufferSize);

  // imageBuffer already contains the large JPEG
  if (!imageBuffer || imageBufferSize == 0) {
    Serial.println("ERROR: No image buffer to downsample");
    return IMAGE_FETCH_ERROR;
  }

  // Step 1: Decode JPEG at reduced size directly (256x256)
  // This avoids needing a 2MB buffer for full 1024x1024 decode
  Serial.println("Step 1: Decoding and scaling JPEG to 256x256...");

  // Create smaller canvas for scaled decode - only 128KB (256*256*2)
  M5Canvas tempCanvas(&M5.Display);
  tempCanvas.setColorDepth(16); // RGB565

  Serial.printf("PSRAM free before createSprite: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Heap free before createSprite: %d bytes\n", ESP.getFreeHeap());

  // Try to create 256x256 sprite (128KB) - much smaller than 1024x1024 (2MB)
  if (!tempCanvas.createSprite(256, 256)) {
    Serial.println("Failed to create 256x256 canvas sprite");
    Serial.printf("Heap free after failed createSprite: %d bytes\n", ESP.getFreeHeap());
    return IMAGE_FETCH_ERROR;
  }

  Serial.printf("Heap free after createSprite: %d bytes\n", ESP.getFreeHeap());

  // Diagnostic: Check JPEG header for color space markers
  if (imageBufferSize >= 20) {
    Serial.printf("JPEG Header bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                  imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3],
                  imageBuffer[4], imageBuffer[5], imageBuffer[6], imageBuffer[7]);

    // Scan for APP14 (Adobe) marker: FF EE
    // Scan for ICC profile marker: FF E2
    for (int i = 0; i < min((int)imageBufferSize - 4, 200); i++) {
      if (imageBuffer[i] == 0xFF && imageBuffer[i+1] == 0xEE) {
        Serial.printf("Found APP14 (Adobe) marker at offset %d\n", i);
      }
      if (imageBuffer[i] == 0xFF && imageBuffer[i+1] == 0xE2) {
        Serial.printf("Found ICC profile marker (APP2) at offset %d\n", i);
      }
    }
  }

  // Decode JPEG with scaling - drawJpg can scale during decode
  // Draw into 256x256 canvas, M5GFX will scale the 1024x1024 JPEG
  tempCanvas.drawJpg(imageBuffer, imageBufferSize, 0, 0, 256, 256);

  // Allocate PSRAM buffer for downsampled pixels
  int outW = 256;
  int outH = 256;
  int pixelCount = outW * outH;
  uint16_t* downsampled = (uint16_t*)ps_malloc(pixelCount * sizeof(uint16_t));

  if (!downsampled) {
    Serial.println("Failed to allocate PSRAM for downsampled buffer");
    tempCanvas.deleteSprite();
    return IMAGE_FETCH_ERROR;
  }

  // Copy scaled pixels from canvas
  memcpy(downsampled, tempCanvas.getBuffer(), pixelCount * sizeof(uint16_t));

  bool swapRB = false;
  if (pixelCount > 0) {
    RGBColor probe;
    int sampleX = outW / 2;
    int sampleY = outH / 2;
    tempCanvas.readRectRGB(sampleX, sampleY, 1, 1, &probe);

    uint16_t raw = downsampled[sampleY * outW + sampleX];
    uint8_t rawR = expand5To8((raw >> 11) & 0x1F);
    uint8_t rawG = expand6To8((raw >> 5) & 0x3F);
    uint8_t rawB = expand5To8(raw & 0x1F);

    int diffNormal = abs((int)rawR - (int)probe.r) +
                     abs((int)rawG - (int)probe.g) +
                     abs((int)rawB - (int)probe.b);
    int diffSwapped = abs((int)rawB - (int)probe.r) +
                      abs((int)rawG - (int)probe.g) +
                      abs((int)rawR - (int)probe.b);
    swapRB = diffSwapped < diffNormal;
    Serial.printf("RGB565 order check: normal=%d swapped=%d => %s\n",
                  diffNormal, diffSwapped, swapRB ? "swap R/B" : "normal");
  }

  tempCanvas.deleteSprite();

  Serial.printf("JPEG decoded and scaled to %dx%d\n", outW, outH);
  Serial.println("Step 2: Re-encoding to JPEG...");

#ifdef ENABLE_JPEG_CONVERSION
  uint8_t* rgb888 = (uint8_t*)ps_malloc(pixelCount * 3);
  if (!rgb888) {
    Serial.println("Failed to allocate buffer for RGB888 conversion");
    free(downsampled);
    return IMAGE_FETCH_ERROR;
  }

  for (int i = 0; i < pixelCount; i++) {
    uint16_t pixel = downsampled[i];
    uint8_t r = expand5To8((pixel >> 11) & 0x1F);
    uint8_t g = expand6To8((pixel >> 5) & 0x3F);
    uint8_t b = expand5To8(pixel & 0x1F);
    if (swapRB) {
      uint8_t tmp = r;
      r = b;
      b = tmp;
    }
    rgb888[i * 3 + 0] = r;
    rgb888[i * 3 + 1] = g;
    rgb888[i * 3 + 2] = b;
  }

  free(downsampled);

  int maxJpegSize = pixelCount * 3 / 2 + 4096; // Conservative estimate + header space
  uint8_t* jpegBuffer = (uint8_t*)malloc(maxJpegSize);
  if (!jpegBuffer) {
    Serial.println("Failed to allocate JPEG output buffer");
    free(rgb888);
    return IMAGE_FETCH_ERROR;
  }

  Serial.printf("Allocated %d bytes for JPEG output buffer\n", maxJpegSize);

  JPEGENC encoder;
  int rc = encoder.open(jpegBuffer, maxJpegSize);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to initialize JPEG encoder: %d\n", rc);
    free(rgb888);
    free(jpegBuffer);
    return IMAGE_FETCH_ERROR;
  }

  Serial.println("JPEG encoder opened successfully");

  JPEGENCODE jpe;
  rc = encoder.encodeBegin(&jpe, outW, outH, JPEGE_PIXEL_RGB888, JPEGE_SUBSAMPLE_420, JPEGE_Q_HIGH);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to begin JPEG encoding: %d\n", rc);
    encoder.close();
    free(rgb888);
    free(jpegBuffer);
    return IMAGE_FETCH_ERROR;
  }

  Serial.printf("encodeBegin successful, encoding %dx%d image\n", outW, outH);
  Serial.printf("Input buffer: %p, stride: %d bytes\n", rgb888, outW * 3);

  rc = encoder.addFrame(&jpe, rgb888, outW * 3);
  if (rc != JPEGE_SUCCESS) {
    Serial.printf("Failed to encode JPEG frame: %d\n", rc);
    Serial.printf("Buffer size: %d, Pixel count: %d\n", maxJpegSize, pixelCount);
    encoder.close();
    free(rgb888);
    free(jpegBuffer);
    return IMAGE_FETCH_ERROR;
  }

  Serial.println("addFrame successful");

  int jpegSize = encoder.close();
  free(rgb888);

  if (jpegSize <= 0) {
    Serial.println("JPEG encoding produced no output");
    free(jpegBuffer);
    return IMAGE_FETCH_ERROR;
  }

  // Step 3: Replace imageBuffer with downsampled JPEG
  free(imageBuffer);
  imageBuffer = jpegBuffer;
  imageBufferSize = jpegSize;

  Serial.printf("Re-encoding complete. Final JPEG size: %d bytes\n", imageBufferSize);
  Serial.printf("PSRAM free after downsample: %d bytes\n", ESP.getFreePsram());
  Serial.printf("Heap free after downsample: %d bytes\n", ESP.getFreeHeap());
  Serial.println("==============================================\n");

  return IMAGE_FETCH_OK;

#else
  Serial.println("ERROR: JPEGENC library not enabled (ENABLE_JPEG_CONVERSION not defined)");
  free(downsampled);
  return IMAGE_FETCH_ERROR;
#endif
}

bool generateImageWithFallback(const String &prompt) {
  ImageFetchResult result = generateImageWithModel(prompt, IMAGE_MODEL, "1024x1024", true, "jpeg", false);

  if (result == IMAGE_FETCH_ERROR) {
    // Older model in case the account has no access to the current one
    Serial.println("Retrying with " IMAGE_MODEL_FALLBACK "...");
    result = generateImageWithModel(prompt, IMAGE_MODEL_FALLBACK, "1024x1024", true, "jpeg", false);
  }

  if (result == IMAGE_FETCH_TOO_LARGE) {
    // dall-e-2 (the former small-image fallback) was removed from the API in May 2026
    result = downsampleLargeImage();
    if (result == IMAGE_FETCH_OK) {
      Serial.println("Downsampling successful! Using downsampled 256x256 image.");
    }
  }

  return result == IMAGE_FETCH_OK;
}

static uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static bool drawGifToCanvas(M5Canvas& canvas, const uint8_t* data, int len, int xOffset, int yOffset, float scale) {
  if (!data || len < 13) {
    return false;
  }

  if (memcmp(data, "GIF", 3) != 0) {
    return false;
  }

  int pos = 6;
  uint16_t screenWidth = data[pos] | (data[pos + 1] << 8);
  pos += 2;
  uint16_t screenHeight = data[pos] | (data[pos + 1] << 8);
  pos += 2;
  uint8_t packed = data[pos++];
  bool gctFlag = packed & 0x80;
  int gctSize = 1 << ((packed & 0x07) + 1);
  uint8_t bgIndex = data[pos++];
  (void)bgIndex;
  pos++;  // Pixel aspect ratio

  uint16_t globalPalette[256];
  int globalPaletteSize = 0;
  if (gctFlag) {
    globalPaletteSize = gctSize;
    if (pos + gctSize * 3 > len) {
      return false;
    }
    for (int i = 0; i < gctSize; i++) {
      uint8_t r = data[pos++];
      uint8_t g = data[pos++];
      uint8_t b = data[pos++];
      globalPalette[i] = rgbTo565(r, g, b);
    }
  }

  int transparentIndex = -1;

  while (pos < len) {
    uint8_t sep = data[pos++];
    if (sep == 0x2C) {
      if (pos + 9 > len) {
        return false;
      }
      uint16_t imgLeft = data[pos] | (data[pos + 1] << 8);
      pos += 2;
      uint16_t imgTop = data[pos] | (data[pos + 1] << 8);
      pos += 2;
      uint16_t imgWidth = data[pos] | (data[pos + 1] << 8);
      pos += 2;
      uint16_t imgHeight = data[pos] | (data[pos + 1] << 8);
      pos += 2;
      uint8_t imgPacked = data[pos++];
      bool lctFlag = imgPacked & 0x80;
      bool interlaced = imgPacked & 0x40;
      int lctSize = 1 << ((imgPacked & 0x07) + 1);

      uint16_t palette[256];
      int paletteSize = 0;
      if (lctFlag) {
        paletteSize = lctSize;
        if (pos + lctSize * 3 > len) {
          return false;
        }
        for (int i = 0; i < lctSize; i++) {
          uint8_t r = data[pos++];
          uint8_t g = data[pos++];
          uint8_t b = data[pos++];
          palette[i] = rgbTo565(r, g, b);
        }
      } else if (globalPaletteSize > 0) {
        paletteSize = globalPaletteSize;
        for (int i = 0; i < globalPaletteSize; i++) {
          palette[i] = globalPalette[i];
        }
      } else {
        return false;
      }

      if (pos >= len) {
        return false;
      }
      uint8_t lzwMinCodeSize = data[pos++];

      uint8_t* pixels = nullptr;
      if (!decodeGifPixels(data, len, pos, lzwMinCodeSize, imgWidth, imgHeight, interlaced, &pixels)) {
        return false;
      }

      if (scale <= 0.0f) {
        scale = 1.0f;
      }

      int scaledWidth = (int)(imgWidth * scale + 0.5f);
      int scaledHeight = (int)(imgHeight * scale + 0.5f);
      int scaledLeft = (int)(imgLeft * scale + 0.5f) + xOffset;
      int scaledTop = (int)(imgTop * scale + 0.5f) + yOffset;

      if (scaledWidth <= 0 || scaledHeight <= 0) {
        free(pixels);
        return false;
      }

      uint16_t* lineBuf = (uint16_t*)malloc(scaledWidth * sizeof(uint16_t));
      uint8_t* lineOpaque = (uint8_t*)malloc(scaledWidth);
      if (!lineBuf || !lineOpaque) {
        free(lineBuf);
        free(lineOpaque);
        free(pixels);
        return false;
      }

      for (int y = 0; y < scaledHeight; y++) {
        int srcY = (int)(y / scale);
        if (srcY >= imgHeight) {
          srcY = imgHeight - 1;
        }
        int targetY = scaledTop + y;
        if (targetY < 0 || targetY >= canvas.height()) {
          continue;
        }

        for (int x = 0; x < scaledWidth; x++) {
          int srcX = (int)(x / scale);
          if (srcX >= imgWidth) {
            srcX = imgWidth - 1;
          }
          uint8_t idx = pixels[srcY * imgWidth + srcX];
          lineOpaque[x] = !(transparentIndex >= 0 && idx == transparentIndex);
          if (idx < paletteSize) {
            lineBuf[x] = palette[idx];
          } else {
            lineBuf[x] = TFT_BLACK;
          }
        }

        int drawX = scaledLeft;
        int drawWidth = scaledWidth;
        int srcOffsetX = 0;
        if (drawX < 0) {
          srcOffsetX = -drawX;
          drawX = 0;
          drawWidth -= srcOffsetX;
        }
        if (drawX + drawWidth > canvas.width()) {
          drawWidth = canvas.width() - drawX;
        }
        if (drawWidth <= 0 || srcOffsetX >= scaledWidth) {
          continue;
        }

        // Transparent pixels keep what is underneath: black on screen, white on paper
        canvas.setSwapBytes(true);  // lineBuf holds plain RGB565 (rgbTo565)
        for (int run = 0; run < drawWidth;) {
          if (!lineOpaque[srcOffsetX + run]) {
            run++;
            continue;
          }
          int runEnd = run;
          while (runEnd < drawWidth && lineOpaque[srcOffsetX + runEnd]) {
            runEnd++;
          }
          canvas.pushImage(drawX + run, targetY, runEnd - run, 1, lineBuf + srcOffsetX + run);
          run = runEnd;
        }
        canvas.setSwapBytes(false);
      }

      free(lineBuf);
      free(lineOpaque);
      free(pixels);
      return true;
    }

    if (sep == 0x21) {
      if (pos >= len) {
        return false;
      }
      uint8_t label = data[pos++];
      if (label == 0xF9) {
        if (pos + 5 > len) {
          return false;
        }
        uint8_t blockSize = data[pos++];
        if (blockSize < 4 || pos + blockSize > len) {
          return false;
        }
        uint8_t gcePacked = data[pos++];
        pos += 2;  // Delay time
        uint8_t transIndex = data[pos++];
        if (blockSize > 4) {
          pos += (blockSize - 4);
        }
        if (pos >= len) {
          return false;
        }
        pos++;  // Block terminator
        if (gcePacked & 0x01) {
          transparentIndex = transIndex;
        } else {
          transparentIndex = -1;
        }
      } else {
        // Skip extension sub-blocks
        while (pos < len) {
          uint8_t blockSize = data[pos++];
          if (blockSize == 0) {
            break;
          }
          pos += blockSize;
          if (pos > len) {
            return false;
          }
        }
      }
    } else if (sep == 0x3B) {
      break;
    } else {
      return false;
    }
  }

  (void)screenWidth;
  (void)screenHeight;
  return false;
}

void displayImage(bool isGeneratedImage) {
  M5.Display.setRotation(getLandscapeRotation());
  Serial.println("\n========== DISPLAYING IMAGE ==========");
  Serial.printf("Display mode: %s\n", isGeneratedImage ? "AI-generated (swap colors)" : "Normal");

  if (!imageBuffer) {
    Serial.println("ERROR: imageBuffer is NULL");
    drawScreen("No image\nbuffer");
    return;
  }

  if (imageBufferSize == 0) {
    Serial.println("ERROR: imageBufferSize is 0");
    drawScreen("Image size\nis zero");
    return;
  }

  Serial.printf("Image buffer: %p, size: %d bytes\n", imageBuffer, imageBufferSize);
  Serial.printf("Free heap before canvas: %d bytes\n", ESP.getFreeHeap());

  // Detect image format from magic bytes
  bool isPNG = false;
  bool isJPEG = false;
  bool isGIF = false;

  // Always print magic bytes for debugging
  if (imageBufferSize >= 6) {
    Serial.printf("Magic bytes: %02X %02X %02X %02X %02X %02X (first 6 bytes)\n",
                  imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3],
                  imageBuffer[4], imageBuffer[5]);
  }

  if (imageBufferSize >= 3) {
    // GIF: Check this FIRST - GIF89a or GIF87a (47 49 46 = "GIF")
    if (imageBuffer[0] == 0x47 && imageBuffer[1] == 0x49 && imageBuffer[2] == 0x46) {
      isGIF = true;
      Serial.println("Detected image format: GIF (by 'GIF' header)");
    }
    // PNG: 89 50 4E 47 (.PNG)
    else if (imageBuffer[0] == 0x89 && imageBuffer[1] == 0x50 &&
             imageBuffer[2] == 0x4E && imageBuffer[3] == 0x47) {
      isPNG = true;
      Serial.println("Detected image format: PNG");
    }
    // JPEG: FF D8 (JPEG SOI marker) - removed the third byte check
    else if (imageBuffer[0] == 0xFF && imageBuffer[1] == 0xD8) {
      isJPEG = true;
      Serial.println("Detected image format: JPEG");
    } else {
      Serial.printf("WARNING: Unknown image format (magic: %02X %02X %02X %02X %02X %02X)\n",
                    imageBuffer[0], imageBuffer[1], imageBuffer[2], imageBuffer[3],
                    imageBuffer[4], imageBuffer[5]);
    }
  }

  int imgWidth = 0;
  int imgHeight = 0;
  bool dimsFound = false;
  if (isPNG) {
    dimsFound = getPngDimensions(imageBuffer, imageBufferSize, imgWidth, imgHeight);
  } else if (isGIF) {
    dimsFound = getGifDimensions(imageBuffer, imageBufferSize, imgWidth, imgHeight);
  } else if (isJPEG) {
    normalizeGrayscaleJpegSampling(imageBuffer, imageBufferSize);
    dimsFound = getJpegDimensions(imageBuffer, imageBufferSize, imgWidth, imgHeight);
  }

  float wolframScale = 1.0f;
  int wolframScaledHeight = 0;

  if (dimsFound) {
    if (imageZoomMode == CONTAIN) {
      // CONTAIN mode: fit entire image within screen (may have letterboxing)
      float scaleX = (float)WIDTH / imgWidth;
      float scaleY = (float)HEIGHT / imgHeight;
      wolframScale = min(scaleX, scaleY);  // Use smaller scale to fit fully
      Serial.printf("CONTAIN mode: scaleX=%.3f, scaleY=%.3f, using=%.3f\n", scaleX, scaleY, wolframScale);
    } else {
      // COVER mode: fill screen (may crop image)
      float scaleX = (float)WIDTH / imgWidth;
      float scaleY = (float)HEIGHT / imgHeight;
      wolframScale = max(scaleX, scaleY);  // Use larger scale to fill screen
      Serial.printf("COVER mode: scaleX=%.3f, scaleY=%.3f, using=%.3f\n", scaleX, scaleY, wolframScale);
    }

    wolframScaledHeight = computeScaledHeightForWidth(imgWidth, imgHeight, wolframScale);
    updateWolframScrollBounds(wolframScaledHeight);
  } else {
    updateWolframScrollBounds(0);
  }

  M5Canvas canvas(&M5.Display);
  bool canvasCreated = canvas.createSprite(WIDTH, HEIGHT);

  if (!canvasCreated) {
    Serial.println("ERROR: Failed to create canvas sprite");
    drawScreen("Canvas\nerror");
    return;
  }

  // Apply byte swap for AI-generated images with BGR color encoding
  if (isGeneratedImage) {
    M5.Display.setSwapBytes(true);
    Serial.println("Display: setSwapBytes(true) - compensating for BGR encoding");
  } else {
    M5.Display.setSwapBytes(false);
    Serial.println("Display: setSwapBytes(false) - normal RGB encoding");
  }

  Serial.printf("Canvas created, free heap: %d bytes\n", ESP.getFreeHeap());
  canvas.fillSprite(TFT_BLACK);

  // Handle PNG images - M5GFX can decode PNG directly
  if (isPNG) {
    Serial.println("Drawing PNG image...");

    // Apply scroll offset
    int yOffset = -wolframScrollOffset;
    Serial.printf("Drawing PNG with Y offset: %d\n", yOffset);

    bool success = false;
    if (dimsFound && wolframScaledHeight > 0) {
      canvas.setClipRect(0, 0, WIDTH, HEIGHT);
      // Draw with scaling only (no maxWidth/maxHeight to avoid cropping)
      success = canvas.drawPng(imageBuffer, imageBufferSize,
                               0, yOffset,
                               0, 0,  // No max width/height - use scale instead
                               0, 0,  // No source offset
                               wolframScale, wolframScale);
      canvas.clearClipRect();
    } else {
      success = canvas.drawPng(imageBuffer, imageBufferSize, 0, yOffset);
    }

    if (!success) {
      Serial.println("ERROR: drawPng() returned false");
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      canvas.drawString("PNG decode", WIDTH / 2, HEIGHT / 2 - 10);
      canvas.drawString("failed", WIDTH / 2, HEIGHT / 2 + 10);
    } else {
      Serial.println("SUCCESS: PNG drawn successfully");
    }

    // Draw battery indicator
    drawBatteryIndicator(canvas);
    captureCanvas(canvas);
    canvas.pushSprite(0, 0);
    canvas.deleteSprite();

    Serial.printf("Free heap after display: %d bytes\n", ESP.getFreeHeap());
    Serial.println("======================================\n");
    return;
  }

  // Handle GIF images - GIF support in M5GFX
  if (isGIF) {
    Serial.println("Drawing GIF image...");

    // Apply scroll offset
    int yOffset = -wolframScrollOffset;
    Serial.printf("Drawing GIF with Y offset: %d\n", yOffset);

    canvas.fillSprite(TFT_BLACK);

    int gifX = 0;
    int gifY = yOffset;
    float gifScale = 1.0f;
    if (dimsFound && wolframScaledHeight > 0) {
      gifScale = wolframScale;
    }

    // Already playing this GIF: keep its current frame instead of jumping back to the first
    if (gifAnimResume(gifScale)) {
      canvas.deleteSprite();
      return;
    }

    bool success = drawGifToCanvas(canvas, imageBuffer, imageBufferSize, gifX, gifY, gifScale);

    if (!success) {
      Serial.println("ERROR: GIF drawing failed, showing error message");
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      canvas.drawString("GIF decode", WIDTH / 2, HEIGHT / 2 - 10);
      canvas.drawString("failed", WIDTH / 2, HEIGHT / 2 + 10);
    } else {
      Serial.println("SUCCESS: GIF drawn successfully");
    }

    // Draw battery indicator on canvas if we used canvas, otherwise directly
    drawBatteryIndicator(canvas);
    captureCanvas(canvas);
    canvas.pushSprite(0, 0);
    canvas.deleteSprite();

    if (success) {
      gifAnimStart(gifScale);  // animated GIFs continue from this first frame
    }

    Serial.printf("Free heap after display: %d bytes\n", ESP.getFreeHeap());
    Serial.println("======================================\n");
    return;
  }

  // Parse JPEG header to extract dimensions
  Serial.println("Parsing JPEG header for dimensions...");
  int16_t jpgWidth = (int16_t)imgWidth;
  int16_t jpgHeight = (int16_t)imgHeight;
  bool dimensionsFound = dimsFound;

  Serial.printf("JPEG dimensions: %dx%d (found=%s)\n", jpgWidth, jpgHeight, dimensionsFound ? "yes" : "no");

  // If dimensions not found, assume image generation default size
  if (!dimensionsFound || jpgWidth <= 0 || jpgHeight <= 0) {
    Serial.println("WARNING: Could not determine JPEG dimensions from header");
    // Assume GPT-image default size (most common is 1024x1024), Dall-E is 512x512
    jpgWidth = 1024;
    jpgHeight = 1024;
    dimensionsFound = true;
    Serial.printf("Using assumed dimensions: %dx%d\n", jpgWidth, jpgHeight);

    // Recalculate scale with assumed dimensions
    wolframScaledHeight = computeScaledHeightForWidth(jpgWidth, jpgHeight, wolframScale);
    updateWolframScrollBounds(wolframScaledHeight);
  }

  if (!dimensionsFound || jpgWidth <= 0 || jpgHeight <= 0) {
    Serial.println("ERROR: Still no valid dimensions, drawing without scaling as last resort");
    // Fallback: draw without scaling
    bool success = canvas.drawJpg(imageBuffer, imageBufferSize, 0, 0);

    if (!success) {
      Serial.println("ERROR: drawJpg() returned false");
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      canvas.drawString("Image decode", WIDTH / 2, HEIGHT / 2 - 10);
      canvas.drawString("failed", WIDTH / 2, HEIGHT / 2 + 10);
    } else {
      Serial.println("SUCCESS: JPEG drawn (unscaled)");
    }
  } else if (wolframScaledHeight > 0) {
    const char* modeStr = (imageZoomMode == CONTAIN) ? "CONTAIN (fit fully)" : "COVER (fill screen)";
    Serial.printf("Zoom mode: %s\n", modeStr);
    Serial.printf("Scale factor: %.2f\n", wolframScale);
    Serial.printf("Scaled dimensions: %dx%d\n", (int)(jpgWidth * wolframScale), (int)(jpgHeight * wolframScale));
    Serial.printf("Original dimensions: %dx%d\n", jpgWidth, jpgHeight);

    // Calculate offsets for positioning
    int scaledWidth = (int)(jpgWidth * wolframScale);
    int scaledHeight = (int)(jpgHeight * wolframScale);
    int xOffset = (WIDTH - scaledWidth) / 2;

    // For Y offset: center only if image is shorter than screen, otherwise align to top
    int yOffset;
    if (scaledHeight <= HEIGHT) {
      // Image fits vertically - center it
      yOffset = (HEIGHT - scaledHeight) / 2;
    } else {
      // Image is taller than screen - align to top and apply scroll offset
      yOffset = -wolframScrollOffset;
    }

    Serial.printf("X offset: %d, Y offset: %d\n", xOffset, yOffset);

    // Use clipping to handle scrolling and keep image within bounds
    canvas.setClipRect(0, 0, WIDTH, HEIGHT);

    // Draw with scaling only (no maxWidth/maxHeight to avoid cropping)
    // The x,y position and scale parameters are enough to control sizing
    bool success = canvas.drawJpg(imageBuffer, imageBufferSize,
                                   xOffset, yOffset,
                                   0, 0,  // No max width/height - use scale instead
                                   0, 0,  // No source offset
                                   wolframScale, wolframScale);
    canvas.clearClipRect();

    if (!success) {
      Serial.println("ERROR: drawJpg() returned false");
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      canvas.drawString("Image decode", WIDTH / 2, HEIGHT / 2 - 10);
      canvas.drawString("failed", WIDTH / 2, HEIGHT / 2 + 10);
    } else {
      Serial.printf("SUCCESS: JPEG drawn (%s)\n", modeStr);
    }
  } else {
    Serial.println("WARNING: Fit-to-width scale unavailable, drawing unscaled");
    bool success = canvas.drawJpg(imageBuffer, imageBufferSize, 0, 0);

    if (!success) {
      Serial.println("ERROR: drawJpg() returned false");
      canvas.setTextColor(TFT_WHITE);
      canvas.setTextDatum(MC_DATUM);
      canvas.setTextFont(2);
      canvas.drawString("Image decode", WIDTH / 2, HEIGHT / 2 - 10);
      canvas.drawString("failed", WIDTH / 2, HEIGHT / 2 + 10);
    } else {
      Serial.println("SUCCESS: JPEG drawn successfully");
    }
  }

  // Draw battery indicator
  drawBatteryIndicator(canvas);

  captureCanvas(canvas);

  canvas.pushSprite(0, 0);
  canvas.deleteSprite();

  // Reset swap bytes to default after displaying
  M5.Display.setSwapBytes(false);
  Serial.println("Display: Reset setSwapBytes(false)");

  Serial.printf("Free heap after display: %d bytes\n", ESP.getFreeHeap());
  Serial.println("======================================\n");
}

String wordWrap(const String &text, int maxChars) {
  String result;
  String word;
  int lineLen = 0;

  for (unsigned int i = 0; i <= text.length(); i++) {
    char c = (i < text.length()) ? text[i] : ' ';

    if (c == ' ' || c == '\n') {
      if (lineLen + word.length() > maxChars) {
        result += '\n';
        lineLen = 0;
      }
      result += word;
      lineLen += word.length();
      word = "";

      if (c == ' ' && lineLen > 0) {
        result += ' ';
        lineLen++;
      }
      if (c == '\n') {
        result += '\n';
        lineLen = 0;
      }
    } else {
      word += c;
    }
  }

  return result;
}

Orientation detectOrientation() {
  // Update IMU data first
  M5.Imu.update();

  auto imu_data = M5.Imu.getImuData();
  float ax = imu_data.accel.x;
  float ay = imu_data.accel.y;
  float az = imu_data.accel.z;

  // Determine orientation based on accelerometer
  // M5StickC Plus 2: when held horizontally (landscape), x-axis dominates
  // When held vertically (portrait on short edge), y-axis dominates
  Orientation newOrientation;

  // Use absolute values to determine which axis has stronger gravity component
  float absX = abs(ax);
  float absY = abs(ay);

  // If Y-axis gravity is stronger, device is vertical (portrait)
  // If X-axis gravity is stronger, device is horizontal (landscape)
  if (absY > absX && absY > 0.5) {
    newOrientation = PORTRAIT;
  } else if (absX > absY && absX > 0.5) {
    newOrientation = LANDSCAPE;
  } else {
    // Ambiguous - keep current state
    return orientationState.current;
  }

  // Apply hysteresis/debouncing
  unsigned long now = millis();
  if (newOrientation != orientationState.current) {
    if (now - orientationState.lastChangeTime > orientationState.debounceMs) {
      orientationState.current = newOrientation;
      orientationState.lastChangeTime = now;
      Serial.printf("Orientation changed to: %s (ax=%.2f, ay=%.2f, az=%.2f)\n",
                    newOrientation == PORTRAIT ? "PORTRAIT" : "LANDSCAPE", ax, ay, az);
      return newOrientation;
    }
  }

  return orientationState.current;
}

int getLandscapeRotation() {
  // Get the appropriate display rotation for landscape mode based on accelerometer
  // Rotation 1: buttons on right side (normal landscape)
  // Rotation 3: buttons on left side (180° rotated landscape)

  M5.Imu.update();
  auto imu_data = M5.Imu.getImuData();
  float ax = imu_data.accel.x;

  // In landscape mode, check x-axis to determine which side is down
  // When ax is negative (< -0.3), buttons are on the left (rotation 3)
  // When ax is positive (> 0.3), buttons are on the right (rotation 1)
  if (ax > 0.3) {
    return 1;  // Buttons on right - normal landscape
  } else {
    return 3;  // Buttons on left - rotated 180°
  }
}

bool detectRollGesture() {
  // Detect rolling motion - when device is rolled so display faces up/sideways/down
  // This is rotation around the X-axis (long axis of the device)
  static unsigned long lastRollTime = 0;
  static bool rollingDetected = false;

  M5.Imu.update();
  auto imu_data = M5.Imu.getImuData();

  // Use gyroscope X-axis (rotation around device's long axis)
  // This detects when you roll the device to make the display face different directions
  float gyroX = imu_data.gyro.x;

  unsigned long now = millis();

  // Detect significant rotation around X axis (rolling the device)
  // Gyroscope measures rotation rate (degrees/second)
  float absGyroX = abs(gyroX);

  // If we detect strong rotation (> 80 deg/s), mark as rolling
  if (absGyroX > 80.0 && !rollingDetected) {
    rollingDetected = true;
    lastRollTime = now;
    Serial.printf("Roll motion started (gyroX=%.2f deg/s)\n", gyroX);
  }

  // If rotation stops and we were rolling, trigger gesture
  if (rollingDetected && absGyroX < 30.0 && (now - lastRollTime > 200)) {
    if (now - lastRollTime < 1000) {  // Must complete within 1 second
      rollingDetected = false;
      Serial.printf("Roll gesture completed (gyroX=%.2f deg/s)\n", gyroX);
      return true;
    }
    // Reset if too slow
    rollingDetected = false;
  }

  // Timeout reset
  if (rollingDetected && (now - lastRollTime > 1500)) {
    Serial.println("Roll gesture timeout, resetting");
    rollingDetected = false;
  }

  return false;
}

bool detectShakeGesture() {
  // Detect shaking motion - rapid acceleration changes
  static unsigned long lastShakeTime = 0;
  static float lastAccelMagnitude = 0;

  M5.Imu.update();
  auto imu_data = M5.Imu.getImuData();

  // Calculate total acceleration magnitude
  float ax = imu_data.accel.x;
  float ay = imu_data.accel.y;
  float az = imu_data.accel.z;
  float accelMagnitude = sqrt(ax * ax + ay * ay + az * az);

  unsigned long now = millis();

  // Debounce: don't trigger another shake within 2 seconds
  if (now - lastShakeTime < 20000) {
    lastAccelMagnitude = accelMagnitude;
    return false;
  }

  // Detect shake: significant change in acceleration magnitude
  // Normal gravity is ~1.0g, shake should cause spikes above 5.0g
  if (accelMagnitude > 5.0) {
    lastShakeTime = now;
    lastAccelMagnitude = accelMagnitude;
    Serial.printf("Shake detected! (accel magnitude=%.2f g)\n", accelMagnitude);
    return true;
  }

  lastAccelMagnitude = accelMagnitude;
  return false;
}

String escapeJson(const String &text) {
  String result = text;
  result.replace("\\", "\\\\");
  result.replace("\"", "\\\"");
  result.replace("\n", "\\n");
  result.replace("\r", "\\r");
  result.replace("\t", "\\t");
  return result;
}

void replayLastAudio() {
  if (!lastAudioBuffer || lastAudioSampleCount == 0) {
    Serial.println("No audio to replay");
    return;
  }

  Serial.println("\n========== REPLAYING AUDIO ==========");
  Serial.printf("Playing %d samples at 24kHz...\n", lastAudioSampleCount);

  M5.Speaker.begin();  // Switched off during sleep (powerDownPeripherals)
  M5.Speaker.playRaw(lastAudioBuffer, lastAudioSampleCount, 24000, false, 1, 0, false);

  // Show voice indicator during playback
  while (M5.Speaker.isPlaying()) {
    drawVoicePlaybackIndicator();
    delay(50);
    M5.update();
  }

  // Clear the voice indicator after playback
  clearVoicePlaybackIndicator();
  discardButtonA(0);  // Taps during playback must not start a recording

  Serial.println("Replay complete");
  Serial.println("=====================================\n");
}

bool speakText(const String &text) {
  modelViewEnd();  // Reading aloud needs the memory; the screen keeps its last frame
  Serial.println("\n========== TEXT-TO-SPEECH ==========");
  Serial.println("Text: " + text);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(60);

  AudioEndpoint ep = audioEndpoint();
  if (!ep.available) {
    Serial.println("ERROR: TTS needs an OpenAI or Zitro key");
    return false;
  }

  Serial.println("TTS: Connecting to " + ep.host + "...");
  if (!client.connect(ep.host.c_str(), 443)) {
    Serial.println("ERROR: TTS connection failed");
    return false;
  }

  // Build JSON request body
  // Using PCM format - no header, just raw 16-bit little-endian PCM at 24kHz mono
  // gpt-4o-mini-tts takes speaking-style instructions instead of a speed parameter
  String jsonBody = "{\"model\":\"" TTS_MODEL "\",\"input\":\""
    + escapeJson(text)
    + "\",\"voice\":\"nova\",\"instructions\":\"Speak slightly slowly and clearly, warm and friendly, for a child.\",\"response_format\":\"pcm\"}";

  // Send HTTP POST request
  Serial.println("TTS: Sending request...");
  client.print("POST /v1/audio/speech HTTP/1.1\r\n");
  client.print("Host: " + ep.host + "\r\n");
  client.print("Authorization: Bearer " + ep.key + "\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: " + String(jsonBody.length()) + "\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(jsonBody);

  // Read response headers
  Serial.println("TTS: Reading response headers...");
  bool headersComplete = false;
  String headerLine;
  int httpCode = 0;

  while (client.connected()) {
    if (client.available()) {
      headerLine = client.readStringUntil('\n');
      Serial.println("  " + headerLine);

      // Parse HTTP response code from first line
      if (httpCode == 0 && headerLine.startsWith("HTTP/")) {
        int spaceIdx = headerLine.indexOf(' ');
        if (spaceIdx > 0) {
          httpCode = headerLine.substring(spaceIdx + 1, spaceIdx + 4).toInt();
          Serial.printf("TTS: HTTP response code: %d\n", httpCode);
        }
      }

      // Log important headers
      if (headerLine.startsWith("Content-Length:")) {
        Serial.println("  >>> " + headerLine);
      }
      if (headerLine.startsWith("Transfer-Encoding:")) {
        Serial.println("  >>> " + headerLine);
      }
      if (headerLine.startsWith("Content-Type:")) {
        Serial.println("  >>> " + headerLine);
      }

      // Empty line marks end of headers
      if (headerLine == "\r" || headerLine == "") {
        headersComplete = true;
        break;
      }
    }
  }

  if (!headersComplete || httpCode != 200) {
    Serial.printf("ERROR: TTS request failed (code: %d)\n", httpCode);
    client.stop();
    return false;
  }

M5.Speaker.end();
delay(10);
M5.Speaker.begin();

  // Now read PCM audio data - handle chunked transfer encoding
  Serial.println("TTS: Reading PCM audio data (chunked encoding)...");

  // Reserve space for roughly 200KB (typical size for short answers)
  const int MAX_AUDIO_SIZE = 200000;
  uint8_t* audioBuffer = (uint8_t*)malloc(MAX_AUDIO_SIZE);

  if (!audioBuffer) {
    Serial.println("ERROR: Failed to allocate audio buffer");
    client.stop();
    return false;
  }

  int totalBytesRead = 0;

  // Decode chunked transfer encoding
  while (client.connected() || client.available()) {
    // Read chunk size line (hex number followed by \r\n)
    String chunkSizeLine = client.readStringUntil('\n');
    chunkSizeLine.trim();  // Remove \r\n

    if (chunkSizeLine.length() == 0) {
      continue;  // Skip empty lines
    }

    // Parse hex chunk size
    long chunkSize = strtol(chunkSizeLine.c_str(), NULL, 16);
    Serial.printf("TTS: Chunk size: %ld (0x%s)\n", chunkSize, chunkSizeLine.c_str());

    if (chunkSize == 0) {
      // Last chunk - end of stream
      Serial.println("TTS: Reached end of chunked stream");
      break;
    }

    if (chunkSize < 0 || chunkSize > 100000) {
      Serial.printf("ERROR: Invalid chunk size: %ld\n", chunkSize);
      break;
    }

    // Read chunk data
    int bytesRead = 0;
    while (bytesRead < chunkSize && (client.connected() || client.available())) {
      if (totalBytesRead + bytesRead >= MAX_AUDIO_SIZE) {
        Serial.println("ERROR: Audio buffer full");
        break;
      }

      int toRead = min((int)(chunkSize - bytesRead), MAX_AUDIO_SIZE - totalBytesRead - bytesRead);
      int n = client.read(audioBuffer + totalBytesRead + bytesRead, toRead);
      if (n > 0) {
        bytesRead += n;
      } else {
        delay(1);
      }
    }

    totalBytesRead += bytesRead;
    Serial.printf("TTS: Read %d bytes (total: %d)\n", bytesRead, totalBytesRead);

    // Read trailing \r\n after chunk data
    client.read();  // \r
    client.read();  // \n
  }

  client.stop();

  Serial.printf("TTS: Buffered %d PCM bytes\n", totalBytesRead);

  if (totalBytesRead < 2) {
    Serial.println("ERROR: No audio data received");
    free(audioBuffer);
    return false;
  }

  // Convert to int16_t samples for playback
  int sampleCount = totalBytesRead / 2;
  int16_t* samples = (int16_t*)audioBuffer;  // Reinterpret as int16_t array

  // Save audio for replay
  if (lastAudioBuffer) {
    free(lastAudioBuffer);
  }
  lastAudioBuffer = (int16_t*)malloc(totalBytesRead);
  if (lastAudioBuffer) {
    memcpy(lastAudioBuffer, samples, totalBytesRead);
    lastAudioSampleCount = sampleCount;
    hasAudioForCurrentText = true;  // Mark that current text has audio
    Serial.println("TTS: Audio saved for replay");
  } else {
    Serial.println("WARNING: Failed to allocate replay buffer");
    lastAudioSampleCount = 0;
    hasAudioForCurrentText = false;
  }

  // Play entire audio buffer in one call - no gaps, no clicks
  Serial.printf("TTS: Playing %d samples at 24kHz...\n", sampleCount);
  M5.Speaker.playRaw(samples, sampleCount, 24000, false, 1, 0, false);

  // Show voice indicator during playback
  while (M5.Speaker.isPlaying()) {
    drawVoicePlaybackIndicator();
    delay(50);  // Update indicator every 50ms
    M5.update();
  }

  // Clear the voice indicator after playback
  clearVoicePlaybackIndicator();

  free(audioBuffer);

  Serial.printf("TTS: Playback complete (%d samples played)\n", sampleCount);
  Serial.println("====================================\n");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n========================================");
  Serial.println("          Zitro Voice Assistant");
  Serial.println("========================================\n");

  auto cfg = M5.config();
  M5.begin(cfg);
  // Release the power latch held through deep sleep. Drive it high first so power stays on
  // when the hold lets go (writes are ignored while held).
  pinMode(GPIO_NUM_4, OUTPUT);
  digitalWrite(GPIO_NUM_4, HIGH);
  gpio_hold_dis(GPIO_NUM_4);
  M5.Display.setRotation(getLandscapeRotation());

  // Initialize custom fonts with European language support
  FontManager::init();

  // Initialize speaker for the jingle and TTS playback
  M5.Speaker.begin();
  {
    Preferences volumePrefs;
    volumePrefs.begin("settings", true);
    M5.Speaker.setVolume(volumePrefs.getInt("volume", 210));
    volumePrefs.end();
  }

  // Show launch logo
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.drawPng(logo_png, logo_png_len, 0, 0);
  // Jingle on power-on and restart only: a wake from deep sleep also passes through here,
  // and shouldn't make a sound the user didn't ask for.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    M5.Speaker.playRaw(splashJingleData, SPLASH_JINGLE_SAMPLES, SPLASH_JINGLE_SAMPLE_RATE, false, 1, 0, true);
  }
  delay(2000);  // Show logo for 2 seconds

  // Configure button timing for better detection
  // M5Unified buttons: BtnA (GPIO37), BtnB (GPIO39), BtnPWR/BtnC (GPIO35)
  M5.BtnA.setDebounceThresh(10);   // 10ms debounce
  M5.BtnB.setDebounceThresh(10);
  M5.BtnPWR.setDebounceThresh(10);

  // Set hold threshold high to avoid interfering with double-click
  // If hold threshold is too low, it might prevent double-click detection
  M5.BtnA.setHoldThresh(1000);     // 1000ms for hold detection
  M5.BtnB.setHoldThresh(1000);     // Increased from default to allow double-click
  M5.BtnPWR.setHoldThresh(1000);

  // Note: M5Unified doesn't have setDoubleClickThresh()
  // Double-click detection uses internal fixed threshold (~200-350ms)
  Serial.println("Button timing configured: debounce=10ms, hold=1000ms");

  // Initialize IMU for orientation detection
  M5.Imu.begin();
  setGyroEnabled(false);  // Gyro only runs in image view (see updateGyroPower)
  Serial.println("IMU initialized");

  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  // Check if waking from deep sleep
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 || wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.println("Wake-up source: GPIO (from deep sleep)");
    wakeFromDeepSleep = true;
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Wake-up source: Normal power-on or reset");
  } else {
    Serial.printf("Wake-up source: %d\n", wakeup_reason);
  }

  loadSleepTimeouts();

  // Load audio settings from preferences
  {
    Preferences settingsPrefs;
    settingsPrefs.begin("settings", true);
    activeSampleRate = settingsPrefs.getInt("sampleRate", DEFAULT_SAMPLE_RATE);
    activeRecordSeconds = settingsPrefs.getInt("recordSec", DEFAULT_RECORD_SECONDS);
    activeRecordSamples = activeSampleRate * activeRecordSeconds;
    settingsPrefs.end();
    Serial.printf("Audio: %d Hz, %d sec, %d samples (%d KB)\n",
      activeSampleRate, activeRecordSeconds, activeRecordSamples,
      activeRecordSamples * 2 / 1024);
  }

  // Try to connect with saved credentials
  if (!connectToWiFi()) {
    startAPMode();
    return;
  }

  // No AI provider configured yet: open the setup page
  if (!aiChatConfigured()) {
    Serial.println("No AI provider configured - entering setup mode");
    drawScreen("Welcome to Zitro!\n\nAdd an AI key on\nthe setup page.");
    delay(2500);
    startAPMode();
    return;
  }

  // Handle deep sleep wake-up restoration
  if (wakeFromDeepSleep) {
    handleDeepSleepWake();
    discardButtonA(0);  // The wake-up press must not start a recording
    timeWokeFromSleep = millis();
  } else {
    drawScreen(MAIN_QUESTION);
  }

  lastActivityTime = millis(); // Initialize activity timer
  currentPowerState = POWER_ACTIVE;

  Serial.println("\nReady! Press BUTTON to ask a question.");
  Serial.println("Double-click BUTTON to hear the answer spoken aloud.");
  Serial.println("Long press button B to reset WiFi settings.\n");
}

// =============== TIMER/REMINDER SYSTEM ===============

String addTimer(int minutes, const String& label) {
  if (timerCount >= MAX_TIMERS) {
    Serial.println("Timer limit reached");
    return "Maximal " + String(MAX_TIMERS) + " Timer erlaubt.";
  }

  // Find first inactive slot
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (!timers[i].active) {
      timers[i].active = true;
      timers[i].fireTime = time(NULL) + (time_t)minutes * 60;
      timers[i].label = label;
      timerCount++;
      Serial.printf("Timer added: '%s' in %d min (fires at %ld)\n", label.c_str(), minutes, (long)timers[i].fireTime);

      return "Timer: " + label + " in " + formatTimerDuration(minutes);
    }
  }
  return "Kein freier Timer-Platz.";
}

// "10 Min.", "2h." or "1h 30m."
String formatTimerDuration(int minutes) {
  if (minutes >= 60) {
    int h = minutes / 60;
    int m = minutes % 60;
    if (m > 0) return String(h) + "h " + String(m) + "m.";
    return String(h) + "h.";
  }
  return String(minutes) + " Min.";
}

// Rotation for the timer screens: they turn with the stick like the print screens,
// portrait included
int heldRotation() {
  return printUiRotation();
}

// Check badge on the stopwatch: the timer is set
static void drawTimerSetBadge(M5Canvas &canvas, int iconX, int iconY, int iconW, int iconH) {
  int bx = iconX + iconW - 2;
  int by = iconY + iconH - 4;
  canvas.fillCircle(bx, by, 11, TFT_WHITE);
  canvas.fillCircle(bx, by, 9, TFT_BLACK);
  drawCheckMark(canvas, bx, by, 9, TFT_WHITE);
}

// "Timer set" confirmation: white card with stopwatch and check badge, label, and when it rings.
// Landscape: stopwatch left of the text. Portrait: stopwatch above, text centered below.
void drawTimerSet(const String &label, const String &when) {
  int rotation = heldRotation();
  bool portrait = isPortraitRotation(rotation);
  timerSetDrawnRotation = rotation;
  M5.Display.setRotation(rotation);
  int cw = M5.Display.width();
  int ch = M5.Display.height();
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(cw, ch);
  canvas.fillSprite(TFT_BLACK);

  const int cardX = 8;
  const int cardY = 14;
  const int cardW = cw - 2 * cardX;
  const int cardH = ch - 2 * cardY;
  canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

  const int scale = 2;
  int iconW = TIMERICON_WIDTH * scale;
  int iconH = TIMERICON_HEIGHT * scale;

  String name = label;
  name.trim();
  if (name.length() == 0) name = "Timer";
  String whenText = "in " + when;
  if (whenText.endsWith(".")) whenText.remove(whenText.length() - 1);
  canvas.setTextColor(TFT_BLACK);

  if (!portrait) {
    int iconX = cardX + 16;
    int iconY = cardY + (cardH - iconH) / 2;
    drawIconScaled(canvas, timerIcon, TIMERICON_WIDTH, TIMERICON_HEIGHT, iconX, iconY, scale, TFT_BLACK);
    drawTimerSetBadge(canvas, iconX, iconY, iconW, iconH);

    int textX = iconX + iconW + 18;
    int textMaxW = cardX + cardW - 10 - textX;

    // Name (up to two lines) above the time, centered together in the card
    canvas.setFreeFont(FontManager::getFont12pt());
    String lines[2];
    int n = wrapToLines(canvas, name, textMaxW, lines, 2);
    int nameLineH = canvas.fontHeight() + 2;
    canvas.setFreeFont(FontManager::getFont16pt());
    int whenH = canvas.fontHeight();
    int blockH = n * nameLineH + 4 + whenH;
    int y = cardY + (cardH - blockH) / 2;

    canvas.setTextDatum(TL_DATUM);
    canvas.setFreeFont(FontManager::getFont12pt());
    for (int i = 0; i < n; i++) {
      canvas.drawString(lines[i], textX, y + i * nameLineH);
    }
    canvas.setFreeFont(FontManager::getFont16pt());
    canvas.drawString(fitToWidth(canvas, whenText, textMaxW), textX, y + n * nameLineH + 4);
  } else {
    int textMaxW = cardW - 12;
    canvas.setFreeFont(FontManager::getFont12pt());
    String lines[3];
    int n = wrapToLines(canvas, name, textMaxW, lines, 3);
    int nameLineH = canvas.fontHeight() + 2;
    // The time in the large font when it fits the narrow card
    canvas.setFreeFont(FontManager::getFont16pt());
    bool whenLarge = canvas.textWidth(whenText) <= textMaxW;
    if (!whenLarge) canvas.setFreeFont(FontManager::getFont12pt());
    int whenH = canvas.fontHeight();
    const int iconGap = 14;
    int blockH = iconH + iconGap + n * nameLineH + 6 + whenH;
    int iconX = (cw - iconW) / 2;
    int iconY = cardY + (cardH - blockH) / 2;
    drawIconScaled(canvas, timerIcon, TIMERICON_WIDTH, TIMERICON_HEIGHT, iconX, iconY, scale, TFT_BLACK);
    drawTimerSetBadge(canvas, iconX, iconY, iconW, iconH);

    int y = iconY + iconH + iconGap;
    canvas.setTextDatum(TC_DATUM);
    canvas.setFreeFont(FontManager::getFont12pt());
    for (int i = 0; i < n; i++) {
      canvas.drawString(lines[i], cw / 2, y + i * nameLineH);
    }
    canvas.setFreeFont(whenLarge ? FontManager::getFont16pt() : FontManager::getFont12pt());
    canvas.drawString(fitToWidth(canvas, whenText, textMaxW), cw / 2, y + n * nameLineH + 6);
  }

  if (!portrait) drawBatteryIndicator(canvas);  // drawn for 240 px width
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

void removeTimer(int index) {
  if (index < 0 || index >= MAX_TIMERS || !timers[index].active) return;
  timers[index].active = false;
  timers[index].label = "";
  timerCount--;
  if (timerCount < 0) timerCount = 0;
  Serial.printf("Timer %d removed, %d remaining\n", index, timerCount);
}

void clearAllTimers() {
  for (int i = 0; i < MAX_TIMERS; i++) {
    timers[i].active = false;
    timers[i].label = "";
  }
  timerCount = 0;
  Serial.println("All timers cleared");
}

// Returns seconds until the earliest active timer fires, or -1 if none
int getNextTimerSeconds() {
  time_t now = time(NULL);
  int minSeconds = -1;
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active) {
      int diff = (int)(timers[i].fireTime - now);
      if (diff < 0) diff = 0;
      if (minSeconds < 0 || diff < minSeconds) {
        minSeconds = diff;
      }
    }
  }
  return minSeconds;
}

String formatTimeRemaining(time_t fireTime) {
  time_t now = time(NULL);
  int diff = (int)(fireTime - now);
  if (diff <= 0) return "jetzt!";

  int hours = diff / 3600;
  int mins = (diff % 3600) / 60;
  int secs = diff % 60;

  if (hours > 0) {
    if (mins > 0) return String(hours) + "h " + String(mins) + "m";
    return String(hours) + "h";
  }
  if (mins > 0) {
    if (secs > 0) return String(mins) + "m " + String(secs) + "s";
    return String(mins) + "m";
  }
  return String(secs) + "s";
}

void playAlarmSound() {
  Serial.println("Playing alert sound...");
  // Re-initialize speaker (may be in undefined state after light sleep)
  M5.Speaker.begin();
  // Restore volume from settings
  Preferences volPrefs;
  volPrefs.begin("settings", true);
  int vol = volPrefs.getInt("volume", 210);
  volPrefs.end();
  M5.Speaker.setVolume(vol);
  delay(50);  // Allow speaker to settle

  // Play alert sound twice for emphasis
  for (int i = 0; i < 2; i++) {
    M5.Speaker.playRaw(alertSoundData, ALERT_SOUND_SAMPLES, ALERT_SOUND_SAMPLE_RATE, false, 1, 0, false);
    while (M5.Speaker.isPlaying()) {
      delay(50);
    }
    if (i == 0) delay(300);  // Pause between plays
  }
  Serial.println("Alert sound complete");
}

// 1-bit icon at an integer scale, turned by angleDeg around a pivot given in icon pixels.
// (x, y) is where the top-left corner of the unturned icon would be.
void drawIconRotated(M5Canvas &canvas, const uint8_t* iconData, int iconWidth, int iconHeight,
                     int x, int y, int scale, float angleDeg, float pivotX, float pivotY, uint16_t color) {
  float a = angleDeg * (float)PI / 180.0f;
  float c = cosf(a);
  float sn = sinf(a);
  int bytesPerRow = (iconWidth + 7) / 8;
  float px = x + pivotX * scale;
  float py = y + pivotY * scale;
  int w = iconWidth * scale;
  int h = iconHeight * scale;
  int reach = (int)ceilf(sqrtf((float)(w * w + h * h)));  // farthest an icon pixel gets from the pivot
  int x0 = max(0, (int)px - reach);
  int x1 = min((int)canvas.width() - 1, (int)px + reach);
  int y0 = max(0, (int)py - reach);
  int y1 = min((int)canvas.height() - 1, (int)py + reach);
  for (int dy = y0; dy <= y1; dy++) {
    for (int dx = x0; dx <= x1; dx++) {
      // Turn the screen pixel back by the angle to find the icon pixel it shows
      float rx = dx + 0.5f - px;
      float ry = dy + 0.5f - py;
      float sx = c * rx + sn * ry + px - x;
      float sy = -sn * rx + c * ry + py - y;
      if (sx < 0 || sy < 0) continue;
      int ix = (int)(sx / scale);
      int iy = (int)(sy / scale);
      if (ix >= iconWidth || iy >= iconHeight) continue;
      if (pgm_read_byte(&iconData[iy * bytesPerRow + ix / 8]) & (1 << (7 - ix % 8))) {
        canvas.drawPixel(dx, dy, color);
      }
    }
  }
}

// Timer alert: bell and label filling the screen. Blinks between a white and a black screen
// while the alert is up (see the timer alert block in loop()), and the bell swings from one
// side to the other with each blink. The ✓ tab sits next to Button A.
// Landscape: bell left of the label. Portrait: bell above the label.
void drawTimerAlert(int index, bool whiteScreen = true) {
  if (index < 0 || index >= MAX_TIMERS) return;
  int rotation = heldRotation();
  bool portrait = isPortraitRotation(rotation);
  M5.Display.setRotation(rotation);
  int cw = M5.Display.width();
  int ch = M5.Display.height();
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(cw, ch);
  uint16_t bg = whiteScreen ? TFT_WHITE : TFT_BLACK;
  uint16_t fg = whiteScreen ? TFT_BLACK : TFT_WHITE;
  canvas.fillSprite(bg);

  const int scale = 3;
  int iconW = BELLICON_WIDTH * scale;
  int iconH = BELLICON_HEIGHT * scale;
  // Swing around the bell's hanger (top center of the icon)
  const float swing = whiteScreen ? -10.0f : 10.0f;
  const float pivotX = BELLICON_WIDTH / 2.0f;
  const float pivotY = 1.0f;

  String label = timers[index].label;
  label.trim();
  if (label.length() == 0) label = "Timer";
  String lines[4];
  canvas.setTextColor(fg);

  if (!portrait) {
    // Keep clear of the A tab
    int areaX = (rotation == 3) ? 18 : 4;
    int areaW = cw - 22;
    const int gap = 18;  // room for the swinging bell

    // Large font for up to two lines, smaller font for longer labels
    int textMaxW = areaW - iconW - gap - 6;
    canvas.setFreeFont(FontManager::getFont16pt());
    int n = wrapToLines(canvas, label, textMaxW, lines, 2);
    if (n == 2 && lines[1].endsWith("...")) {
      canvas.setFreeFont(FontManager::getFont12pt());
      n = wrapToLines(canvas, label, textMaxW, lines, 3);
    }
    int textW = 0;
    for (int i = 0; i < n; i++) {
      int w = canvas.textWidth(lines[i]);
      if (w > textW) textW = w;
    }
    int lh = canvas.fontHeight();

    int startX = areaX + (areaW - (iconW + gap + textW)) / 2;
    drawIconRotated(canvas, bellIcon, BELLICON_WIDTH, BELLICON_HEIGHT,
                    startX, ch / 2 - iconH / 2, scale, swing, pivotX, pivotY, fg);

    canvas.setTextDatum(TL_DATUM);
    int textY = ch / 2 - n * lh / 2;
    for (int i = 0; i < n; i++) {
      canvas.drawString(lines[i], startX + iconW + gap, textY + i * lh);
    }
  } else {
    // Keep clear of the A tab at the top or bottom edge
    bool aOnTop = (rotation == 2);
    int areaY = aOnTop ? 18 : 0;
    int areaH = ch - 18;
    int textMaxW = cw - 12;

    // Large font for up to three lines, smaller font for longer labels
    canvas.setFreeFont(FontManager::getFont16pt());
    int n = wrapToLines(canvas, label, textMaxW, lines, 3);
    if (n == 3 && lines[2].endsWith("...")) {
      canvas.setFreeFont(FontManager::getFont12pt());
      n = wrapToLines(canvas, label, textMaxW, lines, 4);
    }
    int lh = canvas.fontHeight();
    const int gap = 16;
    int blockH = iconH + gap + n * lh;
    int iconY = areaY + (areaH - blockH) / 2;
    drawIconRotated(canvas, bellIcon, BELLICON_WIDTH, BELLICON_HEIGHT,
                    (cw - iconW) / 2, iconY, scale, swing, pivotX, pivotY, fg);

    canvas.setTextDatum(TC_DATUM);
    for (int i = 0; i < n; i++) {
      canvas.drawString(lines[i], cw / 2, iconY + iconH + gap + i * lh);
    }
  }

  drawButtonHints(canvas, rotation, HINT_OK, HINT_NONE, HINT_NONE, whiteScreen);
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Timer list: one timer per screen as a white card with stopwatch, label and time left.
// Tabs like the printer list: ✓ next to A (back), trash can next to B (delete),
// ▼ next to C (next timer). Landscape: stopwatch left of the text, dots in the card.
// Portrait: stopwatch above the text, dots above or below the card (away from the A tab).
void drawTimerListEntry(int displayIndex) {
  // Find the Nth active timer
  int count = 0;
  int realIndex = -1;
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active) {
      if (count == displayIndex) {
        realIndex = i;
        break;
      }
      count++;
    }
  }

  if (realIndex < 0) {
    drawScreen("Keine Timer aktiv.");
    return;
  }

  int rotation = heldRotation();
  bool portrait = isPortraitRotation(rotation);
  M5.Display.setRotation(rotation);
  int cw = M5.Display.width();
  int ch = M5.Display.height();
  M5Canvas canvas(&M5.Display);
  canvas.createSprite(cw, ch);
  canvas.fillSprite(TFT_BLACK);

  const int scale = 2;
  int iconW = TIMERICON_WIDTH * scale;
  int iconH = TIMERICON_HEIGHT * scale;
  String label = timers[realIndex].label;
  label.trim();
  if (label.length() == 0) label = "Timer";
  String timeLeft = formatTimeRemaining(timers[realIndex].fireTime);
  const int spacing = 11;  // between the position dots

  if (!portrait) {
    // Card between the tabs: A tab on the button side, B and C tabs above and below
    int cardX = (rotation == 3) ? 22 : 10;
    int cardW = cw - 32;
    const int cardY = 18;
    const int cardH = ch - 36;
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

    int iconX = cardX + 12;
    drawIconScaled(canvas, timerIcon, TIMERICON_WIDTH, TIMERICON_HEIGHT,
                   iconX, cardY + (cardH - iconH) / 2, scale, TFT_BLACK);

    int textX = iconX + iconW + 14;
    int textMaxW = cardX + cardW - 10 - textX;
    canvas.setTextColor(TFT_BLACK);
    canvas.setTextDatum(ML_DATUM);
    canvas.setFreeFont(FontManager::getFont12pt());
    canvas.drawString(fitToWidth(canvas, label, textMaxW), textX, cardY + 24);
    canvas.setFreeFont(FontManager::getFont16pt());
    canvas.drawString(fitToWidth(canvas, timeLeft, textMaxW), textX, cardY + cardH / 2 + 4);

    // Which timer of how many, centered in the card
    if (timerCount > 1) {
      int dotsY = cardY + cardH - 16;
      int startX = cardX + cardW / 2 - (timerCount - 1) * spacing / 2;
      for (int i = 0; i < timerCount; i++) {
        int dx = startX + i * spacing;
        if (i == displayIndex) {
          canvas.fillCircle(dx, dotsY, 3, TFT_BLACK);
        } else {
          canvas.drawCircle(dx, dotsY, 3, PRINT_GREY);
        }
      }
    }
  } else {
    bool aOnTop = (rotation == 2);
    // Card between the side tabs, sized to its content
    const int cardX = 18;
    const int cardW = cw - 2 * cardX;
    int textMaxW = cardW - 10;
    canvas.setFreeFont(FontManager::getFont12pt());
    String lines[2];
    int n = wrapToLines(canvas, label, textMaxW, lines, 2);
    int lineH = canvas.fontHeight();
    canvas.setFreeFont(FontManager::getFont16pt());
    bool timeLarge = canvas.textWidth(timeLeft) <= textMaxW;
    if (!timeLarge) canvas.setFreeFont(FontManager::getFont12pt());
    int timeH = canvas.fontHeight();
    const int pad = 12;
    const int iconGap = 8;
    int cardH = pad + iconH + iconGap + n * lineH + 2 + timeH + pad;
    int cardY = (ch - cardH) / 2;
    canvas.fillRoundRect(cardX, cardY, cardW, cardH, 10, TFT_WHITE);

    int iconY = cardY + pad;
    drawIconScaled(canvas, timerIcon, TIMERICON_WIDTH, TIMERICON_HEIGHT,
                   (cw - iconW) / 2, iconY, scale, TFT_BLACK);

    int y = iconY + iconH + iconGap;
    canvas.setTextColor(TFT_BLACK);
    canvas.setTextDatum(TC_DATUM);
    canvas.setFreeFont(FontManager::getFont12pt());
    for (int i = 0; i < n; i++) {
      canvas.drawString(lines[i], cw / 2, y + i * lineH);
    }
    canvas.setFreeFont(timeLarge ? FontManager::getFont16pt() : FontManager::getFont12pt());
    canvas.drawString(fitToWidth(canvas, timeLeft, textMaxW), cw / 2, y + n * lineH + 2);

    // Position dots on the side without the A tab
    if (timerCount > 1) {
      int dotsY = aOnTop ? ch - 26 : 26;
      int startX = cw / 2 - (timerCount - 1) * spacing / 2;
      for (int i = 0; i < timerCount; i++) {
        int dx = startX + i * spacing;
        if (i == displayIndex) {
          canvas.fillCircle(dx, dotsY, 3, TFT_WHITE);
        } else {
          canvas.drawCircle(dx, dotsY, 3, PRINT_GREY);
        }
      }
    }
  }

  drawButtonHints(canvas, rotation, HINT_OK, HINT_DELETE, timerCount > 1 ? HINT_NEXT : HINT_NONE);
  if (!portrait) drawBatteryIndicator(canvas);  // drawn for 240 px width
  captureCanvas(canvas);
  canvas.pushSprite(0, 0);
  canvas.deleteSprite();
}

// Get the real array index for a given display index
int getTimerRealIndex(int displayIndex) {
  int count = 0;
  for (int i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active) {
      if (count == displayIndex) return i;
      count++;
    }
  }
  return -1;
}

// Check if any timer has fired, trigger alert
void checkTimers() {
  if (timerCount <= 0) return;

  time_t now = time(NULL);
  // Don't check if time is not synced (epoch < 2024)
  if (now < 1700000000) return;

  for (int i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active && now >= timers[i].fireTime) {
      Serial.printf("Timer %d fired: '%s'\n", i, timers[i].label.c_str());

      // Wake display if sleeping
      if (displaySleeping) {
        M5.Display.wakeup();
        displaySleeping = false;
      }
      resetToActiveState();

      // Show alert and play sound
      timerAlertActive = true;
      timerAlertIndex = i;
      modelViewEnd();  // The alert needs the memory; its dismissal brings the model back
      drawTimerAlert(i);
      playAlarmSound();

      // Mark timer as inactive
      timers[i].active = false;
      timerCount--;
      if (timerCount < 0) timerCount = 0;

      return; // Process one at a time
    }
  }
}

void handleVoiceQuery(bool playAudio) {
  // Recording and the AI need the memory, and this question's answer is what comes next
  modelViewEnd();
  const bool cameFromModel = currentDisplayMode == SHOW_MODEL;
  if (cameFromModel) {
    currentDisplayMode = SHOW_TEXT;
    modelResetRotateToggle = true;
  }
  Serial.println("\n*** BUTTON PRESSED ***\n");
  if (playAudio) {
    Serial.println("Mode: DOUBLE-CLICK (with audio)");
  } else {
    Serial.println("Mode: SINGLE-CLICK (text only)");
  }
  Serial.printf("Free heap before recording: %d bytes\n", ESP.getFreeHeap());
  resetToActiveState(); // Reset to active state

  if (!aiChatConfigured() || !audioEndpoint().available) {
    Serial.println("AI not fully configured (chat or speech key missing)");
    drawScreen(!aiChatConfigured()
      ? "No AI key set.\nTriple-press the\nside button for\nsettings."
      : "Voice needs an\nOpenAI or Zitro\nkey. Triple-press\nside button.");
    delay(3000);
    drawScreen(MAIN_QUESTION);
    return;
  }

  bool recordingCancelled = false;
  const bool typedQuestion = serialTestQuestion.length() > 0;  // Serial "ask": no mic needed
  if (!typedQuestion && !recordAudio(recordingCancelled)) {
    if (recordingCancelled) {
      if (cameFromModel) currentDisplayMode = SHOW_MODEL;
      redrawPreviousResult();
      resetToActiveState();
      return;
    }
    drawScreenWithIcon("Mic error", micBrokenIcon, MICBROKENICON_WIDTH, MICBROKENICON_HEIGHT, 2);
    delay(2000);
    drawScreen(MAIN_QUESTION);
    return;
  }

  Serial.printf("Free heap after recording: %d bytes\n", ESP.getFreeHeap());

  drawScreenWithIcon("Transcribing...", transcribeIcon, TRANSCRIBEICON_WIDTH, TRANSCRIBEICON_HEIGHT);
  String question = typedQuestion ? serialTestQuestion : transcribeAudio();
  serialTestQuestion = "";

  if (question.length() < 2 || question.startsWith("No ") || question.startsWith("Parse") || question.startsWith("Connection") || question.startsWith("Timeout")) {
    Serial.println("Transcription failed or empty");
    drawScreen("Couldn't hear.\nTry again.");
    delay(2000);
    drawScreen(MAIN_QUESTION);
    return;
  }

  // Check if parental lock is enabled and it's restricted hours
  // This check must happen before sending to GPT to avoid unnecessary API calls
  if (parentalLockEnabled && isRestrictedHours()) {
    // Still allow settings changes (e.g., disabling parental lock) during restricted hours
    String normalizedQuestion = normalizeString(question);
    // Only block if it's not a settings-related command
    if (normalizedQuestion.indexOf("kindersicherung") < 0 &&
        normalizedQuestion.indexOf("einstellung") < 0 &&
        normalizedQuestion.indexOf("setting") < 0) {
      Serial.println("Parental lock is ON and current time is restricted (8pm-6am)");
      drawScreenWithIcon("Ich schlafe z z z", nightIcon, NIGHTICON_WIDTH, NIGHTICON_HEIGHT);
      M5.Display.sleep();
      delay(3000);
      return;
    }
  }

  drawScreenWithIcon("Thinking...", thinkingIcon, THINKINGICON_WIDTH, THINKINGICON_HEIGHT, 2);
  GPTResponse gptResponse = askGPT(question);

  // Handle voice-controlled settings changes
  if (gptResponse.isSetting && gptResponse.settingKey.length() > 0) {
    Serial.println("Setting change requested: " + gptResponse.settingKey + " = " + gptResponse.settingValue);
    String result = applySettingChange(gptResponse.settingKey, gptResponse.settingValue);
    response = wordWrap(result, 25);
    Serial.println("Setting change result: " + result);
    // Volume changes show a speaker in front of the text (also when redrawn later)
    if (gptResponse.settingKey == "volume") {
      iconResponse = response;
      iconResponseText = result;
      iconResponseIcon = volumeIcon;
    }
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
    resetToActiveState();
    if (playAudio && result.length() > 0) {
      speakText(result);
    }
    return;
  }

  // Handle onboarding/AP mode request
  if (gptResponse.isOnboarding) {
    Serial.println("Onboarding mode requested via voice");
    response = wordWrap(gptResponse.answer, 25);
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
    if (playAudio && gptResponse.answer.length() > 0) {
      speakText(gptResponse.answer);
    }
    delay(2000);
    drawSetupScreen(true);
    delay(2000);
    if (!apMode) {
      WiFi.disconnect();
    }
    startAPMode();
    return;
  }

  // Handle timer commands
  if (gptResponse.setTimer && gptResponse.timerMinutes > 0) {
    Serial.printf("Timer requested: %d min, label: %s\n", gptResponse.timerMinutes, gptResponse.timerLabel.c_str());
    int timersBefore = timerCount;
    String result = addTimer(gptResponse.timerMinutes, gptResponse.timerLabel);
    response = wordWrap(result, 25);
    if (timerCount > timersBefore) {
      // Show the confirmation as a card (also on later redraws of this response)
      timerSetResponse = response;
      timerSetLabel = gptResponse.timerLabel;
      timerSetWhen = formatTimerDuration(gptResponse.timerMinutes);
    }
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
    resetToActiveState();
    if (playAudio && result.length() > 0) {
      speakText(result);
    }
    return;
  }

  if (gptResponse.listTimers) {
    Serial.println("List timers requested");
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    if (timerCount > 0) {
      inTimerListMode = true;
      timerListIndex = 0;
      drawTimerListEntry(0);
    } else {
      String noTimers = "Keine Timer aktiv.";
      response = noTimers;
      drawScreen(noTimers);
      if (playAudio) speakText(noTimers);
    }
    resetToActiveState();
    return;
  }

  if (gptResponse.clearTimer) {
    Serial.println("Clear timers requested");
    clearAllTimers();
    String cleared = gptResponse.answer.length() > 0 ? gptResponse.answer : "Alle Timer geloescht.";
    response = wordWrap(cleared, 25);
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    drawScreen(response);
    resetToActiveState();
    if (playAudio) speakText(cleared);
    return;
  }

  // Remember this exchange for follow-up questions (only real answers, not errors)
  if (gptResponse.valid && !gptResponse.isWolfram) {
    memoryAdd(question, gptResponse.answer);
  }

  response = wordWrap(gptResponse.answer, 25);
  Serial.println("Final display text:");
  Serial.println(response);

  // A real 3D model replaces the picture; without one, the picture is the answer as before
  hasModel = false;
  modelRefText = "";
  eduUnloadActive();
  ModelRef modelRef;
  if (!gptResponse.createImage && !gptResponse.isWolfram && parseModelRef(gptResponse.model.c_str(), modelRef)) {
    drawScreen("Looking it up...");
    fetchModel(modelRef);
  }

  // Handle image retrieval (DALL-E, Wikipedia, or Wolfram Alpha)
  bool imageRetrievalSuccess = true;
  if (hasModel) {
    hasImage = false;
    currentImageUrl = "";
    isWolframImage = false;
  } else {
    imageRetrievalSuccess = handleImageRetrieval(gptResponse, question);
  }

  // If Wolfram Alpha failed, update the response text
  if (gptResponse.isWolfram && !imageRetrievalSuccess) {
    response = "Nicht verstanden.\nWiederhole das\nnochmals.";
    Serial.println("Wolfram failed, updated response to: " + response);
  }

  // Always show text first in landscape mode
  // User can rotate device to toggle to image view
  M5.Display.setRotation(getLandscapeRotation());  // Landscape with auto-rotation
  currentDisplayMode = SHOW_TEXT;
  imageZoomMode = CONTAIN;  // Reset zoom mode to default for new query
  // A found 3D model is the answer, so it opens right away; Button C shows the text
  if (hasModel) {
    Serial.println("Initial display: showing the 3D model");
    modelViewShow();  // Without memory it shows the text instead
  } else {
    Serial.println("Initial display: showing text in landscape mode");
    drawScreen(response);
  }

  resetToActiveState(); // Reset to active state after showing answer

  // Force audio for music/story/song requests even in text-only mode (if enabled in settings)
  if (!playAudio && gptResponse.isAudio) {
    Preferences audioPrefs;
    audioPrefs.begin("settings", true);
    bool autoAudioEnabled = audioPrefs.getBool("autoAudio", false);
    audioPrefs.end();
    if (autoAudioEnabled) {
      Serial.println("GPT flagged is_audio=true - forcing audio playback");
      playAudio = true;
    } else {
      Serial.println("GPT flagged is_audio=true but auto-audio is disabled in settings");
    }
  }

  // If audio playback requested, speak the answer
  if (playAudio && response.length() > 0) {
    Serial.println("Playing audio...");
    if (!speakText(gptResponse.answer)) {
      Serial.println("TTS failed, continuing without audio");
      hasAudioForCurrentText = false;  // No audio available if TTS failed
    }
  } else {
    // Text-only mode: no audio for current text
    hasAudioForCurrentText = false;
    resetToActiveState(); // Reset to active state after audio
  }

  Serial.printf("Free heap at end: %d bytes\n", ESP.getFreeHeap());
  Serial.println("\n*** INTERACTION COMPLETE ***\n");
}

// Button A click from the main screen. While asleep or just after waking it only shows
// the previous result; otherwise it starts a voice question.
void handleButtonAClick(bool withAudio) {
  if (displaySleeping || currentPowerState != POWER_ACTIVE) {
    Serial.println("Waking from sleep - showing previous result");
    resetToActiveState();
    timeWokeFromSleep = millis();
    redrawPreviousResult();
    return;
  }

  if (inWakeCooldown()) {
    Serial.printf("Just woke up - cooldown active (%lu ms remaining)\n",
                  WAKE_COOLDOWN - (millis() - timeWokeFromSleep));
    redrawPreviousResult();
    return;
  }
  wakeFromDeepSleep = false;

  handleVoiceQuery(withAudio);
  // Taps during transcription, thinking or read-aloud must not queue up a new recording
  discardButtonA(0);
}

void loop() {
  M5.update();
  handleSerialCommands();
  gifAnimTick();

  // Triple-click detection for Button C to enter settings/capture landing page
  // Track presses (not clicks) to detect pattern without interfering with other handlers
  static bool btnCWasPressed = false;
  bool btnCIsPressed = M5.BtnPWR.isPressed();

  // Detect button press edge (not pressed -> pressed).
  // Not counted in the timer and printer lists (Button C is "next" there, so three quick
  // presses must not open the settings) or while a timer alert is up.
  bool btnCIsListButton = timerAlertActive || inTimerListMode || inPrinterSelectionMode;
  if (btnCIsPressed && !btnCWasPressed && buttonCCountsForSettings(btnCIsListButton, hasImage, hasModel)) {
    unsigned long now = millis();

    // Reset count if too much time has passed since last press
    if (now - btnCLastClickTime > TRIPLE_CLICK_TIMEOUT) {
      btnCClickCount = 0;
    }

    btnCClickCount++;
    btnCLastClickTime = now;

    Serial.printf("Button C press detected (count: %d)\n", btnCClickCount);

    // Check for triple-press
    if (btnCClickCount >= 3) {
      Serial.println("\n*** TRIPLE-CLICK DETECTED - ENTERING SETTINGS ***\n");
      btnCClickCount = 0;  // Reset counter

      // Display message
      drawSetupScreen(true);
      delay(2000);

      // Stop any ongoing operations if not in AP mode
      if (!apMode) {
        WiFi.disconnect();
      }

      // Start settings/captive portal
      startAPMode();
      return;
    }
  }

  btnCWasPressed = btnCIsPressed;

  // Handle AP mode web server
  if (apMode) {
    server.handleClient();
    dnsServer.processNextRequest();

    // Check for Button A press to retry WiFi connection
    if (M5.BtnA.wasPressed()) {
      Serial.println("Button A pressed - retrying WiFi connection...");
      drawScreen("Retrying WiFi\nconnection...");
      delay(1000);

      // Stop AP mode
      WiFi.softAPdisconnect(true);
      server.stop();
      dnsServer.stop();
      MDNS.end();
      apMode = false;

      // Try to reconnect
      if (connectToWiFi()) {
        Serial.println("WiFi connection successful!");
        drawScreen("Connected!\n" + WiFi.SSID());
        delay(2000);
        drawScreen(MAIN_QUESTION);
      } else {
        Serial.println("WiFi connection failed - returning to AP mode");
        startAPMode();
      }
      return;
    }

    // Still check for reset in AP mode
    if (M5.BtnB.pressedFor(5000)) {
      resetCredentials();
    }
    return;
  }

  // Only check WiFi connection if display is active (not sleeping)
  // Skip if waking from light sleep - handleLightSleepWake() will handle reconnection
  if (!displaySleeping && currentPowerState == POWER_ACTIVE && !wakeFromLightSleep) {
    checkWiFiConnection();
  }

  // Handle light sleep wake-up
  if (wakeFromLightSleep) {
    handleLightSleepWake();
  }

  // Manage power state transitions
  managePowerState();

  // Check for fired timers (every second)
  if (millis() - lastTimerCheck >= 1000) {
    lastTimerCheck = millis();
    checkTimers();
  }

  // Handle timer alert mode - intercept all buttons until dismissed
  if (timerAlertActive) {
    if (M5.BtnA.wasPressed() || M5.BtnA.wasClicked() || M5.BtnA.wasSingleClicked() || M5.BtnA.wasDoubleClicked()) {
      Serial.println("Timer alert dismissed");
      timerAlertActive = false;
      timerAlertIndex = -1;
      // Restore previous display
      if (currentDisplayMode == SHOW_IMAGE && hasImage && imageBuffer) {
        displayImage(currentImageUrl.length() == 0);
      } else if (currentDisplayMode == SHOW_MODEL && hasModel) {
        modelViewShow();
      } else if (response.length() > 0 && !response.startsWith("Press BUTTON")) {
        drawScreen(response);
      } else {
        drawScreen(MAIN_QUESTION);
      }
      resetToActiveState();
      discardButtonA(0);  // This press dismissed the alert; its click must not start a recording
    }
    // Blink between a white and a black screen until dismissed (stops when the display sleeps)
    if (timerAlertActive && !displaySleeping) {
      static unsigned long lastAlertBlink = 0;
      static bool alertWhite = true;
      if (millis() - lastAlertBlink >= 500) {
        lastAlertBlink = millis();
        alertWhite = !alertWhite;
        drawTimerAlert(timerAlertIndex, alertWhite);
      }
    }
    return;  // Don't process other buttons during alert
  }

  // Handle timer list mode - navigate/clear timers (C: next, B: delete, like the printer list)
  if (inTimerListMode) {
    if (M5.BtnPWR.wasClicked()) {
      // Next timer
      timerListIndex++;
      if (timerListIndex >= timerCount) timerListIndex = 0;
      drawTimerListEntry(timerListIndex);
      resetToActiveState();
    }
    if (M5.BtnB.wasClicked()) {
      // Clear currently displayed timer
      int realIdx = getTimerRealIndex(timerListIndex);
      if (realIdx >= 0) {
        Serial.printf("Clearing timer %d from list view\n", realIdx);
        removeTimer(realIdx);
        if (timerCount <= 0) {
          inTimerListMode = false;
          drawScreen("Alle Timer\ngeloescht.");
          delay(1500);
          drawScreen(MAIN_QUESTION);
        } else {
          if (timerListIndex >= timerCount) timerListIndex = timerCount - 1;
          drawTimerListEntry(timerListIndex);
        }
      }
      resetToActiveState();
    }
    if (M5.BtnA.wasClicked() || M5.BtnA.wasSingleClicked() || M5.BtnA.wasDoubleClicked()) {
      // Exit timer list mode
      inTimerListMode = false;
      Serial.println("Exiting timer list mode");
      if (response.length() > 0 && !response.startsWith("Press BUTTON")) {
        drawScreen(response);
      } else {
        drawScreen(MAIN_QUESTION);
      }
      resetToActiveState();
    }
    // Count down the time left every second (also follows the stick: landscape flip, portrait)
    if (inTimerListMode && !displaySleeping) {
      static unsigned long lastTimerListRefresh = 0;
      if (millis() - lastTimerListRefresh >= 1000) {
        lastTimerListRefresh = millis();
        drawTimerListEntry(timerListIndex);
      }
    }
    return;  // Don't process other buttons in timer list mode
  }

  // 3D view: one frame per pass, so timers, buttons and sleep keep working
  if (currentDisplayMode == SHOW_MODEL && hasModel && !displaySleeping && currentPowerState == POWER_ACTIVE) {
    modelViewTick();
  }

  // Detect shake gesture to trigger surprise fact query
  if (!displaySleeping && currentPowerState == POWER_ACTIVE && !inPrinterSelectionMode &&
      currentDisplayMode != SHOW_MODEL && detectShakeGesture()) {
    Serial.println("\n*** SHAKE DETECTED - SURPRISE FACT ***\n");
    resetToActiveState();

    // Wake display if sleeping (though check above prevents this)
    if (displaySleeping) {
      M5.Display.wakeup();
      displaySleeping = false;
    }

    // Show thinking message (brief)
    drawScreenWithIcon("...", thinkingIcon, THINKINGICON_WIDTH, THINKINGICON_HEIGHT, 2);

    // Use pre-generated surprise responses instead of calling GPT
    // For shake gesture, we only use instruction 0 (learning facts) with all 5 question types
    int questionIndex = random(0, 5);
    int instructionIndex = 0;  // Always use instruction 0 for shake (learning facts)

    Serial.printf("Shake gesture: selecting random fact for instruction %d, question %d\n", instructionIndex, questionIndex);

    // Get a random surprise response from the data arrays
    int responseIndex = getRandomSurpriseIndex(instructionIndex, questionIndex);

    if (responseIndex < 0) {
      Serial.println("ERROR: No surprise response found!");
      response = "Keine\nUeberraschung\nverfuegbar.";
      drawScreen(response);
      resetToActiveState();
      Serial.println("\n*** SHAKE INTERACTION FAILED ***\n");
      return;
    }

    SurpriseResponse surpriseResp = getSurpriseResponse(responseIndex);

    // Read answer and wiki query from PROGMEM, using language-appropriate field
    // Check preferences for language setting (defaults to German if not set)
    Preferences langPrefs;
    langPrefs.begin("settings", true);  // Read-only
    String deviceLanguage = langPrefs.getString("language", "DE");
    langPrefs.end();

    const char* answerField = (deviceLanguage == "EN") ? surpriseResp.answerEN : surpriseResp.answerDE;
    String answer = readProgmemString(answerField);
    String wikiQuery = readProgmemString(surpriseResp.wikiQuery);

    Serial.printf("Selected response index %d\n", responseIndex);
    Serial.printf("Answer: %s\n", answer.c_str());
    Serial.printf("Wiki query: %s\n", wikiQuery.c_str());

    response = wordWrap(answer, 25);
    Serial.println("Final display text:");
    Serial.println(response);

    // Try to fetch Wikipedia image for this fact
    hasImage = false;
    currentImageUrl = "";
    hasModel = false;
    modelRefText = "";
    eduUnloadActive();

    if (wikiQuery.length() > 0) {
      Serial.printf("Fetching Wikipedia image for: %s\n", wikiQuery.c_str());
      WikiResult wikiResult = searchWikipedia(wikiQuery);

      if (wikiResult.found && wikiResult.imageUrl.length() > 0) {
        Serial.printf("Found Wikipedia image: %s\n", wikiResult.imageUrl.c_str());
        bool imageDownloaded = downloadImage(wikiResult.imageUrl);

        if (imageDownloaded) {
          hasImage = true;
          currentImageUrl = wikiResult.imageUrl;
          Serial.println("Successfully downloaded Wikipedia image");
        } else {
          Serial.println("Failed to download Wikipedia image");
        }
      } else {
        Serial.println("No Wikipedia image found");
      }
    }

    // Show text in landscape mode
    M5.Display.setRotation(getLandscapeRotation());
    currentDisplayMode = SHOW_TEXT;
    imageZoomMode = CONTAIN;
    drawScreen(response);
    resetToActiveState();

    // Don't speak the answer for shake-triggered surprise facts
    // User can still see the text and image

    Serial.println("\n*** SHAKE INTERACTION COMPLETE ***\n");
  }

  // Use orientation change as a toggle gesture (always render in landscape)
  // Requires full rotation cycle: must rotate to opposite orientation and back
  // Only detect orientation and gestures when display is active
  // In 3D view turning the stick turns the model, so none of this applies
  if (!displaySleeping && currentPowerState == POWER_ACTIVE && currentDisplayMode != SHOW_MODEL) {
    static Orientation lastOrientation = LANDSCAPE;
    static Orientation startOrientation = LANDSCAPE;
    static bool rotationCycleComplete = false;

    Orientation currentOrientation = detectOrientation();

    // Back from 3D view: whatever the hand did there must not count as half a toggle
    if (modelResetRotateToggle) {
      modelResetRotateToggle = false;
      lastOrientation = currentOrientation;
      startOrientation = currentOrientation;
      rotationCycleComplete = false;
    }

    if (currentOrientation != lastOrientation) {
      // Orientation changed
      if (inPrinterSelectionMode) {
        // The printer list turns with the stick (redrawn in the printer selection block);
        // don't let this count as half of a text/image toggle
        startOrientation = LANDSCAPE;
        rotationCycleComplete = false;
      } else if (currentOrientation != startOrientation) {
        // Rotated to opposite orientation - mark cycle as ready
        rotationCycleComplete = true;
        Serial.println("Rotation detected - ready to toggle on return");
      } else if (rotationCycleComplete) {
        // Returned to starting orientation - complete the toggle
        rotationCycleComplete = false;

        if (hasModel && response.length() > 0 && !inPrinterSelectionMode) {
          startOrientation = currentOrientation;
          if (currentDisplayMode == SHOW_TEXT) modelViewShow();
          resetToActiveState();
        } else if (hasImage && response.length() > 0 && !inPrinterSelectionMode) {
          // Toggle between text and image
          if (currentDisplayMode == SHOW_TEXT) {
            currentDisplayMode = SHOW_IMAGE;
            startOrientation = currentOrientation;
            Serial.println("Toggle: Switching to IMAGE view (landscape)");
            // AI-generated images have empty currentImageUrl
            bool isGenerated = (currentImageUrl.length() == 0);
            displayImage(isGenerated);
          } else {
            currentDisplayMode = SHOW_TEXT;
            startOrientation = currentOrientation;
            Serial.println("Toggle: Switching to TEXT view (landscape)");
            drawScreen(response);
          }
          resetToActiveState(); // Reset to active state on toggle
        }
      }

      lastOrientation = currentOrientation;
    }

    // Continuously update display rotation in landscape mode
    if (currentOrientation == LANDSCAPE) {
      static int lastRotation = 1;
      static unsigned long lastRotationCheck = 0;
      unsigned long now = millis();

      // Check rotation every 200ms to avoid excessive updates
      if (now - lastRotationCheck > 200) {
        int newRotation = getLandscapeRotation();
        if (newRotation != lastRotation) {
          M5.Display.setRotation(newRotation);
          lastRotation = newRotation;
          Serial.printf("Display rotated to: %d\n", newRotation);

          // Redraw the current view with new rotation (the printer list moves its
          // button hints to the other side)
          if (inPrinterSelectionMode) {
            displayPrinterSelection();
          } else if (currentDisplayMode == SHOW_TEXT && response.length() > 0) {
            drawScreen(response);
          } else if (currentDisplayMode == SHOW_IMAGE && hasImage) {
            displayImage(currentImageUrl.length() == 0);
          }
        }
        lastRotationCheck = now;
      }
    }

    // The "timer set" card turns with the stick, portrait included
    if (!inPrinterSelectionMode && currentDisplayMode == SHOW_TEXT &&
        timerSetResponse.length() > 0 && response == timerSetResponse) {
      static unsigned long lastTimerCardCheck = 0;
      if (millis() - lastTimerCardCheck > 200) {
        lastTimerCardCheck = millis();
        if (heldRotation() != timerSetDrawnRotation) drawTimerSet(timerSetLabel, timerSetWhen);
      }
    }
  }

  // Detect roll gesture (gyroscope, only powered in image view)
  updateGyroPower();
  if (!displaySleeping && currentPowerState == POWER_ACTIVE && gyroReady() && currentDisplayMode != SHOW_MODEL) {
    if (detectRollGesture()) {
      if (currentDisplayMode == SHOW_IMAGE && hasImage && !inPrinterSelectionMode) {
        // In IMAGE mode: toggle between CONTAIN and COVER zoom modes
        if (imageZoomMode == CONTAIN) {
          imageZoomMode = COVER;
          Serial.println("Roll: Switching to COVER mode (fill screen)");
        } else {
          imageZoomMode = CONTAIN;
          Serial.println("Roll: Switching to CONTAIN mode (fit fully visible)");
        }

        // Redraw image with new zoom mode
        displayImage(currentImageUrl.length() == 0);
        resetToActiveState(); // Reset to active state on zoom change
      }
    }
  }

  // Handle printer selection mode
  if (inPrinterSelectionMode) {
    // Wake guard: a press on a dark screen (or right after waking) only brings the list back
    if (displaySleeping || currentPowerState != POWER_ACTIVE || wakeFromLightSleep || inWakeCooldown()) {
      if (M5.BtnA.wasClicked() || M5.BtnB.wasClicked() || M5.BtnPWR.wasClicked()) {
        resetToActiveState();
        displayPrinterSelection();
      }
      return;
    }

    // Follow the stick: landscape either way round, portrait either way up
    static unsigned long lastPrintRotationCheck = 0;
    if (millis() - lastPrintRotationCheck > 200) {
      lastPrintRotationCheck = millis();
      if (printUiRotation() != M5.Display.getRotation()) {
        displayPrinterSelection();
      }
    }

    // Button C: next printer, page size or zoom value
    if (M5.BtnPWR.wasClicked()) {
      printMenuNext();
      Serial.printf("Print menu step %d: next (printer %d)\n", printMenuStep, selectedPrinterIndex);
      displayPrinterSelection();
      resetToActiveState();
    }

    // Button A: printer -> page size (-> zoom) -> print
    if (M5.BtnA.wasClicked() && !printMenuConfirm()) {
      displayPrinterSelection();
      resetToActiveState();
    } else if (M5.BtnA.wasClicked()) {
      inPrinterSelectionMode = false;
      Serial.println("Printer and size selected, starting print...");

      drawPrintStatus("Printing...", PRINT_BADGE_NONE);
      resetToActiveState();

      bool success = printImage();

      if (success) {
        drawPrintStatus("Sent!", PRINT_BADGE_OK);
      } else {
        drawPrintStatus("Print\nfailed", PRINT_BADGE_FAIL);
      }

      delay(3000);

      // Return to image display
      if (hasImage) {
        displayImage(currentImageUrl.length() == 0);
      } else {
        drawScreen(response);
      }

      resetToActiveState();
    }

    // Button B: one step back, or leave printing from the printer list
    if (M5.BtnB.wasClicked() && printMenuBack()) {
      displayPrinterSelection();
      resetToActiveState();
    } else if (M5.BtnB.wasClicked()) {
      inPrinterSelectionMode = false;
      Serial.println("Printer selection cancelled");

      // Return to image display
      if (hasImage && currentDisplayMode == SHOW_IMAGE) {
        displayImage(currentImageUrl.length() == 0);
      } else {
        drawScreen(response);
      }

      resetToActiveState();
    }

    return;  // Don't process other button actions in printer selection mode
  }

  // Button B long press (hold): Initiate printer discovery and selection when showing image
  // Hold Button B for 1 second to open print menu (much easier than double-click!)
  if (M5.BtnB.wasHold()) {
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("✓ BUTTON B LONG PRESS DETECTED!");
    Serial.printf("  displaySleeping=%d, mode=%d, hasImage=%d\n",
                  displaySleeping, currentDisplayMode, hasImage);
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    // A Wolfram text result has nothing to print yet: this loads its picture
    if (!displaySleeping) wolframSwitchToPicture();

    if (!displaySleeping && currentDisplayMode == SHOW_IMAGE && hasImage) {
      Serial.println("Button B long press: Starting printer discovery...");
      resetToActiveState();

      drawPrintStatus("Searching...", PRINT_BADGE_NONE);

      discoverPrinters();

      if (printerCount > 0) {
        selectedPrinterIndex = 0;
        printMenuReset();
        inPrinterSelectionMode = true;
        displayPrinterSelection();
      } else {
        drawPrintStatus("No printer\nfound", PRINT_BADGE_FAIL);
        delay(3000);

        // Return to image display
        displayImage(currentImageUrl.length() == 0);
      }

      resetToActiveState();
    } else {
      Serial.println("⚠ Long press conditions not met - not in image view mode");
    }
  }
  // Button B short press: scroll up when showing an image
  else if (M5.BtnB.wasClicked()) {
    Serial.printf("Button B single-click detected! displaySleeping=%d, mode=%d, hasImage=%d\n",
                  displaySleeping, currentDisplayMode, hasImage);
    if (!displaySleeping && currentDisplayMode == SHOW_MODEL && hasModel) {
      modelButtonB();
      resetToActiveState();
    } else if (!displaySleeping && currentDisplayMode == SHOW_IMAGE && hasImage) {
      int scrollStep = HEIGHT / 5;
      if (scrollStep < 1) scrollStep = 1;
      wolframScrollOffset = clampInt(wolframScrollOffset + imageScrollDirection(M5.Display.getRotation(), false) * scrollStep, 0, wolframMaxScrollOffset);
      Serial.printf("Button B: scroll offset now: %d\n", wolframScrollOffset);
      displayImage(currentImageUrl.length() == 0);
      resetToActiveState();
    }
  }

  // Button C (PWR button) short press: scroll down when showing an image
  if (M5.BtnPWR.wasClicked()) {
    if (!displaySleeping && hasModel && response.length() > 0 &&
        (currentDisplayMode == SHOW_MODEL || currentDisplayMode == SHOW_TEXT)) {
      if (currentDisplayMode == SHOW_MODEL) {
        if (!modelButtonC()) {
          leaveModelView();
          drawScreen(response);
        }
      } else {
        modelViewShow();
      }
      resetToActiveState();
    } else if (!displaySleeping && currentDisplayMode == SHOW_IMAGE && hasImage) {
      int scrollStep = HEIGHT / 5;
      if (scrollStep < 1) scrollStep = 1;
      wolframScrollOffset = clampInt(wolframScrollOffset + imageScrollDirection(M5.Display.getRotation(), true) * scrollStep, 0, wolframMaxScrollOffset);
      Serial.printf("Button C: scroll offset now: %d\n", wolframScrollOffset);
      displayImage(currentImageUrl.length() == 0);
      resetToActiveState();
    } else if (!displaySleeping && hasImage && response.length() > 0) {
      // Toggle between text and image (original behavior)
      if (currentDisplayMode == SHOW_TEXT) {
        currentDisplayMode = SHOW_IMAGE;
        Serial.println("Button C: Switching to IMAGE view");
        // AI-generated images have empty currentImageUrl
        bool isGenerated = (currentImageUrl.length() == 0);
        displayImage(isGenerated);
      } else {
        currentDisplayMode = SHOW_TEXT;
        Serial.println("Button C: Switching to TEXT view");
        drawScreen(response);
      }
      resetToActiveState();
    }
  }

  // Long press B to reset WiFi credentials
  if (M5.BtnB.pressedFor(3000)) {
    resetCredentials();
  }

  // Button A: double-click asks with spoken answer, single click text only
  if (M5.BtnA.wasDoubleClicked()) {
    handleButtonAClick(true);
  } else if (M5.BtnA.wasSingleClicked()) {
    handleButtonAClick(false);
  }

  delay(20);
}
