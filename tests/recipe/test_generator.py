"""Tests the page converter on real Wikibooks pages (fixtures/), run by tests/recipe/run.sh."""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "scripts"))
import make_recipe_data as gen  # noqa: E402

with open(gen.CORE, encoding="utf-8") as f:
    CORE_TEXT = f.read()
WORDS = gen.core_list("RECIPE_ALCOHOL_WORDS", CORE_TEXT)
PARTS = gen.core_list("RECIPE_ALCOHOL_PARTS", CORE_TEXT)
failures = 0
checks = 0


def check(ok, what):
    global failures, checks
    checks += 1
    if not ok:
        failures += 1
        print("  FAIL", what)


def load(name):
    with open(os.path.join(HERE, "fixtures", name + ".json"), encoding="utf-8") as f:
        return json.load(f)


def convert(page):
    return gen.convert_page(page["text"], page["title"], page["lang"], "https://example.org/x", page["revid"], WORDS, PARTS)


def numbers(text):
    return sorted(re.findall(r"\d+", text))


def skipped(page, expected):
    try:
        convert(page)
        check(False, page["title"] + " should be left out")
    except gen.Skip as why:
        check(expected in str(why), "%s left out for: %s" % (page["title"], why))


# Real pages convert, with the numbers of the ingredient lines intact
page = load("en_pancakes")
recipe = convert(page)
check(recipe["title"] == "Pancakes (North American)", "title from the page")
check(any("120 g" in item for item in recipe["ingredients"]), "120 g flour survives")
check(any("1 ½ tsp" in item for item in recipe["ingredients"]), "fractions survive")
check(any("250 ml" in item for item in recipe["ingredients"]), "250 ml milk survives")
check(len(recipe["steps"]) == 6, "6 numbered steps")
parts = gen.sections(page["text"])
_, source = gen.wanted_section(parts, gen.INGREDIENT_HEADINGS)
source_numbers = numbers(" ".join(gen.strip_inline(line) for line in source))
check(numbers(" ".join(recipe["ingredients"])) == source_numbers, "every number in the ingredients is the source's")
for text in recipe["ingredients"] + recipe["steps"]:
    check(not re.search(r"\[\[|\]\]|\{\{|'''", text), "no markup left: " + text[:40])

german = convert(load("de_eierpfannkuchen"))
check(german["lang"] == "de" and german["title"] == "Eierpfannkuchen", "German page keeps its title")
check("6 Eier" in german["ingredients"], "German quantities")
check(not any("pro Ei" in item for item in german["ingredients"]), "the alternative after the note isn't mixed in")
check(not any("<ref" in item or "nachgesüßt" in item for item in german["ingredients"]), "footnotes removed")

# One paragraph of prose becomes its sentences, word for word
prose = convert(load("de_bananenbrot"))
check(len(prose["steps"]) >= 4, "paragraph split into steps")
source_steps = gen.wanted_section(gen.sections(load("de_bananenbrot")["text"]), gen.STEP_HEADINGS)[1]
paragraph = " ".join(gen.strip_inline(line) for line in source_steps if line.strip())
check(" ".join(prose["steps"]) == paragraph, "joined again, the sentences are the paragraph")

# Alcohol: from the page's own box, and from keywords when the box says nothing
skipped(load("de_kaiserschmarrn_alcohol"), "alcohol")
skipped(load("de_crepes_alcohol"), "alcohol")

# Templates: the temperature one is expanded, anything else is refused
check(gen.strip_inline("{{#invoke:temperature|f|375}}") == "375°F", "temperature template")
check(gen.strip_inline("{{#invoke:temperature|c|190}}") == "190°C", "temperature template")
try:
    gen.check_clean(gen.strip_inline("Bake {{convert|3|cm}} deep"))
    check(False, "unknown template refused")
except gen.Skip:
    pass
skipped(load("en_banana_table"), "table")

# Characters outside the font: mapped without changing a quantity, or refused
check(gen.strip_inline("1⅓ cups – about ⅔") == "1 1/3 cups - about 2/3", "fractions and dashes")
check(gen.strip_inline("it’s 3&nbsp;g") == "it's 3 g", "typographic quote and entity")
try:
    gen.check_clean("Rühren 中")
    check(False, "character outside the font refused")
except gen.Skip:
    pass

# Sentence splitting leaves abbreviations and numbers alone
check(gen.split_sentences("Ca. 3 Minuten braten. Dann z.B. Salz zugeben.") == ["Ca. 3 Minuten braten.", "Dann z.B. Salz zugeben."], "abbreviations")
check(gen.split_sentences("Stufe 2. Dann rühren!  Fertig") == ["Stufe 2. Dann rühren!", "Fertig"], "number before a dot")

# Same rules as the stick
check(gen.normalize_name("Spaghetti  Bolognese!") == "spaghetti bolognese", "name normalized")
check(gen.normalize_name("Crêpes") == "crêpes", "non-ASCII kept")
check(gen.alcohol_hit("Weißwein", WORDS, PARTS) == "weißwein", "compound")
check(gen.alcohol_hit("Schweinefleisch herum", WORDS, PARTS) is None, "no false hit inside words")
check(gen.alcohol_hit("Rumford baking powder", WORDS, PARTS) is None, "no false hit on a brand")
for name in gen.name_variants(["pancakes", "pancake"]):
    check(gen.normalize_name(name) == name, "index names are normalized")

print("generator: %d checks, %d failures" % (checks, failures))
sys.exit(1 if failures else 0)
