#!/usr/bin/env python3
"""Builds webpage2/data/music/ from the hand-kept song files in scripts/music_library/*.song.

Writes index.txt, one <id>.txt per song and SOURCES.txt, in the formats music_core.h reads, and an .htaccess
that serves them as UTF-8. A song goes on
the server only with its legal evidence (who wrote the melody, when they died, where it was first printed,
each confirmed by sources) and only if its notes, read from score A, agree note by note with an independent
score B. The format is in scripts/music_library/README.md.

Every check names the song it fails for. One failed check stops the whole run, and nothing is written.

Usage: python3 -I scripts/make_music_data.py [--library DIR] [--out DIR]
"""
import argparse
import difflib
import os
import re
import sys
from fractions import Fraction
from urllib.parse import urlsplit

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from music_lily import join, read_score  # noqa: E402

# As in music_core.h; tests/music/test_make_music_data.py checks they agree
MAX_NOTES = 1000
INDEX_LIMIT = 64 * 1024
FILE_LIMIT = 16 * 1024
MAX_INDEX_ENTRIES = 2000
# Field sizes of MusicIndexEntry, terminator included
ID_SIZE, NAME_SIZE, SOURCE_SIZE = 40, 48, 40
NORMALISED_SIZE = 96  # The stick's buffer for a normalised title

LICENCES = ("pre-1931 print", "public domain", "CC0", "CC BY")
DIED_BEFORE = 1926
PRINTED_BEFORE = 1931
SCORE_LANGS = ("nederlands", "deutsch", "english")
RESERVED_IDS = ("index", "sources")  # index.txt and SOURCES.txt, on a file system that ignores case
# The server's default charset isn't UTF-8, so SOURCES.txt would show its umlauts and accents garbled in a browser
HTACCESS = "AddDefaultCharset UTF-8\n"
NOT_SONGS = ("index.txt", "SOURCES.txt", ".htaccess")

REQUIRED = ("id", "de", "en", "tempo", "bar", "pickup", "notes", "source-a", "score-b", "footer")
SINGLE = REQUIRED + ("aliases", "published", "settled")
REPEATED = ("composer", "traditional")


# --- Names, as eduNormaliseName(..., forTitles=true) in name_match.h ---

def normalise_bytes(raw, capacity=NORMALISED_SIZE):
    """Byte for byte the C function, so a name matches here exactly when it matches on the stick.
    None where the C version returns false: a character it can't fold, nothing left, or too long."""
    out = []

    def at(k):
        return raw[k] if k < len(raw) else 0

    def put(c):
        if len(out) + 1 >= capacity:
            return False
        out.append(c)
        return True

    def space():
        return not (out and out[-1] != " ") or put(" ")

    i = 0
    while i < len(raw):
        c = raw[i]
        if c == 0:
            break
        if c == 0xC3:
            i += 1
            d = at(i)
            spelled = {0xA4: "ae", 0x84: "ae", 0xB6: "oe", 0x96: "oe", 0xBC: "ue", 0x9C: "ue", 0x9F: "ss"}.get(d)
            if spelled:
                if not put(spelled[0]) or not put(spelled[1]):
                    return None
            else:
                low = d + 0x20 if 0x80 <= d <= 0x9F else d
                base = None
                if 0xA0 <= low <= 0xA5:
                    base = "a"
                elif low == 0xA7:
                    base = "c"
                elif 0xA8 <= low <= 0xAB:
                    base = "e"
                elif 0xAC <= low <= 0xAF:
                    base = "i"
                elif low == 0xB1:
                    base = "n"
                elif 0xB2 <= low <= 0xB5:
                    base = "o"
                elif 0xB9 <= low <= 0xBB:
                    base = "u"
                if not base or not put(base):
                    return None
        elif c == 0x27:
            pass
        elif c == 0xE2 and at(i + 1) == 0x80 and at(i + 2) == 0x99:
            i += 2
        elif (c == 0xC2 and at(i + 1) in (0xAB, 0xBB)) or (
                c == 0xE2 and at(i + 1) == 0x80 and at(i + 2) in (0x93, 0x94, 0x98, 0x9C, 0x9D, 0x9E)):
            i += 1 if c == 0xC2 else 2
            if not space():
                return None
        elif c in (0x20, 0x2D, 0x5F, 0x09) or (c < 0x7F and not chr(c).isalnum()):
            if not space():
                return None
        elif c < 0x20 or c >= 0x7F:
            return None
        else:
            if not put(chr(c).lower()):
                return None
        i += 1
    text = "".join(out).rstrip(" ")
    return text or None


def normalise(text):
    return normalise_bytes(text.encode("utf-8"))


# --- Notes, as musicParseNotes and musicSongIsSound in music_core.h ---

class Note:
    __slots__ = ("start", "ticks", "midi", "staff_step", "accidental", "letter", "tied")

    def __init__(self, start, ticks, midi, staff_step, accidental, letter, tied):
        self.start, self.ticks, self.midi = start, ticks, midi
        self.staff_step, self.accidental, self.letter, self.tied = staff_step, accidental, letter, tied


LETTERS = "CDEFGAB"
SEMITONES = (0, 2, 4, 5, 7, 9, 11)
ALLOWED_LENGTHS = (1, 2, 3, 4, 6, 8, 12, 16)


def note_name(note):
    if not note.midi:
        return "R"
    return LETTERS[note.letter] + {1: "#", -1: "b", 0: ""}[note.accidental] + str(note.midi // 12 - 1)


def parse_notes(text, capacity=MAX_NOTES):
    """Returns (notes, total ticks). Raises ValueError where the firmware would refuse the whole text."""
    notes = []
    tick = 0
    p = 0
    n = len(text)

    def fail(why):
        raise ValueError(f"{why} at character {p + 1}")

    while p < n:
        if text[p] in " |":
            p += 1
            continue
        if len(notes) >= capacity:
            fail(f"more than {capacity} notes")
        midi = letter = accidental = staff = 0
        if text[p] == "R":
            p += 1
        else:
            if text[p] not in LETTERS:
                fail(f"'{text[p]}' is no note")
            letter = LETTERS.index(text[p])
            p += 1
            if p < n and text[p] == "#":
                accidental = 1
                p += 1
            elif p < n and text[p] == "b":
                accidental = -1
                p += 1
            if p >= n or text[p] not in "23456":
                fail("octave must be 2 to 6")
            octave = int(text[p])
            p += 1
            midi = 12 * (octave + 1) + SEMITONES[letter] + accidental
            staff = octave * 7 + letter - (4 * 7 + 2)
        length = 0
        if p < n and text[p] == ":":
            p += 1
            digits = 0
            while p < n and text[p] in "0123456789" and digits < 3:
                length = length * 10 + int(text[p])
                p += 1
                digits += 1
        if length not in ALLOWED_LENGTHS:
            fail("length must be one of " + ", ".join(map(str, ALLOWED_LENGTHS)))
        tied = False
        if p < n and text[p] == "~":
            if midi == 0:
                fail("a rest can't be tied")
            tied = True
            p += 1
        if p < n and text[p] not in " |":
            fail(f"'{text[p]}' after a note")
        if notes and notes[-1].tied and notes[-1].midi != midi:
            fail("a tie joins different notes")
        if tick + length > 60000:
            fail("too long")
        notes.append(Note(tick, length, midi, staff, accidental, letter, tied))
        tick += length
    if not notes:
        raise ValueError("no notes")
    if notes[-1].tied:
        raise ValueError("the last note is tied")
    return notes, tick


def sound_problem(notes, total, bar, pickup, tempo):
    """None if the song passes musicSongIsSound, else why not."""
    if len(notes) <= 8 or len(notes) > MAX_NOTES:
        return f"{len(notes)} notes: needs 9 to {MAX_NOTES}"
    if not 60 <= tempo <= 160:
        return f"tempo {tempo} must be 60 to 160"
    if bar not in (8, 12, 16):
        return f"bar {bar} must be 8, 12 or 16"
    if not 0 <= pickup < bar:
        return f"pickup {pickup} must be 0 to {bar - 1}"
    if total % bar:
        return f"bars don't add up: {total} sixteenths with the lead-in is not a whole number of {bar}-sixteenth bars"
    sounding = 0
    for i, note in enumerate(notes):
        if note.midi:
            sounding += 1
            if not (-4 <= note.staff_step <= 10 and 57 <= note.midi <= 81):
                return (f"note {i + 1} ({note_name(note)}, MIDI {note.midi}) is outside what the band can draw, "
                        "A3 to A5")
        after = int(math_fmod(note.start + note.ticks - pickup, bar))
        into = (note.start - pickup + bar * 4) % bar
        if not (after == 0 or into + note.ticks <= bar):
            return f"note {i + 1} ({note_name(note)}) runs over a bar line; a tie has to say so"
    if sounding <= 8:
        return f"only {sounding} sounding notes: needs more than 8"
    return None


def math_fmod(a, b):
    """C's % on ints: the sign follows a."""
    r = abs(a) % abs(b)
    return -r if a < 0 else r


# --- Built-in songs, read from music_songs.h ---

LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
C_TOKEN = re.compile(r'\s+|//[^\n]*|/\*.*?\*/|"(?:[^"\\]|\\.)*"|nullptr|\d+|[{},]', re.S)


def c_string(body):
    """Decodes a C string literal's body to bytes, \\x hex escapes greedy as in C."""
    out = bytearray()
    i = 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out += c.encode("utf-8")
            i += 1
            continue
        e = body[i + 1]
        if e == "x":
            j = i + 2
            while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                j += 1
            value = int(body[i + 2:j], 16)
            if value > 0xFF:
                raise ValueError("hex escape out of range: " + body[i:j])
            out.append(value)
            i = j
        elif e in "\\\"'":
            out.append(ord(e))
            i += 2
        else:
            raise ValueError("unexpected escape \\" + e)
    return bytes(out)


def read_built_ins(path=None):
    """[{id, de, en, tempo, bar, pickup, notes, aliases}] from MUSIC_SONGS[] in music_songs.h."""
    path = path or os.path.join(HERE, "..", "sketch", "sketch_jan12a", "music_songs.h")
    with open(path, encoding="utf-8") as f:
        text = f.read()
    start = text.index("MUSIC_SONGS[] = {") + len("MUSIC_SONGS[] = {")
    end = text.index("\n};", start)
    body = text[start:end]
    tokens = []
    pos = 0
    while pos < len(body):
        m = C_TOKEN.match(body, pos)
        if not m:
            raise ValueError("music_songs.h: can't read near " + body[pos:pos + 30])
        tok = m.group(0)
        pos = m.end()
        if tok.isspace() or tok.startswith("//") or tok.startswith("/*"):
            continue
        if tok.startswith('"') and tokens and isinstance(tokens[-1], bytes):
            tokens[-1] += c_string(tok[1:-1])  # Adjacent literals are one string
        elif tok.startswith('"'):
            tokens.append(c_string(tok[1:-1]))
        else:
            tokens.append(tok)
    songs = []
    i = 0
    while i < len(tokens):
        if tokens[i] == ",":
            i += 1
            continue
        if tokens[i] != "{":
            raise ValueError("music_songs.h: expected { in the song table")
        close = tokens.index("}", i)
        fields = [t for t in tokens[i + 1:close] if t != ","]
        i = close + 1
        if len(fields) != 8 or not all(isinstance(f, bytes) for f in fields[0:3] + [fields[6]]):
            raise ValueError(f"music_songs.h: a song with unexpected fields: {fields[:1]}")
        aliases = fields[7]
        if aliases != "nullptr" and not isinstance(aliases, bytes):
            raise ValueError("music_songs.h: aliases must be a string or nullptr")
        songs.append({
            "id": fields[0].decode("utf-8"), "de": fields[1].decode("utf-8"), "en": fields[2].decode("utf-8"),
            "tempo": int(fields[3]), "bar": int(fields[4]), "pickup": int(fields[5]),
            "notes": fields[6].decode("utf-8"),
            "aliases": [] if aliases == "nullptr" else aliases.decode("utf-8").split(";"),
        })
    return songs


# --- Song files ---

def is_control(c):
    """Control characters, C0 and C1, and the Unicode line and paragraph separators."""
    return ord(c) < 0x20 or 0x7F <= ord(c) <= 0x9F or c in "\u2028\u2029"


def read_song_text(text):
    """Returns ({key: value or [values]}, problems). Only a newline ends a line (a \\r before it is dropped),
    so no other separator can start a key, and every key needs a value: an empty settled settles nothing."""
    values, problems, given = {}, [], set()
    for number, line in enumerate(text.split("\n"), 1):
        if line.endswith("\r"):
            line = line[:-1]
        if any(is_control(c) for c in line):
            problems.append(f"line {number} has a control or line-separator character")
            continue
        if not line.strip() or line.startswith("#"):
            continue
        key, _, value = line.partition(" ")
        value = value.strip()
        if key not in REPEATED and key not in SINGLE:
            problems.append(f"unknown key {key} (line {number})")
            continue
        if key in SINGLE and key in given:
            problems.append(f"key {key} given twice (line {number})")
        given.add(key)
        if not value:
            problems.append(f"{key} has no value (line {number})")
        elif key in REPEATED:
            values.setdefault(key, []).append(value)
        else:
            values[key] = value
    for key in REQUIRED:
        if key not in given:
            problems.append(f"missing key {key}")
    return values, problems


def fields(value, count):
    parts = [p.strip() for p in value.split("|", count - 1)]
    return parts if len(parts) == count and all(parts) else None


def host(url):
    """The host of a web address, or None if it isn't one."""
    try:
        parts = urlsplit(url)
    except ValueError:
        return None
    if parts.scheme not in ("http", "https") or not parts.hostname or " " in url:
        return None
    return parts.hostname.lower()


SECOND_LEVEL = ("co", "com", "org", "gov", "ac", "edu", "net")


def site(name):
    """The registrable domain of a host, so en.wikipedia.org and de.wikipedia.org are one source, Wikipedia:
    the last two labels, or three under a country's second level such as bbc.co.uk or something.ac.uk."""
    name = name.lower().rstrip(".")
    if name.startswith("www."):
        name = name[4:]
    labels = name.split(".")
    if len(labels) >= 3 and labels[-2] in SECOND_LEVEL and len(labels[-1]) == 2:
        return ".".join(labels[-3:])
    return ".".join(labels[-2:])


def year(text):
    return int(text) if re.fullmatch(r"\d{3,4}", text) else None


def check_url(url, what, problems):
    if host(url) is None:
        problems.append(f"{what}: '{url}' is not a web address")


def check_legal(values, problems):
    """The legal rule: who wrote the melody, when they died, where it was first printed."""
    a = fields(values.get("source-a", ""), 3)
    if values.get("source-a") and not a:
        problems.append("source-a must be: citation | url | licence")
    elif a:
        check_url(a[1], "source-a", problems)
        if a[2] not in LICENCES:
            problems.append(f"score A licence '{a[2]}' is not one of: {', '.join(LICENCES)}")
    composers, traditional = values.get("composer", []), values.get("traditional", [])
    if composers and traditional:
        problems.append("a melody has a composer or is traditional, not both")
    if not composers and not traditional:
        problems.append("needs a composer or traditional line")
    for line in composers:
        parts = [p.strip() for p in line.split("|")]
        if len(parts) == 4:
            problems.append(f"composer {parts[0]} has only one URL: needs two, on different hosts")
            continue
        if len(parts) != 5 or not all(parts):
            problems.append("composer must be: name | born | died | url | url")
            continue
        name, born, died, url1, url2 = parts
        born, died = year(born), year(died)
        if born is None or died is None:
            problems.append(f"composer {name}: born and died must each be a year")
            continue
        if born >= died:
            problems.append(f"composer {name}: born after dying")
        if died >= DIED_BEFORE:
            problems.append(f"composer {name} died {died}: must be before {DIED_BEFORE}")
        check_url(url1, f"composer {name}", problems)
        check_url(url2, f"composer {name}", problems)
        if host(url1) and host(url2) and site(host(url1)) == site(host(url2)):
            problems.append(f"composer {name}: both URLs are on the same site, {site(host(url1))}; "
                            "needs two independent sources")
    for line in traditional:
        parts = fields(line, 3)
        if not parts or "|" in parts[2]:
            problems.append("traditional must be: citation | year | url")
            continue
        printed = year(parts[1])
        if printed is None:
            problems.append(f"traditional print {parts[0]}: '{parts[1]}' is not a year")
        elif printed >= PRINTED_BEFORE:
            problems.append(f"traditional print {parts[0]} printed {printed}: must be before {PRINTED_BEFORE}")
        check_url(parts[2], f"traditional print {parts[0]}", problems)
    if "published" in values:
        parts = fields(values["published"], 2)
        if not parts or "|" in parts[1]:
            problems.append("published must be: year | url")
        else:
            printed = year(parts[0])
            if printed is None:
                problems.append(f"published '{parts[0]}' is not a year")
            elif printed >= PRINTED_BEFORE:
                problems.append(f"published {printed}: must be before {PRINTED_BEFORE}")
            check_url(parts[1], "published", problems)
    elif composers:
        problems.append("missing key published: a composer's melody needs its first print")


def check_text_field(values, key, size, problems):
    """A field of the index: no '|' or control character, and it fits the stick's buffer."""
    value = values.get(key)
    if not value:
        return
    if any(c == "|" or ord(c) < 0x20 or ord(c) == 0x7F for c in value):
        problems.append(f"{key} has a '|' or a control character")
    length = len(value.encode("utf-8"))
    if length >= size:
        problems.append(f"{key} is {length} bytes, most {size - 1}")


def number(values, key, problems):
    text = values.get(key)
    if text is None:
        return None
    if not re.fullmatch(r"\d{1,4}", text):
        problems.append(f"{key} '{text}' is not a number")
        return None
    return int(text)


SHARP_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
REGION_NOTES = 16  # A region longer than this is cut short in the message, with "..."


def event_text(midi, ticks):
    """One folded event as the settled lines write it, such as G#4:2 or R:4."""
    name = SHARP_NAMES[midi % 12] + str(midi // 12 - 1) if midi else "R"
    return f"{name}:{ticks}"


def events_text(events):
    if not events:
        return "nothing"
    text = " ".join(event_text(m, t) for m, t in events[:REGION_NOTES])
    return text + (" ..." if len(events) > REGION_NOTES else "")


def fold_rests(events):
    """Every rest after a sounding note is added to that note; a rest at the very start stays. Comparison only."""
    out = []
    for midi, ticks in events:
        if midi == 0 and out and out[-1][0] != 0:
            out[-1] = [out[-1][0], out[-1][1] + ticks]
        else:
            out.append([midi, ticks])
    return out


def align(a, b, shift, scale=1):
    """a and b as (midi, ticks) tuples, b moved by shift and its lengths times scale, and how they line up."""
    a = [tuple(e) for e in a]
    b = [(m + shift if m else 0, Fraction(t) * scale) for m, t in b]
    b = [(m, int(t) if t.denominator == 1 else t) for m, t in b]
    matcher = difflib.SequenceMatcher(None, a, b, autojunk=False)
    return matcher, a, b


def best_alignment(a, b, scale=1):
    """The transposition under which the most events of B agree with A once the scores are lined up, so a
    difference early in the song, or a repeat only one score has, doesn't throw the rest off. Every offset
    between a pitch of A and a pitch of B is tried; a tie goes to the smaller move."""
    shifts = {ea[0] - eb[0] for ea in a for eb in b if ea[0] and eb[0]} or {0}
    best = None
    for shift in sorted(shifts, key=lambda s: (abs(s), s)):
        matcher, aa, bb = align(a, b, shift, scale)
        agree = sum(block.size for block in matcher.get_matching_blocks())
        if best is None or agree > best[0]:
            best = (agree, shift, matcher, aa, bb)
    return best


def score_differences(a, b):
    """None if folded score B equals folded score A up to one constant transposition. Else (shift, scale,
    regions): every place where they differ, as text naming A's event numbers and both scores' notes.

    Only scale 1 decides whether the scores agree. If B's lengths are all twice or half A's (B written at
    doubled or halved values), the regions are listed at that scale, so they show the real differences
    and not every note; such a song still needs a settled line."""
    agree, shift, matcher, aa, bb = best_alignment(a, b)
    if aa == bb:
        return None
    scale = 1
    for other in (Fraction(1, 2), Fraction(2)):
        found = best_alignment(a, b, other)
        if found[0] > agree:
            agree, shift, matcher, aa, bb = found
            scale = other
    regions = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        if i2 > i1:
            where = f"event {i1 + 1}" if i2 == i1 + 1 else f"events {i1 + 1}-{i2}"
        else:
            where = f"after event {i1}" if i1 else "before event 1"
        regions.append(f"{where}: A {events_text(aa[i1:i2])}, B {events_text(bb[j1:j2])}")
    return shift, scale, regions


def describe_differences(shift, scale, regions):
    moved = f"B moved by {shift:+d} semitones"
    if scale != 1:
        moved += f", B's lengths {'halved' if scale < 1 else 'doubled'}, as B is written at {1 / scale} times A's values"
    return (f"score B differs from score A in {len(regions)} place{'s' if len(regions) != 1 else ''} "
            f"(events counted in A, lengths in sixteenths, {moved}), after joining ties and folding rests: "
            + "; ".join(regions))


def compare_scores(a, b):
    """None if score B equals score A up to one constant transposition, else every place where they differ.

    A note followed by a rest counts as one longer note (user decision, 2026-11-07), so both scores are folded first.
    At the end of a score the song's end is the next note: the last note and the rests after it count as one length,
    so both scores must have the same total length (the scores come from join(keep_end_rests=True))."""
    found = score_differences(fold_rests(a), fold_rests(b))
    return describe_differences(*found) if found else None


def check_song(name, text):
    """Returns (song, problems, remarks). The song dict holds what the output files need; it is returned even
    with problems, so names and ids can still be checked against the other songs'."""
    values, problems = read_song_text(text)
    remarks = []
    song_id = values.get("id", "")
    if song_id:
        if not re.fullmatch(r"[a-z0-9-]+", song_id) or len(song_id) >= ID_SIZE:
            problems.append(f"id '{song_id}' must be 1 to {ID_SIZE - 1} of a-z, 0-9 and -")
        elif song_id in RESERVED_IDS:
            problems.append(f"id '{song_id}' is reserved: its file would clash with {song_id}.txt or "
                            f"{song_id.upper()}.txt")
        elif name != song_id + ".song":
            problems.append(f"file name must be {song_id}.song")
    for key, size in (("de", NAME_SIZE), ("en", NAME_SIZE), ("footer", SOURCE_SIZE)):
        check_text_field(values, key, size, problems)
    names = []
    for key in ("de", "en"):
        if values.get(key):
            folded = normalise(values[key])
            if folded is None:
                problems.append(f"{key} '{values[key]}' can't be normalised, so the stick could never find it")
            else:
                names.append(folded)
    aliases = []
    if "aliases" in values:
        for alias in values["aliases"].split(";"):
            if not alias:
                problems.append("empty alias: aliases are ';'-separated, with nothing between two ';'")
            elif normalise(alias) != alias:
                problems.append(f"alias '{alias}' is not normalised (would be '{normalise(alias)}')")
            else:
                aliases.append(alias)
    tempo, bar, pickup = (number(values, k, problems) for k in ("tempo", "bar", "pickup"))
    timing_ok = None not in (tempo, bar, pickup)
    if tempo is not None and not 60 <= tempo <= 160:
        problems.append(f"tempo {tempo} must be 60 to 160")
        timing_ok = False
    if bar is not None and bar not in (8, 12, 16):
        problems.append(f"bar {bar} must be 8, 12 or 16")
        timing_ok = False
    if bar is not None and pickup is not None and not 0 <= pickup < bar:
        problems.append(f"pickup {pickup} must be 0 to {bar - 1}")
        timing_ok = False

    check_legal(values, problems)

    notes = None
    differences = None  # (shift, scale, regions) a settled line accepts, for SOURCES.txt
    if values.get("notes"):
        try:
            notes, total = parse_notes(values["notes"])
        except ValueError as e:
            problems.append(f"notes don't parse the way the stick reads them: {e}")
    if notes and timing_ok:
        problem = sound_problem(notes, total, bar, pickup, tempo)
        if problem:
            problems.append(problem)

    b = fields(values.get("score-b", ""), 4)
    if values.get("score-b") and not b:
        problems.append("score-b must be: url | language | relative <pitch> or absolute | LilyPond notes")
    elif b:
        check_url(b[0], "score-b", problems)
        start = None
        if b[2] != "absolute":
            m = re.fullmatch(r"relative ([a-g][',]*)", b[2])
            if not m:
                problems.append(f"score-b octaves '{b[2]}' must be 'relative <pitch>', such as relative c', "
                                "or 'absolute'")
            start = m.group(1) if m else None
        if b[1] not in SCORE_LANGS:
            problems.append(f"score-b language '{b[1]}' is not one of: {', '.join(SCORE_LANGS)}")
        elif notes and (start or b[2] == "absolute"):
            try:
                score_b = read_score(b[3], b[1], start, keep_end_rests=True)
            except Exception as e:  # Anything the reader can't follow is a song that stays out
                problems.append(f"score B can't be read: {e}")
            else:
                score_a = join([[n.midi, n.ticks, n.tied] for n in notes], keep_end_rests=True)
                found = score_differences(fold_rests(score_a), fold_rests(score_b))
                difference = describe_differences(*found) if found else None
                if difference and "settled" in values:
                    remarks.append(f"{difference} (settled: {values['settled']})")
                    differences = found
                elif difference:
                    problems.append(difference + "; fix the notes, or add a settled line saying why a person "
                                    "accepts the difference")
                elif "settled" in values:
                    problems.append("settled line, but score A and B agree: remove it")

    label = song_id if song_id and re.fullmatch(r"[a-z0-9-]+", song_id) else name.rsplit(".", 1)[0]
    problems = [f"{label}: {p}" for p in problems]
    remarks = [f"{label}: {r}" for r in remarks]
    song = dict(values, id=song_id, label=label, file=name, names=names, alias_list=aliases,
                tempo=tempo, bar=bar, pickup=pickup, differences=differences)
    return song, problems, remarks


def song_file(song):
    return f"ZITRO-MUSIC-SONG 1 {song['id']}\n{song['tempo']} {song['bar']} {song['pickup']}\n{song['notes']}\n"


def sources_block(song):
    a = fields(song["source-a"], 3)
    lines = [song["id"], f"  Names: {song['de']} / {song['en']}",
             f"  Score A (the notes): {a[0]} | {a[1]} | {a[2]}",
             f"  Score B (compared only): {fields(song['score-b'], 4)[0]}"]
    for line in song.get("composer", []):
        name, born, died, url1, url2 = [p.strip() for p in line.split("|")]
        lines.append(f"  Composer: {name} ({born}-{died}) | {url1} | {url2}")
    for line in song.get("traditional", []):
        citation, printed, url = fields(line, 3)
        lines.append(f"  Traditional, printed in: {citation} ({printed}) | {url}")
    if "published" in song:
        printed, url = fields(song["published"], 2)
        lines.append(f"  First print: {printed} | {url}")
    if "settled" in song:
        lines.append(f"  Settled: {song['settled']}")
        shift, scale, regions = song["differences"]
        moved = f"B moved by {shift:+d} semitones" + (
            f", B's lengths {'halved' if scale < 1 else 'doubled'}" if scale != 1 else "")
        lines.append(f"  Every difference from score B, after joining ties and folding rests ({moved}, "
                     "events counted in A, lengths in sixteenths):")
        lines += [f"    {region}" for region in regions]
    return "\n".join(lines)


def build(texts, built_ins):
    """texts: {file name: song text}. Returns (files {name: text}, problems, remarks); no files on any problem."""
    problems, remarks, songs = [], [], []
    if not texts:
        problems.append("no songs: nothing to build")
    for name in sorted(texts):
        song, song_problems, song_remarks = check_song(name, texts[name])
        problems += song_problems
        remarks += song_remarks
        songs.append(song)

    # Every name finds one song: the stick looks at built-in songs first, and a title two songs share finds none
    owners = {}
    built_in_ids = {s["id"] for s in built_ins}
    for s in built_ins:
        # The stick also finds a built-in song by its id, so that counts as one of its names
        for folded in [normalise(s["de"]), normalise(s["en"]), normalise(s["id"])] + s["aliases"]:
            if folded:
                owners.setdefault(folded, set()).add("built-in song " + s["id"])
    files_by_id = {}
    for song in songs:
        if song["id"] in built_in_ids:
            problems.append(f"{song['label']}: id {song['id']} is a built-in song's id")
        if song["id"] and song["id"] in files_by_id:
            problems.append(f"{song['label']}: id {song['id']} is used twice, also in {files_by_id[song['id']]}")
        files_by_id.setdefault(song["id"], song["file"])
        for raw in [song.get("de", ""), song.get("en", "")] + song["alias_list"]:
            if raw and raw.lower() in built_in_ids:
                problems.append(f"{song['label']}: name '{raw}' is a built-in song's id")
        for folded in set(song["names"] + song["alias_list"]):
            owners.setdefault(folded, set()).add("song " + song["id"])
    for song in songs:
        for folded in sorted(set(song["names"] + song["alias_list"])):
            others = sorted(o for o in owners[folded] if o != "song " + song["id"])
            if others:
                problems.append(f"{song['label']}: name '{folded}' is already used by {', '.join(others)}")

    if problems:
        return {}, problems, remarks
    files = {}
    if len(songs) > MAX_INDEX_ENTRIES:
        problems.append(f"index.txt: {len(songs)} songs, the stick reads at most {MAX_INDEX_ENTRIES}")
    songs.sort(key=lambda s: s["id"])
    for song in songs:
        text = song_file(song)
        size = len(text.encode("utf-8"))
        if size >= FILE_LIMIT:
            problems.append(f"{song['label']}: {song['id']}.txt is {size} bytes, the stick reads less than {FILE_LIMIT}")
        files[song["id"] + ".txt"] = text
    index = f"ZITRO-MUSIC-INDEX 1 {len(songs)}\n" + "".join(
        f"{s['id']}|{s['de']}|{s['en']}|{';'.join(s['alias_list'])}|{s['footer']}\n" for s in songs)
    if len(index.encode("utf-8")) >= INDEX_LIMIT:
        problems.append(f"index.txt is {len(index.encode('utf-8'))} bytes, the stick reads less than {INDEX_LIMIT}")
    files["index.txt"] = index
    files["SOURCES.txt"] = (
        "Songs on zitro.spot: where each melody and its notes come from.\n"
        "Every melody is in the public domain: its composers died before 1926, or it is traditional and printed\n"
        "before 1931. The notes are read from score A and compared note by note with score B, which is never\n"
        "copied. Composer dates are confirmed by two independent sources.\n\n"
        + "\n\n".join(sources_block(s) for s in songs) + "\n")
    files[".htaccess"] = HTACCESS
    if problems:
        return {}, problems, remarks
    return files, problems, remarks


def song_count(files):
    return sum(1 for name in files if name not in NOT_SONGS)


def check_folder(folder, files):
    """0 if folder holds exactly these files, byte for byte; else 1, naming each difference."""
    present = set(os.listdir(folder)) if os.path.isdir(folder) else set()
    differences = [f"{name}: not built from the library, remove it" for name in sorted(present - set(files))]
    for name in sorted(files):
        if name not in present:
            differences.append(f"{name}: missing")
            continue
        with open(os.path.join(folder, name), "rb") as f:
            if f.read() != files[name].encode("utf-8"):
                differences.append(f"{name}: differs from what the library builds")
    if differences:
        print(f"{folder} is not what scripts/make_music_data.py builds; run it again:\n  "
              + "\n  ".join(differences), file=sys.stderr)
        return 1
    print(f"{folder}: current, {song_count(files)} songs")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--library", default=os.path.join(HERE, "music_library"))
    parser.add_argument("--out", default=os.path.join(HERE, "..", "webpage2", "data", "music"))
    parser.add_argument("--check", metavar="DIR", help="write nothing; fail unless DIR holds exactly what the tool "
                        "would write, no file more or less (tests/music/run.sh uses this on webpage2/data/music)")
    args = parser.parse_args(argv)
    texts, problems = {}, []
    for name in sorted(os.listdir(args.library)):
        if not name.endswith(".song"):
            continue
        with open(os.path.join(args.library, name), "rb") as f:
            raw = f.read()
        try:
            texts[name] = raw.decode("utf-8")
        except UnicodeDecodeError as e:
            problems.append(f"{name}: not UTF-8: {e}")
    files, build_problems, remarks = build(texts, read_built_ins())
    problems += build_problems
    for remark in remarks:
        print("Accepted difference:", remark)
    if problems:
        print("Nothing written. Failed:\n  " + "\n  ".join(problems), file=sys.stderr)
        return 1
    if args.check:
        return check_folder(args.check, files)
    os.makedirs(args.out, exist_ok=True)
    # Each file goes in whole or not at all; nothing outside the out folder is touched, and nothing deleted
    for name, text in files.items():
        path = os.path.join(args.out, name)
        with open(path + ".part", "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        os.replace(path + ".part", path)
    size = sum(len(t.encode("utf-8")) for t in files.values())
    print(f"{song_count(files)} songs written to {args.out}: {len(files)} files, {size} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
