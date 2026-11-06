#!/usr/bin/env python3
"""Compares the song table, dumped by test_music --dump on stdin, with published scores in references.txt."""
import os
import re
import sys

NATURAL = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}
STEP = {"c": 0, "d": 1, "e": 2, "f": 3, "g": 4, "a": 5, "b": 6}


def pitch_name(word, lang):
    """Returns (letter, accidental) for a LilyPond pitch word."""
    if lang == "deutsch":
        if word == "h":
            return "b", 0
        if word == "b":
            return "b", -1
    if word in ("es", "ees"):
        return "e", -1
    if word in ("as", "aes"):
        return "a", -1
    letter = word[0]
    if word[1:] == "is":
        return letter, 1
    if word[1:] == "es":
        return letter, -1
    if word[1:] == "":
        return letter, 0
    raise ValueError("unknown pitch " + word)


TOKEN = re.compile(r"^(r|[a-h](?:is|es|s)?)?([',]*)(\d+)?(\.*)(~)?$")


class Reader:
    """Reads notes in written order, as LilyPond does: relative octaves and carried lengths follow the text,
    so a repeated section is read once and then copied, with the very same pitches."""

    def __init__(self, tokens, lang, start):
        self.tokens = tokens
        self.pos = 0
        self.lang = lang
        self.relative = bool(start)
        self.letter, self.octave = None, 0
        if start:
            m = TOKEN.match(start)
            self.letter = m.group(1)
            self.octave = 3 + m.group(2).count("'") - m.group(2).count(",")
        self.length = 4

    def block(self):
        if self.tokens[self.pos] != "{":
            raise ValueError("expected {")
        self.pos += 1
        events = self.sequence()
        self.pos += 1  # The closing brace
        return events

    def sequence(self):
        events = []
        while self.pos < len(self.tokens) and self.tokens[self.pos] != "}":
            tok = self.tokens[self.pos]
            if tok == "\\repeat":
                kind, times = self.tokens[self.pos + 1], int(self.tokens[self.pos + 2])
                self.pos += 3
                body = self.block()
                if kind == "unfold":
                    events += body * times
                else:
                    if self.tokens[self.pos] != "\\alternative" or self.tokens[self.pos + 1] != "{":
                        raise ValueError("volta without alternative")
                    self.pos += 2
                    while self.tokens[self.pos] != "}":
                        events += body + self.block()
                    self.pos += 1
            else:
                events.append(self.note(tok))
                self.pos += 1
        return events

    def note(self, tok):
        m = TOKEN.match(tok)
        if not m:
            raise ValueError("bad token " + tok)
        word, marks, dur, dots, tie = m.groups()
        if dur:
            if 16 % int(dur):
                raise ValueError("too short: " + tok)
            self.length = 16 // int(dur)
            add, part = 0, self.length
            for _ in dots:
                part //= 2
                add += part
            self.length += add
        if word == "r":
            return [0, self.length, False]
        if word is None:
            raise ValueError("note without a pitch: " + tok)
        name, accidental = pitch_name(word, self.lang)
        up, down = marks.count("'"), marks.count(",")
        if self.relative:
            diff = STEP[name] - STEP[self.letter]
            if diff > 3:
                self.octave -= 1
            elif diff < -3:
                self.octave += 1
            self.octave += up - down
        else:
            self.octave = 3 + up - down
        self.letter = name
        return [12 * (self.octave + 1) + NATURAL[name] + accidental, self.length, bool(tie)]


def join(events):
    """Ties joined, neighbouring rests merged, rests at the end dropped: those only fill up the last bar."""
    out = []
    for midi, ticks, tied in events:
        if out and out[-1][2]:
            if out[-1][0] != midi:
                raise ValueError("tie between different notes")
            out[-1][1] += ticks
            out[-1][2] = tied
        elif out and midi == 0 and out[-1][0] == 0:
            out[-1][1] += ticks
        else:
            out.append([midi, ticks, tied])
    while out and out[-1][0] == 0:
        out.pop()
    return [[e[0], e[1]] for e in out]


def read_score(notes, lang, start):
    tokens = [t for t in notes.replace("{", " { ").replace("}", " } ").split() if t != "|"]
    return join(Reader(tokens, lang, start).sequence())


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
