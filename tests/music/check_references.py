#!/usr/bin/env python3
"""Compares the song table, dumped by test_music --dump on stdin, with published scores in references.txt."""
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
from music_lily import join, read_score  # noqa: E402


def read_references(path):
    songs, current = {}, None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            key, _, value = line.partition(" ")
            if key == "song":
                current = songs.setdefault(value, {"start": None})
            elif key == "relative":
                current["start"] = value
            elif key == "absolute":
                current["start"] = None
            else:
                current[key] = value
    return songs


def read_dump(stream):
    songs = {}
    for line in stream:
        parts = line.split()
        events = []
        for part in parts[1:]:
            midi, ticks, tied = part.split("/")
            events.append([int(midi), int(ticks), tied == "1"])
        songs[parts[0]] = join(events)
    return songs


def main():
    here = os.path.dirname(os.path.abspath(sys.argv[0]))
    references = read_references(os.path.join(here, "references.txt"))
    table = read_dump(sys.stdin)
    failures = 0
    if set(references) != set(table):
        print("  songs without both a table entry and a reference:", sorted(set(references) ^ set(table)))
        failures += 1
    for song in sorted(set(references) & set(table)):
        ref = references[song]
        score = read_score(ref["notes"], ref["lang"], ref["start"])
        if score != table[song]:
            failures += 1
            first = next((i for i, (a, b) in enumerate(zip(score, table[song])) if a != b), min(len(score), len(table[song])))
            print(f"  {song}: differs at event {first}: score {score[first:first + 4]} table {table[song][first:first + 4]}")
    print(f"music references: {len(table)} songs compared, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
