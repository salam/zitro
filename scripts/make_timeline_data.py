#!/usr/bin/env python3
"""Makes the prepared timelines in webpage2/data/timeline/ from Wikidata and Wikimedia Commons.

Run with a Python that has Pillow, from anywhere:
    /opt/homebrew/bin/python3 -I scripts/make_timeline_data.py --cache <scratch dir> [--dates-only]
        Builds everything, or only prints the resolved dates and labels.
    ... --candidates <topic>     Prints the Commons pictures Wikidata suggests for a topic's events.
                                 Only a list to choose from; nothing from it is used by itself.
    ... --contact-sheets <dir>   Writes one captioned sheet per topic, of the pictures exactly as
                                 prepared. This is how you look at them.
    ... --pin                    Records the current SHA-1 of every allow-listed picture. Do this only
                                 after looking at the sheets: from then on a changed file is refused.

What it guarantees, or else it leaves the event out:
- A date comes from Wikidata (see date_of), or, where Wikidata has none, from a sentence quoted from a
  named Wikipedia revision (see wikipedia_date). It matches the person's expectation in the allow-list.
- An image is exactly the one in the allow-list, pinned by SHA-1, and has a licence that allows use.
- The label comes from Wikidata (short name if there is one) and fits the device's font range.

Output per topic, read by sketch_jan12a/timeline_core.h:
    <id>.txt   UTF-8, tab-separated. First line: ZTL1, id, subject QID, title DE, title EN, event count,
               bundle bytes. Then one line per event: QID, year, end year or -, flags (1 circa, 2 the
               year is a birth, 4 the date is from Wikipedia), label DE, label EN, image offset, image
               length, width, height, FNV-1a of the image in hex, credit.
               No image: offset, length, width, height are 0, hash and credit are -.
    <id>.jpgs  The event pictures, baseline JPEGs one after the other.
Also credits.html (who made every picture, licence, link) and timeline_topics.h in the sketch.
"""
import argparse
import ast
import hashlib
import html
import io
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse
from decimal import Decimal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "webpage2", "data", "timeline")
TOPICS_HEADER = os.path.join(ROOT, "sketch", "sketch_jan12a", "timeline_topics.h")
ALLOWLIST = os.path.join(ROOT, "scripts", "make_timeline_allowlist.py")
PINS = os.path.join(ROOT, "scripts", "make_timeline_pins.py")
USER_AGENT = "Zitro-timeline-builder/1.0 (+https://zitro.spot)"
WIKIDATA_API = "https://www.wikidata.org/w/api.php"
COMMONS_API = "https://commons.wikimedia.org/w/api.php"
WIKIPEDIA_API = "https://%s.wikipedia.org/w/api.php"

# Order matters: the start of something is its place on a timeline, so a war's end date in P585
# (World War I has one) doesn't move it.
DATE_PROPS = ["P580", "P585", "P571", "P575", "P619", "P606", "P1619", "P729", "P577"]
END_PROP = "P582"
CIRCA = "Q5727902"  # Qualifier P1480 "sourcing circumstances": circa
MIN_PRECISION = 9  # Year. Deep time sets its own in the topic: millions of years
YEAR_LIMIT = 5_000_000_000  # Years either side of year 0 that the device takes (a signed 64-bit year)
COARSE_PRECISION = 3  # Wikidata precision 3 is a million years; anything coarser is a rough figure
DEEP_TIME = 1_000_000  # From this many years back the device writes millions of years, before that a year

IMAGE_W, IMAGE_H = 240, 96  # The picture band on screen; the device draws at exactly this size
MAX_EVENTS = 20
MAX_BUNDLE = 320 * 1024  # Downloaded into PSRAM in one go
JPEG_QUALITY = 80
THUMB_SOURCE_WIDTH = 500  # A standard Wikimedia thumbnail step; other sizes may be refused
LABEL_LIMIT = 60  # Bytes per label on the device
LABEL_COMFORTABLE = 38  # Characters that fit two lines; tests/timeline checks it with the real font
SHORT_NAME_MIN = 8  # A shorter "short name" is an abbreviation
CREDIT_LIMIT = 60

CONTAIN_TAGS = {"portrait", "map", "flag", "document", "artwork", "artifact"}  # Cropping these would lose them

# Free to use. Anything else (NC, ND, fair use, GFDL alone, unknown) is refused.
LICENCE_OK = re.compile(r"^(pd($|[-\s])|public domain|cc0|cc[- ]by(-sa)?([- ]\d(\.\d)?)?([- ][a-z-]+)?$)", re.I)
LICENCE_BAD = re.compile(r"\b(nc|nd|fair|gfdl)\b", re.I)
# Second net, on the Commons categories of each chosen picture. The real check is looking at them.
CATEGORY_DENY = re.compile(
    r"corpse|dead bod|dead people|casualt|wounded|injur|massacre|atrocit|execution|hanging|genocide|"
    r"holocaust victim|concentration camp|mass grave|victims|gore|\bblood\b|nude|nudity|swastika|nazi flag",
    re.I,
)


def die(message):
    sys.exit("make_timeline_data: " + message)


class Client:
    """Polite GETs: one at a time with a pause, cached on disk, backing off when throttled."""

    def __init__(self, cache):
        self.cache = cache
        os.makedirs(cache, exist_ok=True)

    def get(self, url):
        path = os.path.join(self.cache, hashlib.sha1(url.encode()).hexdigest())
        if os.path.exists(path):
            with open(path, "rb") as f:
                return f.read()
        for attempt in range(7):
            time.sleep(1.0)
            part = path + ".part"
            status = subprocess.run(
                ["curl", "-sS", "-L", "-A", USER_AGENT, "--max-time", "60", "-o", part, "-w", "%{http_code}", url],
                capture_output=True,
                text=True,
            ).stdout.strip()
            if status == "200":
                os.replace(part, path)
                with open(path, "rb") as f:
                    return f.read()
            if os.path.exists(part):
                os.remove(part)
            if status == "404":
                return None
            wait = 10 * (attempt + 1)
            print("  HTTP %s, waiting %d s" % (status, wait), file=sys.stderr)
            time.sleep(wait)
        die("giving up on " + url)

    def api(self, base, params):
        url = base + "?format=json&" + urllib.parse.urlencode(params)
        body = self.get(url)
        try:
            data = json.loads(body)
        except (TypeError, ValueError):
            die("not JSON from " + url)
        if "error" in data:
            os.remove(os.path.join(self.cache, hashlib.sha1(url.encode()).hexdigest()))
            die("API error %s from %s" % (data["error"].get("code"), url))
        return data


def batches(items, size=50):
    for i in range(0, len(items), size):
        yield items[i : i + size]


def wiki_entities(client, titles):
    """English Wikipedia title -> Wikidata entity. Redirects are followed, misses stay out."""
    found = {}
    for chunk in batches(titles):
        data = client.api(
            WIKIDATA_API,
            {
                "action": "wbgetentities",
                "sites": "enwiki",
                "titles": "|".join(chunk),
                "props": "labels|aliases|claims|sitelinks",
                "languages": "en|de|mul",
                "sitefilter": "enwiki",
                "redirects": "yes",
            },
        )
        for key, entity in data.get("entities", {}).items():
            if key.startswith("-") or "missing" in entity:
                continue
            title = entity.get("sitelinks", {}).get("enwiki", {}).get("title")
            if title:
                found[title] = entity
    return found


def wikidata_time(value):
    """(year, precision) from a Wikidata time value. Year stays signed: -44 is 44 BC in this JSON."""
    match = re.match(r"^([+-])0*(\d+)-", value.get("time", ""))
    if not match:
        return None
    year = int(match.group(2))
    return (-year if match.group(1) == "-" else year), value.get("precision", 0)


def statements(entity, prop):
    """Usable statements of a property: not deprecated, real values, preferred rank wins."""
    usable = []
    for claim in entity.get("claims", {}).get(prop, []):
        snak = claim.get("mainsnak", {})
        if claim.get("rank") == "deprecated" or snak.get("snaktype") != "value":
            continue
        usable.append(claim)
    preferred = [c for c in usable if c.get("rank") == "preferred"]
    return preferred or usable


def date_of(entity, min_precision, props, coarse_circa=False):
    """The event's (year, end year or None, circa flag, property), or a reason why not.
    coarse_circa marks a year as circa where Wikidata knows it less exactly than the device writes it:
    to a million years in deep time, to the year after that."""
    for prop in props:
        claims = statements(entity, prop)
        if not claims:
            continue
        values = []
        for claim in claims:
            parsed = wikidata_time(claim["mainsnak"]["datavalue"]["value"])
            if parsed:
                values.append((parsed, claim))
        if not values:
            continue
        years = {v[0][0] for v in values}
        if len(years) > 1:
            return "%s has conflicting years %s" % (prop, sorted(years))
        (year, precision), claim = values[0]
        if precision < min_precision:
            return "%s precision %d is too coarse" % (prop, precision)
        if year == 0 or abs(year) > YEAR_LIMIT:
            return "%s year %d doesn't fit" % (prop, year)
        circa = any(
            q.get("datavalue", {}).get("value", {}).get("id") == CIRCA
            for q in claim.get("qualifiers", {}).get("P1480", [])
        ) or (coarse_circa and precision < (COARSE_PRECISION if year <= -DEEP_TIME else MIN_PRECISION))
        end = None
        end_years = set()
        for end_claim in statements(entity, END_PROP) if prop == "P580" else []:
            parsed = wikidata_time(end_claim["mainsnak"]["datavalue"]["value"])
            if parsed and parsed[1] >= min_precision:
                end_years.add(parsed[0])
        if len(end_years) == 1 and next(iter(end_years)) > year:
            end = next(iter(end_years))
        return year, end, circa, prop
    return "no usable date"


SCALES = {"billion": 10**9, "milliarde": 10**9, "milliarden": 10**9, "mrd": 10**9,
          "million": 10**6, "millionen": 10**6, "mio": 10**6, "thousand": 10**3, "tausend": 10**3}
NUMBER = re.compile(r"(\d+(?:[.,]\d+)*)(?:\s*(billion|milliarden?|mrd\.?|million(?:en)?|mio\.?|thousand|tausend)(?![a-z]))?",
                    re.I)
RANGE_WORDS = {"-", "\u2013", "\u2014", "to", "and", "or"}


def wikitext_plain(text):
    """Wikitext as the plain sentence a reader sees: no refs, templates or link markup."""
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"<ref[^>]*/>|<ref[^>]*>.*?</ref>", "", text, flags=re.S | re.I)

    def template(match):
        parts = [p.strip() for p in match.group(1).split("|")]
        if parts[0].lower() != "convert" or len(parts) < 3:
            return ""
        if len(parts) > 4 and parts[2] in RANGE_WORDS:
            return "%s-%s %s" % (parts[1], parts[3], parts[4])
        return "%s %s" % (parts[1], parts[2])

    previous = None
    while previous != text:  # innermost first
        previous = text
        text = re.sub(r"\{\{([^{}]*)\}\}", template, text)
    text = re.sub(r"\[\[(?:[^\]|]*\|)?([^\]]*)\]\]", r"\1", text)
    text = re.sub(r"<[^>]+>|'{2,}", "", text)
    text = html.unescape(text)  # a non-breaking space then counts as a space below
    return re.sub(r"\s+", " ", text).strip()


def numbers_in(sentence):
    """Every value the sentence states, as whole numbers, each written number read one way only.
    Before a scale word the separator is a decimal mark ("3.5 billion", "3,5 Milliarden"); without
    one it separates thousands ("300,000", "300.000") and anything else is not a whole number."""
    values = set()
    for match in NUMBER.finditer(sentence):
        digits, word = match.groups()
        if word:
            if len(re.findall(r"[.,]", digits)) > 1:
                continue
            reading, scale = digits.replace(",", "."), SCALES[word.lower().rstrip(".")]
        elif re.fullmatch(r"\d{1,3}(?:[.,]\d{3})+", digits):
            reading, scale = re.sub(r"[.,]", "", digits), 1
        elif digits.isdigit():
            reading, scale = digits, 1
        else:
            continue
        value = Decimal(reading) * scale
        if value == value.to_integral_value():
            values.add(int(value))
    return values


def check_date_sentence(sentence, year, revision_plain):
    """None if the sentence stands in the revision's text and states the year, otherwise why not."""
    quoted = re.sub(r"\s+", " ", sentence).strip()
    if not quoted or quoted not in re.sub(r"\s+", " ", revision_plain):
        return "the sentence is not in that revision"
    if abs(year) not in numbers_in(quoted):
        return "the sentence has no number for %d" % abs(year)
    return None


def revision_plain(client, lang, title, revid):
    """The plain text of one Wikipedia revision, or None if that revision is not of the page `title`."""
    data = client.api(
        WIKIPEDIA_API % lang,
        {"action": "query", "prop": "revisions", "revids": str(revid), "rvprop": "content", "rvslots": "main",
         "formatversion": "2"},
    )
    for page in data.get("query", {}).get("pages", []):
        if page.get("title", "").replace("_", " ") != title.replace("_", " "):
            return None
        for revision in page.get("revisions", []):
            content = revision.get("slots", {}).get("main", {}).get("content")
            if content is not None:
                return wikitext_plain(content)
    return None


def wikipedia_date(client, source):
    """(year, circa) from a sentence quoted from a named Wikipedia revision, or a reason why not.
    A year found this way is always circa: such a sentence says "about"."""
    year = source["year"]
    if year == 0 or abs(year) > YEAR_LIMIT:
        return "year %d doesn't fit" % year
    text = revision_plain(client, source["lang"], source["title"], source["revid"])
    if text is None:
        return "revision %d is not of the page %s:%s" % (source["revid"], source["lang"], source["title"])
    problem = check_date_sentence(source["sentence"], year, text)
    return problem or (year, True)


def clean_label(text):
    """Dashes to the plain one, first letter up; None if the device can't draw it or it's too long."""
    if not text:
        return None
    text = text.replace("–", "-").replace("—", "-").replace("‑", "-").replace(" ", " ")
    text = re.sub(r"\s+", " ", text).strip()
    text = text[:1].upper() + text[1:]
    if not text or len(text.encode()) > LABEL_LIMIT:
        return None
    if any(not 0x20 <= ord(ch) <= 0x5FF or ch == "\x7f" for ch in text):
        return None
    return text


def label_of(entity, language):
    """The Wikidata label. A label too long for two lines on the stick gives way to Wikidata's short
    name or alias, the shortest that isn't a bare abbreviation (WW2, HST means nothing to a child)."""
    labels = entity.get("labels", {})
    # "mul" is Wikidata's label for all languages: a person's name usually only has that one
    label = clean_label((labels.get(language) or labels.get("mul") or {}).get("value"))
    if label and len(label) <= LABEL_COMFORTABLE:
        return label
    other = [
        c["mainsnak"]["datavalue"]["value"]["text"]
        for c in statements(entity, "P1813")
        if c["mainsnak"]["datavalue"]["value"].get("language") == language
    ]
    other += [a["value"] for a in entity.get("aliases", {}).get(language, [])]
    other = [t for t in (clean_label(t) for t in other) if t and SHORT_NAME_MIN <= len(t) <= LABEL_COMFORTABLE]
    other.sort(key=len)
    return other[0] if other else label


def strip_html(text):
    text = re.sub(r"<[^>]+>", " ", text or "")
    text = re.sub(r"\s+", " ", html.unescape(text)).strip()
    # Wiki leftovers in a name
    text = re.sub(r"^(\w+:)?User:", "", text)
    return re.sub(r"\s*\(\s*(Diskussion|talk)\s*\)", "", text).strip()


# An e-mail address, with a label before it ("email:", "Contact: "), and the bare "( e-mail )" some makers write
EMAIL = re.compile(r"(?:\b(?:e-?mail|contact)\s*:\s*)?[^\s()<>,;:@]+@[^\s()<>,;@]+|\(\s*e-?mail\s*\)", re.I)


def without_emails(text):
    """The maker's text for a public page: Commons' text with no e-mail address. Web addresses stay, and so
    does everything else; the brackets and commas an address leaves behind go with it. A text with no address
    comes back as it is."""
    if not EMAIL.search(text or ""):
        return text or ""
    text = EMAIL.sub(" ", text)
    text = re.sub(r"\(\s*[,;]\s*", "(", text)  # "(, site)" -> "(site)"
    text = re.sub(r"\s*[,;]\s*\)", ")", text)  # "(Name, )" -> "(Name)"
    text = re.sub(r"\(\s*\)", " ", text)
    text = re.sub(r"\(\s+", "(", re.sub(r"\s+\)", ")", text))
    return re.sub(r"\s+", " ", text).strip(" ,;")


def commons_info(client, names):
    """File name -> SHA-1, thumbnail URL, licence, artist and categories. Missing files stay out."""
    info = {}
    for chunk in batches(names):
        data = client.api(
            COMMONS_API,
            {
                "action": "query",
                "titles": "|".join("File:" + n for n in chunk),
                "prop": "imageinfo|categories",
                "iiprop": "sha1|url|size|mime|extmetadata",
                "iiurlwidth": str(THUMB_SOURCE_WIDTH),
                "cllimit": "max",
                "clshow": "!hidden",
            },
        )
        renamed = {n["to"]: n["from"] for n in data.get("query", {}).get("normalized", [])}
        for page in data.get("query", {}).get("pages", {}).values():
            if "missing" in page or not page.get("imageinfo"):
                continue
            title = renamed.get(page["title"], page["title"])
            ii = page["imageinfo"][0]
            meta = ii.get("extmetadata", {})
            info[title[len("File:") :]] = {
                "sha1": ii.get("sha1", ""),
                "thumb": ii.get("thumburl") or ii.get("url"),
                "page": ii.get("descriptionurl", ""),
                "mime": ii.get("mime", ""),
                "licence": meta.get("LicenseShortName", {}).get("value", ""),
                "code": meta.get("License", {}).get("value", ""),
                "artist": strip_html(meta.get("Artist", {}).get("value", "")),
                "needs_credit": meta.get("AttributionRequired", {}).get("value", "") == "true",
                "restrictions": meta.get("Restrictions", {}).get("value", ""),
                "categories": [c["title"] for c in page.get("categories", [])],
            }
    return info


def licence_problem(file_info):
    """None if this licence allows use with the credit we give, otherwise why not."""
    short, code = file_info["licence"], file_info["code"]
    if LICENCE_BAD.search(short) or LICENCE_BAD.search(code):
        # GFDL dual-licensed with CC BY-SA shows up as the CC licence; GFDL alone would land here
        return "licence not usable: %s / %s" % (short, code)
    if not (LICENCE_OK.match(code) or LICENCE_OK.match(short)):
        return "licence not recognised: %s / %s" % (short, code)
    if file_info["needs_credit"] and not file_info["artist"]:
        return "needs a credit but names nobody"
    return None


def credit_of(file_info):
    """Credit for the credits page: maker, licence, and that we resized it."""
    licence = file_info["licence"] or file_info["code"]
    if not file_info["needs_credit"]:
        return licence
    return "%s, %s, resized" % (file_info["artist"], licence)


def device_credit(file_info):
    """What the bottom line shows: nothing if the licence asks for nothing, otherwise the maker and
    licence, or the licence alone if that wouldn't fit. The credits page always has all of it."""
    if not file_info["needs_credit"]:
        return "-"
    licence = file_info["licence"] or file_info["code"]
    credit = "%s, %s" % (file_info["artist"], licence)
    return credit if len(credit.encode()) <= CREDIT_LIMIT else licence


def prepare_image(data, fit, width=IMAGE_W, height=IMAGE_H):
    """Exactly-sized baseline JPEG: RGB, no metadata, which is what TJpgDec on the device needs.
    width and height give the target box: the timeline's picture band by default."""
    from PIL import Image

    Image.MAX_IMAGE_PIXELS = 40_000_000
    image = Image.open(io.BytesIO(data))
    image.load()
    if image.mode in ("RGBA", "LA", "P"):
        image = image.convert("RGBA")
        backdrop = Image.new("RGBA", image.size, (255, 255, 255, 255))
        image = Image.alpha_composite(backdrop, image)
    image = image.convert("RGB")
    src_w, src_h = image.size
    if fit == "cover":
        scale = max(width / src_w, height / src_h)
        size = (max(width, round(src_w * scale)), max(height, round(src_h * scale)))
        image = image.resize(size, Image.LANCZOS)
        left = (size[0] - width) // 2
        top = (size[1] - height) // 2
        image = image.crop((left, top, left + width, top + height))
    else:
        scale = min(width / src_w, height / src_h)
        image = image.resize((max(1, round(src_w * scale)), max(1, round(src_h * scale))), Image.LANCZOS)
    out = io.BytesIO()
    image.save(out, "JPEG", quality=JPEG_QUALITY, optimize=False, progressive=False, subsampling="4:2:0")
    return out.getvalue(), image.size


def fnv1a(data):
    h = 0x811C9DC5
    for byte in data:
        h = ((h ^ byte) * 0x01000193) & 0xFFFFFFFF
    return h


def load_literal(path):
    with open(path, encoding="utf-8") as f:
        return ast.literal_eval(f.read())


def load_config():
    config = load_literal(ALLOWLIST)
    ids = [t["id"] for t in config["topics"]]
    if len(ids) != len(set(ids)):
        die("a topic id appears twice")
    for topic_id in ids:
        if not re.fullmatch(r"[a-z0-9_]{1,31}", topic_id):
            die("bad topic id " + topic_id)
    return config


def load_pins():
    return load_literal(PINS) if os.path.exists(PINS) else {}


def fit_of(item):
    return item.get("fit") or ("contain" if item.get("tag") in CONTAIN_TAGS else "cover")


def field(text):
    return text.replace("\t", " ").replace("\n", " ")


def resolve_topic(client, topic, entities):
    """The events that have a date and labels, in time order. Prints every rejection."""
    events = []
    min_precision = topic.get("min_precision", MIN_PRECISION)
    for item in topic["events"]:
        entity = entities.get(item["wiki"])
        name = "%s: %s" % (topic["id"], item["wiki"])
        if not entity:
            print("  REJECT %s: no Wikidata item for that Wikipedia title" % name)
            continue
        date = date_of(entity, min_precision, [item["prop"]] if item.get("prop") else DATE_PROPS,
                       topic.get("coarse_circa", False))
        if item.get("wikipedia_date"):
            # Approved only where Wikidata has no date, and Wikidata gains dates over time
            if not isinstance(date, str):
                print("  REJECT %s (%s): Wikidata has a usable date (%d, %s), so no Wikipedia date"
                      % (name, entity["id"], date[0], date[3]))
                continue
            date = wikipedia_date(client, item["wikipedia_date"])
            if not isinstance(date, str):
                date = (date[0], None, date[1], "wikipedia")
        if isinstance(date, str):
            print("  REJECT %s (%s): %s" % (name, entity["id"], date))
            continue
        year, end, circa, prop = date
        if year != item["expect"]:
            print("  REJECT %s (%s): %s says %d, expected %d"
                  % (name, entity["id"], "Wikipedia" if prop == "wikipedia" else "Wikidata", year, item["expect"]))
            continue
        label_en = label_of(entity, "en")
        label_de = label_of(entity, "de")
        if not label_de and label_en:
            print("  note %s (%s): no usable DE label, using EN" % (name, entity["id"]))
            label_de = label_en
        if not label_en and label_de:
            print("  note %s (%s): no usable EN label, using DE" % (name, entity["id"]))
            label_en = label_de
        if not label_en:
            print("  REJECT %s (%s): no usable label" % (name, entity["id"]))
            continue
        events.append(
            {"item": item, "qid": entity["id"], "year": year, "end": end, "circa": circa, "prop": prop,
             "de": label_de, "en": label_en}
        )
    events.sort(key=lambda e: e["year"])
    if len(events) > MAX_EVENTS:
        die("%s has %d events, the limit is %d" % (topic["id"], len(events), MAX_EVENTS))
    return events


def check_image(topic, item, file_info, pins, config):
    """None if this picture may be used, otherwise why not."""
    name = item["image"]
    if not file_info:
        return "no such file on Commons"
    if item.get("author"):
        file_info["artist"] = item["author"]
    restrictions = set(file_info["restrictions"].split("|"))
    if restrictions & {"trademarked", "insignia"}:
        return "restricted beyond its licence: " + file_info["restrictions"]
    if topic.get("sensitive") and item.get("tag") not in config["safe_tags"]:
        return "tag %r not allowed in a sensitive topic" % item.get("tag")
    problem = licence_problem(file_info)
    if problem:
        return problem
    for category in file_info["categories"]:
        if CATEGORY_DENY.search(category):
            return "category says no: " + category
    if pins.get(name) != file_info["sha1"]:
        return "not pinned, or the file changed since it was looked at"
    return None


def event_flags(event):
    """The flags field of an event line: 1 circa, 2 the year is a birth, 4 the date is from Wikipedia."""
    return (1 if event["circa"] else 0) + (2 if event["prop"] == "P569" else 0) + (4 if event["prop"] == "wikipedia" else 0)


def build_topic(client, topic, events, infos, pins, config):
    """Returns the .txt and .jpgs contents and the credit entries."""
    bundle = bytearray()
    lines = []
    credits = []
    for event in events:
        item = event["item"]
        name = item.get("image")
        offset = length = width = height = 0
        digest = credit = "-"
        if name:
            problem = check_image(topic, item, infos.get(name), pins, config)
            data = None
            if problem:
                print("  no image for %s, %s: %s" % (event["qid"], name, problem))
            else:
                data = client.get(infos[name]["thumb"])
                if data is None:
                    print("  no image for %s, %s: thumbnail not found" % (event["qid"], name))
            if data:
                jpeg, (width, height) = prepare_image(data, fit_of(item))
                if len(bundle) + len(jpeg) > MAX_BUNDLE:
                    print("  no image for %s, %s: bundle is full" % (event["qid"], name))
                    width = height = 0
                else:
                    offset = len(bundle)
                    length = len(jpeg)
                    bundle += jpeg
                    digest = "%08x" % fnv1a(jpeg)
                    credit = device_credit(infos[name])
                    credits.append((event, name, infos[name], credit_of(infos[name])))
        lines.append(
            "\t".join(
                [event["qid"], str(event["year"]), str(event["end"]) if event["end"] else "-",
                 str(event_flags(event)),
                 field(event["de"]), field(event["en"]),
                 str(offset), str(length), str(width), str(height), digest, field(credit)]
            )
        )
    return lines, bytes(bundle), credits


def credits_page(all_credits):
    """credits.html: maker, licence and file link of every picture. The maker is Commons' text without e-mail
    addresses, which this public page must not republish."""
    rows = []
    for topic_id, event, name, info, credit in all_credits:
        rows.append(
            "<tr><td>%s</td><td>%s</td><td><a href=\"%s\">%s</a></td><td>%s</td><td>%s</td><td>%s</td></tr>"
            % tuple(html.escape(str(x), quote=True) for x in
                    (topic_id, event["en"], info["page"], name, without_emails(info["artist"]) or "-",
                     info["licence"], "resized"))
        )
    return (
        "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><title>Zitro timeline picture credits</title>"
        "</head><body><h1>Zitro timeline picture credits</h1>"
        "<p>Pictures come from Wikimedia Commons under the licence shown; each was resized and may be cropped to fit "
        "the screen. Dates and names come from Wikidata (CC0), except a few dates of the history-of-life timeline, "
        "which come from pinned sentences of Wikipedia articles (CC BY-SA 4.0).</p>"
        "<table border=\"1\" cellpadding=\"4\"><tr><th>Timeline</th><th>Event</th><th>File</th><th>Author</th>"
        "<th>Licence</th><th>Changes</th></tr>\n" + "\n".join(rows) + "\n</table></body></html>\n"
    )


def write_credits(all_credits):
    for name in sorted({c[2] for c in all_credits if "personality" in c[3]["restrictions"]}):
        print("  note: %s shows a person whose rights may also apply (Commons: personality)" % name)
    with open(os.path.join(OUT_DIR, "credits.html"), "w", encoding="utf-8") as f:
        f.write(credits_page(all_credits))


def write_topics_header(topic_ids):
    lines = [
        "// Generated by scripts/make_timeline_data.py. The closed list of timelines: fetch() only accepts",
        "// these, and the prompt rule names exactly these, so the two can't drift apart.",
        "#pragma once",
        "",
        "static const char* const TIMELINE_TOPICS[] = {",
    ]
    lines += ['    "%s",' % t for t in topic_ids]
    lines += ["};", "", '#define TIMELINE_TOPIC_LIST "%s"' % ", ".join(topic_ids), ""]
    with open(TOPICS_HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def prepare_for_sheet(client, name, file_info, fit):
    data = client.get(file_info["thumb"])
    return prepare_image(data, fit)[0] if data else None


def contact_sheets(client, config, resolved, infos, pins, directory):
    """One captioned sheet per topic, of the pictures exactly as the device would get them."""
    os.makedirs(directory, exist_ok=True)
    for topic in config["topics"]:
        tiles = []
        work = os.path.join(directory, topic["id"])
        os.makedirs(work, exist_ok=True)
        for i, event in enumerate(resolved[topic["id"]]):
            name = event["item"].get("image")
            if not name or name not in infos:
                continue
            jpeg = prepare_for_sheet(client, name, infos[name], fit_of(event["item"]))
            if not jpeg:
                continue
            path = os.path.join(work, "%02d.jpg" % i)
            with open(path, "wb") as f:
                f.write(jpeg)
            caption = "%d %s\n%s\n%s [%s]" % (event["year"], event["en"][:34], name[:38], event["item"].get("tag", "?"),
                                              infos[name]["licence"][:18])
            tiles += ["(", path, "-background", "black", "-gravity", "center", "-extent", "%dx%d" % (IMAGE_W, IMAGE_H),
                      "-set", "label", caption, ")"]
        if not tiles:
            continue
        sheet = os.path.join(directory, topic["id"] + ".png")
        subprocess.run(
            ["magick", "montage", "-background", "black", "-fill", "white", "-pointsize", "11", "-tile", "4x", "-geometry", "%dx%d+4+4" % (IMAGE_W, IMAGE_H)] + tiles
            + [sheet],
            check=True,
        )
        print(sheet)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cache", required=True, help="scratch directory for downloads")
    parser.add_argument("--dates-only", action="store_true")
    parser.add_argument("--candidates")
    parser.add_argument("--contact-sheets")
    parser.add_argument("--pin", action="store_true")
    args = parser.parse_args()

    client = Client(args.cache)
    config = load_config()
    pins = load_pins()

    titles = sorted({e["wiki"] for t in config["topics"] for e in t["events"]} | {t["subject"] for t in config["topics"]})
    entities = wiki_entities(client, titles)
    # Titles come back normalised; look them up the way they were written, too
    for title in titles:
        if title not in entities:
            print("  no item for title: %s" % title)

    resolved = {t["id"]: resolve_topic(client, t, entities) for t in config["topics"]}

    if args.dates_only:
        for topic in config["topics"]:
            print(topic["id"])
            for e in resolved[topic["id"]]:
                print("  %s %11d %s %s%s  %s | %s" % (e["qid"], e["year"], e["end"] or "-", e["prop"],
                                                      " ca" if e["circa"] else "", e["de"], e["en"]))
        return

    if args.candidates:
        topic = next(t for t in config["topics"] if t["id"] == args.candidates)
        for e in resolved[topic["id"]]:
            print("%s %d %s" % (e["qid"], e["year"], e["en"]))
            for claim in statements(entities[e["item"]["wiki"]], "P18"):
                print("    " + claim["mainsnak"]["datavalue"]["value"])
        return

    names = sorted({e["image"] for t in config["topics"] for e in t["events"] if e.get("image")})
    infos = commons_info(client, names)
    for name in names:
        if name not in infos:
            print("  no such Commons file: %s" % name)

    if args.contact_sheets:
        contact_sheets(client, config, resolved, infos, pins, args.contact_sheets)
        return

    if args.pin:
        pinned = {n: infos[n]["sha1"] for n in names if n in infos}
        with open(PINS, "w", encoding="utf-8") as f:
            f.write("# Written by make_timeline_data.py --pin: Commons file -> SHA-1 when it was looked at.\n")
            f.write("# A file whose SHA-1 differs is not used until someone has looked at it again.\n{\n")
            for name in sorted(pinned):
                f.write("    %r: %r,\n" % (name, pinned[name]))
            f.write("}\n")
        print("pinned %d files" % len(pinned))
        return

    os.makedirs(OUT_DIR, exist_ok=True)
    all_credits = []
    topic_ids = []
    for topic in config["topics"]:
        events = resolved[topic["id"]]
        if len(events) < 3:
            print("  SKIP topic %s: only %d events" % (topic["id"], len(events)))
            continue
        subject = entities.get(topic["subject"])
        title_de = topic.get("title_de") or (label_of(subject, "de") if subject else None)
        title_en = topic.get("title_en") or (label_of(subject, "en") if subject else None)
        if not subject or not title_de or not title_en:
            print("  SKIP topic %s: no title from its subject item" % topic["id"])
            continue
        lines, bundle, credits = build_topic(client, topic, events, infos, pins, config)
        header = "\t".join(["ZTL1", topic["id"], subject["id"], field(title_de), field(title_en), str(len(lines)),
                            str(len(bundle))])
        with open(os.path.join(OUT_DIR, topic["id"] + ".txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join([header] + lines) + "\n")
        with open(os.path.join(OUT_DIR, topic["id"] + ".jpgs"), "wb") as f:
            f.write(bundle)
        topic_ids.append(topic["id"])
        all_credits += [(topic["id"],) + c for c in credits]
        with_image = sum(1 for line in lines if line.split("\t")[7] != "0")
        print("%s: %d events, %d with images, %d bytes" % (topic["id"], len(lines), with_image, len(bundle)))
    write_credits(all_credits)
    write_topics_header(topic_ids)


if __name__ == "__main__":
    main()
