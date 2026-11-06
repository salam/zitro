#!/usr/bin/env python3
"""Checks GIF playback on a connected stick: what the screen shows, pixel by pixel, against Pillow.

Serves test GIFs over HTTPS with a throwaway certificate (the stick downloads over TLS without
checking it) and drives the stick's serial test commands.

    python3 tests/gif/device_check.py /dev/cu.usbserial-XXXX
"""
import http.server
import os
import select
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import termios
import threading
import time
import tty
from functools import partial
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import run as gifs  # noqa: E402

WIDTH, HEIGHT = 240, 135
PORT = 8443
failures = 0


def check(ok, msg):
    global failures
    print(("  ok   " if ok else "  FAIL ") + msg)
    sys.stdout.flush()
    if not ok:
        failures += 1


def f32(v):
    return struct.unpack("f", struct.pack("f", v))[0]


class Stick:
    def __init__(self, path):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        tty.setraw(self.fd)
        a = termios.tcgetattr(self.fd)
        a[4] = a[5] = termios.B115200
        a[2] &= ~termios.HUPCL  # closing must not reset the stick
        a[2] |= termios.CLOCAL | termios.CREAD
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self.buf = b""
        self.log = []

    def lines(self, timeout):
        end = time.time() + timeout
        while time.time() < end:
            r, _, _ = select.select([self.fd], [], [], 0.2)
            if r:
                self.buf += os.read(self.fd, 65536)
            while b"\n" in self.buf:
                line, self.buf = self.buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").strip()
                self.log.append(text)
                yield text

    def cmd(self, line, marker, timeout=60):
        self.log.clear()
        os.write(self.fd, (line + "\n").encode())
        for text in self.lines(timeout):
            if text.startswith(marker):
                return text
        raise TimeoutError(line)

    def gif_status(self):
        s = self.cmd("printtest gif", "@@GIF")
        p = s.split()
        return {"state": p[1], "frame": int(p[3]), "shown": int(p[p.index("shown") + 1].strip(",")), "text": s}

    def shot(self):
        os.write(self.fd, b"shot\n")
        self.log.clear()
        payload, size = [], None
        for text in self.lines(120):
            if text.startswith("@@SHOT "):
                size = tuple(int(v) for v in text.split()[2:4])
                payload = []
            elif text == "@@END" and size:
                joined = "".join(payload)
                px = []
                for i in range(0, len(joined) - 7, 8):
                    px += [int(joined[i:i + 4], 16)] * int(joined[i + 4:i + 8], 16)
                return px
            elif size and text and not text.startswith("@@"):
                payload.append(text)
        raise TimeoutError("shot")

    def load(self, url):
        result = self.cmd("printimg " + url, "@@PRINTIMG", 90)
        return result.endswith("OK")

    def show(self, args=""):
        return self.cmd(("printtest show " + args).strip(), "@@PRINTTEST show", 60)


def screen(frame565, w, h, cover=False, scroll=0):
    """What the stick should show for one picture (list of RGB565 values) at this zoom and scroll."""
    sx, sy = f32(f32(WIDTH) / w), f32(f32(HEIGHT) / h)
    s = max(sx, sy) if cover else min(sx, sy)
    sw, sh = min(WIDTH, int(w * s + 0.5)), int(h * s + 0.5)
    out = [0] * (WIDTH * HEIGHT)
    xmap = [min(int(f32(f32(x) / s)), w - 1) for x in range(sw)]
    for dy in range(HEIGHT):
        y = dy + scroll
        if y >= sh:
            break
        row = min(int(f32(f32(y) / s)), h - 1)
        for x in range(sw):
            out[dy * WIDTH + x] = frame565[row * w + xmap[x]]
    return out


def same(a, b):
    """Equal, except for the corner where a low battery symbol can appear."""
    if len(a) != len(b):
        return False
    for i, (p, q) in enumerate(zip(a, b)):
        if p != q and not (i % WIDTH >= WIDTH - 28 and i // WIDTH < 14):
            return False
    return True


def references(path):
    im = Image.open(path)
    w, h = im.size
    frames = []
    for i in range(im.n_frames):
        im.seek(i)
        raw = gifs.rgb565(im)
        frames.append(list(struct.unpack(f"<{w * h}H", raw)))
    return frames, w, h


def serve(folder):
    cert = Path(folder) / "cert.pem"
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(cert), "-out", str(cert),
                    "-days", "1", "-subj", "/CN=gif-test"], check=True, capture_output=True)
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *args):
            pass

    handler = partial(Quiet, directory=str(folder))
    server = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert)
    server.socket = ctx.wrap_socket(server.socket, server_side=True)
    threading.Thread(target=server.serve_forever, daemon=True).start()


def main():
    stick = Stick(sys.argv[1])
    folder = Path(tempfile.mkdtemp(prefix="gifdevice"))
    serve(folder)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.connect(("192.168.1.1", 9))
    base = f"https://{s.getsockname()[0]}:{PORT}/"
    s.close()
    time.sleep(1)
    stick.cmd("printtext Test answer", "@@PRINTTEXT")  # "shot" redraws the picture only with an answer on hand

    all_cases = {name: data for name, data, _ in gifs.cases()}
    W, H = 40, 30

    print("Frames on screen against Pillow")
    for name in ("full-frames", "partial-keep", "disposal-background", "disposal-previous", "transparency", "local-palette", "interlaced"):
        path = folder / f"{name}.gif"
        path.write_bytes(all_cases[name])
        refs, w, h = references(path)
        wanted = [screen(f, w, h) for f in refs]
        check(stick.load(base + path.name), f"{name}: downloaded")
        stick.show()
        check(any("GIF: playing" in l for l in stick.log), f"{name}: plays")
        seen = set()
        for attempt in range(max(8, len(refs) * 3)):
            px = stick.shot()
            match = [i for i, want in enumerate(wanted) if same(px, want)]
            if not match:
                check(False, f"{name}: a shown picture matches no Pillow frame")
                Image.frombytes("RGB", (WIDTH, HEIGHT), bytes(c for v in px for c in ((v >> 8) & 0xF8, (v >> 3) & 0xFC, (v << 3) & 0xF8))).save(f"/tmp/gif-bad-{name}.png")
                break
            seen.update(match)
            time.sleep(0.2 + 0.13 * attempt)  # uneven, so shots don't keep landing on the same frame
        check(len(seen) >= min(2, len(refs)), f"{name}: every shot matched Pillow, frames seen {sorted(seen)} of {len(refs)}")

    print("Scrolling and zoom")
    path = folder / "scroll.gif"
    path.write_bytes(gifs.gif(W, H, [{"rows": gifs.pattern(W, H, 0), "delay": 200}, {"rows": gifs.pattern(W, H, 3), "delay": 200}]))
    refs, w, h = references(path)
    still = folder / "scroll-still.gif"
    still.write_bytes(gifs.gif(W, H, [{"rows": gifs.pattern(W, H, 0)}]))
    for label, p, frames in (("animated", path, refs), ("still", still, refs[:1])):
        check(stick.load(base + p.name), f"{label}: downloaded")
        for scroll in (0, 20, 45):
            result = stick.show(f"{scroll} cover")
            px = stick.shot()
            ok = any(same(px, screen(f, w, h, cover=True, scroll=scroll)) for f in frames)
            check(ok and f"scroll {scroll} of 45" in result, f"{label}: cover zoom, scrolled by {scroll}")
        stick.show()

    print("Stays a still picture")
    path = folder / "single.gif"
    path.write_bytes(gifs.gif(W, H, [{"rows": gifs.pattern(W, H)}]))
    cut = folder / "cut.gif"
    whole = gifs.gif(W, H, [{"rows": gifs.pattern(W, H, 0)}, {"rows": gifs.pattern(W, H, 3)}, {"rows": gifs.pattern(W, H, 5)}])
    cut.write_bytes(whole[:-30])
    for p, label in ((path, "one frame"), (cut, "cut off")):
        check(stick.load(base + p.name), f"{label}: downloaded")
        stick.show()
        check(any("GIF: stays still" in l for l in stick.log), f"{label}: stays still")
        check(stick.gif_status()["state"] == "idle", f"{label}: player idle")
        first = screen(references(path)[0][0], W, H)
        check(same(stick.shot(), first), f"{label}: first frame correct")

    print("Plays once when the file says so")
    path = folder / "once.gif"
    path.write_bytes(gifs.gif(W, H, [{"colour": gifs.RED, "delay": 20}, {"colour": gifs.GREEN, "delay": 20}, {"colour": gifs.BLUE, "delay": 20}], loops=None))
    check(stick.load(base + path.name), "downloaded")
    stick.show()
    time.sleep(2)
    status = stick.gif_status()
    check(status["state"] == "idle" and status["shown"] == 3, f"stopped by itself after 3 frames: {status['text'][:70]}")

    print("Memory across start and stop")
    path = folder / "fast.gif"
    path.write_bytes(gifs.gif(160, 90, [{"rows": gifs.pattern(160, 90, s), "delay": 2} for s in range(5)]))
    check(stick.load(base + path.name), "downloaded")
    stick.cmd("printtest gif stop", "@@GIF")
    free = []
    for cycle in range(5):
        stick.show()
        time.sleep(3)
        status = stick.gif_status()
        check(status["state"] == "playing" and status["shown"] > 10, f"cycle {cycle}: {status['text'][6:96]}")
        text = stick.cmd("printtest gif stop", "@@GIF")
        free.append((int(text.split("PSRAM")[1].split()[0]), int(text.split("heap")[1].split()[0])))
    print("  free PSRAM, heap after each stop:", free)
    check(len({f[0] for f in free[1:]}) == 1, "PSRAM returns to the same level after each stop")
    check(max(f[1] for f in free[1:]) - min(f[1] for f in free[1:]) < 4096, "heap stays level")

    print("Speed at the size limits")
    for size, frames in ((200, 4), (320, 4)):
        path = folder / f"noise{size}.gif"
        noisy = lambda seed: [[(x * x + y * 7 + x * y * (seed + 1) + (x ^ y)) % 8 for x in range(size)] for y in range(size)]
        path.write_bytes(gifs.gif(size, size, [{"rows": noisy(s), "delay": 2} for s in range(frames)]))
        kb = path.stat().st_size // 1024
        if not stick.load(base + path.name):
            check(False, f"{size}x{size}: download failed")
            continue
        stick.show()
        time.sleep(6)
        text = stick.cmd("printtest gif stop", "@@GIF")
        print(f"  {size}x{size} noise, {kb} KB: {text[6:90]}")

    path = folder / "over-limit.gif"
    path.write_bytes(gifs.gif(400, 400, [{"colour": gifs.RED}, {"colour": gifs.GREEN}]))
    check(stick.load(base + path.name), "400x400: downloaded")
    stick.show()
    check(any("GIF: stays still" in l for l in stick.log), "400x400 stays still (over the limit)")

    print("A real GIF from Wikimedia")
    for url in sys.argv[2:]:
        name = url.rsplit("/", 1)[1]
        local = folder / "real.gif"
        subprocess.run(["curl", "-s", "-A", "zitro-gif-test/1.0", "-o", str(local), url], check=True)
        refs, w, h = references(local)
        check(stick.load(url), f"{name} ({local.stat().st_size // 1024} KB, {w}x{h}, {len(refs)} frames): downloaded")
        stick.show()
        check(any("GIF: playing" in l for l in stick.log), "plays")
        wanted = [screen(f, w, h) for f in refs]
        seen = set()
        for attempt in range(8):
            px = stick.shot()
            match = [i for i, want in enumerate(wanted) if same(px, want)]
            if not match:
                check(False, "a shown picture matches no Pillow frame")
                break
            seen.update(match)
            time.sleep(0.15 + 0.17 * attempt)
        check(len(seen) >= 2, f"shots matched Pillow, frames seen {sorted(seen)}")
        time.sleep(3)
        print(" ", stick.gif_status()["text"][6:100])
        stick.cmd("printtest gif stop", "@@GIF")

    print("Display sleep still happens (needs 40 s)")
    path = folder / "sleep.gif"
    path.write_bytes(gifs.gif(W, H, [{"colour": gifs.RED, "delay": 10}, {"colour": gifs.GREEN, "delay": 10}]))
    check(stick.load(base + path.name), "downloaded")
    stick.show()
    slept = False
    stick.log.clear()
    for text in stick.lines(50):
        if "DISPLAY_SLEEP" in text:
            slept = True
            break
    check(slept, "display went to sleep while the GIF was playing")
    time.sleep(1)
    a = stick.gif_status()
    time.sleep(3)
    b = stick.gif_status()
    check(a["shown"] == b["shown"], f"no frames while asleep: {a['shown']} then {b['shown']}")

    print(f"\n{failures} failure(s)" if failures else "\nAll device checks passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
