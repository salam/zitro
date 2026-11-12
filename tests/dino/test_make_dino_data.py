#!/usr/bin/env python3
"""Tests the dinosaur generator's pure functions, with no downloads. Run by tests/dino/run.sh."""
import contextlib
import io
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import make_dino_data as m  # noqa: E402

failures = 0


def check(condition, what):
    global failures
    if not condition:
        failures += 1
        print("  FAIL", what)


def raises(function, *args):
    try:
        function(*args)
    except ValueError:
        return True
    return False


def test_units():
    check(m.convert("+18", "Q3710", "length") == 549, "18 ft")
    check(m.convert("+38000", "Q100995", "mass") == 17236510, "38000 lb")
    check(m.convert("+12.0", "Q11573", "length") == 1200, "12 m")
    check(m.convert("+12.0", "Q999999999", "length") is None, "unknown unit")
    check(m.convert("+12.0", "Q11570", "length") is None, "a mass unit for a length")
    check(m.convert("x", "Q11573", "length") is None, "not a number")
    check(m.convert("+0", "Q11573", "length") is None, "zero")
    check(m.convert("-3", "Q11573", "length") is None, "negative")
    check(m.convert("+2", "Q191118", "mass") == 2000000, "2 t")


def statement(amount, unit="Q11573"):
    return {"mainsnak": {"snaktype": "value", "datavalue": {"value": {
        "amount": amount, "unit": "http://www.wikidata.org/entity/" + unit if unit else "1"}}}, "rank": "normal"}


def test_quantities():
    before = len(m.DROP_LINES)
    entity = {"claims": {"P2043": [statement("+12"), statement("+13"), statement("+40")]}}
    check(m.quantities(entity, "P2043", "length", "X") == (1200, 4000), "disagreeing statements give a range")
    check(len(m.DROP_LINES) == before, "nothing dropped for a wide spread")
    entity = {"claims": {"P2043": [statement("+12"), statement("+900")]}}
    check(m.quantities(entity, "P2043", "length", "X") == (1200, 1200), "a value over the ceiling is left out")
    check(len(m.DROP_LINES) == before + 1 and "implausible" in m.DROP_LINES[-1] and "90000" in m.DROP_LINES[-1],
          "... and logged with value and reason")
    entity = {"claims": {"P2043": [statement("+12"), statement("+5", None), statement("+5", "Q999999")]}}
    check(m.quantities(entity, "P2043", "length", "X") == (1200, 1200), "unit-less and unknown-unit values dropped")
    check(len(m.DROP_LINES) == before + 3, "... and logged")
    entity = {"claims": {"P2043": [statement("+5", None)]}}
    check(m.quantities(entity, "P2043", "length", "X") is None, "no usable value")
    entity = {"claims": {"P2067": [statement("+170", "Q191118")]}}
    check(m.quantities(entity, "P2067", "mass", "X") == (170000000, 170000000), "170 t is accepted")
    entity = {"claims": {"P2048": [statement("+70", "Q3710")]}}
    check(m.quantities(entity, "P2048", "height", "X") == (2100, 2100),
          "Argentinosaurus' 70 ft height is kept, at the 2 significant digits of 70: 21 m, not 21.34 m")


def test_imperial_values_keep_their_precision():
    # Feet and pounds are rounded to the significant digits of the Wikidata amount, so a converted value
    # doesn't look more exact than its source; metric values stay exact (convert() itself stays exact)
    check(m.significant_digits("+18") == 2 and m.significant_digits("+38000") == 2, "18 and 38000: 2 digits")
    check(m.significant_digits("+5.50") == 3, "a zero after the point counts")
    check(m.significant_digits("+70") == 2 and m.significant_digits("+5") == 2, "at least 2")
    entity = {"claims": {"P2048": [statement("+18", "Q3710")]}}
    check(m.quantities(entity, "P2048", "height", "X") == (550, 550), "Spinosaurus 18 ft: 5,5 m, not 5,49 m")
    entity = {"claims": {"P2067": [statement("+38000", "Q100995")]}}
    check(m.quantities(entity, "P2067", "mass", "X") == (17000000, 17000000), "Spinosaurus 38000 lb: 17 t, not 17,2 t")
    entity = {"claims": {"P2048": [statement("+16", "Q3710")]}}
    check(m.quantities(entity, "P2048", "height", "X") == (490, 490), "Quetzalcoatlus 16 ft: 4,9 m")
    entity = {"claims": {"P2043": [statement("+12.34")]}}
    check(m.quantities(entity, "P2043", "length", "X") == (1234, 1234), "metres stay exact")
    entity = {"claims": {"P2043": [statement("+18", "Q3710"), statement("+6")]}}
    check(m.quantities(entity, "P2043", "length", "X") == (550, 600), "a rounded value still makes a range")


def test_ranges():
    check(m.value_range([1200, 1300, 1200]) == (1200, 1300), "min and max")
    check(m.value_range([]) is None, "no values")
    check(m.value_range([5]) == (5, 5), "one value")


def test_classify():
    members = {"dinosaur": {"Q1", "Q2", "Q3", "Q5"}, "early_bird": {"Q2"}, "modern_birds": {"Q3"},
               "pterosaur": {"Q4"}, "marine": {"Q6"}}
    check(m.classify("Q3", members) is None, "dinosaur and modern bird")
    check(m.classify("Q2", members) == 1, "early bird")
    check(m.classify("Q1", members) == 0, "dinosaur")
    check(m.classify("Q4", members) == 2, "pterosaur")
    check(m.classify("Q6", members) == 3, "marine")
    check(m.classify("Q9", members) is None, "unknown")
    members["modern_birds"].add("Q2")
    check(m.classify("Q2", members) is None, "early bird that is also a modern bird")


PERIODS = [(298900000, 251900000, "Perm", "Permian"), (251900000, 201400000, "Trias", "Triassic"),
           (201400000, 145000000, "Jura", "Jurassic"), (145000000, 66000000, "Kreide", "Cretaceous"),
           (66000000, 23000000, "Paläogen", "Paleogene")]


def test_era():
    check(m.era_text(68000000, 66000000, PERIODS) == ("Kreide", "Cretaceous"), "68-66 Ma")
    check(m.era_text(150000000, 140000000, PERIODS) == ("Jura bis Kreide", "Jurassic to Cretaceous"), "150-140 Ma")
    check(m.era_text(145000000, 100000000, PERIODS) == ("Kreide", "Cretaceous"), "from the Jurassic boundary")
    check(m.era_text(230000000, 70000000, PERIODS) == ("Trias bis Kreide", "Triassic to Cretaceous"), "three periods")
    check(m.era_text(900000000, 800000000, PERIODS) is None, "before the periods")
    check(m.era_text(82000000, 65000000, PERIODS) == ("Kreide", "Cretaceous"), "82-65 Ma: Wikidata rounds 66 to 65")
    check(m.era_text(66500000, 65900000, PERIODS) == ("Kreide", "Cretaceous"), "straddling a boundary by less than 1 Ma")
    check(m.era_text(80000000, 60000000, PERIODS) == ("Kreide bis Pal\u00e4ogen", "Cretaceous to Paleogene"),
          "a real crossing")
    check(m.era_text(150000000, 140000000, list(reversed(PERIODS))) == ("Jura bis Kreide", "Jurassic to Cretaceous"),
          "periods in any order")


def test_normalise():
    check(m.normalise("T-Rex") == "t rex", "T-Rex")
    check(m.normalise("Tyrannosaurus  Rex") == "tyrannosaurus rex", "two spaces")
    check(m.normalise("Ärger") == "aerger", "umlaut")
    check(m.normalise("Café") is None, "other accent")
    check(m.normalise("A|B") is None, "separator")
    check(m.normalise("  ") is None, "blank")


def test_slug():
    check(m.slug_of("Tyrannosaurus") == "tyrannosaurus", "plain")
    check(m.slug_of("Gen. X-1") == "gen__x_1", "punctuation")
    check(len(m.slug_of("A" * 100)) <= 40, "length")


def test_index_aliases():
    files = m.index_files([("t rex", "tyrannosaurus"), ("tyrannosaurus", "tyrannosaurus"),
                           ("velociraptor mongoliensis", "velociraptor")])
    check(set(files) == {"t_", "ty", "ve"}, "prefixes %s" % sorted(files))
    check(files["t_"] == "ZDX1\tt_\t1\nt rex\ttyrannosaurus\n", "file text %r" % files["t_"])
    # a name that points to two slugs is dropped
    files = m.index_files([("rex", "a"), ("rex", "b"), ("rexa", "a")])
    check(files == {"re": "ZDX1\tre\t1\nrexa\ta\n"}, "ambiguous dropped %r" % files)
    # a genus name wins over an alias
    files = m.index_files([("bird", "bird"), ("bird", "other")])
    check(files == {"bi": "ZDX1\tbi\t1\nbird\tbird\n"}, "genus wins %r" % files)
    # entries sorted, no duplicates
    files = m.index_files([("tb", "x"), ("ta", "y"), ("ta", "y")])
    check(files == {"tb": "ZDX1\ttb\t1\ntb\tx\n", "ta": "ZDX1\tta\t1\nta\ty\n"}, "split %r" % files)
    files = m.index_files([("b2", "z"), ("b1", "y"), ("b1", "y")])
    check(files == {"b2": "ZDX1\tb2\t1\nb2\tz\n", "b1": "ZDX1\tb1\t1\nb1\ty\n"}, "prefix by two characters")
    files = m.index_files([("ab2", "z"), ("ab1", "y"), ("ab1", "y")])
    check(files["ab"] == "ZDX1\tab\t2\nab1\ty\nab2\tz\n", "sorted %r" % files)
    many = [("aa%04d%s" % (i, "x" * 20), "slug%d" % i) for i in range(1000)]
    check(raises(m.index_files, many), "1000 long names with one prefix")


def good_sheet():
    return {"slug": "tyrannosaurus", "qid": "Q14332", "name": "Tyrannosaurus", "group": 0, "image": None,
            "length": (1200, 1300), "height": None, "wingspan": None, "mass": (8800000, 8800000),
            "speed": None, "egg": None, "lived": (68000000, 66000000), "named": 1905,
            "era": {"de": "Kreide", "en": "Cretaceous"}, "meaning": None, "diet": None, "found": None,
            "fun": None, "source": "Wikidata"}


def test_sheet_text():
    text = m.sheet_text(good_sheet())
    check(text.startswith("ZDS1\ttyrannosaurus\tQ14332\nname\tTyrannosaurus\ngroup\t0\n"), "head %r" % text[:60])
    check("length\t1200\t1300\n" in text and "mass\t8800000\t8800000\n" in text, "ranges")
    check("lived\t68000000\t66000000\n" in text and "named\t1905\n" in text, "lived, named")
    check("era\tKreide\tCretaceous\n" in text and text.endswith("source\tWikidata\nend\n"), "era, end")
    check("height" not in text and "speed" not in text, "missing values left out")


def test_sheet_text_refuses():
    s = good_sheet()
    s["name"] = "Tyranno–saurus"
    check(raises(m.sheet_text, s), "en dash in name")
    s = good_sheet()
    s["meaning"] = {"de": "x" * 161, "en": "y"}
    check(raises(m.sheet_text, s), "161-byte meaning")
    s["meaning"] = {"de": "x" * 160, "en": "y" * 160}
    check(not raises(m.sheet_text, s), "160-byte meaning fits")
    s = good_sheet()
    s["meaning"] = {"de": "\u4e2d", "en": "centre"}
    check(raises(m.sheet_text, s), "a CJK character is outside the device font")
    s["meaning"] = {"de": "\u03b1", "en": "alpha"}
    check(not raises(m.sheet_text, s), "Greek is inside the font")
    s = good_sheet()
    s["meaning"] = {"de": "Flügel", "en": "wing"}
    check(not raises(m.sheet_text, s), "an umlaut is fine in a text field")
    s = good_sheet()
    s["source"] = "Wikidata ä"
    check(raises(m.sheet_text, s), "footer must be ASCII")
    s = good_sheet()
    s["fun"] = {"de": "a", "en": "b", "source": "Wikipedia – EN"}
    check(raises(m.sheet_text, s), "fun source must be ASCII")
    s = good_sheet()
    s["meaning"] = {"de": "a\tb", "en": "b"}
    check(raises(m.sheet_text, s), "tab in a text")
    s = good_sheet()
    s["lived"] = (66000000, 68000000)
    check(raises(m.sheet_text, s), "younger before older")
    s = good_sheet()
    s["length"] = (0, 5)
    check(raises(m.sheet_text, s), "zero length")


def test_sheet_text_languages():
    s = good_sheet()
    s["meaning"] = {"de": "Tyrannenechse"}
    check("meaning\tTyrannenechse\tTyrannenechse\n" in m.sheet_text(s), "DE only")
    s["meaning"] = {"en": "tyrant lizard"}
    check("meaning\ttyrant lizard\ttyrant lizard\n" in m.sheet_text(s), "EN only")
    s["fun"] = {"en": "A fact", "source": "Wikipedia (EN): X"}
    check("fun\tA fact\tA fact\tWikipedia (EN): X\n" in m.sheet_text(s), "fun, EN only")


def test_sheet_text_size():
    s = good_sheet()
    s["meaning"] = {"de": "x" * 160, "en": "y" * 160}
    s["diet"] = {"de": "x" * 160, "en": "y" * 160}
    s["found"] = {"de": "x" * 160, "en": "y" * 160}
    s["era"] = {"de": "x" * 160, "en": "y" * 160}
    s["fun"] = {"de": "x" * 160, "en": "y" * 160, "source": "s" * 160}
    check(not raises(m.sheet_text, s), "the largest possible text sheet fits %d bytes" % m.LIMITS["sheet"])
    m.LIMITS["sheet"] = 500
    check(raises(m.sheet_text, s), "a sheet over the limit")
    m.LIMITS["sheet"] = 2048


def test_worth_showing():
    # The cases of testWorthShowing in test_dino.cpp: sheetWith(image, rows, fun). A fun fact is no row.
    def sheet(image, rows, fun):
        keys = ["length", "height", "mass", "speed", "egg", "lived", "named", "era"]
        s = {"image": {"length": 1} if image else None, "fun": {"de": "d", "en": "e"} if fun else None}
        for i, key in enumerate(keys):
            s[key] = None if i >= rows else ({"de": "a", "en": "b"} if key == "era" else (1, 1))
        return s

    check(m.worth_showing(sheet(True, 0, False)), "a picture alone")
    check(m.worth_showing(sheet(False, 2, False)), "two rows")
    check(not m.worth_showing(sheet(False, 1, False)), "one row")
    check(not m.worth_showing(sheet(False, 0, True)), "a fun fact alone")
    check(not m.worth_showing(sheet(False, 0, False)), "nothing")
    check(m.worth_showing(sheet(False, 8, True)), "eight rows and a fun fact")
    check(not m.worth_showing({"image": None, "length": (1, 1)}), "missing keys count as absent")


def clean_file():
    return {"sha1": "a" * 40, "thumb": "https://upload.wikimedia.org/x.png", "page": "https://commons.wikimedia.org/x",
            "mime": "image/png", "licence": "CC BY-SA 4.0", "code": "cc-by-sa-4.0", "artist": "Nobu Tamura",
            "needs_credit": True, "restrictions": "", "categories": ["Category:Tyrannosaurus in art"]}


def test_picture_problem():
    pins = {"Q14332": "a" * 40}
    f = clean_file()
    f["categories"] = ["Category:Paleoart of predation"]
    check(m.picture_problem(f, {}, pins, "Q14332"), "a predation category")
    f = clean_file()
    f["categories"] = ["Category:Dinosaur carcasses"]
    check(m.picture_problem(f, {}, pins, "Q14332"), "a carcass category")
    f = clean_file()
    f["categories"] = ["Category:Nude people"]
    check(m.picture_problem(f, {}, pins, "Q14332"), "the timeline's words still count")
    f = clean_file()
    f["licence"], f["code"] = "CC BY-NC 4.0", "cc-by-nc-4.0"
    check(m.picture_problem(f, {}, pins, "Q14332"), "CC BY-NC 4.0")
    f = clean_file()
    f["artist"] = ""
    check(m.picture_problem(f, {}, pins, "Q14332"), "no artist")
    f = clean_file()
    f["artist"], f["needs_credit"], f["licence"], f["code"] = "", False, "Public domain", "pd"
    check(m.picture_problem(f, {}, pins, "Q14332"), "public domain still needs a named author")
    f = clean_file()
    f["artist"] = "Unknown author"
    check(m.picture_problem(f, {}, pins, "Q14332"), "'Unknown author' names nobody")
    f = clean_file()
    f["restrictions"] = "trademarked"
    check(m.picture_problem(f, {}, pins, "Q14332"), "trademarked")
    f = clean_file()
    f["mime"] = "application/pdf"
    check(m.picture_problem(f, {}, pins, "Q14332"), "a PDF is no picture")
    check(m.picture_problem(None, {}, pins, "Q14332"), "no such file")
    check(m.picture_problem(clean_file(), {"Q14332": "predation"}, pins, "Q14332"), "a QID in rejects")
    check(m.picture_problem(clean_file(), {}, {"Q14332": "b" * 40}, "Q14332"), "a SHA-1 different from the pin")
    check(m.picture_problem(clean_file(), {}, {}, "Q14332"), "not pinned")
    check(m.picture_problem(clean_file(), {}, {"Q1": "a" * 40}, "Q14332"), "pinned for another genus")
    check(m.picture_problem(clean_file(), {}, pins, "Q14332") is None, "a clean file with CC BY-SA 4.0")
    check(m.picture_problem(clean_file(), {}, {}, "Q14332", check_pin=False) is None, "... before pinning")
    check(m.picture_problem(clean_file(), {"Q14332": "x"}, {}, "Q14332", check_pin=False), "rejects count before pinning")
    # Every reason starts with the kind of check, so the log can be counted
    f = clean_file()
    f["licence"], f["code"] = "CC BY-NC 4.0", "cc-by-nc-4.0"
    check(m.picture_problem(f, {}, pins, "Q14332").startswith("licence:"), "licence prefix")
    f = clean_file()
    f["artist"] = ""
    check(m.picture_problem(f, {}, pins, "Q14332").startswith("author:"), "author prefix")
    f = clean_file()
    f["categories"] = ["Category:Paleoart of predation"]
    check(m.picture_problem(f, {}, pins, "Q14332").startswith("category:"), "category prefix")
    check(m.picture_problem(clean_file(), {"Q14332": "predation"}, pins, "Q14332").startswith("review:"), "review prefix")
    check(m.picture_problem(clean_file(), {}, {}, "Q14332").startswith("pin:"), "pin prefix")


def test_picture_problem_names_nobody():
    pins = {"Q1": "a" * 40}
    for artist in ("Own work", "own work", "Public Domain", "public domain", "PD", "1891", "----", "  ",
                   "I would appreciate being notified if you use my work outside Wikimedia. More of my work can be "
                   "found in my personal gallery."):
        f = clean_file()
        f["artist"] = artist
        problem = m.picture_problem(f, {}, pins, "Q1")
        check(problem and problem.startswith("author:"), "%r names nobody: %r" % (artist, problem))
    # The maker can come from the author field's own link to exactly one Commons user
    f = clean_file()
    f["artist"] = "I would appreciate being notified if you use my work outside Wikimedia."
    f["artist_html"] = ('I would appreciate <a href="//commons.wikimedia.org/wiki/Special:EmailUser/Aktron">being '
                        'notified</a> ... <a href="//commons.wikimedia.org/wiki/Category:Geocoded_images_of_User:Aktron">'
                        'gallery</a>')
    check(m.picture_problem(f, {}, pins, "Q1") is None, "boilerplate with a link to its user")
    check(m.author_of(f) == "Aktron", "the user is the maker: %r" % m.author_of(f))
    check(m.device_credit(f) == "Aktron, CC BY-SA 4.0", "and the credit names them: %r" % m.device_credit(f))
    f["artist"] = "Own work"
    f["artist_html"] = '<a href="//commons.wikimedia.org/wiki/User:Jo_Smith">Jo Smith</a>'
    check(m.author_of(f) == "Jo Smith", "own work by a linked user: %r" % m.author_of(f))
    f["artist_html"] = '<a href="/wiki/User:A">A</a> and <a href="/wiki/User:B">B</a>'
    check(m.picture_problem(f, {}, pins, "Q1"), "two users: not clearly the maker")
    # Only "Own work" and the boilerplate say the linked user made it; under anything else the link may
    # be the uploader's
    for text in ("Public Domain", "PD", "1891", "Unknown author", ""):
        f = clean_file()
        f["artist"], f["artist_html"] = text, text + ' <a href="//commons.wikimedia.org/wiki/User:Uploader">x</a>'
        problem = m.picture_problem(f, {}, pins, "Q1")
        check(problem and problem.startswith("author:"), "%r with one user link is refused: %r" % (text, problem))
    f["artist"], f["artist_html"] = "Public Domain", "Public Domain"
    f["credit_field"] = "George F. Sternberg and Myrl V. Walker"
    check(m.picture_problem(f, {}, pins, "Q1"), "Commons' source field is not taken as the maker")
    # Names that look odd are still names when the author field links them as a user
    f = clean_file()
    f["artist"] = "Definitely NOT Dilophosaurus"
    check(m.picture_problem(f, {}, pins, "Q1") is None, "a user name")


def test_pin_needs_a_sha1():
    f = clean_file()
    f["sha1"] = ""
    check(m.picture_problem(f, {}, {"Q1": ""}, "Q1"), "an empty SHA-1 is never pinned")
    f.pop("sha1")
    check(m.picture_problem(f, {}, {}, "Q1"), "no SHA-1 and no pin")
    check(m.picture_problem(f, {}, {"Q1": None}, "Q1"), "no SHA-1 and a None pin")


def p18(*names, preferred=()):
    return {"claims": {"P18": [{"mainsnak": {"snaktype": "value", "datavalue": {"value": n}},
                                "rank": "preferred" if n in preferred else "normal"} for n in names]}}


def test_choose_picture():
    infos = {
        "Hydrorion brachypterygius skull.JPG": dict(clean_file(), description="Skull of Hydrorion", categories=[]),
        "Anningasaura lymense.jpg": dict(clean_file(), description="", categories=[]),
        "IMG 4711.jpg": dict(clean_file(), description="Skull of <i>Anningasaura</i> in the museum", categories=[]),
        "Patagonykuspuertai.jpg": dict(clean_file(), description="", categories=["Category:Patagonykus"]),
        "Yixianornis.jpg": dict(clean_file(), description="", categories=[]),
        "Unnamed.jpg": dict(clean_file(), description="", categories=["Category:Anningasaura"]),
    }
    names = ["Anningasaura"]
    entity = p18("Hydrorion brachypterygius skull.JPG", "Anningasaura lymense.jpg")
    check(m.choose_picture(entity, infos, names) == ("Anningasaura lymense.jpg", True), "the one that names the genus")
    entity = p18("Hydrorion brachypterygius skull.JPG", "IMG 4711.jpg")
    check(m.choose_picture(entity, infos, names) == ("IMG 4711.jpg", True), "named in the description")
    entity = p18("Hydrorion brachypterygius skull.JPG", "Unnamed.jpg")
    check(m.choose_picture(entity, infos, names) == ("Unnamed.jpg", True), "named in a category")
    entity = p18("Hydrorion brachypterygius skull.JPG")
    check(m.choose_picture(entity, infos, names) == ("Hydrorion brachypterygius skull.JPG", False),
          "the first, flagged, when none names it")
    entity = p18("Anningasaura lymense.jpg", "Unnamed.jpg", preferred=("Unnamed.jpg",))
    check(m.choose_picture(entity, infos, names) == ("Unnamed.jpg", True), "preferred rank first, on equal evidence")
    # A file name or category is stronger evidence than a description: Commons has a Hydrorion skull whose
    # description says Anningasaura, while its categories say Hydrorion
    infos["Hydrorion brachypterygius skull.JPG"]["description"] = "Fossil of Anningasaura, an extinct reptile"
    entity = p18("Hydrorion brachypterygius skull.JPG", "Anningasaura lymense.jpg")
    check(m.choose_picture(entity, infos, names) == ("Anningasaura lymense.jpg", True), "file name beats description")
    entity = p18("Hydrorion brachypterygius skull.JPG", "Yixianornis.jpg")
    check(m.choose_picture(entity, infos, names) == ("Hydrorion brachypterygius skull.JPG", True),
          "a description alone still names it")
    infos["Hydrorion brachypterygius skull.JPG"]["description"] = "Skull of Hydrorion"
    entity = p18("Hydrorion brachypterygius skull.JPG", "Anningasaura lymense.jpg",
                 preferred=("Hydrorion brachypterygius skull.JPG",))
    check(m.choose_picture(entity, infos, names) == ("Anningasaura lymense.jpg", True),
          "a normal-rank file that names the genus beats a preferred one that doesn't")
    check(m.choose_picture(p18("Patagonykuspuertai.jpg"), infos, ["Lenesornis"]) == ("Patagonykuspuertai.jpg", False),
          "another genus' picture is flagged")
    check(not m.names_genus(["Patagonykuspuertai.jpg"], ["Patagonykus"]), "a name run into another word is no name")
    check(m.choose_picture(p18("Patagonykuspuertai.jpg"), infos, ["Patagonykus"])[1], "... but its category names it")
    check(not m.choose_picture(p18("Yixianornis.jpg"), infos, ["Yi"])[1], "a short name needs a whole word")
    check(m.choose_picture(p18("Anningasaura lymense.jpg"), infos, ["Lenesornis", "Anningasaura"])[1], "an alias")
    check(m.choose_picture({"claims": {}}, infos, names) == (None, False), "no picture")
    entity = p18("Hydrorion brachypterygius skull.JPG", "Missing.jpg")
    check(m.choose_picture(entity, infos, names) == ("Hydrorion brachypterygius skull.JPG", False),
          "a file Commons doesn't know is never chosen for naming")


class FakeClient:
    """Answers every Commons query with one canned answer, as the cache would."""

    def __init__(self, answer):
        self.answer = answer

    def api(self, base, params):
        return self.answer


def commons_answer(cont):
    page = {"title": "File:X.jpg", "imageinfo": [{"sha1": "s", "url": "u", "thumburl": "t", "mime": "image/jpeg",
                                                  "extmetadata": {"Artist": {"value": '<a href="/wiki/User:Ann">Ann</a>'},
                                                                  "ImageDescription": {"value": "<b>A</b> skull"}}}],
            "categories": [{"title": "Category:Skulls"}]}
    answer = {"query": {"pages": {"1": page}}}
    if cont:
        answer["continue"] = cont
    return answer


def test_commons_infos():
    info = m.commons_infos(FakeClient(commons_answer({"iistart": "2015-06-22T17:36:21Z", "continue": "||categories"})),
                           ["X.jpg"])
    check(info["X.jpg"]["artist_html"].startswith("<a") and info["X.jpg"]["description"] == "A skull",
          "older file versions left out are fine; the raw author field and description are kept: %r" % info)
    try:
        m.commons_infos(FakeClient(commons_answer({"clcontinue": "1|Skulls", "continue": "||"})), ["X.jpg"])
        check(False, "categories cut short must stop the run")
    except SystemExit:
        pass


def test_names_genus_whole_words():
    check(not m.names_genus(["Category:Troodontidae"], ["Troodon"]), "a family is not its genus")
    check(m.names_genus(["Category:Troodon formosus"], ["Troodon"]), "a species epithet may follow")
    check(m.names_genus(["Troodon_skull.jpg"], ["Troodon"]), "underscores separate words")
    check(not m.names_genus(["Category:Iguanodontia"], ["Iguanodon"]), "a clade is not its genus")
    check(not m.names_genus(["Category:Oviraptorosauria fossils"], ["Oviraptor"]), "Oviraptorosauria")
    check(m.names_genus(["MicroraptorGui-PaleozoologicalMuseumOfChina-May23-08.jpg"], ["Microraptor"]),
          "capitals split run-together words")
    check(m.names_genus(["Tyrannosaurus2.jpg"], ["Tyrannosaurus"]), "a digit ends a word")
    check(not m.names_genus(["Yixianornis.jpg"], ["Yi"]), "a short name is a whole word")
    check(m.names_genus(["Yi qi restoration.jpg"], ["Yi"]), "... and found as one")
    # A text that names another genus too says nothing about this one
    genera = {"dongbeititan", "yutyrannus", "troodon", "saurornithoides", "edmontosaurus", "anatosaurus"}
    check(not m.names_genus(["Dongbeititan and Yutyrannus.jpg"], ["Yutyrannus"], genera), "two genera in a file name")
    check(not m.names_genus(["Category:Troodon and Saurornithoides"], ["Troodon"], genera), "two genera in a category")
    check(m.names_genus(["Category:Troodon"], ["Troodon"], genera), "the genus itself is no other genus")
    check(m.names_genus(["Mounted Edmontosaurus (Anatosaurus) skeleton"], ["Edmontosaurus", "Anatosaurus"], genera),
          "a synonym among the genus' own names is no other genus")
    check(m.genus_words([{"name": "Troodon"}, {"name": "Yi"}, {"name": "Gen. X-1"}]) == {"troodon", "yi"},
          "genus words: single-word names only %r" % m.genus_words([{"name": "Troodon"}, {"name": "Yi"}]))


def test_choose_picture_family_categories():
    infos = {
        "Saurornithoides mount.jpg": dict(clean_file(), description="", categories=["Category:Troodontidae skeletal mounts"]),
        "Troodon skull.jpg": dict(clean_file(), description="", categories=[]),
        "Both.jpg": dict(clean_file(), description="", categories=["Category:Troodon and Saurornithoides"]),
    }
    genera = {"troodon", "saurornithoides"}
    entity = p18("Saurornithoides mount.jpg", "Troodon skull.jpg")
    check(m.choose_picture(entity, infos, ["Troodon"], genera) == ("Troodon skull.jpg", True),
          "a family category doesn't beat the genus' own file")
    check(m.choose_picture(p18("Saurornithoides mount.jpg"), infos, ["Troodon"], genera) ==
          ("Saurornithoides mount.jpg", False), "a family category alone is flagged")
    check(m.choose_picture(p18("Both.jpg"), infos, ["Troodon"], genera) == ("Both.jpg", False),
          "a category naming two genera is flagged")


def test_category_words():
    for word in ("carcass", "prey", "predation", "kill", "killing", "hunting", "blood", "wound", "fight", "attack",
                 "dead", "death", "scavenging", "corpse", "gore"):
        check(m.DINO_CATEGORY_DENY.search("Category:Some %s here" % word.capitalize()), "category word " + word)
    check(not m.DINO_CATEGORY_DENY.search("Category:Tyrannosaurus skeletons"), "a skeleton is fine")
    check(not m.DINO_CATEGORY_DENY.search("Category:Ardeadactylus fossils"), "a word starts at a word boundary")
    for category in ("Category:Predators with prey", "Category:Birds with prey", "Category:Dinosaur carcasses",
                     "Category:Fighting dinosaurs", "Category:Bloodstained", "Category:Paleoart of predation"):
        check(m.DINO_CATEGORY_DENY.search(category), category)


def test_device_credit():
    f = clean_file()
    check(m.device_credit(f) == "Nobu Tamura, CC BY-SA 4.0", "plain %r" % m.device_credit(f))
    f["artist"] = "Jörg Müller"
    check(m.device_credit(f) == "Jorg Muller, CC BY-SA 4.0", "umlauts folded %r" % m.device_credit(f))
    f["artist"] = "Raúl Martín – Łuków"
    check(m.device_credit(f) == "Raul Martin - Lukow, CC BY-SA 4.0", "accents and dash %r" % m.device_credit(f))
    f["artist"] = "Дмитрий Богданов"
    check(m.device_credit(f) == "CC BY-SA 4.0", "Cyrillic gives the licence alone %r" % m.device_credit(f))
    f["artist"] = "No machine-readable author provided. Ghedoghedo assumed (based on copyright claims)."
    check(m.device_credit(f) == "Ghedoghedo, CC BY-SA 4.0", "assumed author %r" % m.device_credit(f))
    for artist, maker in (("Nobu Tamura ( http://spinops.blogspot.com )", "Nobu Tamura"),
                          ("Nobu Tamura email:maker@example.org http://spinops.blogspot.com/", "Nobu Tamura"),
                          ("Jaime A. Headden ( User:Qilong )", "Jaime A. Headden"),
                          ("Original uploader was Ballista at en.wikipedia", "Ballista"),
                          ("The original uploader was Rattis irrittis at English Wikipedia .", "Rattis irrittis"),
                          ("w:en:User:Anky-man", "Anky-man"),
                          ("Creator: Dmitry Bogdanov", "Dmitry Bogdanov"),
                          ("Ferahgo the Assassin (Emily Willoughby, artist.name@example.org) http://emilywilloughby.com/",
                           "Ferahgo the Assassin (Emily Willoughby)")):
        f["artist"] = artist
        check(m.device_credit(f) == maker + ", CC BY-SA 4.0", "tidied %r -> %r" % (artist, m.device_credit(f)))
    f["artist"] = "x" * 70
    check(m.device_credit(f) == "CC BY-SA 4.0", "too long gives the licence alone")
    f["needs_credit"] = False
    check(m.device_credit(f) == "-", "no credit needed")
    f = clean_file()
    f["licence"] = "中"
    f["artist"] = "Д"
    check(m.device_credit(f) is None, "nothing writable in ASCII")
    for artist in ("Jörg", "中", "a’b", "x" * 200):
        f = clean_file()
        f["artist"] = artist
        credit = m.device_credit(f)
        check(credit is None or all(0x20 <= ord(c) <= 0x7E for c in credit), "ASCII for %r" % artist)
        if credit:
            s = good_sheet()
            s["image"] = {"length": 100, "width": 240, "height": 117, "hash": 1, "credit": credit}
            check(not raises(m.sheet_text, s), "sheet_text takes %r" % credit)


def test_fit():
    check(m.fit_for(500, 250) == "cover", "2:1 fills")
    check(m.fit_for(500, 281) == "cover", "16:9 fills")
    check(m.fit_for(500, 333) == "contain", "3:2 is contained")
    check(m.fit_for(300, 500) == "contain", "portrait")
    check(m.fit_for(1000, 200) == "contain", "a long strip")


def test_sheet_image_line():
    s = good_sheet()
    s["image"] = {"length": 5123, "width": 240, "height": 117, "hash": 0xDEADBEEF, "credit": "Nobu Tamura, CC BY 3.0"}
    check("image\t5123\t240\t117\tdeadbeef\tNobu Tamura, CC BY 3.0\n" in m.sheet_text(s), "image line")
    s["image"]["credit"] = "-"
    check("image\t5123\t240\t117\tdeadbeef\t-\n" in m.sheet_text(s), "no credit")
    s["image"]["width"] = 241
    check(raises(m.sheet_text, s), "wider than the screen")
    s["image"]["width"], s["image"]["height"] = 240, 118
    check(raises(m.sheet_text, s), "into the footer")
    s["image"]["height"], s["image"]["credit"] = 117, "Jörg, CC BY 3.0"
    check(raises(m.sheet_text, s), "a credit that isn't ASCII")



def test_parse_quantity():
    check(m.parse_quantity("20\u201330 km/h") == ("speed", 200, 300), "20-30 km/h with an en dash")
    check(m.parse_quantity("bis zu 13 Metern")[1:] == (1300, 1300), "bis zu 13 Metern")
    check(m.parse_quantity("15.3\u201317.9 km/h") == ("speed", 153, 179), "15.3-17.9 km/h")
    check(m.parse_quantity("8,8 Tonnen") == ("mass", 8800000, 8800000), "8,8 Tonnen")
    check(m.parse_quantity("6 bis 7 km/h") == ("speed", 60, 70), "6 bis 7 km/h")
    check(m.parse_quantity("6-7 km/h") == ("speed", 60, 70), "ASCII hyphen")
    check(m.parse_quantity("12 to 13 metres") == ("length", 1200, 1300), "12 to 13 metres")
    check(m.parse_quantity("about 9 m") == ("length", 900, 900), "about 9 m")
    check(m.parse_quantity("30 cm") == ("length", 30, 30), "30 cm")
    check(m.parse_quantity("30 Zentimeter") == ("length", 30, 30), "Zentimeter")
    check(m.parse_quantity("7 t") == ("mass", 7000000, 7000000), "7 t")
    check(m.parse_quantity("2.5 tonnes") == ("mass", 2500000, 2500000), "2.5 tonnes")
    check(m.parse_quantity("1,400 kg", "en") == ("mass", 1400000, 1400000), "1,400 kg reads as thousands in English")
    check(m.parse_quantity("1.400 kg", "de") == ("mass", 1400000, 1400000), "1.400 kg reads as thousands in German")
    check(m.parse_quantity("15 Kilogramm") == ("mass", 15000, 15000), "Kilogramm")
    check(m.parse_quantity("4 meters") == ("length", 400, 400), "meters")
    check(m.parse_quantity("25 mph") == ("speed", 402, 402), "25 mph is 40.2 km/h")
    check(m.parse_quantity("5 m/s") == ("speed", 180, 180), "5 m/s is 18 km/h")
    check(m.parse_quantity("1 ft") == ("length", 30, 30), "1 ft")
    check(m.parse_quantity("8,8") is None, "no unit")
    check(m.parse_quantity("km/h") is None, "no number")
    check(m.parse_quantity("12 m und 8 t") is None, "two quantities")
    check(m.parse_quantity("30\u201320 km/h") is None, "the wrong way round")
    check(m.parse_quantity("5 tons") is None, "tons may be short tons")
    check(m.parse_quantity("12 Monate") is None, "not a unit, only its first letter")
    check(m.parse_quantity("3 times") is None, "t at the start of a word")


REVISION = ("Tyrannosaurus war ein Raubsaurier. Die maximale Geschwindigkeit ausgewachsener Tiere betrug etwa "
            "20\u201330 km/h. Der Name bedeutet \u00abTyrannenechse\u00bb. Henry Fairfield Osborn beschrieb ihn 1905.")


def fact(**changes):
    result = {"field": "speed", "quote": "20\u201330 km/h", "lang": "de", "title": "Tyrannosaurus", "revid": 1,
              "sentence": "Die maximale Geschwindigkeit ausgewachsener Tiere betrug etwa 20\u201330 km/h."}
    result.update(changes)
    return result


def test_check_fact():
    check(m.check_fact(fact(), REVISION) is None, "a good speed")
    check(m.check_fact(fact(quote="20\u201340 km/h"), REVISION) is not None, "a quote that isn't in its sentence")
    check(m.check_fact(fact(sentence="Er lief 20\u201330 km/h."), REVISION) is not None,
          "a sentence that isn't in the revision")
    check(m.check_fact(fact(sentence="Die maximale  Geschwindigkeit ausgewachsener Tiere betrug etwa 20\u201330 km/h."),
                       REVISION) is None, "spaces count once")
    check(m.check_fact(fact(field="mass"), REVISION) is not None, "a speed is no mass")
    check(m.check_fact(fact(field="length"), REVISION) is not None, "a speed is no length")
    check(m.check_fact(fact(quote="20\u201330"), REVISION) is not None, "a number without its unit")
    check(m.check_fact(fact(field="colour"), REVISION) is not None, "an unknown field")
    named = fact(field="named", quote="1905", sentence="Henry Fairfield Osborn beschrieb ihn 1905.")
    check(m.check_fact(named, REVISION) is None, "named 1905")
    check(m.check_fact(dict(named, quote="05"), REVISION) is not None, "a year has four digits")
    check(m.check_fact(dict(named, quote="1906"), REVISION) is not None, "named: a year that isn't in the sentence")
    meaning = {"field": "meaning", "de": "Tyrannenechse", "en": "tyrant lizard", "lang": "de", "title": "Tyrannosaurus",
               "revid": 1, "sentence": "Der Name bedeutet \u00abTyrannenechse\u00bb."}
    check(m.check_fact(meaning, REVISION) is None, "a meaning in the source language")
    check(m.check_fact(dict(meaning, de="K\u00f6nigsechse"), REVISION) is not None, "a meaning that isn't in the sentence")
    check(m.check_fact(dict(meaning, de="Tyrannenechse", quote="Tyrannenechse"), REVISION) is None, "an explicit quote")
    check(m.check_fact(dict(meaning, en=""), REVISION) is not None, "curated text needs both languages")
    check(m.check_fact(dict(meaning, en="tyrant lizard 2"), REVISION) is not None, "a number in a text, not in the sentence")


def test_check_fact_era():
    s = "Argentinosaurus is a genus of giant sauropod dinosaur that lived during the Early Cretaceous period."
    era = {"field": "era", "de": "Kreide", "en": "Cretaceous", "quote": "Early Cretaceous", "lang": "en",
           "title": "Argentinosaurus", "revid": 1, "sentence": s}
    check(m.check_fact(era, s) is None, "an era its sentence names")
    check(m.check_fact(dict(era, de="Jura", en="Jurassic"), s) is not None, "a period the quote doesn't name")
    check(m.check_fact(dict(era, de="Oberkreide", en="Late Cretaceous"), s) is not None,
          "only the generator's period names, as era_text writes them")
    check(m.check_fact(dict(era, de="Kreide", en="Jurassic"), s) is not None, "de and en name one period")
    check(m.check_fact(dict(era, quote="Late Cretaceous"), s) is not None, "a quote that isn't in the sentence")
    check(m.check_fact(dict(era, quote="genus"), s) is not None, "a quote that names no period")
    de = "Der einzige Vertreter der Gattung ist Yutyrannus huali aus der Unterkreide von China."
    check(m.check_fact(dict(era, quote="Unterkreide", lang="de", sentence=de), de) is None, "Unterkreide is Kreide")
    two = "It lived from the Late Jurassic to the Early Cretaceous."
    both = dict(era, de="Jura bis Kreide", en="Jurassic to Cretaceous", sentence=two,
                quote="Late Jurassic to the Early Cretaceous")
    check(m.check_fact(both, two) is None, "two periods, both named")
    check(m.check_fact(dict(both, de="Kreide bis Jura", en="Cretaceous to Jurassic"), two) is not None,
          "two periods, oldest first")
    check(m.era_periods("Jura bis Kreide", "Jurassic to Cretaceous") == [("Jura", "Jurassic"), ("Kreide", "Cretaceous")],
          "the periods of an era")


FUN_REVISION = "Er lief etwa 30 km/h schnell. Sein Ei war 8.8 cm lang. It had two horns."


def fun(**changes):
    result = {"de": "Er lief etwa 30 km/h schnell.", "en": "It ran at about 30 km/h.", "lang": "de",
              "title": "Tyrannosaurus", "revid": 1, "sentences": ["Er lief etwa 30 km/h schnell."]}
    result.update(changes)
    return result


def test_check_fun():
    check(m.check_fun(fun(), FUN_REVISION) is None, "a good fun fact")
    check(m.check_fun(fun(de="Er lief etwa 40 km/h schnell."), FUN_REVISION) is not None, "40 when the sentence says 30")
    check(m.check_fun(fun(en="It ran at about 40 km/h."), FUN_REVISION) is not None, "... in English too")
    check(m.check_fun(fun(sentences=["Er lief 30 km/h."]), FUN_REVISION) is not None, "a sentence not in the revision")
    check(m.check_fun(fun(sentences=[]), FUN_REVISION) is not None, "no sentence")
    three = ["Er lief etwa 30 km/h schnell.", "Sein Ei war 8.8 cm lang.", "It had two horns."]
    check(m.check_fun(fun(sentences=three), FUN_REVISION) is not None, "at most two sentences")
    check(m.check_fun(fun(en=""), FUN_REVISION) is not None, "both languages")
    egg = fun(de="Sein Ei war 8,8 cm lang.", en="Its egg was 8.8 cm long.", sentences=["Sein Ei war 8.8 cm lang."])
    check(m.check_fun(egg, FUN_REVISION) is None, "8,8 and 8.8 are one number")
    check(m.check_fun(dict(egg, de="Sein Ei war 88 cm lang."), FUN_REVISION) is not None, "88 is not 8.8")
    horns = fun(de="Er hatte zwei H\u00f6rner.", en="It had two horns.", lang="en", sentences=["It had two horns."])
    check(m.check_fun(horns, FUN_REVISION) is None, "number words that agree")
    check(m.check_fun(dict(horns, de="Er hatte drei H\u00f6rner."), FUN_REVISION) is not None, "drei when it says two")
    check(m.check_fun(dict(horns, en="It had 3 horns."), FUN_REVISION) is not None, "3 when it says two")


def test_merge_range():
    check(m.merge_range((1200, 1200), (1200, 1300)) == (1200, 1300), "widened")
    check(m.merge_range(None, (5, 6)) == (5, 6), "nothing from Wikidata")
    check(m.merge_range((5, 6), None) == (5, 6), "nothing to add")
    check(m.merge_range((900, 1000), (950, 950)) == (900, 1000), "inside")
    check(m.merge_range((68000000, 66000000), (70000000, 67000000), older=True) == (70000000, 66000000), "years ago")


def curated_entry():
    sentence = "Die maximale Geschwindigkeit ausgewachsener Tiere betrug etwa 20\u201330 km/h."
    return {"wiki": "Tyrannosaurus", "image": "T.jpg", "tag": "fossil", "fit": "cover", "facts": [
        {"field": "speed", "quote": "20\u201330 km/h", "lang": "de", "title": "Tyrannosaurus", "revid": 1,
         "sentence": sentence},
        {"field": "length", "quote": "12\u201313 m", "lang": "de", "title": "Tyrannosaurus", "revid": 1,
         "sentence": "Er war 12\u201313 m lang."},
        {"field": "length", "quote": "12.3 m", "lang": "en", "title": "Tyrannosaurus", "revid": 2,
         "sentence": "It was 12.3 m long."},
        {"field": "egg", "quote": "1 ft", "lang": "en", "title": "Tyrannosaurus", "revid": 2, "sentence": "Eggs: 1 ft."},
        {"field": "named", "quote": "1905", "lang": "de", "title": "Tyrannosaurus", "revid": 1, "sentence": "1905."},
        {"field": "meaning", "de": "Tyrannenechse", "en": "tyrant lizard", "lang": "en", "title": "Tyrannosaurus",
         "revid": 2, "sentence": "Its name means tyrant lizard."},
        {"field": "diet", "de": "Fleisch", "en": "meat", "quote": "carnivore", "lang": "en", "title": "Tyrannosaurus",
         "revid": 2, "sentence": "It was a carnivore."}],
        "fun": {"de": "Er biss sehr fest.", "en": "It bit very hard.", "lang": "en", "title": "Tyrannosaurus",
                "revid": 2, "sentences": ["It bit very hard."]}}


def test_apply_curated():
    s = good_sheet()
    m.apply_curated(s, curated_entry(), PERIODS)
    check(s["speed"] == (200, 300), "speed %r" % (s["speed"],))
    check(s["length"] == (1200, 1300), "Wikidata 12 m, de 12-13 m, en 12.3 m: %r" % (s["length"],))
    check(s["egg"] == (305, 305), "1 ft egg in mm: %r" % (s["egg"],))
    check(s["meaning"] == {"de": "Tyrannenechse", "en": "tyrant lizard"}, "meaning")
    check(s["diet"] == {"de": "Fleisch", "en": "meat"}, "diet")
    check(s["named"] == 1905, "named agrees with Wikidata")
    check(s["fun"] == {"de": "Er biss sehr fest.", "en": "It bit very hard.",
                       "source": "Wikipedia (EN): Tyrannosaurus"}, "fun %r" % s["fun"])
    check(s["source"] == "Wikidata, Wikipedia", "source %r" % s["source"])
    text = m.sheet_text(s)
    check("speed\t200\t300\n" in text and "egg\t305\t305\n" in text, "written")
    check("fun\tEr biss sehr fest.\tIt bit very hard.\tWikipedia (EN): Tyrannosaurus\n" in text, "fun line")
    # A year that disagrees with Wikidata can't be a range: the curated data must be fixed
    entry = curated_entry()
    entry["facts"][4]["quote"] = "1906"
    check(raises(m.apply_curated, good_sheet(), entry, PERIODS), "a named year that disagrees with Wikidata")
    # Where Wikidata has none, the year and the time range come from the sentence
    s = good_sheet()
    s["named"], s["lived"], s["era"] = 0, None, None
    entry = curated_entry()
    entry["facts"].append({"field": "lived", "quote": "155\u2013145 million years ago", "lang": "en",
                           "title": "Tyrannosaurus", "revid": 2, "sentence": "It lived 155\u2013145 million years ago."})
    m.apply_curated(s, entry, PERIODS)
    check(s["named"] == 1905, "named from Wikipedia")
    check(s["lived"] == (155000000, 145000000), "lived %r" % (s["lived"],))
    check(s["era"] == {"de": "Jura", "en": "Jurassic"}, "era from the time range %r" % s["era"])
    # Two facts for one text field can't be merged
    entry = curated_entry()
    entry["facts"].append(dict(entry["facts"][5], en="tyrant"))
    check(raises(m.apply_curated, good_sheet(), entry, PERIODS), "two meanings")
    # Only Wikipedia: the footer says so
    s = good_sheet()
    for key in ("length", "mass", "lived", "era"):
        s[key] = None
    s["named"] = 0
    m.apply_curated(s, curated_entry(), PERIODS)
    check(s["source"] == "Wikipedia", "source %r" % s["source"])
    # An era from a sentence fills a gap, and only a gap
    era = {"field": "era", "de": "Kreide", "en": "Cretaceous", "quote": "Late Cretaceous", "lang": "en",
           "title": "Tyrannosaurus", "revid": 2, "sentence": "It lived in the Late Cretaceous."}
    s = good_sheet()
    s["lived"], s["era"] = None, None
    entry = curated_entry()
    entry["facts"].append(era)
    m.apply_curated(s, entry, PERIODS)
    check(s["era"] == {"de": "Kreide", "en": "Cretaceous"} and s["lived"] is None, "era from a sentence %r" % s["era"])
    check("era\tKreide\tCretaceous\n" in m.sheet_text(s), "written")
    check(raises(m.apply_curated, good_sheet(), entry, PERIODS), "refused where Wikidata's time range gives the era")
    s = good_sheet()
    s["lived"], s["era"] = None, None
    entry["facts"].append({"field": "lived", "quote": "155–145 million years ago", "lang": "en",
                           "title": "Tyrannosaurus", "revid": 2, "sentence": "It lived 155–145 million years ago."})
    check(raises(m.apply_curated, s, entry, PERIODS), "refused where a time range from Wikipedia gives it")
    s = good_sheet()
    s["lived"], s["era"] = None, None
    entry = curated_entry()
    entry["facts"] += [era, dict(era, de="Jura", en="Jurassic")]
    check(raises(m.apply_curated, s, entry, PERIODS), "two eras")
    # A height from a sentence (a hip height is a height) widens Wikidata's like any number
    s = good_sheet()
    entry = curated_entry()
    entry["facts"].append({"field": "height", "quote": "Hüfthöhe von 4 Metern", "lang": "de",
                           "title": "Tyrannosaurus", "revid": 1, "sentence": "Sue hat eine Hüfthöhe von 4 Metern."})
    m.apply_curated(s, entry, PERIODS)
    check(s["height"] == (400, 400), "height %r" % (s["height"],))


def test_parse_ago():
    check(m.parse_ago("155\u2013145 million years ago") == (155000000, 145000000), "en range")
    check(m.parse_ago("vor 155 bis 145 Millionen Jahren") == (155000000, 145000000), "de range")
    check(m.parse_ago("vor etwa 68 bis 66 Mio. Jahren") == (68000000, 66000000), "Mio.")
    check(m.parse_ago("150.8\u2013148.5 million years ago") == (150800000, 148500000), "decimals")
    check(m.parse_ago("about 76.5 million years ago") == (76500000, 76500000), "one value")
    check(m.parse_ago("between 145 and 155 million years ago") == (155000000, 145000000), "either order")
    check(m.parse_ago("155 and 66 million years ago, or 12 million") is None, "two times")
    check(m.parse_ago("155 years ago") is None, "no million")
    check(m.parse_ago("12 m") is None, "no time")



def test_parse_quantity_spelled_units():
    check(m.parse_quantity("38,9 Kilometern pro Stunde") == ("speed", 389, 389), "Kilometern pro Stunde")
    check(m.parse_quantity("14 Meter pro Sekunde") == ("speed", 504, 504), "Meter pro Sekunde is a speed")
    check(m.parse_quantity("4.5 MT") == ("mass", 4500000, 4500000), "metric tons")
    speed_as_length = fact(field="length", quote="14 Meter pro Sekunde", sentence="Es lief 14 Meter pro Sekunde.")
    check(m.check_fact(speed_as_length, "Es lief 14 Meter pro Sekunde.") is not None, "a speed is no length")


def test_curated_text_for_the_device():
    meaning = {"field": "meaning", "de": "Tyrannenechse", "en": "tyrant lizard", "lang": "en", "title": "T",
               "revid": 1, "sentence": "Its name means tyrant lizard."}
    revision = "Its name means tyrant lizard."
    check(m.check_fact(meaning, revision) is None, "a good meaning")
    check(m.check_fact(dict(meaning, de="gro\u00dfe Echse"), revision) is not None, "Swiss spelling: no sharp s")
    check(m.check_fact(dict(meaning, de="Tyrannen\u2013Echse"), revision) is not None, "no en dash on the device")
    check(m.check_fact(dict(meaning, quote="tyrant lizard", en="tyrant\u2019s lizard"), revision) is not None,
          "no typographic apostrophe")
    check(m.check_fact(dict(meaning, de="x" * 161), revision) is not None, "at most 160 bytes")
    good = fun()
    check(m.check_fun(dict(good, de="Er lief etwa 30 km/h, das ist gro\u00df."), FUN_REVISION) is not None,
          "fun: no sharp s")
    check(m.check_fun(dict(good, en="It ran \u201cfast\u201d at about 30 km/h."), FUN_REVISION) is not None,
          "fun: no typographic quotes")


def test_rejects_for_curated():
    rejects = {"Q1": "predation: a kill pose", "Q2": "not legible"}
    kept = m.rejects_for(rejects, {"Q1"})
    check(kept == {"Q2": "not legible"}, "a curated genus' hand-chosen picture is not refused by the reject of another")
    check(rejects == {"Q1": "predation: a kill pose", "Q2": "not legible"}, "the rejects themselves stay")
    info = clean_file()
    info["licence"], info["code"] = "CC BY-NC 2.0", "cc-by-nc-2.0"
    check(m.picture_problem(info, kept, {}, "Q1", check_pin=False).startswith("licence"),
          "the licence is still checked for a curated picture")
    info = clean_file()
    info["categories"] = ["Category:Dinosaur predation"]
    check(m.picture_problem(info, kept, {}, "Q1", check_pin=False).startswith("category"),
          "the categories are still checked for a curated picture")
    check(m.picture_problem(clean_file(), kept, {"Q1": "other"}, "Q1").startswith("pin"),
          "and the pin")



class WikiClient:
    """Wikidata knows one genus; Wikipedia has one revision of its article, or of another page."""

    def __init__(self, wikitext, page="Tyrannosaurus"):
        self.wikitext, self.page = wikitext, page

    def api(self, base, params):
        if params.get("action") == "wbgetentities":
            titles = params["titles"].split("|")
            return {"entities": {"Q14332": {"id": "Q14332", "sitelinks": {"enwiki": {"title": "Tyrannosaurus"}}}}
                    if "Tyrannosaurus" in titles else {"-1": {"missing": ""}}}
        return {"query": {"pages": [{"title": self.page, "revisions": [{"slots": {"main": {
            "content": self.wikitext}}}]}]}}


def refuses(function, *args):
    try:
        function(*args)
    except SystemExit:
        return True
    return False


def test_curated_entries():
    wikitext = ("It was a carnivore.<ref>A book</ref> Its name means [[tyrant]] lizard. It ran 20\u201330 km/h. "
                "It bit very hard.")
    entry = {"wiki": "Tyrannosaurus", "image": "T.jpg", "tag": "artwork", "fit": "cover", "facts": [
        {"field": "speed", "quote": "20\u201330 km/h", "lang": "en", "title": "Tyrannosaurus", "revid": 7,
         "sentence": "It ran 20\u201330 km/h."},
        {"field": "meaning", "de": "Tyrannenechse", "en": "tyrant lizard", "lang": "en", "title": "Tyrannosaurus",
         "revid": 7, "sentence": "Its name means tyrant lizard."}],
        "fun": {"de": "Er biss sehr fest.", "en": "It bit very hard.", "lang": "en", "title": "Tyrannosaurus",
                "revid": 7, "sentences": ["It bit very hard."]}}
    built = {"Q14332": good_sheet()}
    found = m.curated_entries(WikiClient(wikitext), [entry], built)
    check(list(found) == ["Q14332"] and found["Q14332"] is entry, "a good entry, checked against the wiki text")
    bad = dict(entry, facts=[dict(entry["facts"][0], sentence="It ran 30 km/h.")])
    check(refuses(m.curated_entries, WikiClient(wikitext), [bad], built), "a sentence that isn't in the revision")
    bad = dict(entry, fun=dict(entry["fun"], de="Er biss 40-mal fest."))
    check(refuses(m.curated_entries, WikiClient(wikitext), [bad], built), "a fun fact with a number of its own")
    check(refuses(m.curated_entries, WikiClient(wikitext, "Other page"), [entry], built), "a revision of another page")
    check(refuses(m.curated_entries, WikiClient(wikitext), [dict(entry, wiki="Nonexistus")], built),
          "a title Wikidata doesn't know")
    check(refuses(m.curated_entries, WikiClient(wikitext), [entry], {}), "a genus without a sheet")
    other = dict(good_sheet(), qid="Q99")
    with contextlib.redirect_stdout(io.StringIO()):  # It says which genus it took
        found = m.curated_entries(WikiClient(wikitext), [entry], {"Q99": other})
    check(list(found) == ["Q99"], "an article linked to the species item reaches the genus sheet of that name")
    check(refuses(m.curated_entries, WikiClient(wikitext), [entry], {"Q99": other, "Q98": dict(other, qid="Q98")}),
          "but not when two sheets have the name")
    check(refuses(m.curated_entries, WikiClient(wikitext), [entry], {"Q99": dict(other, name="Tarbosaurus")}),
          "nor a sheet of another name")
    check(refuses(m.curated_entries, WikiClient(wikitext), [dict(entry, fit="stretch")], built), "a picture needs a fit")
    check(refuses(m.curated_entries, WikiClient(wikitext), [entry, entry], built), "the same genus twice")



def test_credits_name_wikipedia():
    page = m.credits_page([])
    check("Wikipedia" not in page, "no Wikipedia text, no Wikipedia credit")
    page = m.credits_page([], [(good_sheet(), "fun fact", "en", "Tyrannosaurus", 1381719568)])
    check("also from Wikipedia" in page and "CC BY-SA 4.0" in page, "the licence of the Wikipedia text")
    check('href="https://en.wikipedia.org/w/index.php?oldid=1381719568"' in page, "the revision is linked")
    check("no person reviewed them" in page, "says plainly that nobody reviewed the fun facts")
    check("All of these curated facts (the fun facts, speeds, heights, eras, name meanings, diets and places) were "
          "chosen, translated and shortened" in page, "... for every kind of curated fact")
    check("looked at by the AI that built the data, not by a person" in page, "... and the automatic pictures")
    two = [(good_sheet(), "height", "en", "Tyrannosaurus", 1381719568)] * 2
    check(m.credits_page([], two).count("oldid=1381719568") == 1, "two heights from one revision: one line")


def test_credits_without_email_addresses():
    # Commons' author text, as it came for real, must not put a maker's e-mail address on a public page
    makers = ["Nobu Tamura email:maker@example.org http://spinops.blogspot.com/",
              "Nobu Tamura Email:maker@example.org www.palaeocritti.com", "Nobu Tamura (maker@example.org)",
              "Nobu Tamura (Contact: maker@example.org)", "Emily Willoughby (artist.name@example.org, emilywilloughby.com)",
              "Ferahgo the Assassin (Emily Willoughby, artist.name@example.org) http://emilywilloughby.com/",
              "Bogdanov painter@example.net", "Pavel Riha = user Pavel.Riha.CB ( e-mail )"]
    rows = [(dict(good_sheet(), name="T%d" % i), "T.jpg", dict(clean_file(), artist=maker))
            for i, maker in enumerate(makers)]
    page = m.credits_page(rows)
    check("@" not in page and "mail" not in page.lower() and "Contact" not in page, "no address or its label left")
    check("()" not in page and "(, " not in page and ", )" not in page, "no empty brackets or stray commas")
    for kept in ("<td>Nobu Tamura http://spinops.blogspot.com/</td>", "<td>Nobu Tamura</td>", "<td>Bogdanov</td>",
                 "<td>Emily Willoughby (emilywilloughby.com)</td>", "<td>Pavel Riha = user Pavel.Riha.CB</td>",
                 "<td>Ferahgo the Assassin (Emily Willoughby) http://emilywilloughby.com/</td>"):
        check(kept in page, "the maker stays: %s" % kept)
    page = m.credits_page([(good_sheet(), "T.jpg", dict(clean_file(), artist="Matt Martyniuk ( User:Qilong )"))])
    check("<td>Matt Martyniuk ( User:Qilong )</td>" in page, "a text with no address stays as Commons gives it")
    generated = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "webpage2", "data", "dino",
                             "credits.html")
    if os.path.exists(generated):
        with open(generated, encoding="utf-8") as f:
            check("@" not in f.read(), "no @ in the generated credits.html")



def test_sentence_is_one_sentence():
    long_sentence = "It was 13 m long" + ", and so on" * 60 + "."
    check(m.check_fact(fact(field="length", quote="13 m", sentence=long_sentence), long_sentence) is not None,
          "a 'sentence' of more than 600 characters is more than one sentence")
    heading = "== Size == It was 13 m long."
    check(m.check_fact(fact(field="length", quote="13 m", sentence=heading), heading) is not None, "a heading")
    caption = "thumb|Skeleton It was 13 m long."
    check(m.check_fact(fact(field="length", quote="13 m", sentence=caption), caption) is not None, "a picture caption")
    caption = "mini|Skelett Er war 13 m lang."
    check(m.check_fact(fact(field="length", quote="13 m", sentence=caption), caption) is not None, "a German caption")
    good = fun()
    check(m.check_fun(dict(good, sentences=["== Leben == Er lief etwa 30 km/h schnell."]),
                      "== Leben == Er lief etwa 30 km/h schnell.") is not None, "fun: a heading")
    check(m.check_fun(dict(good, de="Er war lang.", en="It was long.", sentences=[long_sentence]), long_sentence)
          is not None, "fun: too long")


def test_quote_is_a_whole_number():
    s = "It was 13 m long."
    check(m.check_fact(fact(field="length", quote="3 m", sentence=s), s) is not None, "3 m is not the end of 13 m")
    check(m.check_fact(fact(field="length", quote="13 m", sentence=s), s) is None, "13 m is")
    s = "Es wog 18 Tonnen."
    check(m.check_fact(fact(field="mass", quote="8 Tonnen", sentence=s), s) is not None, "8 Tonnen is not 18 Tonnen")
    s = "Es wog 2,8 Tonnen."
    check(m.check_fact(fact(field="mass", quote="8 Tonnen", sentence=s), s) is not None, "nor the end of 2,8 Tonnen")
    s = "Its egg was 13 mm long."
    check(m.check_fact(fact(field="egg", quote="13 m", sentence=s), s) is not None, "13 m is not the start of 13 mm")
    s = "It was found in Nigeria."
    found = {"field": "found", "de": "Niger", "en": "Niger", "lang": "en", "title": "T", "revid": 1, "sentence": s}
    check(m.check_fact(found, s) is not None, "Niger is not the start of Nigeria")
    check(m.check_fact(dict(found, de="Nigeria", en="Nigeria"), s) is None, "Nigeria is")


def test_numbers_read_one_way():
    check(m.parse_quantity("0,750 Meter") == ("length", 75, 75), "0,750 m is 75 cm, never 750 m")
    check(m.parse_quantity("0.750 m") == ("length", 75, 75), "0.750 m too")
    check(m.parse_quantity("1.125 m") is None, "1.125 m is 1.125 m in English and 1125 m in German: refused")
    check(m.parse_quantity("1,400 kg") is None, "1,400 kg without a language: refused")
    check(m.parse_quantity("1.125 m", "en") == ("length", 113, 113), "en: 1.125 m")
    check(m.parse_quantity("1.125 m", "de") == ("length", 112500, 112500), "de: 1.125 m is 1125 m")
    check(m.parse_quantity("1,400 kg", "en") == ("mass", 1400000, 1400000), "en: 1,400 kg")
    check(m.parse_quantity("1.400 kg", "de") == ("mass", 1400000, 1400000), "de: 1.400 kg")
    check(m.parse_quantity("1,4 kg", "en") == ("mass", 1400, 1400), "a comma before one digit is a decimal mark")
    check(m.parse_quantity("1.400.000 kg") == ("mass", 1400000000, 1400000000), "two groups of three are thousands")
    check(m.parse_ago("1,125 Millionen Jahren") == (1125000, 1125000), "before a scale word, a decimal mark")
    s = "Es wurden etwa 1.200 Exemplare gefunden."
    count = fun(de="Man fand etwa 1200 Exemplare.", en="About 1,200 were found.", sentences=[s])
    check(m.check_fun(count, s) is None, "de 1.200 and en 1,200 are both 1200")
    check(m.check_fun(dict(count, en="About 1.200 were found."), s) is not None, "en 1.200 is 1.2")
    e = "It weighed 0.750 t."
    check(m.check_fact(fact(field="mass", quote="0.750 t", lang="en", sentence=e), e) is None, "0.750 t")
    e = "It weighed 1.125 t."
    check(m.check_fact(fact(field="mass", quote="1.125 t", lang="en", sentence=e), e) is None,
          "the sentence's language decides")


for name, function in sorted(globals().items()):
    if name.startswith("test_") and callable(function):
        function()
print("%d failures" % failures)
sys.exit(1 if failures else 0)
