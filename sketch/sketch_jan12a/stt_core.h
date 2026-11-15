// Pure logic of speech-to-text at Infomaniak (Switzerland): the upload answers with a batch id, then the stick
// asks for the result until it is ready. Plain C++ with ArduinoJson, tested on the host: tests/stt/run.sh
//
// POST /1/ai/{product}/openai/audio/transcriptions (multipart, like OpenAI's) -> {"batch_id": "<uuid>"}
// GET  /1/ai/{product}/results/{batch_id} -> {"status": "pending", "data": null}
//                                           {"status": "success", "data": "{\"text\": \"...\"}"}
// data is a string: the Whisper JSON document as text, or the transcript itself with response_format=text.
// Infomaniak's /1/ routes may wrap a reply as {"result": "success", "data": {...}}.
#pragma once

#include <ArduinoJson.h>
#include <ctype.h>
#include <string.h>
#include <string>

#define INFOMANIAK_STT_MODEL "whisper"

enum SttPoll { STT_POLL_PENDING, STT_POLL_DONE, STT_POLL_FAILED };

inline JsonVariantConst sttUnwrap(JsonVariantConst root) {
  if (root["result"].is<const char*>() && root["data"].is<JsonObjectConst>()) return root["data"];
  return root;
}

// Line breaks between Whisper's segments become spaces; outer spaces go
inline std::string sttClean(const char* text) {
  std::string out;
  for (const char* p = text ? text : ""; *p; p++) {
    const char c = (*p == '\n' || *p == '\r' || *p == '\t') ? ' ' : *p;
    if (c == ' ' && (out.empty() || out.back() == ' ')) continue;
    out += c;
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

// Product IDs are numbers; anything else would end up in the request line
inline bool sttValidProductId(const char* id) {
  if (!id || !id[0] || strlen(id) > 20) return false;
  for (const char* p = id; *p; p++) if (!isdigit((unsigned char)*p)) return false;
  return true;
}

// The upload's reply. False without a usable batch id (an error reply, for example).
inline bool sttParseBatchId(const char* body, std::string& batchId) {
  batchId.clear();
  JsonDocument doc;
  if (!body || deserializeJson(doc, body)) return false;
  const char* id = sttUnwrap(doc.as<JsonVariantConst>())["batch_id"] | "";
  const size_t n = strlen(id);
  if (n == 0 || n > 64) return false;
  for (size_t i = 0; i < n; i++) {
    if (!isalnum((unsigned char)id[i]) && id[i] != '-') return false;  // It goes into the next URL
  }
  batchId = id;
  return true;
}

// One poll of the result. DONE fills text (empty when nothing was heard).
inline SttPoll sttParseInfomaniakResult(const char* body, std::string& text) {
  text.clear();
  JsonDocument doc;
  if (!body || deserializeJson(doc, body)) return STT_POLL_FAILED;
  JsonVariantConst r = sttUnwrap(doc.as<JsonVariantConst>());
  const char* status = r["status"] | "";
  if (!strcmp(status, "pending") || !strcmp(status, "processing") || !strcmp(status, "running")) return STT_POLL_PENDING;
  if (strcmp(status, "success") != 0) return STT_POLL_FAILED;

  JsonVariantConst data = r["data"];
  if (data.is<JsonObjectConst>()) {
    text = sttClean(data["text"] | "");
    return STT_POLL_DONE;
  }
  const char* s = data | "";
  while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
  if (*s == '{') {
    JsonDocument inner;
    if (!deserializeJson(inner, s)) {
      text = sttClean(inner["text"] | "");
      return STT_POLL_DONE;
    }
  }
  text = sttClean(s);  // response_format=text: data is the transcript
  return STT_POLL_DONE;
}
