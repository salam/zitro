// Host tests for speech-to-text at Infomaniak (stt_core.h). Run: tests/stt/run.sh
// The reply shapes follow Infomaniak's API reference and replies other clients logged from the live API.
#include <cstdio>
#include <string>
#include "../../sketch/sketch_jan12a/stt_core.h"

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main() {
  std::string id, text;

  // Upload replies
  CHECK(sttParseBatchId("{\"batch_id\": \"a2cc35ef-5616-4ba1-976e-cf6e48b50edf\"}", id));
  CHECK(id == "a2cc35ef-5616-4ba1-976e-cf6e48b50edf");
  CHECK(sttParseBatchId("{\"result\":\"success\",\"data\":{\"batch_id\":\"abc-123\"}}", id) && id == "abc-123");
  CHECK(!sttParseBatchId("{\"result\":\"error\",\"error\":{\"code\":\"not_authorized\"}}", id) && id.empty());
  CHECK(!sttParseBatchId("{\"batch_id\": \"../../x\"}", id));
  CHECK(!sttParseBatchId("{\"batch_id\": \"a b\"}", id));
  CHECK(!sttParseBatchId("{\"batch_id\": \"\"}", id));
  CHECK(!sttParseBatchId("{\"batch_id\": 42}", id));
  CHECK(!sttParseBatchId("<html>502</html>", id));
  CHECK(!sttParseBatchId(nullptr, id));
  CHECK(!sttParseBatchId(("{\"batch_id\":\"" + std::string(65, 'a') + "\"}").c_str(), id));

  // Still working
  CHECK(sttParseInfomaniakResult("{\"status\":\"pending\",\"url\":null,\"file_name\":null,\"file_size\":null,\"data\":null}", text) == STT_POLL_PENDING);
  CHECK(sttParseInfomaniakResult("{\"status\":\"processing\",\"data\":null}", text) == STT_POLL_PENDING);
  CHECK(sttParseInfomaniakResult("{\"result\":\"success\",\"data\":{\"status\":\"pending\"}}", text) == STT_POLL_PENDING);

  // Done: data is the Whisper JSON as a string (no or json response_format)
  CHECK(sttParseInfomaniakResult("{\"status\":\"success\",\"file_name\":\"transcription_x.json\",\"file_size\":51,"
                                 "\"data\":\"{\\\"text\\\": \\\" Warum ist der Himmel blau?\\\"}\"}", text) == STT_POLL_DONE);
  CHECK(text == "Warum ist der Himmel blau?");
  // verbose_json carries more fields
  CHECK(sttParseInfomaniakResult("{\"status\":\"success\",\"data\":\"{\\\"text\\\":\\\"Hallo\\\",\\\"language\\\":\\\"de\\\",\\\"segments\\\":[]}\"}", text) == STT_POLL_DONE);
  CHECK(text == "Hallo");
  // response_format=text: data is the transcript, one segment per line
  CHECK(sttParseInfomaniakResult("{\"status\":\"success\",\"data\":\"Vielen Dank.\\nVielen Dank.\\n\"}", text) == STT_POLL_DONE);
  CHECK(text == "Vielen Dank. Vielen Dank.");
  // An object instead of a string, inside the v1 envelope
  CHECK(sttParseInfomaniakResult("{\"result\":\"success\",\"data\":{\"status\":\"success\",\"data\":{\"text\":\"Grüezi\"}}}", text) == STT_POLL_DONE);
  CHECK(text == "Grüezi");
  // Nothing heard
  CHECK(sttParseInfomaniakResult("{\"status\":\"success\",\"data\":\"{\\\"text\\\": \\\"\\\"}\"}", text) == STT_POLL_DONE && text.empty());
  CHECK(sttParseInfomaniakResult("{\"status\":\"success\",\"data\":null}", text) == STT_POLL_DONE && text.empty());

  // Failed or unreadable
  CHECK(sttParseInfomaniakResult("{\"status\":\"error\",\"data\":null}", text) == STT_POLL_FAILED);
  CHECK(sttParseInfomaniakResult("{\"result\":\"error\",\"error\":{\"code\":\"access_result_forbidden\"}}", text) == STT_POLL_FAILED);
  CHECK(sttParseInfomaniakResult("Bad Gateway", text) == STT_POLL_FAILED);
  CHECK(sttParseInfomaniakResult(nullptr, text) == STT_POLL_FAILED);

  // Product IDs
  CHECK(sttValidProductId("105432"));
  CHECK(!sttValidProductId(""));
  CHECK(!sttValidProductId(nullptr));
  CHECK(!sttValidProductId("12 34"));
  CHECK(!sttValidProductId("123\r\nX-Evil: 1"));
  CHECK(!sttValidProductId("123456789012345678901"));

  // Cleaning
  CHECK(sttClean("  a\n\nb  ") == "a b");
  CHECK(sttClean("") == "");

  printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
