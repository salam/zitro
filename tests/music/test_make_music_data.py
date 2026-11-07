#!/usr/bin/env python3
"""Tests the song tool's parsing and its checks, with in-memory song texts and no downloads.

Run by tests/music/run.sh, which passes the compiled C++ test: the tool's output must then pass the
firmware's own reading of the server data (testServerData), and a damaged copy must fail it.
"""
import os
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import make_music_data as m  # noqa: E402

failures = 0
checks = 0


def check(condition, what):
    global failures, checks
    checks += 1
    if not condition:
        failures += 1
        print("  FAIL", what)


def raises(fn, *args):
    try:
        fn(*args)
    except ValueError:
        return True
    return False


# --- Names: the same cases as testNormaliseTitles in test_music.cpp ---

def test_normalise():
    good = [
        ("Für Elise", "fuer elise"),
        ("Frère Jacques", "frere jacques"),
        ("Funiculì, Funiculà", "funiculi funicula"),
        ("Oh! Susanna", "oh susanna"),
        ("Beethoven's  Symphony", "beethovens symphony"),
        ("Beethoven’s Symphony", "beethovens symphony"),
        ("  Hänschen-klein ", "haenschen klein"),
        ("„Stille Nacht“", "stille nacht"),
        ("«Frère Jacques»", "frere jacques"),
        ("“Silent Night”", "silent night"),
        ("‘Silent Night’", "silent night"),
        ("Ode an die Freude – Beethoven", "ode an die freude beethoven"),
        ("Ode—Freude", "ode freude"),
        ("„“ London Bridge", "london bridge"),
        # More from name_match.h: capitals fold, sharp s, and every ASCII sign is a space
        ("SCHNEEFLÖCKCHEN, WEIßRÖCKCHEN", "schneefloeckchen weissroeckchen"),
        ("École Ñandú", "ecole nandu"),
        ("a|b;c_d\te", "a b c d e"),
    ]
    for text, want in good:
        got = m.normalise(text)
        check(got == want, f"normalise({text!r}) = {got!r}, want {want!r}")
    # The C version works on bytes; these cut-off sequences only occur there, so they're given as bytes
    for raw in [b"Ode \xe2\x80", b"Ode \xc2", b"Ode \xc3"]:
        check(m.normalise_bytes(raw) is None, f"normalise_bytes({raw!r}) is not None")
    bad = ["Ode …", "© Ode", "!!!", "日本", "øre", "Æsop", "2×3", "Ode\x7f", ""]
    for text in bad:
        check(m.normalise(text) is None, f"normalise({text!r}) = {m.normalise(text)!r}, want None")
    # The stick's buffer is 96 bytes: a longer form never matches there
    check(m.normalise("a" * 95) == "a" * 95, "95 letters normalise")
    check(m.normalise("a" * 96) is None, "96 letters are too long")


# --- Notes: the same cases as testParser in test_music.cpp ---

def test_parser():
    notes, total = m.parse_notes("C4:4 F#4:2 | Bb4:2 R:4 G4:4~ G4:4")
    check(len(notes) == 6 and total == 20, "good notes: count and total")
    n = notes
    check((n[0].midi, n[0].ticks, n[0].staff_step, n[0].start) == (60, 4, -2, 0), "C4:4")
    check((n[1].midi, n[1].accidental, n[1].staff_step, n[1].start) == (66, 1, 1, 4), "F#4:2")
    check((n[2].midi, n[2].accidental, n[2].staff_step) == (70, -1, 4), "Bb4:2")
    check((n[3].midi, n[3].ticks) == (0, 4), "R:4")
    check(n[4].tied and not n[5].tied and n[4].midi == n[5].midi, "tie")
    check(n[5].start == 16, "start tick after a tie")
    check(m.parse_notes("Cb4:4 B#3:4 C4:016")[0][0].staff_step == -2, "Cb4 sits on C's step")
    check(m.parse_notes("C4:016")[1] == 16, "three length digits are read, like the C loop")
    bad = ["", "   ", "|", "H4:4", "c4:4", "C:4", "C4", "C4:", "C4:5", "C4:0", "C4:99999", "C4:-4",
           "C1:4", "C7:4", "C#b4:4", "R:3~ C4:4", "R:5", "C4:4~", "C4:4~ D4:4", "C4:4~ R:4",
           "C4:4,D4:4", "C4:4 ä", "C4:4x", "Z", ":", "C4:4 :4",
           # More: only spaces and bars separate notes, and a fourth digit is one too many
           "C4:4\tD4:4", "C4:4\nD4:4", "C4:0016"]
    for text in bad:
        check(raises(m.parse_notes, text), f"parse_notes accepted {text!r}")
    check(raises(m.parse_notes, "C4:1 " * (m.MAX_NOTES + 1)), "more than MAX_NOTES notes")
    check(len(m.parse_notes("C4:1 " * m.MAX_NOTES)[0]) == m.MAX_NOTES, "exactly MAX_NOTES notes")


def test_constants_match_firmware():
    with open(os.path.join(ROOT, "sketch", "sketch_jan12a", "music_core.h"), encoding="utf-8") as f:
        core = f.read()
    check(f"MUSIC_MAX_NOTES = {m.MAX_NOTES};" in core, "MAX_NOTES matches music_core.h")
    check(re.search(r"#define MUSIC_INDEX_LIMIT \((\d+) \* 1024\)", core).group(1) == str(m.INDEX_LIMIT // 1024),
          "INDEX_LIMIT matches music_core.h")
    check(re.search(r"#define MUSIC_FILE_LIMIT \((\d+) \* 1024\)", core).group(1) == str(m.FILE_LIMIT // 1024),
          "FILE_LIMIT matches music_core.h")


# --- Built-in songs, read from music_songs.h ---

BUILT_INS = m.read_built_ins()


def test_built_ins():
    check(len(BUILT_INS) == 26, f"26 built-in songs, got {len(BUILT_INS)}")
    by_id = {s["id"]: s for s in BUILT_INS}
    check("bruder jakob" in by_id["frere-jacques"]["aliases"], "frere-jacques aliases")
    check(by_id["frere-jacques"]["en"] == "Frère Jacques", "escaped UTF-8 decoded")
    check(by_id["haenschen-klein"]["de"] == "Hänschen klein", "Haenschen klein decoded")
    check(by_id["schneefloeckchen"]["de"] == "Schneeflöckchen, Weißröckchen", "split literals joined")
    check(by_id["alle-meine-entchen"]["aliases"] == [], "nullptr aliases")
    check("beethovens 9th symphony" in by_id["ode-to-joy"]["aliases"]
          and "freude schoener goetterfunken" in by_id["ode-to-joy"]["aliases"], "aliases over several lines")
    # The Python sound check agrees with the firmware's on every built-in song
    for s in BUILT_INS:
        notes, total = m.parse_notes(s["notes"])
        problem = m.sound_problem(notes, total, s["bar"], s["pickup"], s["tempo"])
        check(problem is None, f"built-in {s['id']} is sound: {problem}")


# --- The sound check: the same cases as testSongIsSound ---

def test_sound():
    s = BUILT_INS[0]
    notes, total = m.parse_notes(s["notes"])
    bar, pickup, tempo = s["bar"], s["pickup"], s["tempo"]
    check(m.sound_problem(notes, total, bar, pickup, 200) is not None, "tempo 200")
    check(m.sound_problem(notes, total, bar, pickup, 59) is not None, "tempo 59")
    check(m.sound_problem(notes, total, 10, pickup, tempo) is not None, "bar 10")
    check(m.sound_problem(notes, total, bar, bar, tempo) is not None, "pickup a whole bar")
    check(m.sound_problem(notes[:5], total, bar, pickup, tempo) is not None, "5 notes")
    high, high_total = m.parse_notes("F#6:2 " + s["notes"].split(" ", 1)[1])  # First note C4:2 made F#6
    check(high[0].midi == 90 and "A3 to A5" in (m.sound_problem(high, high_total, bar, pickup, tempo) or ""),
          "MIDI 90")
    short, short_total = m.parse_notes(s["notes"][: s["notes"].rindex(":")] + ":4")  # Last C4:8 made C4:4
    check("bars don't add up" in (m.sound_problem(short, short_total, bar, pickup, tempo) or ""), "a bar short")
    # A note over a bar line, though the total is right
    over, over_total = m.parse_notes("C4:12 C4:8 C4:12 " + "C4:4 " * 8)
    check("bar line" in (m.sound_problem(over, over_total, 16, 0, 100) or ""), "note over a bar line")


# --- Song files ---

NOTES_A = ("D4:2 | G4:2 G4:2 B4:2 B4:2 | D5:4 B4:4 | A4:2 A4:2 G4:2 E4:2 | D4:6 D4:2 | "
           "G4:2 G4:2 B4:2 B4:2 | A4:4 G4:2~ G4:2 | A4:4 B4:4 | G4:6")
NOTES_B = "d8 | g g b b | d4 b | a8 a g e | d4. d8 | g g b b | a4 g8~ g | a4 b | g4."
NOTES_B_FOURTH_UP = "g8 | c c e e | g4 e | d8 d c a | g4. g8 | c c e e | d4 c8~ c | d4 e | c4."
NOTES_B_ONE_WRONG = "d8 | g g b b | d4 b | a8 a g d | d4. d8 | g g b b | a4 g8~ g | a4 b | g4."

GOOD = [
    ("id", "test-song"),
    ("de", "Ein Testlied"),
    ("en", "A Test Song"),
    ("aliases", "testlied;test song"),
    ("tempo", "112"),
    ("bar", "8"),
    ("pickup", "2"),
    ("notes", NOTES_A),
    ("source-a", "Example Songbook, Leipzig 1900, p. 12 | https://archive.org/details/example | pre-1931 print"),
    ("score-b", "https://de.wikipedia.org/wiki/Testlied | nederlands | relative c' | " + NOTES_B),
    ("composer", "Jane Example | 1801 | 1870 | https://en.wikipedia.org/wiki/Jane_Example | "
                 "https://imslp.org/wiki/Category:Example,_Jane"),
    ("published", "1850 | https://imslp.org/wiki/Testlied_(Example,_Jane)"),
    ("footer", "Songbook 1900"),
]


def song(changes=None, extra=(), base=GOOD):
    """A song text: changes replace a key's value, or drop the key when None; extra lines are appended."""
    changes = changes or {}
    lines = ["# A song for the tests", ""]
    for key, value in base:
        value = changes.get(key, value)
        if value is not None:
            lines.append(f"{key} {value}")
    lines += list(extra)
    return "\n".join(lines) + "\n"


def second(changes=None, extra=()):
    base = dict(GOOD)
    base.update({"id": "other-song", "de": "Ein anderes Lied", "en": "Another Song", "aliases": "anderes lied",
                 "footer": "Other Book 1910"})
    return song(changes, extra, list(base.items()))


def errors_of(texts):
    files, errors, notes = m.build(texts, BUILT_INS)
    return errors


def has_error(texts, *parts):
    errors = errors_of(texts)
    found = any(all(p in e for p in parts) for e in errors)
    if not found:
        print(f"    no error with {parts}; got {errors}")
    return found


def test_good_song():
    files, errors, notes = m.build({"test-song.song": song()}, BUILT_INS)
    check(errors == [], f"good song has no errors: {errors}")
    check(files.get("index.txt") == "ZITRO-MUSIC-INDEX 1 1\n"
          "test-song|Ein Testlied|A Test Song|testlied;test song|Songbook 1900\n", f"index: {files.get('index.txt')!r}")
    check(files.get("test-song.txt") == f"ZITRO-MUSIC-SONG 1 test-song\n112 8 2\n{NOTES_A}\n", "song file")
    sources = files.get("SOURCES.txt", "")
    for part in ["test-song", "Ein Testlied", "A Test Song", "Example Songbook, Leipzig 1900, p. 12",
                 "https://archive.org/details/example", "pre-1931 print", "https://de.wikipedia.org/wiki/Testlied",
                 "Jane Example", "1801", "1870", "https://en.wikipedia.org/wiki/Jane_Example",
                 "https://imslp.org/wiki/Category:Example,_Jane", "1850", "https://imslp.org/wiki/Testlied"]:
        check(part in sources, f"SOURCES.txt names {part}")
    # Two songs, sorted by id
    files, errors, _ = m.build({"test-song.song": song(), "other-song.song": second()}, BUILT_INS)
    check(errors == [] and files["index.txt"].splitlines()[0] == "ZITRO-MUSIC-INDEX 1 2"
          and files["index.txt"].splitlines()[1].startswith("other-song|"), f"two songs: {errors}")


def test_brief_cases():
    t = "test-song.song"
    # Each of the brief's cases fails with its own message, naming the song
    check(has_error({t: song({"composer": "Jane Example | 1880 | 1946 | https://en.wikipedia.org/wiki/J | "
                                          "https://imslp.org/wiki/J"})}, "test-song:", "died 1946"),
          "composer died 1946")
    check(has_error({t: song({"published": "1935 | https://imslp.org/wiki/J"})}, "test-song:", "published 1935"),
          "published 1935")
    check(has_error({t: song({"composer": "Jane Example | 1801 | 1870 | https://en.wikipedia.org/wiki/J"})},
                    "test-song:", "only one URL"), "only one composer URL")
    check(has_error({t: song({"composer": "Jane Example | 1801 | 1870 | https://en.wikipedia.org/wiki/J | "
                                          "https://en.wikipedia.org/wiki/K"})}, "test-song:", "same host"),
          "two URLs on the same host")
    check(has_error({t: song({"source-a": "Example Songbook | https://archive.org/details/x | CC BY-SA"})},
                    "test-song:", "licence 'CC BY-SA'"), "source-a licence CC BY-SA")
    check(has_error({t: song({"aliases": "testlied;bruder jakob"})}, "test-song:", "'bruder jakob'",
                    "built-in song frere-jacques"), "alias of a built-in")
    check(has_error({t: song({"score-b": "https://x.org/a | nederlands | relative c' | " + NOTES_B_ONE_WRONG})},
                    "test-song:", "score B differs", "settled"), "score B one note different")
    check(has_error({t: song({"notes": NOTES_A.replace("D4:6 D4:2", "D4:6 D4:1")})}, "test-song:",
                    "bars don't add up"), "a bar a sixteenth short")
    check(has_error({t: song({"notes": NOTES_A.replace("D5:4", "F#6:4")})}, "test-song:", "MIDI 90", "A3 to A5"),
          "a note at MIDI 90")
    # Score B a fourth up passes
    check(errors_of({t: song({"score-b": "https://x.org/a | nederlands | relative c'' | " + NOTES_B_FOURTH_UP})})
          == [], "score B a fourth up")
    # The legal-only cases give exactly one error
    for changes in [{"published": "1935 | https://imslp.org/wiki/J"},
                    {"source-a": "Example Songbook | https://archive.org/details/x | CC BY-SA"}]:
        check(len(errors_of({t: song(changes)})) == 1, f"one error for {changes}")


def test_settled():
    t = "test-song.song"
    wrong = {"score-b": "https://x.org/a | nederlands | relative c' | " + NOTES_B_ONE_WRONG}
    files, errors, notes = m.build({t: song(wrong, ["settled The scan reads E, the 1850 print too; B has D"])},
                                   BUILT_INS)
    check(errors == [], f"settled difference passes: {errors}")
    check(any("test-song" in n and "score B differs" in n for n in notes), f"difference still printed: {notes}")
    check("The scan reads E" in files.get("SOURCES.txt", ""), "settled note in SOURCES.txt")
    check(has_error({t: song(extra=["settled nothing to settle"])}, "test-song:", "settled", "agree"),
          "settled with nothing to settle")


def test_more_checks():
    t = "test-song.song"
    cases = [
        (song({"footer": None}), "missing key footer"),
        (song({"en": None}), "missing key en"),
        (song({"notes": None}), "missing key notes"),
        (song(extra=["de Noch ein Name"]), "key de given twice"),
        (song(extra=["compser Jane"]), "unknown key compser"),
        (song({"id": "Test_Song"}), "id 'Test_Song'"),
        (song({"de": "Ode an die Freude"}), "built-in song ode-to-joy"),
        (song({"en": "Twinkle-twinkle little star!"}), "built-in song twinkle-twinkle"),
        (song({"aliases": "Testlied"}), "alias 'Testlied' is not normalised"),
        (song({"aliases": "testlied;;x"}), "empty alias"),
        (song({"aliases": "test lied;test  lied"}), "not normalised"),
        (song({"de": "Ein Testlied mit einem viel zu langen Namen, fas"}), "de is 48 bytes"),
        (song({"de": "Ein Lied | zwei"}), "'|'"),
        (song({"en": "日本"}), "can't be normalised"),
        (song({"footer": "Ein sehr langer Quellenname, viel zu lang"}), "footer is 41 bytes"),
        (song({"tempo": "200"}), "tempo 200"),
        (song({"tempo": "fast"}), "tempo 'fast'"),
        (song({"bar": "10"}), "bar 10"),
        (song({"pickup": "8"}), "pickup 8"),
        (song({"notes": NOTES_A + " X"}), "notes don't parse"),
        (song({"source-a": "Example Songbook | https://archive.org/details/x"}), "source-a"),
        (song({"source-a": "Example Songbook | archive.org | pre-1931 print"}), "not a web address"),
        (song({"score-b": "https://x.org/a | italiano | relative c' | " + NOTES_B}), "language 'italiano'"),
        (song({"score-b": "https://x.org/a | nederlands | relative c' | d8 <g b> d"}), "score B can't be read"),
        (song({"score-b": "https://x.org/a | nederlands | c' | " + NOTES_B}), "relative"),
        (song({"composer": "Jane Example | 1801 | 1870? | https://a.org/x | https://b.org/y"}), "year"),
        (song({"composer": "Jane Example | 1871 | 1870 | https://a.org/x | https://b.org/y"}), "born after"),
        (song({"composer": "Jane Example | 1801 | 1870 | https://www.imslp.org/x | https://imslp.org/y"}),
         "same host"),
        (song({"composer": None}), "composer or traditional"),
        (song({"published": None}), "missing key published"),
        (song(extra=["traditional Old Book | 1900 | https://archive.org/details/old"]), "not both"),
        (song({"composer": None, "published": None}, ["traditional Old Book | 1935 | https://archive.org/details/o"]),
         "printed 1935"),
        (song({"composer": None, "published": None}, ["traditional Old Book | 1900"]), "traditional"),
    ]
    for text, part in cases:
        check(has_error({t: text}, "test-song:", part), f"error {part!r}")
    # The year limits themselves
    composer = "Jane Example | 1850 | {} | https://en.wikipedia.org/wiki/J | https://imslp.org/wiki/J"
    check(errors_of({t: song({"composer": composer.format(1925), "published": "1930 | https://a.org/p"})}) == [],
          "died 1925 and published 1930 pass")
    check(has_error({t: song({"composer": composer.format(1926)})}, "died 1926"), "died 1926 fails")
    check(has_error({t: song({"published": "1931 | https://a.org/p"})}, "published 1931"), "published 1931 fails")
    trad = {"composer": None, "published": None}
    check(errors_of({t: song(trad, ["traditional Old Book | 1930 | https://a.org/o"])}) == [], "printed 1930 passes")
    check(has_error({t: song(trad, ["traditional Old Book | 1931 | https://a.org/o"])}, "printed 1931"),
          "printed 1931 fails")
    check(has_error({t: song(trad, ["traditional Old Book | 1900 | https://a.org/o",
                                    "traditional Late Book | 1931 | https://a.org/l"])}, "Late Book printed 1931"),
          "every traditional print is checked")
    # Several composers: each must pass
    check(has_error({t: song(extra=["composer Late Person | 1880 | 1950 | https://a.org/x | https://b.org/y"])},
                    "Late Person died 1950"), "a second composer died too late")
    check(errors_of({t: song(extra=["composer Early Person | 1780 | 1840 | https://a.org/x | https://b.org/y"])})
          == [], "two composers who both qualify")
    # A traditional melody needs no published line; several prints are fine
    check(errors_of({t: song({"composer": None, "published": None},
                             ["traditional Old Book, Berlin | 1900 | https://archive.org/details/old",
                              "traditional Older Book | 1850 | https://books.google.com/x"])}) == [],
          "traditional melody")
    check(has_error({"wrong-name.song": song()}, "test-song:", "file name must be test-song.song"),
          "file name differs from id")
    check(has_error({"ode-to-joy.song": song({"id": "ode-to-joy"})}, "ode-to-joy:", "built-in song's id"),
          "id of a built-in song")
    check(has_error({t: song(), "other.song": second({"id": "test-song"})}, "test-song", "twice"), "duplicate id")
    check(has_error({t: song(), "other-song.song": second({"aliases": "a test song"})}, "'a test song'",
                    "test-song"), "alias equal to another server song's name")
    check(has_error({t: song(), "other-song.song": second({"de": "Ein Testlied!"})}, "'ein testlied'"),
          "name equal to another server song's name after normalising")
    check(has_error({}, "no songs"), "no songs at all")
    check(has_error({t: b"\xff\xfe".decode("latin-1") + song()}, "test-song"), "stray bytes before the first key")
    check(errors_of({t: song({"de": "Ein Testlied mit einem viel zu langen Namen, \u00e4",
                              "footer": "Ein sehr langer Quellenname, viel zu la"})}) == [], "47 and 39 bytes fit")
    # One song may use the same name in both languages, and an alias may repeat it
    check(errors_of({t: song({"en": "Ein Testlied", "aliases": "ein testlied"})}) == [], "same name twice")


def test_english_lilypond():
    # LilyPond's english names: s sharp, f flat; es is E sharp there, not E flat. Absolute c is C3.
    check(m.read_score("fs'4 bf es", "english", None) == [[66, 4], [58, 4], [53, 4]], "english names")
    check(m.read_score("fsharp'4 bflat", "english", None) == [[66, 4], [58, 4]], "english long names")
    check(m.read_score("fis'4 bes es", "nederlands", None) == [[66, 4], [58, 4], [51, 4]], "nederlands unchanged")
    check(m.read_score("fis'4 b h", "deutsch", None) == [[66, 4], [58, 4], [59, 4]], "deutsch unchanged")
    for notes, lang in [("fis4", "english"), ("bf4", "nederlands"), ("h4", "nederlands"), ("his4", "deutsch")]:
        check(raises(m.read_score, notes, lang, None), f"{notes} in {lang} is refused")


def test_sizes():
    saved = m.FILE_LIMIT
    m.FILE_LIMIT = len(f"ZITRO-MUSIC-SONG 1 test-song\n112 8 2\n{NOTES_A}\n")
    check(has_error({"test-song.song": song()}, "test-song:", "bytes, the stick reads"), "song file at the limit")
    m.FILE_LIMIT = saved
    saved = m.INDEX_LIMIT
    m.INDEX_LIMIT = 50
    check(has_error({"test-song.song": song()}, "index.txt"), "index over the limit")
    m.INDEX_LIMIT = saved


def run_tool(library, out):
    return subprocess.run([sys.executable, "-I", os.path.join(ROOT, "scripts", "make_music_data.py"),
                           "--library", library, "--out", out], capture_output=True, text=True)


def test_main(binary):
    with tempfile.TemporaryDirectory() as tmp:
        library = os.path.join(tmp, "library")
        out = os.path.join(tmp, "out")
        os.makedirs(library)
        with open(os.path.join(library, "README.md"), "w") as f:
            f.write("Not a song\n")
        with open(os.path.join(library, "test-song.song"), "w", encoding="utf-8") as f:
            f.write(song())
        with open(os.path.join(library, "other-song.song"), "w", encoding="utf-8") as f:
            f.write(second({"aliases": "testlied"}))
        # A clash writes nothing at all
        result = run_tool(library, out)
        check(result.returncode == 1 and not os.path.exists(out), f"clash: exit {result.returncode}")
        check("Nothing written" in result.stderr and "'testlied'" in result.stderr, f"clash: {result.stderr}")
        with open(os.path.join(library, "other-song.song"), "w", encoding="utf-8") as f:
            f.write(second())
        result = run_tool(library, out)
        check(result.returncode == 0, f"good library: exit {result.returncode} {result.stderr}")
        check(sorted(os.listdir(out)) == ["SOURCES.txt", "index.txt", "other-song.txt", "test-song.txt"],
              f"files written: {os.listdir(out) if os.path.exists(out) else None}")
        # A failing run later leaves what is there untouched
        with open(os.path.join(out, "index.txt"), "rb") as f:
            before = f.read()
        with open(os.path.join(library, "bad-song.song"), "w", encoding="utf-8") as f:
            f.write(song({"id": "bad-song", "de": "Schlecht", "en": "Bad", "aliases": None, "tempo": "300"}))
        result = run_tool(library, out)
        with open(os.path.join(out, "index.txt"), "rb") as f:
            check(result.returncode == 1 and f.read() == before, "failed run leaves the old files")
        check(not os.path.exists(os.path.join(out, "bad-song.txt")), "failed run writes no song file")
        os.remove(os.path.join(library, "bad-song.song"))
        if binary:
            test_firmware_reads(binary, out)


def run_firmware(binary, folder):
    return subprocess.run([binary, folder], capture_output=True, text=True)


def test_firmware_reads(binary, out):
    """The C++ test reads the tool's output with the firmware's own code (testServerData)."""
    result = run_firmware(binary, out)
    check(result.returncode == 0 and "server data: 2 songs checked" in result.stdout,
          f"firmware reads the tool's output: {result.stdout[-300:]} {result.stderr[-500:]}")
    with open(os.path.join(out, "index.txt"), encoding="utf-8") as f:
        index = f.read()
    damaged = [
        # An alias written as typed, not normalised: the stick could never match it
        ("alias not normalised", index.replace("|testlied;test song|", "|Testlied;test song|"), None),
        # Another entry's alias is this one's English name: neither finds it
        ("name taken by an alias", index.replace("|anderes lied|", "|anderes lied;a test song|"), None),
        # A built-in song's name: the stick finds the built-in first
        ("built-in name", index.replace("|Ein Testlied|", "|Bruder Jakob|"), None),
        # A song file whose bars don't add up
        ("unsound song file", index, ("test-song.txt", lambda t: t.replace("G4:6", "G4:4"))),
    ]
    for what, new_index, song_change in damaged:
        with tempfile.TemporaryDirectory() as tmp:
            for name in os.listdir(out):
                with open(os.path.join(out, name), encoding="utf-8") as f:
                    text = f.read()
                if name == "index.txt":
                    text = new_index
                elif song_change and name == song_change[0]:
                    text = song_change[1](text)
                with open(os.path.join(tmp, name), "w", encoding="utf-8", newline="\n") as f:
                    f.write(text)
            check(new_index != index or song_change, f"{what}: the damage was applied")
            result = run_firmware(binary, tmp)
            check(result.returncode != 0, f"firmware test refuses {what}: {result.stdout[-200:]}")


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else None
    test_normalise()
    test_parser()
    test_constants_match_firmware()
    test_built_ins()
    test_sound()
    test_good_song()
    test_brief_cases()
    test_settled()
    test_more_checks()
    test_english_lilypond()
    test_sizes()
    test_main(binary)
    print(f"music tool tests: {checks} checks, {failures} failures"
          + ("" if binary else " (firmware reading skipped: no test binary given)"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
