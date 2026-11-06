# Zitro installer: developer guide

End-user instructions are in [README.txt](README.txt), which ships inside the DMG.

## Building a release

```bash
cd installer
NOTARY_PROFILE=<keychain profile> ./rebuild_installer.sh   # or NOTARY_KEY, NOTARY_KEY_ID, NOTARY_ISSUER
./publish.sh                                               # uploads the DMG to zitro.spot
```

`./rebuild_installer.sh --no-notarize` makes a local test build. Do not publish those.

The script stops at the first problem. An earlier DMG stays in place until the new one has passed every check.

| Step | What it does | What it protects against |
|---|---|---|
| Compile | Fresh build folder every time, from the sketch as it is now | Shipping old firmware |
| Package | Each part must match the build's merged image at its offset; the partition table must match; nothing may overlap the settings | Wrong offsets, stale files |
| Scan | No credentials in the firmware | Keys compiled in |
| Test | The real flasher against a simulated ESP32 | Regressions, including failures a real stick can't produce |
| Sign, notarize | Developer ID, hardened runtime, Apple's notary | Gatekeeper blocking the app |
| Check | Opens the final DMG and checks what's really inside | Everything above going wrong anyway |

Output goes to ignored folders. Nothing built is checked into git: an old build in git is how an API key spread.

## How an install works

1. `esp32flash` checks every firmware file against `manifest.json`. A mismatch stops here, before the stick is touched.
2. It resets the stick into the ROM loader and reads the chip ID. Anything but an ESP32 stops here.
3. It uploads esptool's stub (the tests pin its hash) and reads the flash size. Under 8 MB stops here.
4. It writes each region, compressed, at 460800 baud. The chip then computes an MD5 from its flash and it must match. On failure it retries once at 115200. 921600 loses data on this board.
5. It restarts the stick.
6. With settings: it waits for the firmware to answer, then sends them, and waits for its answer.

`0x9000-0xDFFF` (settings) is never written, so an update keeps the keys and WiFi networks. The first sector of the unused SPIFFS partition is wiped, because installers before 1.1 left credentials there in plain text.

## Settings over USB

The app passes settings to `esp32flash` on stdin, never as arguments or files. `esp32flash` never prints the stick's serial output, because it can name WiFi networks.

```
> zitro-ping
< @@ZITRO READY
> provision {"provider":"openai","apiKey":"...","ssid":"...","password":"..."}
< @@PROVISION OK                  (then the stick restarts)
< @@PROVISION FAIL <kind>         (json, provider, ssid, password, key, text, unknown)
```

All or nothing, and fields left out keep what's stored. The firmware checks with the same functions as the setup page (`ai_payload.ino`). There is no command to read a key back. Details: [docs/CONFIG_HANDOVER.md](../docs/CONFIG_HANDOVER.md).

## Tests

```bash
python3 native_flasher/tests/run_tests.py    # no hardware needed
../test/host/run_tests.sh                    # firmware's settings rules
```

Testing on a real stick overwrites its firmware. Back up its settings first if they matter:

```bash
esptool.py --chip esp32 --port <port> --baud 115200 read_flash 0x9000 0x5000 nvs.bin
```

That file holds API keys and WiFi passwords in plain text: `chmod 600` it and delete it afterwards.
