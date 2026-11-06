# M5StickC Plus 2 Voice Assistant - Deployment Guide

## Hardware Requirements
- M5StickC Plus 2 (available on Amazon/The Pi Hut for ~$20/£25)
- USB-C cable for programming

## Software Requirements

### Option 1: Arduino IDE (Recommended for beginners)

1. **Install Arduino IDE**
   - Download from: https://www.arduino.cc/en/software
   - Install version 2.0 or later

2. **Add ESP32 Board Support**
   - Open Arduino IDE
   - Go to `File > Preferences`
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to `Tools > Board > Boards Manager`
   - Search for "esp32" and install "esp32 by Espressif Systems"

3. **Install Required Libraries**
   - Go to `Tools > Manage Libraries`
   - Install the following:
     - **M5Unified** (by M5Stack)
     - **M5StickCPlus2** (by M5Stack) - if available separately

   Note: WiFi, HTTPClient, WebServer, and Preferences are built into ESP32 core

4. **Configure Board Settings**
   - Go to `Tools > Board > ESP32 Arduino`
   - Select: **M5Stick-C** or **M5StickC Plus**
   - Port: Select the COM/tty port where your M5Stick is connected
   - Upload Speed: 921600 (or 115200 if upload fails)
   - Partition Scheme: Default 4MB with spiffs

### Option 2: PlatformIO (Advanced)

1. **Install PlatformIO**
   - Install VS Code
   - Install PlatformIO IDE extension

2. **Create platformio.ini**
   ```ini
   [env:m5stick-c-plus2]
   platform = espressif32
   board = m5stick-c
   framework = arduino
   lib_deps =
       m5stack/M5Unified@^0.1.14
   monitor_speed = 115200
   upload_speed = 921600
   ```

## Deployment Steps

### 1. Prepare the Code
- Your OpenAI API key is already configured in the code
- Verify line 11 contains your valid API key

### 2. Upload to M5Stick

**Using Arduino IDE:**
1. Open `M5Stick-with-OpenAI-Accesspoint.ino`
2. Connect M5StickC Plus 2 via USB-C
3. Select correct board and port from Tools menu
4. Click the Upload button (right arrow icon)
5. Wait for "Done uploading" message

**Using PlatformIO:**
1. Open the project folder in VS Code
2. Connect M5StickC Plus 2 via USB-C
3. Click the PlatformIO icon in the sidebar
4. Click "Upload" under PROJECT TASKS

### 3. First Time Setup

1. **Power on the device** - The screen should show "Connecting to..." or "WiFi Setup"

2. **If no WiFi is configured:**
   - Device will create an access point named: `M5Voice-Setup`
   - Connect to this network from your phone
   - Open browser and go to: `192.168.4.1`
   - Enter your WiFi credentials
   - Device will restart and connect

3. **If WiFi fails to connect:**
   - Long press button B (side button) for 3 seconds
   - This will reset WiFi credentials
   - Device will restart in AP mode

### 4. Using the Assistant

- **Button A (large front button)**: Press to record and ask a question
  - Hold the device close to your mouth
  - Speak your question clearly
  - 5 seconds of audio will be recorded
  - Device transcribes using Whisper API
  - Response appears on screen (max 20 words)

- **Button B (side button - long press)**: Reset WiFi settings
  - Hold for 3 seconds to clear credentials
  - Device restarts in setup mode

## Troubleshooting

### Upload Issues
- Try a different USB cable (some are charge-only)
- Reduce upload speed to 115200 in Tools menu
- Press and hold button A during upload if it fails
- Check that the correct port is selected

### WiFi Connection Issues
- Ensure 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check WiFi password is correct
- Try moving closer to the router
- Long press button B to reset and reconfigure

### Audio Issues
- Speak clearly and at normal volume
- Hold device 6-12 inches from mouth
- Ensure environment isn't too noisy
- Check Serial Monitor for error messages

### API Issues
- Verify OpenAI API key is valid
- Check you have API credits available
- Monitor Serial output for HTTP error codes
- Whisper API and GPT-4o-mini both incur costs

### Memory Issues
- If device crashes or reboots randomly
- Check Serial Monitor for "heap" messages
- Device has limited RAM for audio buffering

## Serial Monitor Debugging

1. Open Serial Monitor at 115200 baud
2. You'll see detailed logs for:
   - WiFi connection status
   - Audio recording stats
   - API requests/responses
   - Memory usage

## Cost Estimate
- Whisper API: ~$0.006 per transcription (5 sec audio)
- GPT-4o-mini: ~$0.0001 per response (20 words)
- **Total per question: ~$0.007** (less than 1 cent)
- 66 questions ≈ $0.06 (as mentioned in your testing)

## Hardware Buttons Reference
```
                    ┌─────────┐
                    │  POWER  │ (Top)
                    └─────────┘
        ┌───────────────────────────┐
        │                           │
        │       M5STICK C+2         │
        │                           │
        │      ┌───────────┐        │
        │      │  SCREEN   │        │
        │      └───────────┘        │
        │                           │
        │         [  A  ]           │ Button A (Record)
        │                           │
        └───────────────────────────┘
                    │ B │             Button B (Reset WiFi - 3s)
```

## Notes for Parents
- Device works great for kids without phones
- Responses limited to 20 words for quick, digestible answers
- WiFi must be available (connects to your phone's hotspot or home WiFi)
- Very low cost per question (~$0.001)
- Encourages curiosity and learning
- Consider setting up spending limits on OpenAI account

## Support
If you encounter issues, check:
1. Serial Monitor output for errors
2. OpenAI API dashboard for usage/errors
3. M5Stack documentation: https://docs.m5stack.com/
4. ESP32 Arduino core issues: https://github.com/espressif/arduino-esp32

Enjoy your pocket AI assistant!
