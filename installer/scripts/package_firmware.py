#!/usr/bin/env python3
"""Turn a fresh arduino-cli build into the installer's firmware package.

usage: package_firmware.py <arduino output dir> <firmware dir> <label>

The merged image from the same build is the reference: every part must sit in it at the
offset we flash it to, so a wrong offset or a stale file stops the release here.
"""
import hashlib
import json
import os
import struct
import sys

SKETCH = "sketch_jan12a.ino"
PARTS = [
    ("Bootloader", "bootloader.bin", 0x1000, f"{SKETCH}.bootloader.bin"),
    ("Partition table", "partitions.bin", 0x8000, f"{SKETCH}.partitions.bin"),
    ("Boot selector", "boot_app0.bin", 0xE000, None),
    ("Zitro app", "application.bin", 0x10000, f"{SKETCH}.bin"),
]
BOOT_APP0_SIZE = 0x2000
FLASH_SIZE = 8 * 1024 * 1024
SPIFFS_OFFSET = 0x670000


def die(message):
    sys.exit(f"Error: {message}")


def partition_table(data):
    table = {}
    for i in range(0, len(data), 32):
        entry = data[i : i + 32]
        if entry[:2] != b"\xaa\x50":
            break
        offset, size = struct.unpack("<II", entry[4:12])
        table[entry[12:28].rstrip(b"\0").decode()] = (offset, size)
    return table


def main():
    if len(sys.argv) != 4:
        die(__doc__)
    build, target, label = sys.argv[1:]
    merged_path = os.path.join(build, f"{SKETCH}.merged.bin")
    if not os.path.exists(merged_path):
        die(f"no merged image in {build}")
    merged = open(merged_path, "rb").read()
    if len(merged) != FLASH_SIZE:
        die(f"merged image is {len(merged)} bytes, expected {FLASH_SIZE}")

    # Nothing from an earlier build may survive
    os.makedirs(target, exist_ok=True)
    for name in os.listdir(target):
        if name.endswith(".bin") or name == "manifest.json":
            os.remove(os.path.join(target, name))

    parts = []
    for name, filename, offset, source in PARTS:
        if source:
            data = open(os.path.join(build, source), "rb").read()
            if merged[offset : offset + len(data)] != data:
                die(f"{source} doesn't match the merged image at {offset:#x}")
        else:
            data = merged[offset : offset + BOOT_APP0_SIZE]
        open(os.path.join(target, filename), "wb").write(data)
        parts.append(
            {
                "name": name,
                "file": filename,
                "offset": offset,
                "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
            }
        )

    table = partition_table(merged[0x8000 : 0x8000 + 0xC00])
    expected = {"nvs": (0x9000, 0x5000), "otadata": (0xE000, 0x2000), "spiffs": (SPIFFS_OFFSET, 0x180000)}
    for name, value in expected.items():
        if table.get(name) != value:
            die(f"partition {name} is {table.get(name)}, expected {value}")
    app_offset, app_size = table["app0"]
    app_bytes = parts[-1]["size"]
    if app_offset != 0x10000 or app_bytes > app_size:
        die(f"the app ({app_bytes} bytes) doesn't fit app0 {table['app0']}")
    # The settings must stay out of reach of everything we write
    nvs_end = table["nvs"][0] + table["nvs"][1]
    for part in parts:
        if part["offset"] < nvs_end and part["offset"] + part["size"] > table["nvs"][0]:
            die(f"{part['name']} would overwrite the stored settings")

    manifest = {
        "firmware": label,
        "minFlashBytes": FLASH_SIZE,
        "parts": parts,
        # Installers before 1.1 left API keys and WiFi passwords in plain text here
        "wipes": [{"name": "Old installer settings", "offset": SPIFFS_OFFSET, "size": 0x1000}],
    }
    with open(os.path.join(target, "manifest.json"), "w") as handle:
        json.dump(manifest, handle, indent=2)
        handle.write("\n")
    print(f"Packaged firmware {label}: app {app_bytes} bytes, {app_bytes * 100 // app_size}% of its partition")


if __name__ == "__main__":
    main()
