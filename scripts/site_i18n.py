#!/usr/bin/env python3
"""Languages for the zitro.spot website (webpage2/).

The English text stays in the HTML pages. Every translatable text block gets a key made from its English
content (data-i18n="1a2b3c4d"), attributes such as alt or placeholder get data-i18n-a="alt:5e6f7a8b".
webpage2/i18n/en.json lists every key with its English text; webpage2/i18n/<lang>.json holds the translations.
site.js downloads only the file of the chosen language. A changed English sentence gets a new key, so its
old translation is never shown: the sentence stays English until it is translated again.

  python3 scripts/site_i18n.py extract   # set the keys in the pages, write i18n/en.json
  python3 scripts/site_i18n.py check     # every translation: same markup, no unknown keys, coverage
  python3 scripts/site_i18n.py todo de   # the English entries that de.json lacks (to translate)
"""
import html as htmllib
import json
import re
import sys
from html.parser import HTMLParser
from pathlib import Path

SITE = Path(__file__).resolve().parent.parent / "webpage2"
I18N = SITE / "i18n"
PAGES = ["index.html", "learn.html", "detox.html", "create.html", "compare.html", "shop.html"]
SCRIPTS = ["site.js", "script.js", "shop.js"]

# Code, name in the language itself, writing direction. Europe's languages plus the 7 most spoken worldwide.
LANGUAGES = [
    ("en", "English", "ltr"), ("de", "Deutsch", "ltr"), ("fr", "Français", "ltr"), ("it", "Italiano", "ltr"),
    ("rm", "Rumantsch", "ltr"), ("es", "Español", "ltr"), ("pt", "Português", "ltr"), ("nl", "Nederlands", "ltr"),
    ("da", "Dansk", "ltr"), ("sv", "Svenska", "ltr"), ("nb", "Norsk", "ltr"), ("fi", "Suomi", "ltr"),
    ("is", "Íslenska", "ltr"), ("et", "Eesti", "ltr"), ("lv", "Latviešu", "ltr"), ("lt", "Lietuvių", "ltr"),
    ("pl", "Polski", "ltr"), ("cs", "Čeština", "ltr"), ("sk", "Slovenčina", "ltr"), ("hu", "Magyar", "ltr"),
    ("ro", "Română", "ltr"), ("bg", "Български", "ltr"), ("el", "Ελληνικά", "ltr"), ("hr", "Hrvatski", "ltr"),
    ("sl", "Slovenščina", "ltr"), ("bs", "Bosanski", "ltr"), ("sr", "Srpski", "ltr"), ("mk", "Македонски", "ltr"),
    ("sq", "Shqip", "ltr"), ("tr", "Türkçe", "ltr"), ("uk", "Українська", "ltr"), ("be", "Беларуская", "ltr"),
    ("ru", "Русский", "ltr"), ("ga", "Gaeilge", "ltr"), ("mt", "Malti", "ltr"), ("lb", "Lëtzebuergesch", "ltr"),
    ("zh", "中文", "ltr"), ("hi", "हिन्दी", "ltr"), ("ar", "العربية", "rtl"), ("bn", "বাংলা", "ltr"),
]

VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}
# Elements that may be translated as a whole, if nothing block-like is inside
CANDIDATES = {"title", "h1", "h2", "h3", "h4", "h5", "h6", "p", "li", "td", "th", "dt", "dd", "button", "a", "label",
              "span", "div", "figcaption", "summary", "option", "small", "strong", "em", "blockquote", "caption",
              "legend", "b", "i", "sup"}
BLOCKS = {"div", "p", "ul", "ol", "li", "section", "article", "aside", "table", "thead", "tbody", "tr", "td", "th",
          "h1", "h2", "h3", "h4", "h5", "h6", "form", "header", "footer", "nav", "figure", "blockquote", "button",
          "select", "details", "dl", "main", "label", "input", "textarea", "svg"}
SKIP_CONTENT = {"script", "style", "svg", "noscript"}
ATTRS = ["alt", "placeholder", "aria-label", "title"]
META_NAMES = {"description", "og:title", "og:description", "twitter:title", "twitter:description"}
LETTER = re.compile(r"[^\W\d_]", re.UNICODE)


def norm(s):
    return re.sub(r"\s+", " ", s).strip()


def fnv(s):
    """FNV-1a, 32 bit, over UTF-16 code units: site.js computes the same from the live page."""
    h = 0x811C9DC5
    b = s.encode("utf-16-le")
    for i in range(0, len(b), 2):
        h ^= b[i] | (b[i + 1] << 8)
        h = (h * 0x01000193) & 0xFFFFFFFF
    return f"{h:08x}"


def block_key(inner_html):
    """Key of a text block: its visible text and its link targets. site.js recomputes it before swapping in a
    translation and keeps the English when the page changed since the last extract."""
    text = htmllib.unescape(re.sub(r"<[^>]*>", "", inner_html))
    hrefs = [htmllib.unescape(h) for h in re.findall(r'<a\b[^>]*?\shref="([^"]*)"', inner_html)]
    return fnv("\x01".join([norm(text)] + hrefs))


def attr_key(value):
    return fnv(norm(htmllib.unescape(value)))


class Node:
    def __init__(self, tag, attrs, start, inner_start):
        self.tag, self.attrs, self.start, self.inner_start = tag, dict(attrs), start, inner_start
        self.letters = False      # visible letters somewhere inside
        self.block_inside = False  # a block element inside
        self.marks = []           # (start, inner_start, inner_end) of text blocks inside
        self.no = False           # inside translate="no" or the wordmark
        self.id_inside = False    # an element with an id inside: scripts hold it, so it must not be replaced


class Scan(HTMLParser):
    """Finds the outermost text blocks and the translatable attributes, with their positions in the source."""

    def __init__(self, src):
        super().__init__(convert_charrefs=False)
        self.src = src
        self.line_start = [0]
        for m in re.finditer("\n", src):
            self.line_start.append(m.end())
        self.stack = [Node("#root", [], 0, 0)]
        self.blocks = []  # (start of the start tag, end of the start tag, inner html)
        self.attrs = []   # (start of the start tag, end of the start tag, [(attr, value)])
        self.skip = 0

    def pos(self):
        line, col = self.getpos()
        return self.line_start[line - 1] + col

    def handle_starttag(self, tag, attrs):
        start = self.pos()
        raw = self.get_starttag_text()
        end = start + len(raw)
        a = dict(attrs)
        found = [(n, a[n]) for n in ATTRS if a.get(n) and LETTER.search(a[n])]
        if tag == "meta" and (a.get("name") in META_NAMES or a.get("property") in META_NAMES) and a.get("content"):
            found.append(("content", a["content"]))
        if found and not self.skip and not self.stack[-1].no:
            self.attrs.append((start, end, found))
        if tag in VOID:
            if tag in BLOCKS:
                self.stack[-1].block_inside = True
            return
        node = Node(tag, attrs, start, end)
        cls = (a.get("class") or "").split()
        node.no = self.stack[-1].no or a.get("translate") == "no" or "wordmark" in cls
        if tag in SKIP_CONTENT:
            self.skip += 1
        self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in VOID:
            self.handle_endtag(tag)

    def handle_data(self, data):
        if not self.skip and LETTER.search(data):
            self.stack[-1].letters = True

    def handle_entityref(self, name):
        pass

    def handle_endtag(self, tag):
        if tag in VOID:
            return
        if self.stack[-1].tag != tag:
            raise SystemExit(f"Unbalanced </{tag}> at line {self.getpos()[0]} (open: {self.stack[-1].tag})")
        node = self.stack.pop()
        inner_end = self.pos()
        if tag in SKIP_CONTENT:
            self.skip -= 1
        parent = self.stack[-1]
        no = node.no or tag in SKIP_CONTENT
        qualifies = (tag in CANDIDATES and node.letters and not node.block_inside and not node.id_inside
                     and not no and not self.skip)
        if qualifies:
            parent.marks.append((node.start, node.inner_start, inner_end))
        else:
            parent.marks.extend(node.marks)
        if tag in SKIP_CONTENT:
            parent.block_inside = parent.block_inside or tag in BLOCKS
            return
        parent.id_inside = parent.id_inside or node.id_inside or "id" in node.attrs
        parent.letters = parent.letters or (node.letters and not no)
        parent.block_inside = parent.block_inside or tag in BLOCKS or node.block_inside

    def result(self):
        if len(self.stack) != 1:
            raise SystemExit(f"Unclosed <{self.stack[-1].tag}>")
        return self.stack[0].marks, self.attrs


def set_attr(tag_text, name, value):
    """Sets name="value" in a start tag's text, replacing an earlier value."""
    pattern = re.compile(r'\s' + re.escape(name) + r'="[^"]*"')
    if pattern.search(tag_text):
        return pattern.sub(f' {name}="{value}"', tag_text, count=1)
    close = "/>" if tag_text.endswith("/>") else ">"
    return tag_text[: -len(close)].rstrip() + f' {name}="{value}"' + close


def strip_attr(tag_text, name):
    return re.sub(r'\s' + re.escape(name) + r'="[^"]*"', "", tag_text)


def extract_page(path, strings, write=True):
    """Sets the keys in one page. Returns (text blocks, attributes, whether the page had to change)."""
    original = path.read_text(encoding="utf-8")
    src = original
    # Start from a clean page so removed text loses its key
    src = re.sub(r'\sdata-i18n(-a)?="[^"]*"', "", src)
    scan = Scan(src)
    scan.feed(src)
    scan.close()
    blocks, attrs = scan.result()
    edits = {}  # start of start tag -> (end of start tag, [(attr name, value)])
    for start, inner_start, inner_end in blocks:
        html = src[inner_start:inner_end]
        k = block_key(html)
        if strings.setdefault(k, norm(html)) != norm(html):
            print(f"  note: {path.name} has the same text with other markup: {norm(html)[:60]}")
        edits.setdefault(start, [inner_start, []])[1].append(("data-i18n", k))
    for start, end, found in attrs:
        parts = []
        for name, value in found:
            k = attr_key(value)
            strings.setdefault(k, norm(value))
            parts.append(f"{name}:{k}")
        edits.setdefault(start, [end, []])[1].append(("data-i18n-a", ",".join(parts)))
    out, last = [], 0
    for start in sorted(edits):
        end, sets = edits[start]
        tag = src[start:end]
        for name, value in sets:
            tag = set_attr(tag, name, value)
        out.append(src[last:start])
        out.append(tag)
        last = end
    out.append(src[last:])
    new = "".join(out)
    if write and new != original:
        path.write_text(new, encoding="utf-8")
    return len(blocks), sum(len(f) for _, _, f in attrs), new != original


JS_STRING = re.compile(r"""\b(?:T|zitroT)\(\s*(['"`])((?:\\.|(?!\1).)*)\1""", re.S)


def js_strings():
    found = {}
    for name in SCRIPTS:
        src = (SITE / name).read_text(encoding="utf-8")
        for m in JS_STRING.finditer(src):
            text = m.group(2).encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")
            found[text] = text
    return found


def extract():
    strings = {}
    for page in PAGES:
        n_blocks, n_attrs, _ = extract_page(SITE / page, strings)
        print(f"{page}: {n_blocks} text blocks, {n_attrs} attributes")
    data = {"lang": "en", "dir": "ltr", "t": strings, "js": js_strings(),
            "languages": [[c, n, d] for c, n, d in LANGUAGES]}
    I18N.mkdir(exist_ok=True)
    (I18N / "en.json").write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    write_language_list()
    words = sum(len(re.sub(r"<[^>]+>", " ", v).split()) for v in list(strings.values()) + list(data["js"]))
    print(f"en.json: {len(strings)} entries, {len(data['js'])} script strings, about {words} words")


def write_language_list():
    """site.js offers the languages that have a file (and English)."""
    path = SITE / "site.js"
    src = path.read_text(encoding="utf-8")
    have = [l for l in LANGUAGES if l[0] == "en" or (I18N / f"{l[0]}.json").exists()]
    rows = ",\n".join(f"    ['{c}', '{n}', '{d}']" for c, n, d in have)
    block = f"// LANGUAGES-BEGIN\nconst ZITRO_LANGUAGES = [\n{rows}\n];\n// LANGUAGES-END"
    new = re.sub(r"// LANGUAGES-BEGIN.*?// LANGUAGES-END", lambda m: block, src, flags=re.S)
    if new != src:
        path.write_text(new, encoding="utf-8")
    print(f"site.js: {len(have)} languages offered")


TAG = re.compile(r"<(/?)([a-zA-Z0-9]+)([^>]*)>")


def markup(html):
    """The tags of a text block with the attributes that must not change (links, classes, ids)."""
    out = []
    for m in TAG.finditer(html):
        attrs = dict(re.findall(r'([a-zA-Z-]+)="([^"]*)"', m.group(3)))
        keep = tuple(sorted((k, v) for k, v in attrs.items() if k in ("href", "class", "id", "src", "target", "rel")))
        out.append((m.group(1), m.group(2).lower(), keep))
    return sorted(out)


def check():
    en = json.loads((I18N / "en.json").read_text(encoding="utf-8"))
    codes = {c for c, _, _ in LANGUAGES}
    problems = 0
    # The keys must match the pages as they are now; otherwise a changed sentence would show an old translation
    strings = {}
    for page in PAGES:
        if extract_page(SITE / page, strings, write=False)[2]:
            print(f"{page}: keys are out of date, run: python3 scripts/site_i18n.py extract")
            problems += 1
    if strings != en["t"] or js_strings() != en["js"]:
        print("en.json is out of date, run: python3 scripts/site_i18n.py extract")
        problems += 1
    for path in sorted(I18N.glob("*.json")):
        code = path.stem
        if code == "en":
            continue
        if code not in codes:
            print(f"{path.name}: not in LANGUAGES")
            problems += 1
            continue
        tr = json.loads(path.read_text(encoding="utf-8"))
        t, js = tr.get("t", {}), tr.get("js", {})
        bad = []
        for k, v in t.items():
            if k not in en["t"]:
                bad.append(f"unknown key {k}")
            elif markup(v) != markup(en["t"][k]):
                bad.append(f"markup differs in {k}: {v[:80]}")
            elif "<script" in v.lower() or "javascript:" in v.lower() or re.search(r"\son[a-z]+=", v):
                bad.append(f"script in {k}")
        for k in js:
            if k not in en["js"]:
                bad.append(f"unknown script string {k[:40]}")
        for k, v in js.items():
            if k in en["js"] and sorted(re.findall(r"\{\w+\}", v)) != sorted(re.findall(r"\{\w+\}", k)):
                bad.append(f"placeholders differ in script string {k[:40]}")
        have = len([k for k in en["t"] if k in t]) + len([k for k in en["js"] if k in js])
        total = len(en["t"]) + len(en["js"])
        print(f"{code}: {have}/{total} translated" + (f", {len(bad)} problems" if bad else ""))
        for b in bad[:20]:
            print("   ", b)
        problems += len(bad)
    return problems


def todo(code):
    en = json.loads((I18N / "en.json").read_text(encoding="utf-8"))
    path = I18N / f"{code}.json"
    tr = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {"t": {}, "js": {}}
    missing = {"t": {k: v for k, v in en["t"].items() if k not in tr.get("t", {})},
               "js": {k: v for k, v in en["js"].items() if k not in tr.get("js", {})}}
    print(json.dumps(missing, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else ""
    if cmd == "extract":
        extract()
    elif cmd == "check":
        sys.exit(1 if check() else 0)
    elif cmd == "todo" and len(sys.argv) > 2:
        todo(sys.argv[2])
    else:
        print(__doc__)
        sys.exit(2)
