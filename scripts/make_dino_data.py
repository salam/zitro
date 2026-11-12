#!/usr/bin/env python3
"""Builds the dinosaur fact sheets in webpage2/data/dino/ from Wikidata and Wikimedia Commons.

Automatic tier: every genus under Dinosauria (modern birds excluded), plus early birds, pterosaurs and
the marine reptiles of the dinosaur age. Facts come from Wikidata only: length, height, wingspan, mass,
the time range and the year of naming. Nothing is written by an AI and nothing is averaged: values that
disagree are shown as a range from the smallest to the largest.

Pictures: one of Wikidata's pictures (P18) of the genus, the first whose Commons file name, description
or categories name the genus or one of its names (see choose_picture); if none does, the first, marked
CHECK on the contact sheets because it may show another animal. It is used only if it passes every check
(see picture_problem): a free licence, a named author, no Commons category from DINO_CATEGORY_DENY, not
turned down after looking (make_dino_rejects.py), and pinned by SHA-1 after looking (make_dino_pins.py).
A genus whose picture fails gets none, and its sheet is kept only if it still has two fact rows.

Curated tier (make_dino_curated.py, about 50 of the best-known genera): a picture chosen by hand, and facts
and a fun fact from Wikipedia sentences. Each sentence is checked against the revision it was copied from
(check_fact, check_fun) and the run stops on any that doesn't stand there. Numbers merge with Wikidata's into
one range (apply_curated). The fun facts and speeds were chosen, translated and shortened by the AI that built
the data; no person reviewed them.

Output, read by sketch/sketch_jan12a/dino_core.h:
  index-<pp>.txt  ZDX1, <pp> the first two characters of a normalised name ([a-z0-9], else "_")
  <slug>.txt      ZDS1, one sheet per genus
  <slug>.jpg      its picture, baseline JPEG, at most 240x117
  credits.html    maker, licence and file link of every picture

Run with a Python that has Pillow, and ImageMagick for the contact sheets:
  /opt/homebrew/bin/python3 -I scripts/make_dino_data.py --cache <dir>
      Builds everything. [--no-pictures] builds the facts only.
  ... --contact-sheets <dir>   Writes captioned sheets of 48 pictures each, exactly as prepared, of every
                               picture that passes the checks before the pin (dino-NN.png), and of those that
                               need a look now: new, changed or marked CHECK (dino-check-NN.png). This is how
                               they are looked at.
  ... --pin                    Pins the SHA-1 of every picture that was on the last contact sheets and still
                               passes, once they have been looked at. A changed file is refused from then on.
Wikimedia throttles hard: everything goes through Client (1 s pause, disk cache, back-off on 429).
"""
import argparse
import html
import io
import json
import os
import re
import subprocess
import shutil
import sys
import unicodedata
import urllib.parse
from html import escape
from collections import Counter
from decimal import ROUND_HALF_UP, Decimal, InvalidOperation

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_timeline_data as tl  # noqa: E402

OUT_DIR = os.path.join(tl.ROOT, "webpage2", "data", "dino")
CURATED = os.path.join(tl.ROOT, "scripts", "make_dino_curated.py")
REJECTS = os.path.join(tl.ROOT, "scripts", "make_dino_rejects.py")
PINS = os.path.join(tl.ROOT, "scripts", "make_dino_pins.py")
MANIFEST = "dino-contact-sheets.json"  # In the cache folder: QID -> SHA-1 of every picture on the last sheets
SPARQL = "https://query.wikidata.org/sparql"
LIMITS = {"index": 16384, "sheet": 2048, "picture": 32768}
FIELD_LIMIT = 160  # Bytes of one text field on the device
GENUS = "Q34740"
SPECIES = "Q7432"
TEXT_FIELDS = ("era", "meaning", "diet", "found")
NAME_LIMIT = 60  # Characters of one index name; the device folds a query in 80 bytes
IMAGE_W, IMAGE_H = 240, 117  # DINO_IMAGE_W, DINO_IMAGE_H: the screen above the footer
FILL_KEEP = 0.8  # A picture is cropped to fill the screen only if that keeps 80 % of it; else it is contained
COMMONS_CHUNK = 10  # Files per Commons query: the 500 categories of one answer are shared by all its files
PER_CONTACT_SHEET = 48
# Second net, on the Commons categories of every picture; the real check is looking at them
# Each added word starts at a word boundary, like tl's \bblood\b: "dead" must not refuse "Ardeadactylus"
DINO_CATEGORY_DENY = re.compile(
    tl.CATEGORY_DENY.pattern + r"|\b(?:carcass|prey|predation|kill|killing|hunting|blood|wound|fight|attack|dead|death|"
    r"scavenging)", re.I)
# Author fields that name nobody: unknowns, the licence or "own work" instead of a maker, Commons boilerplate
NO_AUTHOR = re.compile(r"^\W*(unknown|anonym|unbekannt|author unknown|not known|no machine-readable author|own work|"
                       r"public domain|pd\b|i would appreciate being notified)", re.I)
# Author texts that say the user linked in the field made it; under any other non-name the link may be the uploader's
MADE_BY_LINKED_USER = re.compile(r"^\W*(own work|i would appreciate being notified)", re.I)
USER_LINK = re.compile(r"(?:\bUser:|Special:EmailUser/)([^\"'&/#?<>|\s]+)")
NAME_TEXT = re.compile(r"[^a-z]+")
ASSUMED = re.compile(r"^No machine-readable author provided\.\s*(.+?)\s+assumed\b", re.I | re.S)
# Letters that NFKD doesn't take apart, and punctuation with a plain ASCII twin
ASCII_TWINS = {"\u00f8": "o", "\u00d8": "O", "\u00df": "ss", "\u0142": "l", "\u0141": "L", "\u00e6": "ae",
               "\u00c6": "AE", "\u0153": "oe", "\u0152": "OE", "\u0111": "d", "\u0110": "D", "\u00f0": "d",
               "\u00fe": "th", "\u0131": "i", "\u2013": "-", "\u2014": "-", "\u2010": "-", "\u2011": "-",
               "\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"', "\u00a0": " ", "\u00b7": "-"}

# Unit item -> (kind, factor to cm for a length or to g for a mass). Every unit was checked on its item.
UNITS = {
    "Q11573": ("length", 100),  # metre
    "Q174728": ("length", 1),  # centimetre
    "Q174789": ("length", 0.1),  # millimetre
    "Q3710": ("length", 30.48),  # foot
    "Q218593": ("length", 2.54),  # inch
    "Q11570": ("mass", 1000),  # kilogram
    "Q41803": ("mass", 1),  # gram
    "Q191118": ("mass", 1e6),  # tonne
    "Q100995": ("mass", 453.59237),  # pound
}
# Largest value that is believable for the group of animals; anything above is a data error, not a fact
PLAUSIBLE = {"length": 6000, "height": 3000, "wingspan": 2000, "mass": 200_000_000}  # Guards against unit errors only
QUANTITY_PROPS = (("length", "P2043"), ("height", "P2048"), ("wingspan", "P2050"), ("mass", "P2067"))
# Parser limits in dino_core.h
LIMIT_OF = {"length": (1, 100000), "height": (1, 100000), "wingspan": (1, 100000), "mass": (1, 999999999999)}

PERIOD_NAMES = {  # Wikidata item -> German, English
    "Q76402": ("Perm", "Permian"),
    "Q47158": ("Trias", "Triassic"),
    "Q45805": ("Jura", "Jurassic"),
    "Q44626": ("Kreide", "Cretaceous"),
    "Q55810": ("Paläogen", "Paleogene"),
}
OUTSIDE = "outside the age of the dinosaurs"  # lived_of's answer for a taxon that is no fit
LIVED_OLDEST = 260_000_000  # Years ago: nothing in these groups is older
LIVED_YOUNGEST = 55_000_000  # ... or younger: a taxon outside the dinosaur age is a data error
NAMED_FIRST, NAMED_LAST = 1750, 2026
MIN_PRECISION = 3  # A million years
AGE_PRECISION = 2  # The geological ages that P523 and P524 point at are dated more coarsely

GROUP_NAMES = ["dinosaur", "early bird", "pterosaur", "marine reptile"]
DROPS = Counter()  # reason -> count
DROP_LINES = []


def drop(what, reason):
    """Log a value that did not make it, with the reason."""
    DROPS[reason] += 1
    DROP_LINES.append("  dropped %s: %s" % (what, reason))


# ---- Pure functions (tested without downloads) ----


def classify(qid, members):
    """The group of a genus: 0 dinosaur, 1 early bird, 2 pterosaur, 3 marine reptile, None for none.
    Modern birds are excluded first: they are dinosaurs, but they are no sheet of ours."""
    if qid in members.get("modern_birds", ()):
        return None
    if qid in members.get("early_bird", ()):
        return 1
    if qid in members.get("dinosaur", ()):
        return 0
    if qid in members.get("pterosaur", ()):
        return 2
    if qid in members.get("marine", ()):
        return 3
    return None


def convert(amount, unit_qid, kind):
    """A Wikidata amount in its unit as whole cm (length) or g (mass); None for an unknown unit or a
    value that is no positive number."""
    unit = UNITS.get(unit_qid)
    if not unit or unit[0] != kind:
        return None
    try:
        value = Decimal(amount) * Decimal(str(unit[1]))
    except (InvalidOperation, TypeError, ValueError):
        return None
    if not value.is_finite():
        return None
    result = int(value.quantize(Decimal(1), rounding=ROUND_HALF_UP))
    return result if result >= 1 else None


INEXACT_UNITS = {"Q3710", "Q218593", "Q100995"}  # Foot, inch, pound: converted, they gain digits nobody measured


def significant_digits(amount):
    """How many significant digits a Wikidata amount has: "18" 2, "5.50" 3, "38000" 2 (the zeros of a whole
    number only fill places), and at least 2, so "70" keeps its 7 and 0 and doesn't become "a few tens"."""
    whole, _, fraction = amount.lstrip("+-").partition(".")
    digits = (whole + fraction).lstrip("0") if fraction else whole.lstrip("0").rstrip("0")
    return max(2, len(digits))


def at_source_precision(amount, unit_qid, kind):
    """convert(), rounded to as many significant digits as the amount has where the unit is a foot, inch or
    pound: 18 ft is 549 cm exactly but 550 cm here (5,5 m), 70 ft 2100 cm (21 m), 38000 lb 17 t. A metric
    amount stays exact. Whole cm or g, or None like convert()."""
    result = convert(amount, unit_qid, kind)
    if result is None or unit_qid not in INEXACT_UNITS:
        return result
    exact = Decimal(amount) * Decimal(str(UNITS[unit_qid][1]))
    place = exact.adjusted() - significant_digits(amount) + 1  # The power of ten of the last digit kept
    rounded = (exact.scaleb(-place).quantize(Decimal(1), rounding=ROUND_HALF_UP)).scaleb(place)
    result = int(rounded.quantize(Decimal(1), rounding=ROUND_HALF_UP))
    return result if result >= 1 else None


def value_range(values):
    """(smallest, largest) of the values that disagree, never an average; None for no values."""
    return (min(values), max(values)) if values else None


ERA_TOLERANCE = 1_000_000  # Wikidata's date precision: a range that enters a period by less is not in it


def era_text(older, younger, periods):
    """("Kreide", "Cretaceous") for a time range, or ("Jura bis Kreide", "Jurassic to Cretaceous").
    periods: (older, younger, German, English) in years ago. A boundary belongs to the period it ends,
    and a range that crosses a boundary by less than a million years (the precision of the dates)
    stays in its period: 82-65 Ma is the Cretaceous, because Wikidata rounds its end from 66 Ma."""
    periods = sorted(periods, reverse=True)
    first = last = None
    for i, (p_older, p_younger, _, _) in enumerate(periods):
        if first is None and p_younger < older - ERA_TOLERANCE <= p_older:
            first = i
        if last is None and p_younger <= younger + ERA_TOLERANCE < p_older:
            last = i
    if first is None or last is None:
        return None
    first = min(first, last)  # A range of under two million years that straddles a boundary
    if first == last:
        return periods[first][2], periods[first][3]
    return "%s bis %s" % (periods[first][2], periods[last][2]), "%s to %s" % (periods[first][3], periods[last][3])


def normalise(name):
    """Same rules as eduNormaliseName(..., false), copied from make_protein_data.normalise.
    None if a character isn't allowed."""
    for a, b in (("ä", "ae"), ("ö", "oe"), ("ü", "ue"), ("Ä", "ae"), ("Ö", "oe"),
                 ("Ü", "ue"), ("ß", "ss")):
        name = name.replace(a, b)
    out = []
    for c in name:
        if c in "-_ \t":
            if out and out[-1] != " ":
                out.append(" ")
        elif c in "|;" or ord(c) < 0x20 or ord(c) >= 0x7F:
            return None
        else:
            out.append(c.lower())
    return "".join(out).strip() or None


def slug_of(genus):
    """The file name of a genus: lower case [a-z0-9_], at most 40 characters."""
    folded = unicodedata.normalize("NFKD", genus).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]", "_", folded)[:40]


def prefix_of(name):
    """The first two characters of a normalised name, each [a-z0-9] or else "_"."""
    return "".join(c if c in "abcdefghijklmnopqrstuvwxyz0123456789" else "_" for c in (name + "__")[:2])


def index_files(entries):
    """{prefix: file text} from (normalised name, slug). A name that points to two slugs is dropped and
    logged, except that a genus name (the name is the slug) wins over an alias. ValueError if a file
    is over the device limit."""
    slugs = {}
    for name, slug in entries:
        slugs.setdefault(name, set()).add(slug)
    files = {}
    for name in sorted(slugs):
        candidates = slugs[name]
        if len(candidates) > 1:
            own = [s for s in candidates if s == name]
            if len(own) != 1:
                drop("name '%s'" % name, "points to %d genera" % len(candidates))
                continue
            candidates = set(own)
        if len(name) > NAME_LIMIT or any(not 0x20 <= ord(c) <= 0x7E for c in name):
            raise ValueError("index name %r can't be written" % name)
        files.setdefault(prefix_of(name), []).append((name, next(iter(candidates))))
    texts = {}
    for prefix, lines in files.items():
        text = "ZDX1\t%s\t%d\n" % (prefix, len(lines)) + "".join("%s\t%s\n" % line for line in lines)
        if len(text.encode()) > LIMITS["index"]:
            raise ValueError("index-%s.txt is %d bytes, the limit is %d" % (prefix, len(text.encode()), LIMITS["index"]))
        texts[prefix] = text
    return texts


def _device_text(text, what, ascii_only=False):
    """The text if the device can show it: U+0020 to U+05FF without DEL and C1, at most 160 bytes."""
    if not isinstance(text, str) or not text:
        raise ValueError("%s is empty" % what)
    for ch in text:
        code = ord(ch)
        if not 0x20 <= code <= 0x5FF or 0x7F <= code <= 0x9F:
            raise ValueError("%s has U+%04X, outside the device font: %r" % (what, code, text))
        if ascii_only and code > 0x7E:
            raise ValueError("%s has U+%04X, the footer font is ASCII only: %r" % (what, code, text))
    if len(text.encode()) > FIELD_LIMIT:
        raise ValueError("%s is %d bytes, the limit is %d" % (what, len(text.encode()), FIELD_LIMIT))
    return text


def _both_languages(value, what):
    """(de, en) of a {"de": ..., "en": ...} where one language may be missing: the other fills it."""
    de, en = value.get("de"), value.get("en")
    if not de and not en:
        raise ValueError("%s has no text" % what)
    return _device_text(de or en, what), _device_text(en or de, what)


def sheet_text(sheet):
    """The ZDS1 text of a sheet. ValueError for anything the device would refuse."""
    slug = sheet["slug"]
    if not re.fullmatch(r"[a-z0-9_]{1,40}", slug):
        raise ValueError("bad slug %r" % slug)
    if not re.fullmatch(r"Q[1-9][0-9]{0,10}", sheet["qid"]):
        raise ValueError("bad QID %r" % sheet["qid"])
    lines = ["ZDS1\t%s\t%s" % (slug, sheet["qid"]), "name\t" + _device_text(sheet["name"], "name")]
    if sheet["group"] not in (0, 1, 2, 3):
        raise ValueError("bad group %r" % sheet["group"])
    lines.append("group\t%d" % sheet["group"])
    image = sheet.get("image")
    if image:
        credit = _device_text(image["credit"], "picture credit", True) if image.get("credit") else "-"
        if len(credit.encode()) > 80:
            raise ValueError("picture credit is over 80 bytes")
        if not 1 <= image["length"] <= LIMITS["picture"]:
            raise ValueError("picture size %d" % image["length"])
        if not (1 <= image["width"] <= IMAGE_W and 1 <= image["height"] <= IMAGE_H):
            raise ValueError("picture %dx%d is over %dx%d" % (image["width"], image["height"], IMAGE_W, IMAGE_H))
        lines.append("image\t%d\t%d\t%d\t%08x\t%s" % (image["length"], image["width"], image["height"], image["hash"], credit))

    def number(key, minimum, maximum, older=False):
        value = sheet.get(key)
        if not value:
            return
        lo, hi = value
        if not (minimum <= lo <= maximum and minimum <= hi <= maximum):
            raise ValueError("%s %r is outside %d..%d" % (key, value, minimum, maximum))
        if lo < hi if older else lo > hi:
            raise ValueError("%s %r is the wrong way round" % (key, value))
        lines.append("%s\t%d\t%d" % (key, lo, hi))

    for key, (minimum, maximum) in (("length", (1, 100000)), ("height", (1, 100000)), ("wingspan", (1, 100000)),
                                    ("mass", (1, 999999999999)), ("speed", (1, 10000)), ("egg", (1, 10000))):
        number(key, minimum, maximum)
    number("lived", 1, 5_000_000_000, older=True)
    if sheet.get("named"):
        if not 1 <= sheet["named"] <= 9999:
            raise ValueError("named %r" % sheet["named"])
        lines.append("named\t%d" % sheet["named"])
    for key in TEXT_FIELDS:
        if sheet.get(key):
            lines.append("%s\t%s\t%s" % ((key,) + _both_languages(sheet[key], key)))
    if sheet.get("fun"):
        de, en = _both_languages(sheet["fun"], "fun")
        lines.append("fun\t%s\t%s\t%s" % (de, en, _device_text(sheet["fun"].get("source"), "fun source", True)))
    if sheet.get("source"):
        lines.append("source\t" + _device_text(sheet["source"], "source", True))
    lines.append("end")
    text = "\n".join(lines) + "\n"
    if len(text.encode()) > LIMITS["sheet"]:
        raise ValueError("%s.txt is %d bytes, the limit is %d" % (slug, len(text.encode()), LIMITS["sheet"]))
    return text


def worth_showing(sheet):
    """The rule of dinoWorthShowing: its own picture or at least two fact rows."""
    if sheet.get("image"):
        return True
    rows = sum(1 for key in ("length", "height", "wingspan", "mass", "speed", "egg", "lived", "named",
                             "era", "meaning", "diet", "found") if sheet.get(key))
    return rows >= 2



# ---- Curated tier: facts from Wikipedia sentences, checked against the revision they were copied from ----

FACT_KINDS = {"length": "length", "height": "length", "wingspan": "length", "egg": "length", "mass": "mass",
              "speed": "speed"}  # Numeric field -> the kind its quote must state
CURATED_TEXT = ("meaning", "diet", "found")
CURATED_TAGS = {"fossil", "skeleton", "artwork", "model"}  # What a hand-chosen picture shows
# Unit -> (kind, factor to cm, g or tenths of km/h). Case matters ("t" is a tonne, "T" is not), and the longer
# spelling comes first. "tons" is left out: it may be short tons.
QUANTITY_UNITS = [
    ("km/h|Kilometern? pro Stunde|Stundenkilometern?|kilomet(?:re|er)s per hour", "speed", Decimal(10)),
    ("mph|miles per hour", "speed", Decimal("16.09344")),
    ("m/s|Metern? pro Sekunde|met(?:re|er)s per second", "speed", Decimal(36)),
    ("cm|Zentimetern?|centimet(?:re|er)s?", "length", Decimal(1)),
    ("mm|Millimetern?|millimet(?:re|er)s?", "length", Decimal("0.1")),
    ("m|Metern?|met(?:re|er)s?", "length", Decimal(100)),
    ("ft|feet|foot", "length", Decimal("30.48")),
    ("kg|Kilogramm|kilogram(?:me)?s?", "mass", Decimal(1000)),
    ("t|MT|Tonnen|Tonne|tonnes?", "mass", Decimal(1000000)),
    ("lbs?|pounds?", "mass", Decimal("453.59237")),
]
# A written number. How its separator reads is decided by _decimal
NUM = r"(?<![\d.,])(\d{1,3}(?:[.,]\d{3})+(?![\d])|\d+(?:[.,]\d+)?)"
DASHES = {"\u2013": "-", "\u2014": "-", "\u2212": "-", "\u00a0": " ", "\u2009": " ", "\u202f": " "}
QUANTITY = re.compile(NUM + r"(?:\s*(?:-|bis|to)\s*" + NUM + r")?\s*(" + "|".join(
    "(?:%s)" % unit for unit, _, _ in QUANTITY_UNITS) + r")(?![\w/])")
AGO = re.compile(NUM + r"(?:\s*(?:-|bis|to|and|und)\s*" + NUM + r")?\s*(?:million|Millionen|Mio\.)", re.I)
NUMBER_WORDS = {"two": 2, "three": 3, "four": 4, "five": 5, "six": 6, "seven": 7, "eight": 8, "nine": 9, "ten": 10,
                "eleven": 11, "twelve": 12, "twenty": 20, "thirty": 30, "forty": 40, "fifty": 50, "hundred": 100,
                "zwei": 2, "drei": 3, "vier": 4, "f\u00fcnf": 5, "sechs": 6, "sieben": 7, "acht": 8, "neun": 9,
                "zehn": 10, "elf": 11, "zw\u00f6lf": 12, "zwanzig": 20, "dreissig": 30, "drei\u00dfig": 30,
                "vierzig": 40, "f\u00fcnfzig": 50, "hundert": 100}
SCALE_WORDS = {"thousand": 10**3, "tausend": 10**3, "million": 10**6, "millions": 10**6, "millionen": 10**6,
               "mio": 10**6, "billion": 10**9, "billions": 10**9, "milliarde": 10**9, "milliarden": 10**9, "mrd": 10**9}
WRITTEN_NUMBER = re.compile(NUM + r"(?:\s*(%s)(?![a-z]))?" % "|".join(sorted(SCALE_WORDS, key=len, reverse=True)), re.I)


def _plain(text):
    """Dashes and special spaces as their ASCII twins, runs of spaces as one."""
    text = "".join(DASHES.get(c, c) for c in text or "")
    return re.sub(r"\s+", " ", text).strip()


THOUSANDS = {"de": ".", "en": ","}  # The other separator is the decimal mark in that language


def _decimal(digits, lang=None, scaled=False):
    """The value of a written number, read one way only, or None. Before a scale word ("3,5 Millionen") the
    separator is the decimal mark. Otherwise "8,8", "15.3" and "0,750" have one; "1.400.000" has thousands;
    and "1.125" or "1,400", one separator before three digits, is 1125 in German and 1.125 in English (or
    the other way round for the comma), so it needs the language of the text and is None without it."""
    groups = re.split(r"[.,]", digits)
    separators = re.findall(r"[.,]", digits)
    if not separators:
        return Decimal(digits)
    if len(separators) > 1:
        thousands = len(set(separators)) == 1 and all(len(g) == 3 for g in groups[1:])
        return Decimal("".join(groups)) if thousands and not scaled else None
    head, tail = groups
    if scaled or len(tail) != 3 or not head.strip("0"):
        return Decimal(head + "." + tail)
    if lang not in THOUSANDS:
        return None
    return Decimal(head + tail) if separators[0] == THOUSANDS[lang] else Decimal(head + "." + tail)


def _whole(value):
    return int(value.quantize(Decimal(1), rounding=ROUND_HALF_UP))


def _quantity(quote, lang=None):
    """(kind, lo, hi) of the one quantity a quote states, exact in cm, g or tenths of km/h; None otherwise.
    lang: the language the quote is written in, which decides how "1.125" reads (see _decimal)."""
    found = list(QUANTITY.finditer(_plain(quote)))
    if len(found) != 1:
        return None
    first, second, unit = found[0].groups()
    for pattern, kind, factor in QUANTITY_UNITS:
        if re.fullmatch(pattern, unit):
            lo, hi = _decimal(first, lang), _decimal(second, lang) if second else _decimal(first, lang)
            if lo is None or hi is None:
                return None
            lo, hi = lo * factor, hi * factor
            return (kind, lo, hi) if 0 < lo <= hi else None
    return None


def parse_quantity(quote, lang=None):
    """("length", cm, cm), ("mass", g, g) or ("speed", tenths of km/h, ...) of the one quantity a quote
    states: "20-30 km/h", "bis zu 13 Metern", "8,8 Tonnen". None for no quantity, two, a range the wrong
    way round, or a number that reads two ways in the language given ("1.125 m" without one)."""
    found = _quantity(quote, lang)
    if not found:
        return None
    kind, lo, hi = found
    lo, hi = _whole(lo), _whole(hi)
    return (kind, lo, hi) if lo >= 1 else None


def parse_ago(quote):
    """(older, younger) in years ago of "155-145 million years ago" or "vor 68 bis 66 Mio. Jahren"; None
    if it states no time in millions of years, or two."""
    found = list(AGO.finditer(_plain(quote)))
    if len(found) != 1:
        return None
    first, second = found[0].groups()
    values = [_decimal(d, scaled=True) for d in (first, second or first)]
    if None in values:
        return None
    a, b = (_whole(v * 10**6) for v in values)
    return (max(a, b), min(a, b)) if min(a, b) >= 1 else None


def written_numbers(text, lang):
    """Every number a text in that language states: digits (with a scale word: "66 million"), number words,
    scale words. A number that can't be read one way is kept as its text, so it matches nothing else."""
    values = set()
    for match in WRITTEN_NUMBER.finditer(_plain(text)):
        digits, scale = match.groups()
        value = _decimal(digits, lang, scaled=bool(scale))
        values.add(digits if value is None else value * (SCALE_WORDS[scale.lower()] if scale else 1))
    for word in re.findall(r"[^\W\d_]+", text.lower()):
        if word in NUMBER_WORDS:
            values.add(Decimal(NUMBER_WORDS[word]))
        elif word in SCALE_WORDS:
            values.add(Decimal(SCALE_WORDS[word]))
    return values


def _text_problem(text, language):
    """Why a curated text can't go on the device as it is, or None: the device font, the field size, and
    the Swiss spelling of German ("ss", never the sharp s)."""
    try:
        _device_text(text, "text")
    except ValueError as error:
        return str(error)
    if language == "de" and "\u00df" in text:
        return "Swiss spelling: ss, not \u00df, in %r" % text
    return None


def rejects_for(rejects, curated):
    """The rejects that apply. A reject names a genus, but it was a reject of the picture Wikidata gave it;
    a curated genus gets a picture chosen by hand and looked at on the contact sheet of the curated tier, so
    that reject doesn't refuse it. Licence, author, categories and the pin are still checked."""
    return {qid: reason for qid, reason in rejects.items() if qid not in curated}


SENTENCE_LIMIT = 600  # Characters. Longer, it is a paragraph, and a quote somewhere in it proves nothing


def _sentence_problem(sentence):
    """Why a stored text is not one sentence, or None: too long, or a heading ("==") or a picture caption
    or table ("|", as wikitext_plain leaves them) inside it."""
    if len(sentence) > SENTENCE_LIMIT:
        return "the sentence has %d characters, more than one sentence" % len(sentence)
    if "==" in sentence or "|" in sentence:
        return "the sentence holds a heading, a picture caption or a table"
    return None


def _quote_in(quote, sentence):
    """True if the quote stands in the sentence as written, not as the tail or start of a longer number or
    word: "3 m" is not in "13 m", "8 Tonnen" not in "2,8 Tonnen", "13 m" not in "13 mm"."""
    quote, sentence = quote.casefold(), sentence.casefold()
    pattern = re.escape(quote)
    if quote[:1].isdigit():
        pattern = r"(?<![\d.,])" + pattern
    elif quote[:1].isalpha():
        pattern = r"(?<!\w)" + pattern
    if quote[-1:].isdigit():
        pattern += r"(?![\d]|[.,]\d)"
    elif quote[-1:].isalpha():
        pattern += r"(?!\w)"
    return re.search(pattern, sentence) is not None


def era_periods(de, en):
    """The periods an era names, oldest first, as (German, English) pairs: "Kreide" / "Cretaceous", or "Jura bis
    Kreide" / "Jurassic to Cretaceous" as era_text writes them. None for any other text."""
    parts_de, parts_en = (de or "").split(" bis "), (en or "").split(" to ")
    order = list(PERIOD_NAMES.values())  # Oldest first
    pairs = list(zip(parts_de, parts_en))
    if len(parts_de) != len(parts_en) or len(pairs) > 2 or any(pair not in order for pair in pairs):
        return None
    if len(pairs) == 2 and order.index(pairs[0]) >= order.index(pairs[1]):
        return None
    return pairs


def _source_problem(item):
    if item.get("lang") not in ("de", "en") or not item.get("title") or not isinstance(item.get("revid"), int):
        return "needs lang (de or en), title and revid"
    return None


def check_fact(fact, revision_plain):
    """None if a curated fact stands in its revision, otherwise why not. The sentence must be in the
    revision and the quote in the sentence; a number must state the field's kind, a year named its four
    digits; a text needs de and en, and its source-language text (or its quote) must be in the sentence; an
    era is one of the generator's periods (or two, "Jura bis Kreide"), and its quote names them ("Early
    Cretaceous", "Unterkreide")."""
    problem = _source_problem(fact)
    if problem:
        return problem
    field = fact.get("field")
    sentence = _plain(fact.get("sentence"))
    if not sentence or sentence not in _plain(revision_plain):
        return "the sentence is not in that revision"
    problem = _sentence_problem(sentence)
    if problem:
        return problem
    if field in CURATED_TEXT:
        if not fact.get("de") or not fact.get("en"):
            return "%s needs de and en" % field
        for language in ("de", "en"):
            problem = _text_problem(fact[language], language)
            if problem:
                return "%s %s: %s" % (field, language, problem)
        quote = _plain(fact.get("quote") or fact[fact["lang"]])
    elif field in FACT_KINDS or field in ("named", "lived", "era"):
        quote = _plain(fact.get("quote"))
    else:
        return "unknown field %r" % field
    if not quote or not _quote_in(quote, sentence):
        return "the quote %r is not in the sentence" % quote
    if field == "era":
        periods = era_periods(fact.get("de"), fact.get("en"))
        if not periods:
            return "era %r / %r is no period of the generator (%s)" % (
                fact.get("de"), fact.get("en"), ", ".join(en for _, en in PERIOD_NAMES.values()))
        names = [pair[0] if fact["lang"] == "de" else pair[1] for pair in periods]
        missing = [name for name in names if name.casefold() not in quote.casefold()]
        return "the quote %r doesn't name %s" % (quote, " and ".join(missing)) if missing else None
    if field in FACT_KINDS:
        parsed = parse_quantity(quote, fact["lang"])
        if not parsed or parsed[0] != FACT_KINDS[field]:
            return "%r states no %s" % (quote, FACT_KINDS[field])
    elif field == "named":
        if not re.fullmatch(r"\d{4}", quote) or not NAMED_FIRST <= int(quote) <= NAMED_LAST:
            return "a year named is four digits, %d to %d: %r" % (NAMED_FIRST, NAMED_LAST, quote)
        if not re.search(r"(?<!\d)%s(?!\d)" % quote, sentence):
            return "the sentence has no year %s" % quote
    elif field == "lived":
        if not parse_ago(quote):
            return "%r states no time in millions of years" % quote
    else:
        stated = written_numbers(sentence, fact["lang"])
        for language in ("de", "en"):
            extra = written_numbers(fact[language], language) - stated
            if extra:
                return "%s %s has numbers the sentence doesn't: %s" % (field, language, sorted(map(str, extra)))
    return None


def check_fun(fun, revision_plain):
    """None if a fun fact comes from one or two sentences of its revision, in de and en, and every number
    either language writes is in those sentences; otherwise why not. That it makes no new claim is not
    something a program can check: the one who writes it must."""
    problem = _source_problem(fun)
    if problem:
        return problem
    sentences = fun.get("sentences") or []
    if not 1 <= len(sentences) <= 2:
        return "a fun fact comes from one or two sentences, not %d" % len(sentences)
    text = _plain(revision_plain)
    for sentence in sentences:
        if not _plain(sentence) or _plain(sentence) not in text:
            return "a sentence is not in that revision: %r" % sentence[:60]
        problem = _sentence_problem(_plain(sentence))
        if problem:
            return problem
    if not fun.get("de") or not fun.get("en"):
        return "a fun fact needs de and en"
    for language in ("de", "en"):
        problem = _text_problem(fun[language], language)
        if problem:
            return "fun %s: %s" % (language, problem)
    stated = set().union(*(written_numbers(s, fun["lang"]) for s in sentences))
    for language in ("de", "en"):
        extra = written_numbers(fun[language], language) - stated
        if extra:
            return "fun %s has numbers the sentences don't: %s" % (language, sorted(map(str, extra)))
    return None


def merge_range(current, value, older=False):
    """One range of both, from the smallest to the largest (for years ago: the oldest to the youngest)."""
    if not current or not value:
        return current or value
    if older:
        return max(current[0], value[0]), min(current[1], value[1])
    return min(current[0], value[0]), max(current[1], value[1])


def apply_curated(sheet, entry, periods):
    """Merges an entry's checked facts into its sheet: a number widens Wikidata's range, a year named must
    agree with Wikidata's (one year can't be a range), a time range also gives the era, and a text field
    or the fun fact is taken as written. An era from a sentence only fills a gap: where a time range gives
    one, it is refused. The footer names the sources. ValueError for what can't merge."""
    from_wikidata = any(sheet.get(key) for key in ("length", "height", "wingspan", "mass", "lived", "named"))
    texts, lived, era_fact = {}, False, None
    for fact in entry.get("facts", []):
        field = fact["field"]
        if field in FACT_KINDS:
            _, lo, hi = _quantity(fact["quote"], fact["lang"])
            if field == "egg":  # cm to mm
                lo, hi = lo * 10, hi * 10
            sheet[field] = merge_range(sheet.get(field), (_whole(lo), _whole(hi)))
        elif field == "lived":
            sheet["lived"] = merge_range(sheet.get("lived"), parse_ago(fact["quote"]), older=True)
            lived = True
        elif field == "named":
            year = int(fact["quote"])
            if sheet.get("named") and sheet["named"] != year:
                raise ValueError("%s: named %d on Wikidata, %d in %s:%s" % (
                    sheet["name"], sheet["named"], year, fact["lang"], fact["title"]))
            sheet["named"] = year
        elif field in CURATED_TEXT:
            if field in texts:
                raise ValueError("%s: two facts for %s" % (sheet["name"], field))
            texts[field] = {"de": fact["de"], "en": fact["en"]}
        elif field == "era":
            if era_fact:
                raise ValueError("%s: two facts for era" % sheet["name"])
            era_fact = {"de": fact["de"], "en": fact["en"]}
        else:
            raise ValueError("%s: unknown field %r" % (sheet["name"], field))
    sheet.update(texts)
    if lived:
        era = era_text(sheet["lived"][0], sheet["lived"][1], periods)
        if not era:
            raise ValueError("%s: no period for %r" % (sheet["name"], sheet["lived"]))
        sheet["era"] = {"de": era[0], "en": era[1]}
    if era_fact:
        if sheet.get("era"):
            raise ValueError("%s: an era from a sentence, but its time range gives one already (%s)" % (
                sheet["name"], sheet["era"]["en"]))
        sheet["era"] = era_fact
    fun = entry.get("fun")
    if fun:
        title = ascii_fold(fun["title"])
        if not title:
            raise ValueError("%s: the fun fact's title can't be written in ASCII" % sheet["name"])
        sheet["fun"] = {"de": fun["de"], "en": fun["en"], "source": "Wikipedia (%s): %s" % (fun["lang"].upper(), title)}
    sources = (["Wikidata"] if from_wikidata else []) + (["Wikipedia"] if entry.get("facts") else [])
    if sources:
        sheet["source"] = ", ".join(sources)


def author_of(file_info):
    """The maker Commons names, or "" if it names nobody: "Unknown author", "Own work", "Public Domain",
    a year or Commons boilerplate are no name. Only where the text is "Own work" or the boilerplate "I would
    appreciate being notified...", both of which say the linked user made it, is the author field's own
    link to exactly one Commons user the maker. Under "Public Domain", a year or anything else the link may
    be the uploader's, so nobody is named. Commons' source field (Credit) is never taken."""
    text = (file_info.get("artist") or "").strip()
    assumed = ASSUMED.match(text)
    if assumed:
        return assumed.group(1).strip()
    if text and not NO_AUTHOR.match(text) and any(c.isalpha() for c in text):
        return text
    if not MADE_BY_LINKED_USER.match(text):
        return ""
    users = {urllib.parse.unquote(u).replace("_", " ").strip()
             for u in USER_LINK.findall(html.unescape(file_info.get("artist_html") or ""))}
    return users.pop() if len(users) == 1 else ""


def picture_problem(file_info, rejects, pins, qid, check_pin=True):
    """None if this genus may show this Commons file, otherwise why not. Every reason starts with the
    kind of check that refused it: file, licence, author, category, review or pin. check_pin=False is
    the check before looking: what goes on the contact sheets."""
    if not file_info:
        return "file: not on Commons"
    if not file_info.get("mime", "").startswith("image/"):
        return "file: not a picture (%s)" % file_info.get("mime")
    restricted = set(file_info.get("restrictions", "").split("|")) & {"trademarked", "insignia"}
    if restricted:
        return "licence: restricted beyond its licence (%s)" % ", ".join(sorted(restricted))
    problem = tl.licence_problem(dict(file_info, needs_credit=False))  # The author is checked next, always
    if problem:
        return "licence: " + problem
    if not author_of(file_info):
        return "author: names nobody (%r)" % (file_info.get("artist") or "")[:40]
    for category in file_info.get("categories", []):
        if DINO_CATEGORY_DENY.search(category):
            return "category: " + category
    if qid in rejects:
        return "review: " + rejects[qid]
    if check_pin and (not pins.get(qid) or not file_info.get("sha1") or pins[qid] != file_info["sha1"]):
        return "pin: not pinned" if not pins.get(qid) else "pin: the file changed since it was looked at"
    return None


def ascii_fold(text):
    """The text in plain ASCII, accents taken off, or None if a letter has no ASCII form."""
    text = "".join(ASCII_TWINS.get(c, c) for c in text or "")
    text = "".join(c for c in unicodedata.normalize("NFKD", text) if not unicodedata.combining(c))
    text = re.sub(r"\s+", " ", text).strip()
    return text if text and all(0x20 <= ord(c) <= 0x7E for c in text) else None


def short_author(text):
    """The maker as the bottom line shows it: no web or mail addresses, user-page markup or uploader
    phrases, which leave no room for the licence. credits.html keeps Commons' text."""
    uploader = re.match(r"^(?:the )?original uploader was (.+?) at \S.*$", text, re.I)
    if uploader:
        text = uploader.group(1)
    text = re.sub(r"email:[^\s()]+|[^\s()]+@[^\s()]+|(?:https?://|www\.)[^\s()]+", " ", text)
    text = re.sub(r"\(\s*User:[^)]*\)", " ", text)
    text = re.sub(r"^\s*(?:creator|author|artist)\s*:\s*", "", text, flags=re.I)
    text = re.sub(r"^(?:\w+:)*User:", "", text.strip())
    text = re.sub(r"\s*[,;]?\s*\)", ")", re.sub(r"\(\s*[,;]?\s*", "(", text))
    text = re.sub(r"\(\)", " ", text)
    return re.sub(r"\s+", " ", text).strip(" ,;.-")


def device_credit(file_info):
    """The bottom line's credit, in ASCII because the footer font has nothing else: "-" if the licence
    asks for none, else "maker, licence", or the licence alone if the maker can't be written in ASCII or
    doesn't fit. None if not even the licence can be written. credits.html always has all of it."""
    if not file_info["needs_credit"]:
        return "-"
    licence = ascii_fold(file_info["licence"] or file_info["code"])
    if not licence or len(licence) > tl.CREDIT_LIMIT:
        return None
    artist = ascii_fold(short_author(author_of(file_info)))
    credit = "%s, %s" % (artist, licence) if artist else licence
    return credit if len(credit) <= tl.CREDIT_LIMIT else licence


def fit_for(width, height):
    """Cropped to fill the screen ("cover") if that keeps at least FILL_KEEP of the picture's width or
    height, otherwise shown whole ("contain"): a skull or a fossil slab cut to a strip loses the animal."""
    box, aspect = IMAGE_W / IMAGE_H, width / height
    return "cover" if min(aspect / box, box / aspect) >= FILL_KEEP else "contain"


def prepare_picture(data, fit=None):
    """(baseline JPEG, (width, height)) at most IMAGE_W x IMAGE_H, cropped or contained as `fit` says or
    else as fit_for decides. Raises on a file Pillow can't read."""
    from PIL import Image

    if not fit:
        with Image.open(io.BytesIO(data)) as image:
            fit = fit_for(*image.size)
    return tl.prepare_image(data, fit, width=IMAGE_W, height=IMAGE_H)


# ---- Wikidata ----


def sparql(client, query):
    data = client.api(SPARQL, {"query": query})
    return data["results"]["bindings"]


def qid_of(uri):
    return uri.rsplit("/", 1)[-1]


def resolve_clades(client, clades):
    """Wikipedia titles -> lists of QIDs, printed. A title that Wikidata doesn't know stops the run."""
    wanted = {key: [titles] if isinstance(titles, str) else list(titles) for key, titles in clades.items()}
    found = tl.wiki_entities(client, sorted({t for titles in wanted.values() for t in titles}))
    roots = {}
    for key, titles in wanted.items():
        missing = [t for t in titles if t not in found]
        if missing:
            tl.die("clade %s: not found on Wikidata: %s" % (key, missing))
        roots[key] = [found[t]["id"] for t in titles]
        print("  clade %-12s %s" % (key, ", ".join("%s (%s)" % (found[t]["id"], t) for t in titles)))
    return roots


def genera_under(client, root):
    rows = sparql(client, "SELECT ?t WHERE { ?t wdt:P171+ wd:%s . ?t wdt:P105 wd:%s }" % (root, GENUS))
    return {qid_of(r["t"]["value"]) for r in rows}


def fetch_entities(client, ids):
    """QID -> entity (labels, aliases, claims), in batches of 50."""
    found = {}
    for chunk in tl.batches(sorted(ids)):
        data = client.api(tl.WIKIDATA_API, {"action": "wbgetentities", "ids": "|".join(chunk),
                                            "props": "labels|aliases|claims", "languages": "en|de"})
        for key, entity in data.get("entities", {}).items():
            if "missing" not in entity:
                found[key] = entity
    return found


def species_names(client, genera):
    """Genus QID -> names of its species: scientific names and the de and en labels and aliases."""
    names = {}
    for chunk in tl.batches(sorted(genera), 120):
        rows = sparql(client, """SELECT ?t ?n WHERE {
  VALUES ?t { %s }
  ?s wdt:P171 ?t . ?s wdt:P105 wd:%s .
  { ?s wdt:P225 ?n } UNION { ?s rdfs:label ?n . FILTER(lang(?n) IN ("en", "de")) }
    UNION { ?s skos:altLabel ?n . FILTER(lang(?n) IN ("en", "de")) }
}""" % (" ".join("wd:" + q for q in chunk), SPECIES))
        for row in rows:
            names.setdefault(qid_of(row["t"]["value"]), set()).add(row["n"]["value"])
    return names


def periods_of(client):
    """The five periods with their real boundaries, as years ago: (older, younger, German, English)."""
    entities = fetch_entities(client, set(PERIOD_NAMES))
    periods = []
    for qid, (de, en) in PERIOD_NAMES.items():
        date = tl.date_of(entities[qid], AGE_PRECISION, ["P580"])
        if isinstance(date, str) or date[1] is None:
            tl.die("period %s (%s) has no usable dates: %s" % (qid, en, date))
        periods.append((-date[0], -date[1], de, en))
    periods.sort(reverse=True)
    for (_, younger, _, a), (older, _, _, b) in zip(periods, periods[1:]):
        if younger != older:
            tl.die("periods %s and %s don't meet: %d and %d" % (a, b, younger, older))
    print("  periods: " + ", ".join("%s %d-%d" % (p[3], p[0], p[1]) for p in periods))
    return periods


def quantities(entity, prop, kind, what):
    """The converted values of one property, with a log line for each that can't be used."""
    values = []
    for claim in tl.statements(entity, prop):
        value = claim["mainsnak"]["datavalue"]["value"]
        unit = qid_of(value.get("unit", "1"))
        # A value in feet or pounds would look exact in m or t: it keeps only the digits its source has
        result = at_source_precision(value.get("amount", ""), unit, "mass" if kind == "mass" else "length")
        if result is None:
            drop("%s %s %s %s" % (what, prop, value.get("amount"), unit), "unknown unit or no positive number")
        elif result > PLAUSIBLE[kind]:
            drop("%s %s %d" % (what, kind, result), "implausible size")
        elif not LIMIT_OF[kind][0] <= result <= LIMIT_OF[kind][1]:
            drop("%s %s %d" % (what, kind, result), "outside the device range")
        else:
            values.append(result)
    return value_range(values)


def years_ago(entity, prop, min_precision):
    """Positive years ago of every usable date of a property."""
    years = []
    for claim in tl.statements(entity, prop):
        parsed = tl.wikidata_time(claim["mainsnak"]["datavalue"]["value"])
        if parsed and parsed[1] >= min_precision and parsed[0] < 0:
            years.append(-parsed[0])
    return years


def age_range(entity, prop, ages):
    """Years ago of the geological age that P523 or P524 name: (older, younger) from the age's own dates."""
    result = []
    for claim in tl.statements(entity, prop):
        value = claim["mainsnak"].get("datavalue", {}).get("value", {})
        if value.get("id") in ages:
            result.append(ages[value["id"]])
    return result


def lived_of(entity, ages, what):
    """(older, younger) in years ago. Statements that disagree give the widest range."""
    starts = years_ago(entity, "P580", MIN_PRECISION)
    ends = years_ago(entity, "P582", MIN_PRECISION)
    if not starts:
        starts = [a[0] for a in age_range(entity, "P523", ages)]
    if not ends:
        ends = [a[1] for a in age_range(entity, "P524", ages)]
    if not starts or not ends:
        return None
    older, younger = max(starts), min(ends)
    if older < younger:
        drop("%s lived %d-%d" % (what, older, younger), "starts after it ends")
        return None
    if older > LIVED_OLDEST or younger < LIVED_YOUNGEST:
        drop("%s lived %d-%d" % (what, older, younger), "outside the age of the dinosaurs")
        return OUTSIDE
    return older, younger


def named_of(entity, name, what):
    years = set()
    for claim in tl.statements(entity, "P225"):
        if claim["mainsnak"]["datavalue"]["value"] != name:
            continue
        for q in claim.get("qualifiers", {}).get("P574", []):
            parsed = tl.wikidata_time(q.get("datavalue", {}).get("value", {})) if q.get("snaktype") == "value" else None
            if parsed and parsed[1] >= 9:
                years.add(parsed[0])
    if len(years) > 1:
        drop("%s named %s" % (what, sorted(years)), "conflicting years")
        return 0
    if years and not NAMED_FIRST <= next(iter(years)) <= NAMED_LAST:
        drop("%s named %s" % (what, sorted(years)), "implausible year")
        return 0
    return next(iter(years)) if years else 0


def genus_name(entity):
    for claim in tl.statements(entity, "P225"):
        value = claim["mainsnak"]["datavalue"]["value"]
        if isinstance(value, str):
            return value
    return None


def ages_of(client, genera):
    """Geological age item -> (older, younger) in years ago, for every age that a P523 or P524 names."""
    wanted = set()
    for entity in genera.values():
        for prop in ("P523", "P524"):
            for claim in tl.statements(entity, prop):
                value = claim["mainsnak"].get("datavalue", {}).get("value", {})
                if isinstance(value, dict) and value.get("id"):
                    wanted.add(value["id"])
    ages = {}
    for qid, entity in fetch_entities(client, wanted).items():
        date = tl.date_of(entity, AGE_PRECISION, ["P580"])
        if isinstance(date, str) or date[1] is None:
            drop("age %s" % qid, "geological age has no usable dates")
        else:
            ages[qid] = (-date[0], -date[1])
    return ages


def build_sheet(qid, entity, group, ages, periods):
    name = genus_name(entity)
    if not name:
        drop(qid, "no scientific name (P225)")
        return None
    slug = slug_of(name)
    if not slug or normalise(name) is None:
        drop("%s %s" % (qid, name), "name can't be matched")
        return None
    sheet = {"slug": slug, "qid": qid, "name": name, "group": group, "image": None, "speed": None, "egg": None,
             "meaning": None, "diet": None, "found": None, "fun": None, "source": "Wikidata"}
    for kind, prop in QUANTITY_PROPS:
        sheet[kind] = quantities(entity, prop, kind, name)
    lived = lived_of(entity, ages, name)
    if lived == OUTSIDE:
        lived = None
        if group == 1:  # A bird from after the dinosaurs got past the exclusion of modern birds
            drop("%s %s" % (qid, name), "early bird that lived after the dinosaurs")
            return None
    sheet["lived"] = lived
    sheet["era"] = None
    if lived:
        era = era_text(lived[0], lived[1], periods)
        if era:
            sheet["era"] = {"de": era[0], "en": era[1]}
        else:
            drop("%s lived %d-%d" % (name, lived[0], lived[1]), "no period for the time range")
    sheet["named"] = named_of(entity, name, name)
    return sheet


def alias_names(entity):
    """The de and en labels and aliases of an entity."""
    names = []
    for language in ("de", "en"):
        label = entity.get("labels", {}).get(language)
        if label:
            names.append(label["value"])
        names += [a["value"] for a in entity.get("aliases", {}).get(language, [])]
    return names


def pictures_of(entity):
    """Every Commons file of Wikidata's pictures (P18), not deprecated, preferred rank first."""
    claims = [c for c in entity.get("claims", {}).get("P18", [])
              if c.get("rank") != "deprecated" and c.get("mainsnak", {}).get("snaktype") == "value"]
    claims.sort(key=lambda c: c.get("rank") != "preferred")  # Stable: Wikidata's order within a rank
    files = []
    for claim in claims:
        value = claim["mainsnak"].get("datavalue", {}).get("value")
        if isinstance(value, str) and value and value not in files:
            files.append(value)
    return files


def name_key(text):
    """Lower case words of letters only, separated and framed by single spaces; accents taken off, and
    run-together capitals split ("MicroraptorGui" is "microraptor gui")."""
    text = unicodedata.normalize("NFKD", text or "").encode("ascii", "ignore").decode()
    text = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", text).lower()
    return " " + NAME_TEXT.sub(" ", text).strip() + " "


def genus_words(sheets):
    """The one-word genus names of the sheets, as name_key writes them: what "another genus" can be."""
    return {key.strip() for key in (name_key(s["name"]) for s in sheets) if key.strip() and " " not in key.strip()}


def names_genus(texts, names, genera=frozenset()):
    """True if one text names one of the names as whole words ("Troodon formosus" and "Troodon_skull.jpg"
    name Troodon; "Troodontidae" and "Iguanodontia" are other taxa) and names no other genus of `genera`:
    a category or caption naming two genera says nothing about which one is shown. The genus' own names,
    synonyms among its aliases included, are never "another genus"."""
    keys = {name_key(n).strip() for n in names} - {""}
    others = set(genera) - {word for key in keys for word in key.split()}
    for text in texts:
        haystack = name_key(text)
        if any(" %s " % key in haystack for key in keys) and not set(haystack.split()) & others:
            return True
    return False


def picture_texts(name, file_info):
    return [name, file_info.get("description", ""), file_info.get("object", "")] + list(file_info.get("categories", []))


def naming(name, file_info, names, genera=frozenset()):
    """How strongly a Commons file says it shows the genus: 2 in its file name or a category, 1 only in
    its description or title, 0 not at all (see names_genus)."""
    if names_genus([name] + list(file_info.get("categories", [])), names, genera):
        return 2
    return 1 if names_genus([file_info.get("description", ""), file_info.get("object", "")], names, genera) else 0


def choose_picture(entity, infos, names, genera=frozenset()):
    """(Commons file, names it) for Wikidata's pictures of a genus: the one that names the genus or one
    of its names most strongly (see naming), the first in Wikidata's order among equals. If none names
    it, the first one, flagged False so the contact sheets mark it: it may show another animal. A file
    Commons doesn't know never wins by naming. There is no second try after the checks: a refused genus
    gets no picture, never one that nobody looked at."""
    files = pictures_of(entity)
    best, strength = (files[0] if files else None), 0
    for name in files:
        found = naming(name, infos[name], names, genera) if name in infos else 0
        if found > strength:
            best, strength = name, found
    return best, strength > 0


def commons_infos(client, names):
    """tl.commons_info in small batches. Commons shares 500 categories between all files of one answer
    and says "continue" when it cut them; a cut answer would hide categories, so it stops the run."""
    info = {}
    for chunk in tl.batches(sorted(names), COMMONS_CHUNK):
        found = tl.commons_info(client, chunk)
        # The same query as commons_info's, so this comes from the cache
        data = client.api(tl.COMMONS_API, {
            "action": "query", "titles": "|".join("File:" + n for n in chunk), "prop": "imageinfo|categories",
            "iiprop": "sha1|url|size|mime|extmetadata", "iiurlwidth": str(tl.THUMB_SOURCE_WIDTH),
            "cllimit": "max", "clshow": "!hidden"})
        # "iistart" only continues to older versions of a file; "clcontinue" means categories were cut
        if "clcontinue" in data.get("continue", {}):
            tl.die("Commons cut the categories for %s; use smaller batches" % chunk)
        # What commons_info leaves out: the author field's links, the description and the title
        renamed = {n["to"]: n["from"] for n in data.get("query", {}).get("normalized", [])}
        for page in data.get("query", {}).get("pages", {}).values():
            name = renamed.get(page.get("title", ""), page.get("title", ""))[len("File:"):]
            if name in found and page.get("imageinfo"):
                meta = page["imageinfo"][0].get("extmetadata", {})
                found[name]["artist_html"] = meta.get("Artist", {}).get("value", "")
                found[name]["description"] = tl.strip_html(meta.get("ImageDescription", {}).get("value", ""))
                found[name]["object"] = tl.strip_html(meta.get("ObjectName", {}).get("value", ""))
        info.update(found)
    return info


def load_reviewed():
    """(rejects, pins): QID -> reason, QID -> SHA-1. Plain literals, no code runs from them."""
    rejects = tl.load_literal(REJECTS) if os.path.exists(REJECTS) else {}
    pins = tl.load_literal(PINS) if os.path.exists(PINS) else {}
    for qid in list(rejects) + list(pins):
        if not re.fullmatch(r"Q[1-9][0-9]*", qid):
            tl.die("bad QID %r in the rejects or pins" % qid)
    return rejects, pins


def curated_entries(client, entries, built):
    """QID -> entry of the curated tier, every fact and fun fact checked against the Wikipedia revision it
    was copied from. Stops the run on any problem: these are hand-written and must be fixed by hand."""
    titles = [entry.get("wiki") for entry in entries]
    if len(set(titles)) != len(titles):
        tl.die("curated: a title is there twice: %s" % sorted(t for t in titles if titles.count(t) > 1))
    found = tl.wiki_entities(client, [t for t in titles if t])
    revisions, result, problems = {}, {}, []

    def revision(item):
        key = (item.get("lang"), item.get("title"), item.get("revid"))
        if key not in revisions and not _source_problem(item):
            revisions[key] = tl.revision_plain(client, *key)
        return revisions.get(key)

    for entry in entries:
        what = "curated %s" % entry.get("wiki")
        entity = found.get(entry.get("wiki"))
        if not entity:
            problems.append("%s: no Wikidata item has this English title" % what)
            continue
        qid = entity["id"]
        if qid not in built:
            # The article may be linked to another item (the species) than the genus' own: then the one sheet whose
            # scientific name is the title is the genus
            named = [q for q, sheet in built.items() if sheet["name"] == entry["wiki"]]
            if len(named) != 1:
                problems.append("%s (%s): no sheet of the automatic tier to add to" % (what, qid))
                continue
            print("  %s: the article is linked to %s, the genus with this name is %s" % (what, qid, named[0]))
            qid = named[0]
        if qid in result:
            problems.append("%s (%s): a second entry for this genus" % (what, qid))
        if entry.get("image") and (entry.get("tag") not in CURATED_TAGS or entry.get("fit") not in ("cover", "contain")):
            problems.append("%s: a picture needs a tag (%s) and a fit (cover, contain)" % (what, ", ".join(sorted(CURATED_TAGS))))
        items = [("fact %s" % fact.get("field"), fact, check_fact) for fact in entry.get("facts", [])]
        if entry.get("fun"):
            items.append(("fun fact", entry["fun"], check_fun))
        for label, item, check in items:
            plain = revision(item)
            problem = _source_problem(item) or (
                "revision %s is not of %s:%s" % (item.get("revid"), item.get("lang"), item.get("title"))
                if plain is None else check(item, plain))
            if problem:
                problems.append("%s, %s: %s" % (what, label, problem))
        result[qid] = entry
    if problems:
        tl.die("curated entries refused:\n  " + "\n  ".join(problems))
    return result


def prepared(client, file_info, fit=None):
    """(jpeg, (width, height)) of a file's thumbnail, or a reason why not. fit: see prepare_picture."""
    data = client.get(file_info["thumb"])
    if data is None:
        return "file: thumbnail not found"
    try:
        jpeg, size = prepare_picture(data, fit)
    except Exception as error:  # Pillow raises many kinds; any of them means no picture
        return "file: can't be read (%s)" % error
    if len(jpeg) > LIMITS["picture"]:
        return "file: %d bytes, over %d" % (len(jpeg), LIMITS["picture"])
    return jpeg, size


def attach_pictures(client, built, wanted, infos, rejects, pins, fits):
    """Gives each sheet its checked picture. Returns QID -> (jpeg, file name, info) and the refusals.
    fits: QID -> "cover" or "contain" where the curated tier says how a picture is fitted."""
    pictures, refused = {}, {}
    for qid in sorted(wanted, key=lambda q: int(q[1:])):
        name, info = wanted[qid], infos.get(wanted[qid])
        problem = picture_problem(info, rejects, pins, qid)
        result = None
        if not problem:
            result = prepared(client, info, fits.get(qid))
            problem = result if isinstance(result, str) else None
        credit = None
        if not problem:
            credit = device_credit(info)
            if credit is None:
                problem = "credit: can't be written in ASCII (%r)" % info["licence"]
        if problem:
            refused[qid] = problem
            print("  no picture for %s %s (%s): %s" % (qid, built[qid]["slug"], name, problem))
            continue
        jpeg, (width, height) = result
        built[qid]["image"] = {"length": len(jpeg), "width": width, "height": height, "hash": tl.fnv1a(jpeg),
                               "credit": credit}
        pictures[qid] = (jpeg, name, info)
    return pictures, refused


def montage(shown, built, flagged, path):
    """One contact sheet: each tile exactly as prepared, on a 240x117 box, captioned with slug, QID and file.
    A picture whose Commons data never names the genus is marked CHECK: it may show another animal."""
    tiles = []
    for qid, name, tile in shown:
        caption = "%s%s %s\n%s" % ("CHECK " if qid in flagged else "", built[qid]["slug"][:24], qid, name[:38])
        tiles += ["(", tile, "-background", "#202020", "-gravity", "center", "-extent", "%dx%d" % (IMAGE_W, IMAGE_H),
                  "-set", "label", caption, ")"]
    subprocess.run(["magick", "montage", "-background", "black", "-fill", "white", "-pointsize", "11", "-tile", "6x",
                    "-geometry", "%dx%d+4+4" % (IMAGE_W, IMAGE_H)] + tiles + [path], check=True)


def contact_sheets(client, built, wanted, infos, rejects, pins, flagged, cache, directory, fits, curated=()):
    """Captioned sheets of PER_CONTACT_SHEET pictures, each exactly as the device would get it, of every
    picture that passes the checks before the pin: dino-NN.png has all of them, dino-check-NN.png only
    those that need a look now (not pinned yet, changed since, or marked CHECK), dino-curated-NN.png the
    hand-chosen pictures of the curated tier. Tiles are named by QID, which is unique; a slug is made
    unique only later. Writes the manifest that --pin reads."""
    os.makedirs(directory, exist_ok=True)
    tiles_dir = os.path.join(directory, "tiles")
    shutil.rmtree(tiles_dir, ignore_errors=True)
    os.makedirs(tiles_dir)
    for old in os.listdir(directory):
        if re.fullmatch(r"dino-(check-|curated-)?\d+\.png", old):
            os.remove(os.path.join(directory, old))
    shown, manifest, refused = [], {}, Counter()
    for qid in sorted(wanted, key=lambda q: (built[q]["slug"], int(q[1:]))):
        name, info = wanted[qid], infos.get(wanted[qid])
        problem = picture_problem(info, rejects, {}, qid, check_pin=False)
        result = None if problem else prepared(client, info, fits.get(qid))
        if isinstance(result, str):
            problem = result
        if problem:
            refused[problem.split(":")[0]] += 1
            continue
        path = os.path.join(tiles_dir, "%s.jpg" % qid)
        with open(path, "wb") as f:
            f.write(result[0])
        shown.append((qid, name, path))
        manifest[qid] = info["sha1"]
    check = [t for t in shown if t[0] in flagged or pins.get(t[0]) != manifest[t[0]]]
    sheets = []
    for prefix, tiles in (("dino-check-", check), ("dino-curated-", [t for t in shown if t[0] in curated]),
                          ("dino-", shown)):
        for number, start in enumerate(range(0, len(tiles), PER_CONTACT_SHEET), 1):
            sheets.append(os.path.join(directory, "%s%02d.png" % (prefix, number)))
            montage(tiles[start:start + PER_CONTACT_SHEET], built, flagged, sheets[-1])
    with open(os.path.join(cache, MANIFEST), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=0, sort_keys=True)
    print("Contact sheets: %d pictures on %d sheets in %s" % (len(shown), len(sheets), directory))
    print("  to look at now (dino-check-*.png): %d, of them %d not pinned or changed, %d marked CHECK" % (
        len(check), sum(1 for t in check if pins.get(t[0]) != manifest[t[0]]), sum(1 for t in check if t[0] in flagged)))
    for qid, name, _ in check:
        print("    %s %s (%s)%s%s" % (qid, built[qid]["slug"], name, " CHECK" if qid in flagged else "",
                                     " new" if pins.get(qid) != manifest[qid] else ""))
    print("  curated pictures (dino-curated-*.png): %d of %d" % (sum(1 for t in shown if t[0] in curated), len(curated)))
    print("  not on the sheets, by check: %s" % dict(refused))


def pin(wanted, infos, rejects, pins, cache):
    """Pins every picture that was on the last contact sheets with the same SHA-1 and still passes, and
    keeps older pins that still match."""
    path = os.path.join(cache, MANIFEST)
    if not os.path.exists(path):
        tl.die("no contact sheets in this cache yet: run --contact-sheets and look at them first")
    with open(path, encoding="utf-8") as f:
        manifest = json.load(f)
    pinned = {}
    for qid, name in wanted.items():
        info = infos.get(name)
        if not info or picture_problem(info, rejects, {}, qid, check_pin=False):
            continue
        if manifest.get(qid) == info["sha1"] or pins.get(qid) == info["sha1"]:
            pinned[qid] = (info["sha1"], name)
    with open(PINS, "w", encoding="utf-8") as f:
        f.write("# Written by make_dino_data.py --pin: genus QID -> SHA-1 of its Commons picture when it was looked at\n"
                "# on the contact sheets, by the AI that built the data, not by a person. A file whose SHA-1\n"
                "# differs is not used until it has been looked at again.\n{\n")
        for qid in sorted(pinned, key=lambda q: int(q[1:])):
            f.write("    %r: %r,  # %s\n" % (qid, pinned[qid][0], pinned[qid][1].replace("\n", " ")))
        f.write("}\n")
    print("pinned %d pictures" % len(pinned))


def credits_page(rows, texts=()):
    """credits.html: maker, licence and file link of every picture on a sheet, and the Wikipedia revision
    of every curated fact and fun fact. texts: (sheet, what, lang, title, revid)."""
    lines = []
    for sheet, name, info in sorted(rows, key=lambda r: r[0]["name"]):
        maker = author_of(info)  # Commons' text, unless that names nobody and its user link does
        maker = info["artist"] if maker and maker in info["artist"] else maker
        maker = tl.without_emails(maker)  # A public page: no maker's e-mail address
        lines.append("<tr><td>%s</td><td><a href=\"https://www.wikidata.org/wiki/%s\">%s</a></td>"
                     "<td><a href=\"%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td></tr>" % tuple(
                         escape(str(x), quote=True) for x in (sheet["name"], sheet["qid"], sheet["qid"], info["page"],
                                                               name, maker or "-", info["licence"] or info["code"],
                                                               "resized, may be cropped")))
    return ("<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><title>Zitro dinosaur picture credits</title>"
            "</head><body><h1>Zitro dinosaur picture credits</h1>"
            "<p>Pictures come from Wikimedia Commons under the licence shown; each was resized and may be cropped to fit "
            "the screen. Pictures under CC BY-SA stay under that licence after resizing. Pictures chosen automatically "
            "were looked at by the AI that built the data, not by a person. Facts and names come from "
            "Wikidata (CC0)" + (", and for the best-known animals also from Wikipedia (see below)" if texts else "") +
            ".</p><table border=\"1\" cellpadding=\"4\"><tr><th>Genus</th><th>Wikidata</th><th>File</th><th>Author</th>"
            "<th>Licence</th><th>Changes</th></tr>\n" + "\n".join(lines) + "\n</table>" + wikipedia_credits(texts) +
            "</body></html>\n")


def wikipedia_credits(texts):
    """The table of credits.html for facts and fun facts taken from Wikipedia sentences."""
    if not texts:
        return ""
    lines, seen = [], set()
    for sheet, what, lang, title, revid in sorted(texts, key=lambda t: (t[0]["name"], t[1], t[2])):
        if (sheet["name"], what, lang, title, revid) in seen:  # Two heights from one revision: one line
            continue
        seen.add((sheet["name"], what, lang, title, revid))
        url = "https://%s.wikipedia.org/w/index.php?oldid=%d" % (lang, revid)
        lines.append("<tr><td>%s</td><td>%s</td><td><a href=\"%s\">%s.wikipedia.org: %s, revision %d</a></td></tr>" % (
            escape(sheet["name"]), escape(what), escape(url, quote=True), lang, escape(title), revid))
    return ("<h2>Facts and fun facts from Wikipedia</h2><p>For the best-known animals, some facts and the fun fact "
            "come from sentences of these Wikipedia articles, under CC BY-SA 4.0. All of these curated facts (the fun "
            "facts, speeds, heights, eras, name meanings, diets and places) were chosen, translated and shortened "
            "from these sentences by the AI that built the data; no person reviewed them.</p>"
            "<table border=\"1\" cellpadding=\"4\"><tr><th>Genus</th><th>What</th><th>Source</th></tr>\n" +
            "\n".join(lines) + "\n</table>")


def kind_of(name):
    if name.startswith("index-"):
        return "index"
    return {".txt": "sheet", ".jpg": "picture"}.get(os.path.splitext(name)[1])


def write_all(files):
    """Empties the output folder and writes the files, checking each against LIMITS. Text is UTF-8."""
    for name, content in files.items():
        data = content.encode() if isinstance(content, str) else content
        kind = kind_of(name)
        if kind and len(data) > LIMITS[kind]:
            tl.die("%s is %d bytes, over the %s limit of %d" % (name, len(data), kind, LIMITS[kind]))
    shutil.rmtree(OUT_DIR, ignore_errors=True)
    os.makedirs(OUT_DIR)
    for name, content in files.items():
        with open(os.path.join(OUT_DIR, name), "wb") as f:
            f.write(content.encode() if isinstance(content, str) else content)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cache", required=True, help="folder for the downloads")
    parser.add_argument("--no-pictures", action="store_true", help="facts only")
    parser.add_argument("--contact-sheets", metavar="DIR", help="write the sheets to look at, and nothing else")
    parser.add_argument("--pin", action="store_true", help="pin the pictures on the last contact sheets")
    args = parser.parse_args()
    if args.no_pictures and (args.contact_sheets or args.pin):
        tl.die("--no-pictures has no pictures to show or pin")
    client = tl.Client(args.cache)
    config = tl.load_literal(CURATED)
    rejects, pins = load_reviewed()

    print("Resolving the clades")
    roots = resolve_clades(client, config["clades"])
    periods = periods_of(client)
    print("Listing genera")
    members = {}
    for key, root in roots.items():
        members[key] = set().union(*(genera_under(client, r) for r in root))
        print("  %-12s %d genera" % (key, len(members[key])))
    candidates = {}
    for qid in sorted(set().union(*members.values()), key=lambda q: int(q[1:])):
        group = classify(qid, members)
        if group is not None:
            candidates[qid] = group
    print("  %d candidates; counts per group: %s" % (len(candidates), dict(Counter(GROUP_NAMES[g] for g in candidates.values()))))

    print("Downloading genus data")
    entities = fetch_entities(client, set(candidates))
    for qid in set(candidates) - set(entities):
        drop(qid, "item is gone")
    ages = ages_of(client, entities)
    species = species_names(client, set(entities))

    print("Building the sheets")
    built = {}
    for qid in sorted(entities, key=lambda q: int(q[1:])):
        sheet = build_sheet(qid, entities[qid], candidates[qid], ages, periods)
        if sheet:
            built[qid] = sheet

    print("Curated tier")
    curated = curated_entries(client, config.get("curated", []), built)
    texts = []
    for qid, entry in curated.items():
        try:
            apply_curated(built[qid], entry, periods)
        except ValueError as error:
            tl.die("curated %s: %s" % (entry["wiki"], error))
        for fact in entry.get("facts", []):
            texts.append((built[qid], fact["field"], fact["lang"], fact["title"], fact["revid"]))
        if entry.get("fun"):
            texts.append((built[qid], "fun fact", entry["fun"]["lang"], entry["fun"]["title"], entry["fun"]["revid"]))
    print("  %d genera, %d facts and fun facts checked against their Wikipedia revisions" % (len(curated), len(texts)))

    pictures, refused = {}, {}
    if not args.no_pictures:
        files = {qid: pictures_of(entities[qid]) for qid in built}
        files = {qid: names for qid, names in files.items() if names}
        # Wikidata's first pictures, then the others: the first set's batches stay the same, so the cache holds
        first = {names[0] for names in files.values()}
        print("Checking %d genera with pictures (%d files, %d more) on Commons" % (
            len(files), len(first), len({n for names in files.values() for n in names} - first)))
        infos = commons_infos(client, first)
        infos.update(commons_infos(client, {n for names in files.values() for n in names} - first))
        wanted, flagged = {}, set()
        genera = genus_words(built.values())
        for qid in files:
            names = [built[qid]["name"]] + alias_names(entities[qid]) + sorted(species.get(qid, ()))
            wanted[qid], named = choose_picture(entities[qid], infos, names, genera)
            if not named:
                flagged.add(qid)
        changed = [q for q in wanted if wanted[q] != files[q][0]]
        print("  %d genera get a later picture of theirs because Wikidata's first doesn't name them" % len(changed))
        for qid in sorted(changed, key=lambda q: int(q[1:])):
            print("    %s %s: %s instead of %s" % (qid, built[qid]["slug"], wanted[qid], files[qid][0]))
        print("  %d pictures never name their genus or its names on Commons (marked CHECK on the contact sheets)" % len(flagged))
        # The curated tier's pictures were chosen by hand: they replace Wikidata's and are never marked CHECK
        fits = {}
        for qid, entry in curated.items():
            if entry.get("image"):
                wanted[qid], fits[qid] = entry["image"], entry["fit"]
                flagged.discard(qid)
        infos.update(commons_infos(client, {wanted[q] for q in fits} - set(infos)))
        rejects = rejects_for(rejects, set(fits))
        if args.contact_sheets:
            contact_sheets(client, built, wanted, infos, rejects, pins, flagged, args.cache, args.contact_sheets, fits,
                           set(fits))
            return
        if args.pin:
            pin(wanted, infos, rejects, pins, args.cache)
            return
        pictures, refused = attach_pictures(client, built, wanted, infos, rejects, pins, fits)
        kinds = Counter(problem.split(":")[0] for problem in refused.values())
        print("Pictures: %d genera with a Wikidata picture, %d kept, %d refused: %s" % (
            len(wanted), len(pictures), len(refused), dict(kinds.most_common())))

    sheets, seen = {}, {}
    lost_picture = 0
    for qid, sheet in built.items():
        if not worth_showing(sheet):
            drop("%s %s" % (qid, sheet["name"]), "no picture and fewer than two fact rows")
            lost_picture += qid in refused
            continue
        if sheet["slug"] in seen:
            drop("%s %s" % (qid, sheet["name"]), "same file name as %s" % seen[sheet["slug"]])
            continue
        seen[sheet["slug"]] = qid
        sheets[qid] = sheet
    if not args.no_pictures:
        print("Sheets dropped because their picture was refused and they have fewer than two rows: %d" % lost_picture)

    genus_names = {}
    for qid, sheet in sheets.items():
        genus_names[normalise(sheet["name"])] = sheet["slug"]
    entries = [(name, slug) for name, slug in genus_names.items()]
    alias_counts = Counter()
    for qid, sheet in sheets.items():
        for raw in alias_names(entities[qid]) + sorted(species.get(qid, ())):
            name = normalise(raw)
            if not name or len(name) > NAME_LIMIT or len(name) < 3:
                alias_counts["can't be matched"] += 1
            elif name in genus_names and genus_names[name] != sheet["slug"]:
                alias_counts["is the name of another genus"] += 1
            else:
                entries.append((name, sheet["slug"]))
    for reason, count in alias_counts.items():
        print("  aliases skipped, %s: %d" % (reason, count))

    files = {"index-%s.txt" % prefix: text for prefix, text in index_files(entries).items()}
    credit_rows = []
    for qid, sheet in sheets.items():
        files[sheet["slug"] + ".txt"] = sheet_text(sheet)
        if qid in pictures:
            jpeg, name, info = pictures[qid]
            files[sheet["slug"] + ".jpg"] = jpeg
            credit_rows.append((sheet, name, info))
    if credit_rows:
        files["credits.html"] = credits_page(credit_rows, [t for t in texts if t[0]["qid"] in sheets])
    write_all(files)

    print("\n".join(DROP_LINES))
    print("Dropped values by reason:")
    for reason, count in DROPS.most_common():
        print("  %5d  %s" % (count, reason))
    print("Sheets per group: %s" % dict(Counter(GROUP_NAMES[s["group"]] for s in sheets.values())))
    total = sum(len(t.encode()) if isinstance(t, str) else len(t) for t in files.values())
    print("Wrote %d files (%d indexes, %d sheets, %d pictures), %d bytes, to %s" % (
        len(files), sum(1 for n in files if n.startswith("index-")), len(sheets), len(credit_rows), total, OUT_DIR))


if __name__ == "__main__":
    main()
