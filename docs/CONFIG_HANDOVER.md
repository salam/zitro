# Configuration handover from installer to stick

The installer sends settings to the running firmware over USB. The firmware validates them and stores them in Preferences, the same place the setup page uses.

An earlier design wrote a `config.json` to the SPIFFS partition for the firmware to read on boot. It was never implemented in the firmware, so those settings had no effect, and it left credentials in plain text in flash and in `/tmp` on the Mac. Installer 1.1 replaced it and wipes that sector.

## Why USB and not a flash image

- Settings that are already stored stay. A generated settings partition would replace them all.
- One set of rules: the setup page and the installer call the same validators.
- The installer gets an answer, so it can say honestly whether the settings arrived.
- No second copy of a secret stays behind anywhere.

## Protocol

Lines at 115200 baud, handled by `handleSerialCommands()`. See [provisioning.ino](../sketch/sketch_jan12a/provisioning.ino).

| Sent | Answer |
|---|---|
| `zitro-ping` | `@@ZITRO READY` |
| `provision <one-line JSON>` | `@@PROVISION OK`, then a restart |
| | `@@PROVISION FAIL <kind>`, nothing stored |

`<kind>` is one of `json`, `provider`, `ssid`, `password`, `key`, `text`, `unknown`.

## Fields

All optional and all strings. A field that's left out keeps its stored value.

| Field | Rule |
|---|---|
| `provider` | `zitro`, `openai`, `anthropic`, `infomaniak`, `custom` |
| `apiKey`, `claudeKey`, `ikKey`, `customKey`, `zitroKey` | 8 to 300 bytes, no spaces |
| `oaModel`, `claudeModel`, `ikModel`, `ikProduct`, `customModel` | At most 200 bytes |
| `customUrl` | At most 200 bytes, empty or `https://` |
| `ssid` | 1 to 32 bytes |
| `password` | Needs `ssid`. Empty (open network) or 8 to 63 bytes |

## Security

- The channel is write-only: no serial command reads a key back, and answers never contain a value.
- It needs physical USB access, which already allows replacing the whole firmware.
- The Mac side keeps secrets in memory only and passes them to the flasher through a pipe.
