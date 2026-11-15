// Pure logic of read-aloud at Microsoft Azure Speech (region of the family's choice, Switzerland North by
// default): voice choice, SSML and checks of what the setup page stores. Plain C++, tested on the host:
// tests/tts/run.sh
//
// POST https://{region}.tts.speech.microsoft.com/cognitiveservices/v1
//   Ocp-Apim-Subscription-Key, Content-Type: application/ssml+xml, User-Agent (required),
//   X-Microsoft-OutputFormat: raw-24khz-16bit-mono-pcm -> the same raw PCM that OpenAI's "pcm" sends, so the
//   player stays as it is. Azure processes the speech in the resource's region.
// Voices: learn.microsoft.com/azure/ai-services/speech-service/language-support?tabs=tts (checked 2026-11-15).
// Multilingual voices read German, English and other languages and detect the language themselves.
#pragma once

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <string>

#define AZURE_TTS_DEFAULT_REGION "switzerlandnorth"
#define AZURE_TTS_DEFAULT_VOICE  "de-DE-SeraphinaMultilingualNeural"
#define AZURE_TTS_FORMAT         "raw-24khz-16bit-mono-pcm"

// What is being read: an answer in the language of the question, or English for a child learning it
enum TtsStyle : uint8_t {
  TTS_STYLE_ANSWER,        // the answer, any language
  TTS_STYLE_ENGLISH,       // English words (vocabulary trainer), British accent, a little slower
  TTS_STYLE_ENGLISH_SLOW,  // English, very slowly (shop game hints)
  TTS_STYLE_GERMAN,        // German words (vocabulary trainer): "Hand" or "Arm" alone could pass for English
};

// The region goes into the host name: lowercase letters and digits only
inline bool ttsValidAzureRegion(const char* region) {
  if (!region || !region[0] || strlen(region) > 32) return false;
  for (const char* p = region; *p; p++) {
    if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p)) return false;
  }
  return true;
}

// The voice goes into the SSML: letters, digits, '-' and ':' (HD voices), like "de-CH-LeniNeural"
inline bool ttsValidAzureVoice(const char* voice) {
  if (!voice || strlen(voice) < 5 || strlen(voice) > 64) return false;
  for (const char* p = voice; *p; p++) {
    if (!isalnum((unsigned char)*p) && *p != '-' && *p != ':') return false;
  }
  return true;
}

inline bool ttsMultilingualVoice(const char* voice) { return voice && strstr(voice, "Multilingual") != nullptr; }

// Text inside SSML
inline std::string ttsXmlEscape(const char* text) {
  std::string out;
  for (const char* p = text ? text : ""; *p; p++) {
    switch (*p) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:
        // Control characters are not allowed in XML 1.0, except tab and line breaks
        if ((unsigned char)*p < 0x20 && *p != '\t' && *p != '\n' && *p != '\r') out += ' ';
        else out += *p;
    }
  }
  return out;
}

// Speaking rate in percent: a little slower for children. Kept small, the stick buffers about 4 s of audio.
inline int ttsAzureRate(TtsStyle style) {
  switch (style) {
    case TTS_STYLE_ENGLISH:
    case TTS_STYLE_GERMAN: return -10;
    case TTS_STYLE_ENGLISH_SLOW: return -20;
    default: return -5;
  }
}

// English voices for the people in the shop game, by the OpenAI voice name they have there (shop_core.h).
// Only standard multilingual voices: Switzerland North has no Azure OpenAI ("Turbo") voices.
inline const char* ttsAzureEnglishVoice(const char* openaiVoice) {
  static const char* const map[][2] = {
      {"alloy", "en-US-AvaMultilingualNeural"},      {"ash", "en-US-AndrewMultilingualNeural"},
      {"ballad", "en-US-BrianMultilingualNeural"},   {"coral", "en-US-EmmaMultilingualNeural"},
      {"echo", "en-US-AdamMultilingualNeural"},      {"fable", "en-GB-OllieMultilingualNeural"},
      {"nova", "en-GB-AdaMultilingualNeural"},       {"onyx", "en-US-DavisMultilingualNeural"},
      {"sage", "en-US-AmandaMultilingualNeural"},    {"shimmer", "en-US-PhoebeMultilingualNeural"},
      {"verse", "en-US-ChristopherMultilingualNeural"}, {"marin", "en-US-CoraMultilingualNeural"},
      {"cedar", "en-US-SerenaMultilingualNeural"},
  };
  for (const auto& m : map) {
    if (openaiVoice && strcmp(openaiVoice, m[0]) == 0) return m[1];
  }
  return "en-GB-AdaMultilingualNeural";
}

inline bool ttsEnglishStyle(TtsStyle style) { return style == TTS_STYLE_ENGLISH || style == TTS_STYLE_ENGLISH_SLOW; }

// The voice for one request: the family's voice for answers and German words, an English voice for English
inline std::string ttsAzureVoice(TtsStyle style, const char* openaiVoice, const char* familyVoice) {
  if (ttsEnglishStyle(style)) return ttsAzureEnglishVoice(openaiVoice);
  return ttsValidAzureVoice(familyVoice) ? familyVoice : AZURE_TTS_DEFAULT_VOICE;
}

// Plain voices that every region with neural voices has: the second try when Azure refuses a voice (HTTP 400)
inline const char* ttsAzureFallbackVoice(TtsStyle style) {
  return ttsEnglishStyle(style) ? "en-GB-SoniaNeural" : "de-CH-LeniNeural";
}

// The SSML document. On multilingual voices, English practice asks for a British accent and German words for
// German; answers leave the language to the voice, which hears it from the text. Plain voices can't take
// <lang>. <lang> may hold <prosody> (Azure's SSML reference), not the other way round.
inline std::string ttsAzureSsml(const char* text, const std::string& voice, TtsStyle style) {
  std::string locale = voice.size() >= 5 && voice[2] == '-' ? voice.substr(0, 5) : std::string("de-DE");
  const char* lang = !ttsMultilingualVoice(voice.c_str()) ? nullptr
                     : ttsEnglishStyle(style)             ? "en-GB"
                     : style == TTS_STYLE_GERMAN          ? "de-DE"
                                                          : nullptr;
  std::string s = "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='" + locale + "'>";
  s += "<voice name='" + voice + "'>";
  if (lang) s += std::string("<lang xml:lang='") + lang + "'>";
  s += "<prosody rate='" + std::to_string(ttsAzureRate(style)) + "%'>";
  s += ttsXmlEscape(text);
  s += "</prosody>";
  if (lang) s += "</lang>";
  s += "</voice></speak>";
  return s;
}

// One response header line, any capitalisation: "Content-Length: 123" -> value "123"
inline bool ttsHeaderValue(const char* line, const char* name, std::string& value) {
  value.clear();
  if (!line || !name) return false;
  const size_t n = strlen(name);
  for (size_t i = 0; i < n; i++) {
    if (!line[i] || tolower((unsigned char)line[i]) != tolower((unsigned char)name[i])) return false;
  }
  if (line[n] != ':') return false;
  const char* v = line + n + 1;
  while (*v == ' ' || *v == '\t') v++;
  value = v;
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ')) value.pop_back();
  return true;
}
