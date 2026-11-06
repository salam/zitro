#!/usr/bin/env python3
"""Capture Zitro screens from the stick over USB serial and save them as PNGs.

Sends a command to the firmware (see sketch/sketch_jan12a/screenshot_tour.ino), reads the
run-length encoded RGB565 frames it streams back, and writes one PNG per screen.
Standard library + Pillow only (no pyserial).

Usage:
  python3 scripts/capture_screens.py /dev/cu.usbserial-XXXX [tour|shot] [out_dir] [--scale N]
"""
import os
import select
import sys
import termios
import time
import tty
from pathlib import Path

from PIL import Image


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    attrs[4] = attrs[5] = termios.B115200
    attrs[2] &= ~termios.HUPCL  # keep DTR/RTS so closing doesn't reset the stick
    attrs[2] |= termios.CLOCAL | termios.CREAD
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_lines(fd, until, timeout):
    buf = b""
    deadline = time.time() + timeout
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.5)
        if not r:
            continue
        chunk = os.read(fd, 65536)
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode("utf-8", "replace").strip()
            yield text
            if until(text):
                return
    raise TimeoutError("serial timeout")


def rgb565(c):
    r = (c >> 11) & 0x1F
    g = (c >> 5) & 0x3F
    b = c & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def decode(name, w, h, payload, out_dir, scale):
    pixels = []
    for i in range(0, len(payload) - 7, 8):
        color = int(payload[i:i + 4], 16)
        run = int(payload[i + 4:i + 8], 16)
        pixels.extend([rgb565(color)] * run)
    if len(pixels) != w * h:
        print(f"  {name}: expected {w * h} pixels, got {len(pixels)} (skipped)")
        return None
    img = Image.new("RGB", (w, h))
    img.putdata(pixels)
    if scale > 1:
        img = img.resize((w * scale, h * scale), Image.NEAREST)
    path = out_dir / f"{name}.png"
    img.save(path)
    print(f"  saved {path}")
    return path


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    scale = 2
    if "--scale" in sys.argv:
        scale = int(sys.argv[sys.argv.index("--scale") + 1])
        args = [a for a in args if a != str(scale)]
    port = args[0]
    command = args[1] if len(args) > 1 else "tour"
    out_dir = Path(args[2] if len(args) > 2 else "screenshots/states")
    out_dir.mkdir(parents=True, exist_ok=True)

    fd = open_port(port)
    time.sleep(0.5)
    os.write(fd, (command + "\n").encode())

    done_marker = "@@TOUR END" if command == "tour" else "@@END"
    current = None
    payload = []
    saved = []
    for line in read_lines(fd, lambda t: t == done_marker, timeout=300):
        if line.startswith("@@SHOT "):
            _, name, w, h = line.split()
            current = (name, int(w), int(h))
            payload = []
        elif line == "@@END" and current:
            path = decode(*current, "".join(payload), out_dir, scale)
            if path:
                saved.append(path)
            current = None
        elif current and line and not line.startswith("@@"):
            payload.append(line)
    os.close(fd)
    print(f"{len(saved)} screenshots in {out_dir}")


if __name__ == "__main__":
    main()
