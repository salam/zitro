// AI provider layer: chat (OpenAI, Claude, Infomaniak, OpenAI-compatible, Zitro relay),
// speech/images endpoint selection, and the short sliding-window conversation memory.
// Implementation lives in ai_provider.ino.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// Chat providers (value of the "provider" preference)
#define AI_PROVIDER_ZITRO      "zitro"
#define AI_PROVIDER_OPENAI     "openai"
#define AI_PROVIDER_ANTHROPIC  "anthropic"
#define AI_PROVIDER_INFOMANIAK "infomaniak"
#define AI_PROVIDER_CUSTOM     "custom"
#define AI_PROVIDER_DEFAULT    AI_PROVIDER_OPENAI

// Hosts
#define OPENAI_HOST       "api.openai.com"
#define ANTHROPIC_HOST    "api.anthropic.com"
#define INFOMANIAK_HOST   "api.infomaniak.com"
#define ZITRO_RELAY_HOST  "relay.zitro.spot"   // OpenAI-compatible relay, see docs/ZITRO_RELAY_DESIGN.md

#define ANTHROPIC_VERSION "2026-10-21"

// Default models per provider (overridable on the setup page)
#define DEFAULT_OPENAI_MODEL     "gpt-6-luna"
#define DEFAULT_CLAUDE_MODEL     "claude-opus-5-5"
#define DEFAULT_INFOMANIAK_MODEL "qwen3"
#define DEFAULT_ZITRO_MODEL      "zitro-default"

// Speech and image models (OpenAI API, or the Zitro relay which speaks the same API)
#define STT_MODEL            "gpt-transcribe"
#define TTS_MODEL            "gpt-4o-mini-tts"   // OpenAI shuts /v1/audio/speech down on 2027-01-06
#define IMAGE_MODEL          "gpt-image-2.5-flare"
#define IMAGE_MODEL_FALLBACK "gpt-image-1.5"

// Sliding-window conversation memory (RTC RAM only, survives deep sleep, never written to flash)
#define MEMORY_WINDOW_SECONDS 600
#define MEMORY_MAX_TURNS      5
#define MEMORY_Q_LEN          160
#define MEMORY_A_LEN          320

struct ChatResult {
  bool ok;
  String content;  // model text output
  String error;    // short, display-friendly error ("No WiFi", "HTTP 401", ...)
};

// Parsed answer of the main assistant prompt
struct GPTResponse {
  String answer;
  String imageQuery;
  bool valid;
  bool createImage;  // New field to indicate if user wants to create an image
  bool isWolfram;    // New field to indicate if this is a Wolfram Alpha query
  bool isAudio;      // Force audio playback for music/story/song requests
  bool isSetting;    // User wants to change a device setting
  String settingKey;  // Preferences key to change
  String settingValue; // New value as string
  bool setTimer;      // User wants to set a timer/reminder
  int timerMinutes;   // Minutes from now until timer fires
  String timerLabel;  // What to display when timer fires
  bool listTimers;    // User wants to list pending timers
  bool clearTimer;    // User wants to clear all timers
  bool isOnboarding;  // User wants to enter AP/setup mode
  bool isTour;  // User wants to see what Zitro can do: the guided tour
  String model;       // What to look up as a 3D model: "<kind>:<query>", or empty. Filled by name, not by position
  String vocab;       // Vocabulary trainer request: "upload" or "<practice|spell|print>|<book>|<unit>", or empty
};

// Chat endpoint configuration for the selected provider
struct ChatConfig {
  String provider;
  String url;
  String key;
  String model;
  bool anthropic;      // Messages API instead of Chat Completions
  bool openaiStyle;    // OpenAI or Zitro: max_completion_tokens, reasoning_effort, JSON mode
};

// Host + bearer key for OpenAI-style speech and image endpoints
struct AudioEndpoint {
  bool available;
  String host;
  String key;
};

// Public API (implemented in ai_provider.ino)
String aiProvider();
bool aiChatConfigured();
AudioEndpoint audioEndpoint();
ChatResult aiChat(const String& systemPrompt, const String& userText, int maxTokens, bool withMemory, bool jsonMode);
bool memoryEnabled();
void memoryAdd(const String& question, const String& answer);
void memoryClear();
void memoryAppendMessages(JsonArray messages);

// Pure request/response helpers (ai_payload.ino)
String httpErrorText(int code, const String& provider);
String buildChatRequest(const ChatConfig& cfg, const String& systemPrompt, const String& userText,
                        int maxTokens, bool withMemory, bool jsonMode, bool& useFallback);
ChatResult parseChatResponse(int httpCode, const String& resp, const ChatConfig& cfg);
bool parseAiAnswer(const String& content, GPTResponse& out);
