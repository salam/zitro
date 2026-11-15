// Host tests for read-aloud at Azure Speech (tts_core.h). Run: tests/tts/run.sh
// Voice names follow Microsoft's voice list (language-support?tabs=tts, checked 2026-11-15).
#include <cstdio>
#include <string>
#include "../../sketch/sketch_jan12a/tts_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

int main() {
  // Regions end up in the host name
  CHECK(ttsValidAzureRegion("switzerlandnorth"));
  CHECK(ttsValidAzureRegion("westeurope"));
  CHECK(ttsValidAzureRegion("westus2"));
  CHECK(!ttsValidAzureRegion(""));
  CHECK(!ttsValidAzureRegion(nullptr));
  CHECK(!ttsValidAzureRegion("SwitzerlandNorth"));
  CHECK(!ttsValidAzureRegion("evil.example.com/"));
  CHECK(!ttsValidAzureRegion("switzerland north"));
  CHECK(!ttsValidAzureRegion("abcdefghijklmnopqrstuvwxyz0123456"));  // 33 characters

  // Voices end up in the SSML
  CHECK(ttsValidAzureVoice("de-CH-LeniNeural"));
  CHECK(ttsValidAzureVoice("de-DE-SeraphinaMultilingualNeural"));
  CHECK(ttsValidAzureVoice("de-DE-Seraphina:DragonHDLatestNeural"));
  CHECK(!ttsValidAzureVoice("de-CH-Leni'/><x"));
  CHECK(!ttsValidAzureVoice("de CH"));
  CHECK(!ttsValidAzureVoice(""));
  CHECK(!ttsValidAzureVoice(nullptr));
  CHECK(!ttsValidAzureVoice(std::string(65, 'a').c_str()));

  // Escaping: the answer can hold anything the AI wrote
  CHECK(ttsXmlEscape("Tom & Jerry <3 \"Hi\" it's") == "Tom &amp; Jerry &lt;3 &quot;Hi&quot; it&apos;s");
  CHECK(ttsXmlEscape("Grüezi, wie gäht's?") == "Grüezi, wie gäht&apos;s?");  // UTF-8 stays as it is
  CHECK(ttsXmlEscape("a\x01" "b\nc") == "a b\nc");
  CHECK(ttsXmlEscape(nullptr) == "");

  // Voice choice
  CHECK(ttsAzureVoice(TTS_STYLE_ANSWER, "nova", "") == AZURE_TTS_DEFAULT_VOICE);
  CHECK(ttsAzureVoice(TTS_STYLE_ANSWER, "nova", "de-CH-JanNeural") == "de-CH-JanNeural");
  CHECK(ttsAzureVoice(TTS_STYLE_ANSWER, "nova", "bad voice'") == AZURE_TTS_DEFAULT_VOICE);
  CHECK(ttsAzureVoice(TTS_STYLE_ENGLISH, "nova", "de-CH-JanNeural") == "en-GB-AdaMultilingualNeural");
  CHECK(ttsAzureVoice(TTS_STYLE_ENGLISH, "onyx", "") == "en-US-DavisMultilingualNeural");
  CHECK(ttsAzureVoice(TTS_STYLE_ENGLISH_SLOW, "nova", "") == "en-GB-AdaMultilingualNeural");
  CHECK(ttsAzureVoice(TTS_STYLE_ENGLISH, "unknown", "") == "en-GB-AdaMultilingualNeural");
  CHECK(ttsAzureVoice(TTS_STYLE_ENGLISH, nullptr, "") == "en-GB-AdaMultilingualNeural");
  // Every shop voice (shop_core.h) gets its own multilingual English voice, none of them "Turbo"
  const char* shopVoices[] = {"alloy", "ash", "ballad", "coral", "echo", "fable", "nova",
                              "onyx",  "sage", "shimmer", "verse", "marin", "cedar"};
  for (const char* v : shopVoices) {
    const std::string az = ttsAzureEnglishVoice(v);
    CHECK(ttsValidAzureVoice(az.c_str()));
    CHECK(ttsMultilingualVoice(az.c_str()));
    CHECK(!has(az, "Turbo"));
  }
  CHECK(std::string(ttsAzureFallbackVoice(TTS_STYLE_ANSWER)) == "de-CH-LeniNeural");
  CHECK(std::string(ttsAzureFallbackVoice(TTS_STYLE_ENGLISH)) == "en-GB-SoniaNeural");
  CHECK(std::string(ttsAzureFallbackVoice(TTS_STYLE_ENGLISH_SLOW)) == "en-GB-SoniaNeural");
  CHECK(std::string(ttsAzureFallbackVoice(TTS_STYLE_GERMAN)) == "de-CH-LeniNeural");
  // German words keep the family's voice
  CHECK(ttsAzureVoice(TTS_STYLE_GERMAN, "nova", "") == AZURE_TTS_DEFAULT_VOICE);
  CHECK(ttsAzureVoice(TTS_STYLE_GERMAN, "nova", "de-CH-LeniNeural") == "de-CH-LeniNeural");

  // SSML for an answer: the voice's locale, no forced language, a little slower
  std::string s = ttsAzureSsml("Der Himmel ist blau & schön.", AZURE_TTS_DEFAULT_VOICE, TTS_STYLE_ANSWER);
  CHECK(s == "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='de-DE'>"
             "<voice name='de-DE-SeraphinaMultilingualNeural'><prosody rate='-5%'>"
             "Der Himmel ist blau &amp; schön.</prosody></voice></speak>");
  CHECK(!has(s, "<lang"));
  // English practice on a multilingual voice: British accent, slower
  s = ttsAzureSsml("apple", "en-GB-AdaMultilingualNeural", TTS_STYLE_ENGLISH);
  CHECK(has(s, "xml:lang='en-GB'><voice name='en-GB-AdaMultilingualNeural'><lang xml:lang='en-GB'><prosody rate='-10%'>apple</prosody></lang>"));
  s = ttsAzureSsml("two apples", "en-US-DavisMultilingualNeural", TTS_STYLE_ENGLISH_SLOW);
  CHECK(has(s, "xml:lang='en-US'>") && has(s, "<lang xml:lang='en-GB'><prosody rate='-20%'>"));
  // Plain voices can't take <lang>: the fallback never gets it
  s = ttsAzureSsml("apple", "en-GB-SoniaNeural", TTS_STYLE_ENGLISH);
  CHECK(!has(s, "<lang") && has(s, "xml:lang='en-GB'") && has(s, "rate='-10%'"));
  // German words on a multilingual voice: German for sure ("Hand" alone could pass for English)
  s = ttsAzureSsml("Hand", AZURE_TTS_DEFAULT_VOICE, TTS_STYLE_GERMAN);
  CHECK(has(s, "<lang xml:lang='de-DE'><prosody rate='-10%'>Hand</prosody></lang>"));
  s = ttsAzureSsml("Hand", "de-CH-LeniNeural", TTS_STYLE_GERMAN);
  CHECK(!has(s, "<lang") && has(s, "rate='-10%'>Hand<"));
  s = ttsAzureSsml("Hallo", "de-CH-LeniNeural", TTS_STYLE_ANSWER);
  CHECK(has(s, "xml:lang='de-CH'") && has(s, "<voice name='de-CH-LeniNeural'>"));

  // Response headers in any capitalisation
  std::string v;
  CHECK(ttsHeaderValue("Content-Length: 48000\r", "Content-Length", v) && v == "48000");
  CHECK(ttsHeaderValue("content-length:48000", "Content-Length", v) && v == "48000");
  CHECK(ttsHeaderValue("TRANSFER-ENCODING: chunked\r", "Transfer-Encoding", v) && v == "chunked");
  CHECK(!ttsHeaderValue("Content-Type: audio/x-wav", "Content-Length", v) && v.empty());
  CHECK(!ttsHeaderValue("Content-Lengthy: 5", "Content-Length", v));
  CHECK(!ttsHeaderValue("", "Content-Length", v));
  CHECK(!ttsHeaderValue(nullptr, "Content-Length", v));

  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
