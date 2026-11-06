#!/usr/bin/env python3
"""Runs the real esp32flash binary against a simulated ESP32 on a pseudo-terminal.

The stick in front of you can't show what happens with the wrong chip or a bad write.
This can. Needs no hardware. Usage: run_tests.py [path to esp32flash]
"""
import base64
import hashlib
import json
import os
import pty
import re
import select
import struct
import subprocess
import sys
import tempfile
import threading
import time
import tty
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
FLASHER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "bin", "esp32flash")
STUB_SOURCE = os.path.join(HERE, "..", "Sources", "StubLoader.swift")

# esptool v4.7.0 stub_flasher_32.json. A single wrong byte here once crept in unnoticed.
STUB_TEXT_SHA256 = "c3d3ddfa74e6b41710863f07a2d0496bb6f758a2011bf1d9cdd5b280479db6b9"
STUB_DATA_SHA256 = "d28a40ec32f40afd656ddbc1a89b0cc042203548dfa255617b55067d80350f2f"

ESP32_MAGIC = 0x00F01D83
FLASH_SIZE = 8 * 1024 * 1024
SECRET_KEY = "sk-test-DO-NOT-LEAK-0123456789"
SECRET_PASS = "wifi-pass-DO-NOT-LEAK"


def slip(frame):
    return b"\xc0" + frame.replace(b"\xdb", b"\xdb\xdd").replace(b"\xc0", b"\xdb\xdc") + b"\xc0"


class FakeESP32(threading.Thread):
    """ROM loader, stub and running firmware, as far as the flasher can tell."""

    def __init__(self, master, magic=ESP32_MAGIC, capacity=0x17, flip_byte_times=0, silent=False,
                 provision_reply=b"@@PROVISION OK", firmware_answers=True):
        super().__init__(daemon=True)
        self.master = master
        self.magic = magic
        self.capacity = capacity
        self.flip_byte_times = flip_byte_times
        self.silent = silent
        self.provision_reply = provision_reply
        self.firmware_answers = firmware_answers
        self.flash = bytearray(b"\xff" * FLASH_SIZE)
        self.flash[0x9000:0xE000] = b"NVS!" * (0x5000 // 4)
        self.regs = {}
        self.stub = False
        self.writes = 0
        self.provisioned = None
        self.inflater = None
        self.offset = 0
        self.running = True
        self.raw = bytearray()
        self.frame = None
        self.esc = False

    def run(self):
        while self.running:
            ready, _, _ = select.select([self.master], [], [], 0.05)
            if not ready:
                continue
            try:
                data = os.read(self.master, 65536)
            except OSError:
                time.sleep(0.01)
                continue
            if not self.silent:
                self.feed(data)

    def feed(self, data):
        for byte in data:
            if self.frame is None:
                if byte == 0xC0:
                    self.frame = bytearray()
                    self.raw.clear()
                else:
                    self.raw.append(byte)
                    if byte == 0x0A:
                        self.line(bytes(self.raw[:-1]))
                        self.raw.clear()
                continue
            if self.esc:
                self.frame.append(0xC0 if byte == 0xDC else 0xDB)
                self.esc = False
            elif byte == 0xDB:
                self.esc = True
            elif byte == 0xC0:
                frame, self.frame = bytes(self.frame), None
                if frame:
                    self.command(frame)
                else:
                    self.frame = bytearray()
            else:
                self.frame.append(byte)

    def line(self, line):
        if not self.firmware_answers:
            return
        if line == b"zitro-ping":
            os.write(self.master, b"some boot log\r\n@@ZITRO READY\r\n")
        elif line.startswith(b"provision "):
            self.provisioned = line[10:]
            os.write(self.master, self.provision_reply + b"\r\n")

    def reply(self, op, value=0, data=b""):
        status = b"\0\0" if self.stub else b"\0\0\0\0"
        body = data + status
        os.write(self.master, slip(struct.pack("<BBHI", 1, op, len(body), value) + body))

    def command(self, frame):
        _, op, size, _ = struct.unpack("<BBHI", frame[:8])
        data = frame[8 : 8 + size]
        if op == 0x08:
            # Only the ROM loader answers sync: the reset line this can't see has brought it back
            self.stub = False
            for _ in range(8):
                self.reply(op, 0x07071220)
        elif op == 0x0A:
            address = struct.unpack("<I", data[:4])[0]
            value = self.magic if address == 0x40001000 else self.regs.get(address, 0)
            self.reply(op, value)
        elif op == 0x09:
            address, value = struct.unpack("<II", data[:8])
            if address == 0x3FF42000:
                self.regs[0x3FF42080] = (self.capacity << 16) | 0x4020
                value = 0
            self.regs[address] = value
            self.reply(op)
        elif op in (0x05, 0x07, 0x0B, 0x0D, 0x02):
            self.reply(op)
        elif op == 0x06:
            self.reply(op)
            os.write(self.master, b"OHAI")
            self.stub = True
        elif op == 0x0F:
            self.reply(op)
        elif op == 0x10:
            _, _, _, self.offset = struct.unpack("<IIII", data[:16])
            self.inflater = zlib.decompressobj()
            self.reply(op)
        elif op == 0x11:
            length = struct.unpack("<I", data[:4])[0]
            out = self.inflater.decompress(data[16 : 16 + length])
            self.flash[self.offset : self.offset + len(out)] = out
            self.offset += len(out)
            self.writes += 1
            self.reply(op)
        elif op == 0x13:
            offset, length = struct.unpack("<II", data[:8])
            digest = bytearray(hashlib.md5(self.flash[offset : offset + length]).digest())
            if self.flip_byte_times > 0:
                self.flip_byte_times -= 1
                digest[0] ^= 0xFF
            self.reply(op, 0, bytes(digest))
        elif op == 0x12:
            self.reply(op)
        else:
            self.reply(op)


def make_package(directory):
    parts = []
    for name, offset, size in [("bootloader.bin", 0x1000, 25184), ("partitions.bin", 0x8000, 3072),
                               ("boot_app0.bin", 0xE000, 8192), ("application.bin", 0x10000, 400000)]:
        # Both compressible and random-looking content, with bytes SLIP has to escape
        data = (hashlib.sha256(name.encode()).digest() * (size // 32 + 1))[: size // 2] + os.urandom(size - size // 2)
        with open(os.path.join(directory, name), "wb") as handle:
            handle.write(data)
        parts.append({"name": name, "file": name, "offset": offset, "size": size,
                      "sha256": hashlib.sha256(data).hexdigest()})
    manifest = {"firmware": "test", "minFlashBytes": FLASH_SIZE, "parts": parts,
                "wipes": [{"name": "Old installer settings", "offset": 0x670000, "size": 0x1000}]}
    with open(os.path.join(directory, "manifest.json"), "w") as handle:
        json.dump(manifest, handle)
    return manifest


def run_flasher(fake_options, args, stdin=b""):
    master, slave = pty.openpty()
    tty.setraw(master)
    port = os.ttyname(slave)
    fake = FakeESP32(master, **fake_options)
    fake.start()
    try:
        done = subprocess.run([FLASHER, "--port", port] + args, input=stdin, capture_output=True, timeout=120)
    finally:
        fake.running = False
        fake.join(1)
        os.close(slave)
        os.close(master)
    return done, fake


results = []


def check(name, condition, detail=""):
    results.append(condition)
    print(f"  {'ok  ' if condition else 'FAIL'} {name}{'' if condition else ': ' + str(detail)}")


def error_code(done):
    match = re.search(rb"@@ERROR (\S+)", done.stdout)
    return match.group(1).decode() if match else None


def main():
    print("Embedded stub")
    source = open(STUB_SOURCE).read()
    text = base64.b64decode(re.search(r'textBase64 = "([^"]+)"', source).group(1))
    data = base64.b64decode(re.search(r'dataBase64 = "([^"]+)"', source).group(1))
    check("is esptool's, byte for byte", hashlib.sha256(text).hexdigest() == STUB_TEXT_SHA256
          and hashlib.sha256(data).hexdigest() == STUB_DATA_SHA256)

    with tempfile.TemporaryDirectory() as package:
        manifest = make_package(package)
        install = ["--install", package, "--baud", "115200"]

        print("Normal install")
        done, fake = run_flasher({}, install)
        check("succeeds", done.returncode == 0, done.stdout[-300:])
        for part in manifest["parts"]:
            expected = open(os.path.join(package, part["file"]), "rb").read()
            actual = bytes(fake.flash[part["offset"] : part["offset"] + part["size"]])
            check(f"wrote {part['file']} at {part['offset']:#x}", actual == expected)
        check("left the stored settings alone", fake.flash[0x9000:0xE000] == b"NVS!" * (0x5000 // 4))
        progress = [int(x) for x in re.findall(rb"@@PROGRESS (\d+)", done.stdout)]
        check("progress only moves forward, to 100", progress == sorted(progress) and progress[-1] == 100)
        check("sent nothing to the firmware", fake.provisioned is None)

        print("Old installer's leftovers")
        done, fake = run_flasher({}, install)
        check("wiped the sector with the old plain-text config", set(fake.flash[0x670000:0x671000]) == {0xFF})

        print("Settings")
        settings = json.dumps({"provider": "openai", "apiKey": SECRET_KEY, "ssid": "Café Wifi", "password": SECRET_PASS}).encode()
        done, fake = run_flasher({}, install + ["--provision"], settings)
        check("succeeds", done.returncode == 0, done.stdout[-300:])
        check("reached the firmware intact", json.loads(fake.provisioned) == json.loads(settings))
        everything = done.stdout + done.stderr
        check("no secret in the flasher's output", SECRET_KEY.encode() not in everything and SECRET_PASS.encode() not in everything)
        check("no device log in the flasher's output", b"some boot log" not in everything)

        done, fake = run_flasher({"provision_reply": b"@@PROVISION FAIL password"}, install + ["--provision"], settings)
        check("rejection: exit 3 and its own code", done.returncode == 3 and error_code(done) == "settings-rejected", done.stdout[-200:])
        check("rejection names the field", b"WiFi password" in done.stdout)
        check("rejection: firmware is still reported as written", b"@@PROGRESS 94" in done.stdout)

        done, fake = run_flasher({}, ["--provision"], b"not json")
        check("bad settings stop before the port is used", done.returncode == 3 and fake.writes == 0)

        print("Wrong device: nothing may be written")
        done, fake = run_flasher({"magic": 0x00000009}, install)
        check("other chip refused", error_code(done) == "wrong-board" and fake.writes == 0, done.stdout[-200:])
        check("  and flash is untouched", set(fake.flash[0x10000:0x20000]) == {0xFF})
        done, fake = run_flasher({"capacity": 0x16}, install)
        check("4 MB flash refused", error_code(done) == "wrong-board" and fake.writes == 0, done.stdout[-200:])
        done, fake = run_flasher({"capacity": 0x00}, install)
        check("unreadable flash refused", error_code(done) == "wrong-board" and fake.writes == 0, done.stdout[-200:])

        print("Failures")
        done, fake = run_flasher({"flip_byte_times": 99}, install)
        check("checksum mismatch is an error, never success", done.returncode == 1 and error_code(done) == "verify-failed", done.stdout[-200:])
        done, fake = run_flasher({"flip_byte_times": 1}, ["--install", package])
        check("fast write fails once: retried slowly, then succeeds", done.returncode == 0 and b"more slowly" in done.stdout, done.stdout[-300:])
        done, fake = run_flasher({"silent": True}, install)
        check("silent device: clear error", error_code(done) == "no-answer", done.stdout[-200:])

        print("Damaged firmware package: refused before touching the stick")
        path = os.path.join(package, "application.bin")
        original = open(path, "rb").read()
        with open(path, "wb") as handle:
            handle.write(original[:-1] + bytes([original[-1] ^ 1]))
        done, fake = run_flasher({}, install)
        check("one flipped bit", error_code(done) == "firmware-corrupt" and fake.writes == 0 and not fake.regs, done.stdout[-200:])
        with open(path, "wb") as handle:
            handle.write(original[:-16])
        done, fake = run_flasher({}, install)
        check("truncated file", error_code(done) == "firmware-corrupt" and fake.writes == 0)
        os.remove(path)
        done, fake = run_flasher({}, install)
        check("missing file", error_code(done) == "firmware-corrupt" and fake.writes == 0)

    print("Secret scanner")
    scanner = os.path.join(HERE, "..", "..", "scripts", "scan_secrets.py")
    with tempfile.TemporaryDirectory() as folder:
        secret = "sk-proj-" + "a1B2" * 20
        with open(os.path.join(folder, "firmware.bin"), "wb") as handle:
            handle.write(b"\0\0" + secret.encode() + b"\0")
        done = subprocess.run([sys.executable, scanner, folder], capture_output=True)
        check("finds a key inside a binary", done.returncode == 1)
        check("doesn't print the key", secret.encode() not in done.stdout)
        with open(os.path.join(folder, "firmware.bin"), "wb") as handle:
            handle.write(b"-----BEGIN PRIVATE KEY-----\n" + b"MIIEvQIBADANBgkqhkiG9w0BAQEFAASC" * 3)
        check("finds a private key", subprocess.run([sys.executable, scanner, folder], capture_output=True).returncode == 1)
        with open(os.path.join(folder, "firmware.bin"), "wb") as handle:
            handle.write(b"\0-----BEGIN PRIVATE KEY-----\0-----END PRIVATE KEY-----\0")
        check("ignores a TLS library's bare header text", subprocess.run([sys.executable, scanner, folder], capture_output=True).returncode == 0)

    failed = results.count(False)
    print(f"{len(results)} checks, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
