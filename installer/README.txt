Firebird AI Stick Installer for macOS
======================================

This installer will flash the Firebird AI Stick firmware to your M5StickC Plus2 device with automatic configuration.

REQUIREMENTS:
-------------
- macOS 10.13 or later
- M5StickC Plus2 connected via USB
- NO other dependencies required! (uses native flasher)

INSTALLATION STEPS:
-------------------
1. Connect your M5StickC Plus2 to your Mac via USB cable

2. Double-click "Install Firebird AI Stick" to launch the installer

3. Follow the on-screen prompts:
   - Click "Continue" to proceed
   - Verify the detected USB device

4. Configure your device (OPTIONAL but recommended):
   - Enter your OpenAI API key (or skip to configure later)
   - Enter your WiFi network name and password (or skip)
   - These settings will be uploaded to the device automatically

5. Click "Flash" to begin flashing (takes 30-60 seconds)
   - Step 1: Erase flash
   - Step 2: Write firmware
   - Step 3: Write configuration (if provided)

6. When finished:
   - Disconnect the M5StickC Plus2
   - Press the reset button on the device to start AI Stick
   - If WiFi was configured, device will connect automatically
   - If API key was configured, AI features will work immediately

TROUBLESHOOTING:
----------------
If the device is not detected:
- Ensure the USB cable is properly connected
- Try a different USB port
- Wait a few seconds after connecting
- The device should appear as /dev/cu.usbserial-*

If flashing fails:
- Press the reset button on the M5StickC Plus2
- Close any other programs that might be using the serial port
- Try flashing again

About the Native Flasher:
- This installer uses a custom-built native ESP32 flasher
- Zero external dependencies - no Python, no esptool needed
- Fast and reliable firmware flashing
- Works on all macOS systems (10.13+)

SUPPORT:
--------
For issues or questions, visit:
https://github.com/anthropics/ai-stick

---
Version 1.0
