// Microphone check for development, compiled only with -DZITRO_MIC_DIAG (never in a release).
// Serial command "micdiag <seconds> <slot>" records like a question does and prints the raw
// samples as hex, so the recording can be checked on a computer. <slot>: r = right I2S slot
// (what Zitro records), l = left slot, s = both slots, interleaved left/right.
#ifdef ZITRO_MIC_DIAG

void runMicDiag(const String& args) {
  int seconds = 3;
  char slot = 'r';
  sscanf(args.c_str(), "%d %c", &seconds, &slot);
  seconds = constrain(seconds, 1, 5);
  bool stereo = (slot == 's');

  auto saved = M5.Mic.config();
  auto cfg = saved;
  cfg.input_channel = stereo ? m5::input_stereo : (slot == 'l' ? m5::input_only_left : m5::input_only_right);
  M5.Mic.config(cfg);

  size_t count = (size_t)activeSampleRate * seconds * (stereo ? 2 : 1);
  int16_t* samples = (int16_t*)malloc(count * sizeof(int16_t));
  if (!samples) {
    Serial.println("@@MICDIAG FAIL memory");
    M5.Mic.config(saved);
    return;
  }

  if (!micStart()) {
    Serial.println("@@MICDIAG FAIL start");
    micStop();
    M5.Mic.config(saved);
    free(samples);
    return;
  }
  unsigned long started = millis();
  M5.Mic.record(samples, count, activeSampleRate, stereo);
  while (M5.Mic.isRecording()) delay(1);
  unsigned long took = millis() - started;
  micStop();
  M5.Mic.config(saved);

  Serial.printf("@@MICDIAG BEGIN rate=%d stereo=%d count=%u ms=%lu\n",
                activeSampleRate, stereo ? 1 : 0, (unsigned)count, took);
  char line[32 * 5 + 2];
  for (size_t i = 0; i < count; i += 32) {
    size_t pos = 0;
    for (size_t j = i; j < i + 32 && j < count; j++) {
      pos += snprintf(line + pos, sizeof(line) - pos, "%04x", (uint16_t)samples[j]);
    }
    Serial.println(line);
  }
  Serial.println("@@MICDIAG END");
  free(samples);
}

// Serial command "asktest": a question without the button, from the recording to Whisper.
// Prints "@@ASK <transcription>", empty when Whisper heard nothing.
void runAskTest() {
  bool cancelled = false;
  if (!recordAudio(cancelled)) {
    Serial.println("@@ASK FAIL record");
    return;
  }
  String text = transcribeAudio();
  Serial.println("@@ASK " + text);
  redrawPreviousResult();
}

// Serial command "hwinfo": what the libraries see of the hardware, one "@@HW" line each.
void runHwInfo() {
  Serial.printf("@@HW board=%d m5unified=%d.%d.%d\n", (int)M5.getBoard(),
                M5UNIFIED_VERSION_MAJOR, M5UNIFIED_VERSION_MINOR, M5UNIFIED_VERSION_PATCH);
  Serial.printf("@@HW psram=%u psram_free=%u heap_free=%u\n",
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getFreeHeap());
  Serial.printf("@@HW battery=%d%% battery_mv=%d charging=%d\n", (int)M5.Power.getBatteryLevel(),
                (int)M5.Power.getBatteryVoltage(), (int)M5.Power.isCharging());
  m5::rtc_datetime_t dt;
  bool rtcRead = M5.Rtc.isEnabled() && M5.Rtc.getDateTime(&dt);
  Serial.printf("@@HW rtc=%d read=%d time=%04d-%02d-%02d %02d:%02d:%02d\n", (int)M5.Rtc.isEnabled(), (int)rtcRead,
                rtcRead ? dt.date.year : 0, rtcRead ? dt.date.month : 0, rtcRead ? dt.date.date : 0,
                rtcRead ? dt.time.hours : 0, rtcRead ? dt.time.minutes : 0, rtcRead ? dt.time.seconds : 0);
  M5.Imu.update();
  auto imu = M5.Imu.getImuData();
  Serial.printf("@@HW imu=%d accel=%.2f,%.2f,%.2f rotation=%d\n", (int)M5.Imu.getType(),
                imu.accel.x, imu.accel.y, imu.accel.z, (int)M5.Display.getRotation());
  Serial.println("@@HW END");
}

#endif
