#!/usr/bin/env python3
"""Builds the prepared recipes in webpage2/data/recipe/ from Wikibooks (CC BY-SA 4.0).

  python3 -I scripts/make_recipe_data.py --cache <scratch dir> [--out webpage2/data/recipe] [--report]

Each dish below is a hand-checked pairing of an English and a German Wikibooks page. Nothing is rewritten: only
markup is removed and characters the stick's font lacks are replaced (a fraction sign by "1/3", a long dash by "-").
A page that doesn't parse cleanly, contains alcohol or is too long is left out, and the run says why.

Writes:
  index.txt           one line per name: <name>|<slug>|<languages>
  <slug>.<lang>.json  title, lang, url, revid, license, ingredients, steps
  CREDITS.txt         every page used, with its link and the licence
"""
import argparse
import html
import json
import os
import re
import subprocess
import sys
import time
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.join(HERE, "..", "sketch", "sketch_jan12a", "recipe_core.h")
USER_AGENT = "Zitro/1.0 (https://zitro.spot)"
HOST = {"en": "en.wikibooks.org", "de": "de.wikibooks.org"}
PREFIX = {"en": "Cookbook:", "de": "Kochbuch/ "}
LICENSE = "CC BY-SA 4.0"
LICENSE_URL = "https://creativecommons.org/licenses/by-sa/4.0/"
MAX_FILE_BYTES = 8 * 1024  # The stick accepts 16 KB
MAX_STEP_BYTES = 700
MAX_STEPS = 30
MAX_INGREDIENTS = 30

# slug, English names, English page, German page (either may be None). The first name is what the AI is told about
# through the prompt's example; German names are there in case the AI doesn't translate.
DISHES = [
    ("pancakes", ["pancakes", "american pancakes", "pfannkuchen", "eierpfannkuchen"], "Pancakes (North American)", "Eierpfannkuchen"),
    ("apple-pancakes", ["apple pancakes", "apfelpfannkuchen"], None, "Apfelpfannkuchen"),
    ("crepes", ["crepes", "crêpes", "crepe"], "Crêpes I", "Crêpes"),
    ("waffles", ["waffles", "waffeln"], "Breakfast Waffles", "Waffeln"),
    ("french-toast", ["french toast", "arme ritter"], "French Toast", None),
    ("scrambled-eggs", ["scrambled eggs", "ruehrei", "rührei"], "Basic Scrambled Eggs", "Rührei"),
    ("fried-eggs", ["fried eggs", "fried egg", "spiegelei"], "Fried Eggs", None),
    ("fried-rice", ["fried rice", "gebratener reis"], "Fried Rice", None),
    ("grilled-cheese", ["grilled cheese sandwich", "grilled cheese"], "Grilled Cheese Sandwich", None),
    ("white-bread", ["white bread", "bread", "brot", "weissbrot"], "White Bread", None),
    ("omelette", ["omelette", "omelet", "omelett"], "Cheese Omelette", "Omelette"),
    ("pizza", ["pizza", "pizza dough", "pizzateig"], "Pizza Dough", "Pizza mit Rukola"),
    ("spaghetti-bolognese", ["spaghetti bolognese", "bolognese", "spagetti bolognese"], "Bolognese Sauce", "Bologneser Sauce"),
    ("tomato-sauce", ["tomato sauce", "spaghetti with tomato sauce", "tomatensauce", "tomatensosse"], "Basic Pizza Sauce", "Tomatensauce"),
    ("lasagna", ["lasagna", "lasagne"], "Basic Lasagna", "Lasagne Bolognese"),
    ("macaroni-and-cheese", ["macaroni and cheese", "mac and cheese", "macaroni cheese", "kaesemakkaroni"], "Macaroni and Cheese I", None),
    ("mashed-potatoes", ["mashed potatoes", "kartoffelbrei", "kartoffelpueree"], "Mashed Potatoes", None),
    ("fried-potatoes", ["fried potatoes", "home fries", "bratkartoffeln"], None, "Bratkartoffeln"),
    ("french-fries", ["french fries", "fries", "chips", "pommes", "pommes frites"], "French Fries", None),
    ("potato-salad", ["potato salad", "kartoffelsalat"], "Potato Salad", "Kartoffelsalat"),
    ("potato-gratin", ["potato gratin", "gratin", "kartoffelgratin"], None, "Kartoffelgratin"),
    ("chicken-soup", ["chicken soup", "huehnersuppe", "hühnersuppe"], "Chicken Soup I", "Hühnersuppe"),
    ("vegetable-soup", ["vegetable soup", "gemuesesuppe"], "Vegetable Soup", None),
    ("lentil-soup", ["lentil soup", "linsensuppe"], "Egyptian Lentil Soup", None),
    ("chili", ["chili", "chili con carne"], "Easy Chili", "Chili con Carne einfach"),
    ("meatballs", ["meatballs", "frikadellen", "hamburger patties"], "Meatballs with Honey Lime Sauce", "Frikadellen"),
    ("spaetzle", ["spaetzle", "spätzle", "spatzle"], "Spaetzle", "Spätzle"),
    ("rice-pudding", ["rice pudding", "milchreis"], "Rice Pudding", "Milchreis"),
    ("vanilla-pudding", ["vanilla pudding", "vanillepudding"], None, "Vanillepudding"),
    ("hummus", ["hummus"], "Hummus I", None),
    ("guacamole", ["guacamole"], "Chunky Guacamole", "Guacamole"),
    ("fruit-salad", ["fruit salad", "obstsalat"], "Fruit Salad with Orange Juice", "Obstsalat"),
    ("popcorn", ["popcorn"], "Popcorn", None),
    ("lemonade", ["lemonade", "limonade"], "Lemonade", None),
    ("hot-chocolate", ["hot chocolate", "hot cocoa", "kakao", "heisse schokolade"], "Hot Chocolate", None),
    ("milkshake", ["milkshake", "vanilla milkshake", "milchshake"], "Classic Vanilla Milkshake", None),
    ("chocolate-milkshake", ["chocolate milkshake", "schoko milchshake"], "Chocolate Milkshake", None),
    ("banana-milkshake", ["banana milkshake", "bananenmilch"], "Banana Milkshake", None),
    ("muffins", ["muffins", "muffin", "blueberry muffins"], "Blueberry Muffin", "Muffins"),
    ("banana-bread", ["banana bread", "bananenbrot"], "Banana Bread II", "Bananenbrot"),
    ("chocolate-cake", ["chocolate cake", "schokoladenkuchen", "schokokuchen"], None, "Schokokuchen"),
    ("apple-cake", ["apple cake", "apple pie", "apfelkuchen"], "Apple Cake", "Apfelkuchen"),
    ("brownies", ["brownies", "brownie"], "Cake Brownies", "Brownies"),
    ("chocolate-chip-cookies", ["chocolate chip cookies", "cookies", "kekse", "plaetzchen"], "Chocolate Chip Cookies II", "Chocolate Cookies"),
    ("peanut-butter-cookies", ["peanut butter cookies", "erdnussbutterkekse"], "Peanut Butter Cookies", None),
    ("shortbread", ["shortbread"], "Shortbread", None),
    ("gingerbread", ["gingerbread", "lebkuchen"], "Gingerbread", None),
]

# Everything the 10 pt font lacks that real pages use. Nothing here may change a quantity.
CHAR_MAP = {
    "–": "-", "—": "-", "‒": "-", "−": "-", "‐": "-", "‑": "-",
    "‘": "'", "’": "'", "‚": "'", "“": '"', "”": '"', "„": '"', "…": "...",
    " ": " ", " ": " ", " ": " ", " ": " ", " ": " ", "​": "",
    "⅓": " 1/3", "⅔": " 2/3", "⅛": " 1/8", "⅜": " 3/8", "⅝": " 5/8", "⅞": " 7/8",
    "⅕": " 1/5", "⅙": " 1/6",
    "⁄": "/", "•": "-", "→": "->",
}
FONT_LAST = 0x5FF

INGREDIENT_HEADINGS = re.compile(r"^(ingredients?|zutaten|grundrezept)\b", re.I)
STEP_HEADINGS = re.compile(r"^(procedure|procedures|directions?|method|preparation|instructions?|steps?|zubereitung|arbeitsschritte|anleitung)\b", re.I)


# A sentence ends here unless the word before the dot is one of these, or a number ("2. Stufe")
ABBREVIATIONS = {"ca", "evtl", "z", "b", "bzw", "ggf", "etc", "el", "tl", "kl", "gr", "min", "std", "approx", "e", "g", "i",
                 "e.g", "i.e", "z.b", "u", "a", "u.a", "d", "h", "d.h", "ggfs", "max", "esp", "pkg", "oz", "tbsp", "tsp"}


class Skip(Exception):
    pass


def core_list(name, core_text):
    match = re.search(r"static const char\* const " + name + r"\[\] = \{(.*?)\};", core_text, re.S)
    if not match:
        raise SystemExit("keyword list %s not found in recipe_core.h" % name)
    return re.findall(r'"([^"]*)"', match.group(1))


def has_word(lowered, word):
    return re.search(r"(?<![a-z\u0080-\U0010ffff])" + re.escape(word) + r"(?![a-z\u0080-\U0010ffff])", lowered) is not None


def alcohol_hit(text, words, parts):
    """The same test as the stick's, from the same lists. Returns the word found, or None."""
    lowered = text.lower()
    for word in words:
        if has_word(lowered, word):
            return word
    for part in parts:
        if part in lowered:
            return part
    return None


def normalize_name(text):
    """As recipeNormalizeQuery: lower case, letters and digits, one space between words."""
    out = []
    pending = False
    for ch in text:
        lower = ch.lower() if ch.isascii() else ch
        keep = lower.isascii() and lower.isalnum() or not lower.isascii()
        if not keep:
            pending = bool(out)
            continue
        if pending:
            out.append(" ")
            pending = False
        out.append(lower)
    return "".join(out)


def split_sentences(text):
    """A paragraph of prose into its sentences, word for word."""
    out, start = [], 0
    for match in re.finditer(r"[.!?]\s+(?=[A-Z\u00c4\u00d6\u00dc])", text):
        before = re.search(r"([\w.]*)$", text[start:match.start()]).group(1).lower()
        if text[match.start()] == "." and (before in ABBREVIATIONS or before.isdigit()):
            continue
        out.append(text[start:match.start() + 1].strip())
        start = match.end()
    out.append(text[start:].strip())
    return [piece for piece in out if piece]


def strip_inline(text):
    """Wiki markup to plain text. Only the temperature template is expanded, the way the wiki shows it: a line
    with any other template left is refused later."""
    text = re.sub(r"\{\{#invoke:temperature\|([fc])\|([\d.]+)\}\}", lambda m: m.group(2) + "\u00b0" + m.group(1).upper(), text, flags=re.I)
    text = re.sub(r"<ref\b[^>]*/>", "", text, flags=re.I)
    text = re.sub(r"<ref\b.*?</ref>", "", text, flags=re.I | re.S)
    text = re.sub(r"<br\s*/?>", " ", text, flags=re.I)
    text = re.sub(r"</?(?:small|big|sup|sub|b|i|u|nowiki|span|div)\b[^>]*>", "", text, flags=re.I)
    text = re.sub(r"\[\[(?:File|Image|Bild|Datei|Category|Kategorie):[^\]]*\]\]", "", text, flags=re.I)
    text = re.sub(r"\[\[[^\]|]*\|([^\]]*)\]\]", r"\1", text)
    text = re.sub(r"\[\[([^\]]*)\]\]", lambda m: m.group(1).split(":")[-1].replace("Kochbuch/ ", ""), text)
    text = re.sub(r"\[https?://\S+\s+([^\]]*)\]", r"\1", text)
    text = text.replace("'''", "").replace("''", "")
    text = html.unescape(text)
    for bad, good in CHAR_MAP.items():
        text = text.replace(bad, good)
    return re.sub(r"\s+", " ", text).strip()


def check_clean(text):
    if re.search(r"\{\{|\}\}|\[\[|\]\]|<[a-zA-Z/]|\{\||\|\}|https?://", text):
        raise Skip("markup left: " + text[:60])
    for ch in text:
        if ord(ch) > FONT_LAST or ord(ch) < 0x20:
            raise Skip("character U+%04X not in the font: %s" % (ord(ch), text[:40]))


def sections(wikitext):
    """(level, heading, body lines) in page order."""
    wikitext = re.sub(r"<!--.*?-->", "", wikitext, flags=re.S)
    result = []
    level, title, lines = 0, "", []
    for raw in wikitext.split("\n"):
        heading = re.match(r"^(={1,6})\s*(.*?)\s*\1\s*$", raw)
        if heading:
            result.append((level, title, lines))
            level, title, lines = len(heading.group(1)), strip_inline(heading.group(2)), []
        else:
            lines.append(raw)
    result.append((level, title, lines))
    return result


def wanted_section(parts, pattern, after=0):
    """Index of the first matching heading, and its lines plus those of deeper headings below it."""
    for i in range(after, len(parts)):
        level, title, lines = parts[i]
        if level and pattern.search(title):
            out = list(lines)
            j = i + 1
            while j < len(parts) and parts[j][0] > level:
                out.append("\0" + parts[j][1])  # A sub-heading: names the part that follows
                out.extend(parts[j][2])
                j += 1
            return i, out
    return None, None


def is_bold_label(line):
    return re.match(r"^'''[^']+'''\s*:?\s*$", line) is not None


def parse_ingredients(lines):
    """List items and the labels between them. A note of plain text ends the list: what follows is an alternative."""
    items = []
    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("\0"):
            label = strip_inline(line[1:])
            if label:
                items.append(label.rstrip(":") + ":")
            continue
        if line[0] == ";" or is_bold_label(line):
            items.append(strip_inline(line.lstrip("; ")).rstrip(":") + ":")
            continue
        if line[0] not in "*#:":
            if line.startswith(("[[", "{{", "{|", "<")):
                if line.startswith("{|"):
                    raise Skip("ingredients in a table")
                continue
            text = strip_inline(line)
            if len(text) <= 40 and (text.endswith(":") or text.lower().startswith(("f\u00fcr ", "for "))):
                items.append(text.rstrip(":") + ":")
                continue
            if items:
                break
            continue
        text = strip_inline(line.lstrip("*#: "))
        if text:
            items.append(text)
    return items


def parse_steps(lines):
    """List items and paragraphs. Prose next to a list is an introduction if it's the smaller part, and left out."""
    entries = []  # (is a list item, text)
    label = ""
    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        if line.startswith("\0"):
            label = strip_inline(line[1:]).rstrip(":")
            continue
        if line[0] == ";":
            label = strip_inline(line.lstrip("; ")).rstrip(":")
            if re.match(r"^(variante|variation|tipp|note)", label, re.I):
                break
            continue
        if is_bold_label(line):
            label = strip_inline(line).rstrip(":")
            continue
        if re.match(r"^(\[\[(File|Image|Bild|Datei|Kategorie|Category):|\{\{|<noinclude)", line, re.I):
            continue
        text = strip_inline(line.lstrip("*#: "))
        if not text:
            continue
        # "Rohe Kartoffeln:" or "Schritt 1 - Der Teig" names what follows; it isn't a step of its own
        if text.endswith(":") or re.match(r"^Schritt \d+\b", text):
            label = text.rstrip(":")
            continue
        entries.append((line[0] in "*#:", label + ": " + text if label else text))
        label = ""
    listed = sum(len(t) for is_list, t in entries if is_list)
    prose = sum(len(t) for is_list, t in entries if not is_list)
    if listed > prose:
        entries = [e for e in entries if e[0]]
    steps = [t for _, t in entries]
    if len(steps) == 1:
        steps = split_sentences(steps[0])
    return steps


def convert_page(wikitext, title, lang, url, revid, alcohol_words, alcohol_parts):
    """One page to one recipe, or Skip with the reason."""
    infobox = re.search(r"\|\s*Alkohol\s*=\s*([^|\n<]*)", wikitext)
    if infobox and infobox.group(1).strip().lower().startswith("ja"):
        raise Skip("page says it has alcohol")
    parts = sections(wikitext)
    first, ingredient_lines = wanted_section(parts, INGREDIENT_HEADINGS)
    if first is None:
        raise Skip("no ingredients section")
    _, step_lines = wanted_section(parts, STEP_HEADINGS, first + 1)
    if step_lines is None:
        raise Skip("no steps section after the ingredients")
    ingredients = parse_ingredients(ingredient_lines)
    steps = parse_steps(step_lines)
    if len(ingredients) < 2:
        raise Skip("fewer than 2 ingredients")
    if len(steps) < 2:
        raise Skip("fewer than 2 steps")
    if len(ingredients) > MAX_INGREDIENTS or len(steps) > MAX_STEPS:
        raise Skip("too many ingredients or steps")
    label = strip_inline(title.replace(PREFIX[lang], "", 1))
    if len(label.encode("utf-8")) > 60:
        raise Skip("title too long")
    for text in [label] + ingredients + steps:
        check_clean(text)
        hit = alcohol_hit(text, alcohol_words, alcohol_parts)
        if hit:
            raise Skip("alcohol: " + hit)
    for step in steps:
        if len(step.encode("utf-8")) > MAX_STEP_BYTES:
            raise Skip("a step of %d bytes" % len(step.encode("utf-8")))
    recipe = {
        "title": label,
        "lang": lang,
        "source": "Wikibooks",
        "url": url,
        "revid": revid,
        "license": LICENSE,
        "ingredients": ingredients,
        "steps": steps,
    }
    if len(json.dumps(recipe, ensure_ascii=False, separators=(",", ":")).encode("utf-8")) > MAX_FILE_BYTES:
        raise Skip("file too large")
    return recipe


def fetch_batch(lang, titles):
    """Page text and revision of up to 50 titles at once, which is what the API prefers."""
    for attempt in range(6):
        raw = subprocess.check_output(
            ["curl", "-sS", "-A", USER_AGENT, "-m", "60", "-G", "https://%s/w/api.php" % HOST[lang],
             "--data-urlencode", "action=query", "--data-urlencode", "prop=revisions",
             "--data-urlencode", "rvprop=ids|content", "--data-urlencode", "rvslots=main",
             "--data-urlencode", "format=json", "--data-urlencode", "formatversion=2",
             "--data-urlencode", "redirects=1", "--data-urlencode", "titles=" + "|".join(titles)])
        try:
            return json.loads(raw)["query"]
        except (ValueError, KeyError):
            time.sleep(20 * (attempt + 1))
    raise SystemExit("Wikibooks keeps refusing: " + raw[:200].decode("utf-8", "replace"))


def load_pages(lang, wanted, cache_dir):
    path = os.path.join(cache_dir, "pages_%s.json" % lang)
    pages = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            pages = json.load(f)
    missing = [t for t in wanted if t not in pages]
    for i in range(0, len(missing), 50):
        batch = missing[i:i + 50]
        query = fetch_batch(lang, batch)
        renames = {r["from"]: r["to"] for key in ("normalized", "redirects") for r in query.get(key, [])}
        found = {p["title"]: p for p in query["pages"]}
        for title in batch:
            target = title
            while target in renames:
                target = renames[target]
            page = found.get(target, {})
            revisions = page.get("revisions")
            pages[title] = {"title": target, "revid": revisions[0]["revid"], "text": revisions[0]["slots"]["main"]["content"]} if revisions else None
        with open(path, "w", encoding="utf-8") as f:
            json.dump(pages, f, ensure_ascii=False)
        time.sleep(3)
    return pages


def page_url(lang, title):
    return "https://%s/wiki/%s" % (HOST[lang], urllib.parse.quote(title.replace(" ", "_"), safe=":/"))


def name_variants(names):
    out = []
    for name in names:
        norm = normalize_name(name)
        for variant in (norm, norm[:-1] if norm.endswith("s") else norm + "s"):
            if variant and variant not in out:
                out.append(variant)
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cache", required=True, help="scratch directory for the downloaded pages")
    parser.add_argument("--out", default=os.path.join(HERE, "..", "webpage2", "data", "recipe"))
    parser.add_argument("--report", action="store_true", help="only print what would be built")
    args = parser.parse_args()
    with open(CORE, encoding="utf-8") as f:
        core = f.read()
    alcohol_words = core_list("RECIPE_ALCOHOL_WORDS", core)
    alcohol_parts = core_list("RECIPE_ALCOHOL_PARTS", core)
    os.makedirs(args.cache, exist_ok=True)
    wanted = {"en": [], "de": []}
    for _, _, en, de in DISHES:
        if en:
            wanted["en"].append(PREFIX["en"] + en)
        if de:
            wanted["de"].append(PREFIX["de"] + de)
    pages = {lang: load_pages(lang, wanted[lang], args.cache) for lang in wanted}

    index_lines, credits, files, seen = [], [], {}, {}
    for slug, names, en, de in DISHES:
        langs = []
        for lang, page in (("de", de), ("en", en)):
            if not page:
                continue
            title = PREFIX[lang] + page
            record = pages[lang].get(title)
            try:
                if not record:
                    raise Skip("page not found")
                recipe = convert_page(record["text"], record["title"], lang, page_url(lang, record["title"]),
                                      record["revid"], alcohol_words, alcohol_parts)
            except Skip as why:
                print("  left out %-26s %s: %s" % (slug, lang, why))
                continue
            langs.append(lang)
            files["%s.%s.json" % (slug, lang)] = recipe
            credits.append('"%s", %s, %s, %s; markup removed, characters adapted to the display.' % (
                recipe["title"], "Wikibooks contributors", recipe["url"], LICENSE))
            longest = max(len(s) for s in recipe["steps"])
            print("  %-26s %s %2d ingredients, %2d steps, longest step %3d chars: %s" % (
                slug, lang, len(recipe["ingredients"]), len(recipe["steps"]), longest, recipe["title"]))
        if not langs:
            continue
        for name in name_variants(names):
            if name in seen:
                raise SystemExit("name '%s' is used by %s and %s" % (name, seen[name], slug))
            seen[name] = slug
            index_lines.append("%s|%s|%s" % (name, slug, " ".join(langs)))
    print("%d files, %d names, %d dishes" % (len(files), len(index_lines), len(set(seen.values()))))
    if args.report:
        return
    os.makedirs(args.out, exist_ok=True)
    for name in os.listdir(args.out):
        if name.endswith(".json"):
            os.remove(os.path.join(args.out, name))
    for name, recipe in files.items():
        with open(os.path.join(args.out, name), "w", encoding="utf-8") as f:
            json.dump(recipe, f, ensure_ascii=False, separators=(",", ":"))
    with open(os.path.join(args.out, "index.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(index_lines) + "\n")
    with open(os.path.join(args.out, "CREDITS.txt"), "w", encoding="utf-8") as f:
        f.write("Recipes shown by Zitro. Text from Wikibooks, by its contributors, under %s (%s).\n"
                "Authors are listed in each page's history. Changes: wiki markup removed, characters adapted to the "
                "display, only the first variant of a page used.\n\n" % (LICENSE, LICENSE_URL))
        f.write("\n".join(sorted(credits)) + "\n")


if __name__ == "__main__":
    sys.exit(main())
