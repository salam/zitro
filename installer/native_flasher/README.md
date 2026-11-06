# Native ESP32 Flasher

A standalone, dependency-free ESP32 flasher written in Swift for macOS.

## Overview

This is a native macOS implementation of the essential ESP32 flashing functionality, eliminating the need for Python and esptool.py. It provides a self-contained binary that can be bundled with the installer.

## Features

- ✓ No Python dependency
- ✓ No external dependencies (uses only macOS frameworks)
- ✓ ESP32 flash writing via serial port
- ✓ Automatic device detection
- ✓ Progress reporting
- ✓ Error handling

## Implementation

The flasher implements the ESP32 ROM bootloader protocol:
1. Serial port communication (IOKit)
2. ESP32 boot sequence (reset, boot mode)
3. SLIP protocol for command framing
4. Flash write commands
5. Verification

## Building

```bash
cd native_flasher
./build.sh
```

This creates a standalone `esp32flash` binary in `bin/`.

## Usage

```bash
# Erase flash
./esp32flash --port /dev/cu.usbserial-XXXX --erase

# Write firmware
./esp32flash --port /dev/cu.usbserial-XXXX --write 0x0 firmware.bin

# Write with progress
./esp32flash --port /dev/cu.usbserial-XXXX --write 0x0 firmware.bin --progress
```

## Architecture

```
esp32flash
├── SerialPort.swift      - Serial communication (IOKit)
├── ESP32Protocol.swift   - ESP32 bootloader protocol
├── SLIPProtocol.swift    - SLIP framing
├── Flasher.swift         - High-level flashing logic
└── main.swift            - CLI interface
```

## Protocol Implementation

Based on ESP32 ROM bootloader protocol:
- SLIP framing for packet encapsulation
- Commands: FLASH_BEGIN, FLASH_DATA, FLASH_END
- CRC32 verification
- Packet acknowledgment

## Size

Compiled binary: ~200-500 KB (depending on optimization)

## License

Same as parent project

---

**Note**: This is a minimal implementation focused on firmware flashing. For advanced features (encryption, secure boot, etc.), use the full esptool.py.
