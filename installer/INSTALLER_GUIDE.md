# AI Stick Flasher - Installation Guide

## Overview

The AI Stick Flasher is a macOS disk image (DMG) installer that makes it easy to flash the AI Stick firmware to M5StickC Plus2 devices. The installer includes a native macOS application with AppleScript-based GUI dialogs.

## What's Included

The installer package contains:

- **AI Stick Flasher.app** - Native macOS application for flashing firmware
- **firmware/** - Pre-compiled AI Stick firmware (8.0MB merged binary)
- **README.txt** - Quick start guide for end users

## Installation Package Structure

```
AI-Stick-Flasher-1.0.dmg
├── AI Stick Flasher.app/
│   ├── Contents/
│   │   ├── Info.plist
│   │   ├── MacOS/
│   │   │   └── flasher (shell script executable)
│   │   └── Resources/
│   │       └── firmware/
│   │           └── ai-stick-firmware.bin
├── Applications (symlink)
└── README.txt
```

## For End Users

### Requirements
- macOS 10.13 or later
- M5StickC Plus2 device
- USB-C cable
- esptool.py (auto-installed if needed)

### How to Install

1. **Download and Mount**
   - Download `AI-Stick-Flasher-1.0.dmg`
   - Double-click to mount the disk image

2. **Connect Device**
   - Connect your M5StickC Plus2 via USB-C cable
   - Wait for the device to be recognized (appears as `/dev/cu.usbserial-*`)

3. **Run Installer**
   - Double-click **AI Stick Flasher** in the mounted disk image
   - Click "Continue" when prompted
   - Verify the detected device

4. **Configure Device (Optional but Recommended)**
   - **OpenAI API Key**: Enter your API key for AI features, or skip to configure later
   - **WiFi Credentials**: Enter network name (SSID) and password, or skip to configure later
   - These settings are uploaded directly to the device's SPIFFS partition

5. **Flash Firmware**
   - Review your configuration summary
   - Click "Flash" to begin
   - The process takes 30-60 seconds and includes:
     - Step 1: Erase flash memory
     - Step 2: Write firmware (8 MB)
     - Step 3: Write configuration to SPIFFS (if provided)
   - A Terminal window shows real-time progress
   - Do not disconnect the device during flashing

6. **Finish**
   - When complete, disconnect the M5StickC Plus2
   - Press the reset button on the device
   - AI Stick will start automatically
   - If WiFi was configured, the device will connect automatically
   - If API key was configured, AI features work immediately

### Troubleshooting

**Device Not Detected**
- Ensure USB-C cable is properly connected
- Try a different USB port
- Wait 5-10 seconds after connecting
- The device should appear as `/dev/cu.usbserial-XXXXXXXX`

**Flash Failed**
- Press the reset button on the M5StickC Plus2
- Close any Arduino IDE or serial monitor programs
- Try flashing again

**esptool.py Not Found**
- The installer will attempt to auto-install via pip3
- If that fails, install manually:
  ```bash
  pip3 install esptool
  ```
- Or via Homebrew:
  ```bash
  brew install esptool
  ```

## For Developers

### Rebuilding the Installer

After updating the firmware, rebuild the DMG:

```bash
# 1. Compile the latest firmware
./verify.sh

# 2. Navigate to installer directory
cd installer

# 3. Update firmware in the app bundle
cp ../.build/sketch_jan12a.ino.merged.bin firmware/ai-stick-firmware.bin
cp firmware/ai-stick-firmware.bin "AI Stick Flasher.app/Contents/Resources/firmware/"

# 4. Rebuild the DMG
./build_dmg.sh
```

The new DMG will be created as `AI-Stick-Flasher-1.0.dmg`.

### Quick Rebuild Script

Use the provided quick rebuild script:

```bash
./rebuild_installer.sh
```

This script:
1. Compiles the latest firmware using `verify.sh`
2. Updates the firmware in the app bundle
3. Rebuilds the DMG
4. Verifies the DMG integrity

### Customizing the Installer

**Change Version Number**

Edit [`build_dmg.sh`](build_dmg.sh:7):
```bash
VERSION="1.0"  # Change to your version
```

**Change App Name**

1. Rename the app directory
2. Update [`build_dmg.sh`](build_dmg.sh:6)
3. Update [`Info.plist`](AI Stick Flasher.app/Contents/Info.plist)

**Add Custom Icon**

1. Create an `.icns` file
2. Place it in `assets/volume_icon.icns`
3. Rebuild the DMG

### Technical Details

**Flashing Process**

The installer uses `esptool.py` to flash firmware and configuration:

1. Detects device at `/dev/cu.usbserial-*`
2. Collects user configuration (OpenAI API key, WiFi credentials) via AppleScript dialogs
3. Erases flash: `esptool.py --chip esp32 --port <PORT> erase_flash`
4. Writes firmware: `esptool.py --chip esp32 --port <PORT> --baud 921600 write_flash -z 0x0 <FIRMWARE>`
5. Creates SPIFFS image with config.json (if configuration provided)
6. Writes configuration: `esptool.py --chip esp32 --port <PORT> --baud 921600 write_flash 0x670000 <CONFIG>`

**Configuration Storage**

User settings are stored in a `config.json` file on the device's SPIFFS partition:
- SPIFFS partition address: `0x670000`
- SPIFFS partition size: 1.5 MB (`0x180000`)
- Configuration format: JSON with OpenAI API key and WiFi credentials
- The SPIFFS image is created using `create_spiffs_config.py` (bundled in app)

**App Bundle**

The application is a shell script wrapped in a macOS `.app` bundle:
- Main script: [`AI Stick Flasher.app/Contents/MacOS/flasher`](AI Stick Flasher.app/Contents/MacOS/flasher)
- Uses AppleScript (`osascript`) for native dialogs
- Terminal window shows real-time flash progress
- No Python or other runtime dependencies (except esptool)

**DMG Creation**

The DMG is created using `hdiutil`:
- Temporary read-write DMG is created
- Files are copied
- Converted to compressed read-only format (UDZO)
- 96.6% compression ratio (29MB → 1.0MB)

## Distribution

### Sharing the Installer

The DMG file can be distributed via:
- Direct download from website
- GitHub releases
- Email attachment (1.0MB)
- USB drive

### Code Signing (Optional)

For broader distribution, consider code signing:

```bash
# Sign the app
codesign --deep --force --verify --verbose --sign "Developer ID Application: Your Name" "AI Stick Flasher.app"

# Sign the DMG
codesign --sign "Developer ID Application: Your Name" AI-Stick-Flasher-1.0.dmg
```

### Notarization (Optional)

For macOS 10.15+ without Gatekeeper warnings:

```bash
# Notarize the DMG
xcrun altool --notarize-app --primary-bundle-id "com.aistick.flasher" --username "your@email.com" --password "@keychain:AC_PASSWORD" --file AI-Stick-Flasher-1.0.dmg

# Staple the notarization ticket
xcrun stapler staple AI-Stick-Flasher-1.0.dmg
```

## File Sizes

- Firmware binary: 8.0 MB (merged)
- App bundle: ~8.0 MB
- Uncompressed DMG: ~29 MB
- Compressed DMG: ~1.0 MB (96.6% compression)

## Support

For issues or questions:
- Check [REQUIREMENTS.md](../REQUIREMENTS.md)
- Review [USERGUIDE.md](../USERGUIDE.md)
- Check device connection and drivers
- Verify esptool installation

---

**Version:** 1.0
**Created:** 2026-01-30
**Platform:** macOS 10.13+
