#!/usr/bin/env python3
"""Reads LilyPond melodies, as copied from published scores, into [midi, ticks] events in sixteenths.

Shared by tests/music/check_references.py and scripts/make_music_data.py.
Only plain notes and rests, ties, relative or absolute octaves, \\repeat unfold and \\repeat volta with
\\alternative. Anything else (chords, triplets, marks) raises ValueError.
"""
import re

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
