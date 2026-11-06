#!/usr/bin/env python3
"""Pretends to be an IPP printer so the stick's print pages can be checked without paper.

Accepts Print-Job requests on the given port, saves each PWG raster page and writes a
downscaled PNG preview next to it. Use it with the serial commands
`printtest <layout> <zoomIndex> <this-mac-ip>:<port>` and `printtest caps <this-mac-ip>:<port>`.

    python3 tests/pwg/fake_printer.py 8631 /tmp/pages

Options act out other printers:
    --advertise 300,600   resolutions it reports ("" = reports none)
    --accept 300          resolutions it really takes (default: what it advertises, or 600)
    --types sgray_8       raster types it reports and takes ("" = reports none, takes colour and grey)
    --no-pwg              no PWG raster at all, only image/jpeg
    --hang-up             closes the connection mid-upload instead of answering, when rejecting
"""
import argparse
import http.server
import struct
import sys
import time
from pathlib import Path

from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("port", type=int, nargs="?", default=8631)
ap.add_argument("out", nargs="?", default="/tmp/pages")
ap.add_argument("--advertise", default="600")
ap.add_argument("--accept", default=None)
ap.add_argument("--types", default="srgb_8,sgray_8")
ap.add_argument("--no-pwg", action="store_true")
ap.add_argument("--hang-up", action="store_true")
ARGS = ap.parse_args()
PORT = ARGS.port
OUT = Path(ARGS.out)
ADVERTISE = [int(d) for d in ARGS.advertise.split(",") if d]
ACCEPT = [int(d) for d in ARGS.accept.split(",")] if ARGS.accept else (ADVERTISE or [600])
TYPES = [t for t in ARGS.types.split(",") if t]
PREVIEW_STEP = 8  # keep every 8th pixel and line of 600 dpi pages in the preview


def attribute(tag, name, value):
    n = name.encode()
    return bytes([tag]) + struct.pack(">H", len(n)) + n + struct.pack(">H", len(value)) + value


def attribute_set(tag, name, values):
    return b"".join(attribute(tag, name if i == 0 else "", v) for i, v in enumerate(values))


def split_ipp(body: bytes):
    """Returns (attributes dict, document bytes) of an IPP request."""
    pos = 8
    attrs = {}
    name = None
    while pos < len(body):
        tag = body[pos]
        pos += 1
        if tag == 0x03:
            return attrs, body[pos:]
        if tag < 0x10:
            continue  # group delimiter
        nlen = struct.unpack(">H", body[pos:pos + 2])[0]
        pos += 2
        if nlen:
            name = body[pos:pos + nlen].decode()
        pos += nlen
        vlen = struct.unpack(">H", body[pos:pos + 2])[0]
        pos += 2
        attrs.setdefault(name, []).append(body[pos:pos + vlen])
        pos += vlen
    raise ValueError("no end-of-attributes tag")


def decode_pwg(data: bytes) -> Image.Image:
    """Independent PWG raster decoder (8-bit sRGB or grey), returns a downscaled preview."""
    assert data[:4] == b"RaS2", data[:4]
    h = data[4:4 + 1796]
    dpi = struct.unpack(">I", h[276:280])[0]
    width, height = struct.unpack(">II", h[372:380])
    bpp, bpl, cspace = struct.unpack(">I", h[388:392])[0], struct.unpack(">I", h[392:396])[0], struct.unpack(">I", h[400:404])[0]
    assert (bpp, cspace) in ((24, 19), (8, 18)), (bpp, cspace)
    pix = bpp // 8
    assert bpl == width * pix, (bpl, width)
    pos = 4 + 1796
    step = max(1, PREVIEW_STEP * dpi // 600)  # same preview size at any resolution
    preview = Image.new("RGB", (width // step, height // step), "white")
    px = preview.load()
    y = 0
    while y < height:
        repeat = data[pos] + 1
        pos += 1
        line = bytearray()
        while len(line) < bpl:
            n = data[pos]
            pos += 1
            if n < 128:
                line += data[pos:pos + pix] * (n + 1)
                pos += pix
            else:
                count = 257 - n
                line += data[pos:pos + pix * count]
                pos += pix * count
        assert len(line) == bpl, (y, len(line))
        for r in range(repeat):
            yy = y + r
            if yy % step == 0 and yy // step < preview.height:
                for x in range(0, preview.width):
                    o = x * step * pix
                    px[x, yy // step] = (line[o], line[o + 1], line[o + 2]) if pix == 3 else (line[o],) * 3
        y += repeat
    assert pos == len(data), f"{len(data) - pos} trailing bytes"
    return preview


def page_problem(doc: bytes):
    """Why this printer refuses the page, or None."""
    h = doc[4:4 + 1796]
    dpi = struct.unpack(">I", h[276:280])[0]
    grey = struct.unpack(">I", h[400:404])[0] == 18
    if dpi not in ACCEPT:
        return f"{dpi} dpi"
    if TYPES and ("sgray_8" if grey else "srgb_8") not in TYPES:
        return "grey" if grey else "colour"
    return None


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def reply(self, status, req_id, attrs=b""):
        resp = bytes([1, 1]) + struct.pack(">H", status) + req_id + bytes([0x01])
        resp += attribute(0x47, "attributes-charset", b"utf-8") + attribute(0x48, "attributes-natural-language", b"en")
        resp += attrs + bytes([0x03])
        self.send_response(200)
        self.send_header("Content-Type", "application/ipp")
        # Answer chunked, like the Canon TS5100 does
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        self.wfile.write(b"%x\r\n" % len(resp) + resp + b"\r\n0\r\n\r\n")
        sys.stdout.flush()

    def do_POST(self):
        if "chunked" in self.headers.get("Transfer-Encoding", ""):
            print("!! chunked upload (the Canon TS5100 stops on these)")
        length = int(self.headers["Content-Length"])
        start = time.time()
        if ARGS.hang_up and length > 8192:  # read the page header only, then judge it
            first = self.rfile.read(4096)
            _, doc = split_ipp(first)
            if doc[:4] == b"RaS2" and len(doc) >= 1800 and page_problem(doc):
                print(f"hanging up on a page: {page_problem(doc)}")
                sys.stdout.flush()
                self.close_connection = True
                return
            body = first + self.rfile.read(length - len(first))
        else:
            body = self.rfile.read(length)
        secs = time.time() - start
        op = struct.unpack(">H", body[2:4])[0]
        req_id = body[4:8]

        if op == 0x000B:  # Get-Printer-Attributes
            print(f"capabilities asked: advertising {ADVERTISE} {TYPES}")
            formats = [b"image/jpeg"] if ARGS.no_pwg else [b"image/jpeg", b"image/pwg-raster"]
            attrs = bytes([0x04]) + attribute_set(0x49, "document-format-supported", formats)
            if not ARGS.no_pwg:
                if ADVERTISE:
                    res = [struct.pack(">IIB", d, d, 3) for d in ADVERTISE]
                    attrs += attribute_set(0x32, "pwg-raster-document-resolution-supported", res)
                if TYPES:
                    attrs += attribute_set(0x44, "pwg-raster-document-type-supported", [t.encode() for t in TYPES])
            self.reply(0x0000, req_id, attrs)
            return

        attrs, doc = split_ipp(body)
        fmt = attrs.get("document-format", [b"?"])[0].decode()
        media = attrs.get("media", [b"-"])[0].decode()
        print(f"received {len(body)} bytes in {secs:.1f} s ({len(body) / 1048576 / max(secs, 0.001):.2f} MB/s), "
              f"format {fmt}, media {media}")
        stamp = time.strftime("%H%M%S")
        OUT.mkdir(parents=True, exist_ok=True)
        status = 0x0000
        if fmt != "image/pwg-raster":
            path = OUT / f"file-{stamp}.bin"
            path.write_bytes(doc)
            print(f"plain file {path}, starts with {doc[:4].hex()}")
        elif ARGS.no_pwg or page_problem(doc):
            print(f"rejected: {'no PWG raster' if ARGS.no_pwg else page_problem(doc)}")
            status = 0x040A  # client-error-document-format-not-supported
        else:
            path = OUT / f"page-{stamp}.pwg"
            path.write_bytes(doc)
            try:
                preview = decode_pwg(doc)
                preview.save(path.with_suffix(".png"))
                print(f"preview {path.with_suffix('.png')}")
            except Exception as e:  # report a broken page as a client error, like a printer would
                print(f"!! decode failed: {e}")
                status = 0x0400
        self.reply(status, req_id)

    def log_message(self, *args):
        pass


print(f"fake IPP printer on port {PORT}, pages go to {OUT}")
sys.stdout.flush()
http.server.ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
