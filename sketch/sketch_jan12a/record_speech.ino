// Recording speech for the vocabulary trainer and the shop game: into PSRAM at any rate, optionally until the
// speaker stops. Moved out of vocab.ino unchanged, with the screen drawn by the caller.

// Records into PSRAM. stopOnSilence: ends 1.2 s after the speech stops (never before 1.5 s). One tap on Button A
// restarts, a second quick tap cancels. progress(second, seconds) draws the screen once a second.
bool recordSpeech(int rate, int seconds, bool stopOnSilence, int16_t*& buffer, int& samples, bool& cancelled,
                  void (*progress)(int secondsDone, int seconds)) {
  cancelled = false;
  samples = 0;
  buffer = (int16_t*)ps_malloc((size_t)rate * seconds * sizeof(int16_t));
  if (!buffer) return false;
  const int chunk = rate / 20;  // 50 ms
  const int total = rate * seconds;
  bool restart;
  do {
    restart = false;
    int pos = 0, lastSecond = -1, calibrated = 0, quietMs = 0;
    long noise = 0;
    bool speech = false;
    if (!micStart()) {
      micStop();
      free(buffer);
      buffer = nullptr;
      return false;
    }
    while (pos + chunk <= total) {
      const int second = pos / rate;
      if (second != lastSecond) {
        lastSecond = second;
        if (progress) progress(second + 1, seconds);
      }
      M5.Mic.record(buffer + pos, chunk, rate);
      while (M5.Mic.isRecording()) {
        M5.update();
        if (M5.BtnA.wasPressed()) {
          micStop();
          const unsigned long tap = millis();
          bool second = false;
          while (millis() - tap < MULTI_TAP_WINDOW) {
            M5.update();
            if (M5.BtnA.wasPressed()) {
              second = true;
              break;
            }
            delay(10);
          }
          if (second) {
            discardButtonA(800);
            cancelled = true;
            free(buffer);
            buffer = nullptr;
            return false;
          }
          restart = true;
          break;
        }
        delay(1);
      }
      if (restart) break;
      // Loudness of this 50 ms: mean distance from its own mean, so the microphone's offset doesn't count
      long sum = 0;
      for (int k = 0; k < chunk; k++) sum += buffer[pos + k];
      const long mean = sum / chunk;
      long dev = 0;
      for (int k = 0; k < chunk; k++) dev += labs(buffer[pos + k] - mean);
      dev /= chunk;
      pos += chunk;
      if (!stopOnSilence) continue;
      if (calibrated < 6) {
        noise += dev;
        if (++calibrated == 6) noise /= 6;
        continue;
      }
      const long threshold = max(200L, noise * 3);
      if (dev > threshold) {
        speech = true;
        quietMs = 0;
      } else if (speech) {
        quietMs += 50;
      }
      const int elapsedMs = pos * 1000 / rate;
      if (speech && quietMs >= 1200 && elapsedMs >= 1500) break;
    }
    if (!restart) {
      micStop();
      samples = pos;
    }
  } while (restart);
  discardButtonA(0);
  return samples > 0;
}
