#!/usr/bin/env python3
"""Make the boot splash jingle with ElevenLabs and embed it in the firmware.

generate  asks ElevenLabs for one take per prompt and saves them to a folder to listen to.
          Costs ElevenLabs credits. The key comes from ELEVENLABS_API_KEY, or from the
          gitignored .env in the repo root.
build     turns one take into sketch/sketch_jan12a/splash_jingle_data.h. Also keeps the take
          in test/ and writes test/splash_jingle.wav, which is what the stick will play.

Usage: python3 scripts/make_splash_jingle.py generate OUT_DIR [PROMPT_NAME ...]
       python3 scripts/make_splash_jingle.py build TAKE_FILE

Needs ffmpeg.
"""
import argparse
import array
import json
import os
import shutil
import ssl
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "sketch" / "sketch_jan12a" / "splash_jingle_data.h"
TEST_DIR = ROOT / "test"

API_URL = "https://api.elevenlabs.io/v1/sound-generation"
MAC_CERTS = Path("/etc/ssl/cert.pem")  # python.org builds of Python ship without root certificates
SAMPLE_RATE = 16000  # Same as the alert sound
GENERATE_SECONDS = 1.0
MAX_SECONDS = 1.9  # Has to end before the 2 s splash does
FADE_OUT_SECONDS = 0.15
PEAK = 0.9  # Short pops are much quieter than their peak, and stay well under the timer alarm

# Two tones from real things instead of an instrument preset, so it doesn't sound like every other
# startup chime. The stick's small speaker can't play bass, so these stay in the mid range.
PROMPTS = {
    # Before building, crop to the two pops (0.31 to 0.90 s) and raise the quieter first one
    "cheek": (
        "Two playful cheek pops made with a finger in the mouth: pop then pop, the second one higher. "
        "Real close-up human mouth sound, funny and warm. Nothing else, no music, no words."
    ),
    # Crop to the two rising bloops (about 0.10 to 0.29 s in the take we used) before building
    "drops": (
        "Exactly two clean round water drops falling into a deep glass of lemonade: bloop then blip, "
        "the second one higher, with clear rising pitch. No splash, no fizz, nothing else."
    ),
}


def read_api_key():
    key = os.environ.get("ELEVENLABS_API_KEY", "").strip()
    env_file = ROOT / ".env"
    if not key and env_file.exists():
        for line in env_file.read_text().splitlines():
            name, _, value = line.strip().removeprefix("export ").partition("=")
            if name.strip() == "ELEVENLABS_API_KEY":
                key = value.strip().strip("'\"")
    if not key:
        sys.exit("No ElevenLabs key: set ELEVENLABS_API_KEY or add it to .env in the repo root")
    return key


def generate(out_dir, names):
    key = read_api_key()
    out_dir.mkdir(parents=True, exist_ok=True)
    tls = ssl.create_default_context(cafile=MAC_CERTS if MAC_CERTS.exists() else None)
    for name in names or PROMPTS:
        prompt = PROMPTS[name]
        request = urllib.request.Request(
            API_URL,
            data=json.dumps({
                "text": prompt,
                "duration_seconds": GENERATE_SECONDS,
                "prompt_influence": 0.7,
            }).encode(),
            headers={"xi-api-key": key, "Content-Type": "application/json"},
        )
        try:
            with urllib.request.urlopen(request, timeout=120, context=tls) as response:
                audio = response.read()
        except urllib.error.HTTPError as error:
            sys.exit(f"ElevenLabs said {error.code} for '{name}': {error.read().decode()[:300]}")
        path = out_dir / f"{name}.mp3"
        path.write_bytes(audio)
        print(f"{path} ({len(audio)} bytes)")


def decode(take):
    """Mono 16-bit samples at SAMPLE_RATE: starts on the first sound, cleaned up for the small speaker."""
    filters = ",".join([
        "silenceremove=start_periods=1:start_threshold=-45dB",
        "highpass=f=300",  # Nothing below this comes out of the speaker anyway
        f"atrim=end={MAX_SECONDS}",
    ])
    raw = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", str(take), "-af", filters, "-ac", "1",
         "-ar", str(SAMPLE_RATE), "-f", "s16le", "-acodec", "pcm_s16le", "-"],
        check=True, capture_output=True,
    ).stdout
    samples = array.array("h", raw)
    if sys.byteorder == "big":
        samples.byteswap()
    if not samples:
        sys.exit(f"{take} has no sound in it")
    return samples


def shape(samples):
    """Fade the ends so the speaker doesn't click, then bring the peak up to PEAK."""
    fade_in = int(SAMPLE_RATE * 0.005)
    fade_out = int(SAMPLE_RATE * FADE_OUT_SECONDS)
    count = len(samples)
    gains = [
        min(1.0, i / fade_in, (count - 1 - i) / fade_out)
        for i in range(count)
    ]
    faded = [s * g for s, g in zip(samples, gains)]
    peak = max(abs(s) for s in faded)
    scale = PEAK * 32767 / peak if peak else 0
    return array.array("h", (round(s * scale) for s in faded))


def write_header(samples, take_name):
    rows = [
        "  " + ", ".join(str(s) for s in samples[i:i + 16]) + ","
        for i in range(0, len(samples), 16)
    ]
    HEADER.write_text(
        "#pragma once\n\n"
        f"// Boot splash jingle, made with ElevenLabs sound effects from test/{take_name}\n"
        "// Made by scripts/make_splash_jingle.py\n"
        f"// 16-bit PCM mono, {SAMPLE_RATE} Hz, {len(samples)} samples, {len(samples) * 2} bytes\n"
        f"// Duration: {len(samples) / SAMPLE_RATE:.2f} seconds\n\n"
        f"const int SPLASH_JINGLE_SAMPLES = {len(samples)};\n"
        f"const int SPLASH_JINGLE_SAMPLE_RATE = {SAMPLE_RATE};\n\n"
        f"const int16_t splashJingleData[{len(samples)}] PROGMEM = {{\n"
        + "\n".join(rows) + "\n};\n"
    )


def build(take):
    samples = shape(decode(take))
    kept = TEST_DIR / f"splash_jingle_source{take.suffix}"
    if take.resolve() != kept.resolve():
        shutil.copyfile(take, kept)
    write_header(samples, kept.name)
    preview = TEST_DIR / "splash_jingle.wav"
    subprocess.run(
        ["ffmpeg", "-v", "error", "-y", "-f", "s16le", "-ar", str(SAMPLE_RATE), "-ac", "1",
         "-i", "-", str(preview)],
        input=samples.tobytes(), check=True,
    )
    print(f"{HEADER}: {len(samples)} samples, {len(samples) / SAMPLE_RATE:.2f} s")
    print(f"Listen to it: afplay {preview}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)
    generate_parser = commands.add_parser("generate")
    generate_parser.add_argument("out_dir", type=Path)
    generate_parser.add_argument("names", nargs="*", choices=[*PROMPTS, []], metavar="NAME")
    commands.add_parser("build").add_argument("take", type=Path)
    args = parser.parse_args()
    if args.command == "generate":
        generate(args.out_dir, args.names)
    else:
        build(args.take)


if __name__ == "__main__":
    main()
