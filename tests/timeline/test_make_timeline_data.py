#!/usr/bin/env python3
"""Tests the timeline generator's date checks, with no downloads. Run by tests/timeline/run.sh."""
import contextlib
import io
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "scripts"))
import make_timeline_data as m  # noqa: E402

failures = 0


def check(condition, what):
    global failures
    if not condition:
        failures += 1
        print("  FAIL", what)


check(m.check_date_sentence("Life arose about 3.5 billion years ago.", -3500000000,
                            "x Life arose about 3.5 billion years ago. y") is None, "billion, EN")
check(m.check_date_sentence("Vor etwa 3,5 Milliarden Jahren entstand das Leben.", -3500000000,
                            "Vor etwa 3,5 Milliarden Jahren entstand das Leben.") is None, "Milliarden, DE")
check(m.check_date_sentence("Homo sapiens appeared about 300,000 years ago.", -300000,
                            "Homo sapiens appeared about 300,000 years ago.") is None, "thousands with comma")
check("number" in m.check_date_sentence("Homo sapiens appeared about 300,000 years ago.", -200000,
                                        "Homo sapiens appeared about 300,000 years ago."), "wrong number refused")
check("revision" in m.check_date_sentence("Not there.", -300000, "Something else."), "missing sentence refused")
check(m.wikitext_plain("a {{convert|15.3|–|17.9|km/h|mph}} b<ref>x</ref> [[Link|text]]") == "a 15.3-17.9 km/h b text",
      "wikitext cleanup")
ent = {"claims": {"P580": [{"rank": "normal", "mainsnak": {"snaktype": "value", "datavalue": {"value":
       {"time": "-2700000000-00-00T00:00:00Z", "precision": 1}}}}]}}
check(m.date_of(ent, 1, ["P580"])[0] == -2700000000, "years beyond 32 bits")
check(isinstance(m.date_of(ent, 3, ["P580"]), str), "too coarse for min_precision 3")

check(m.date_of(ent, 1, ["P580"])[2] is False, "no circa by default")
check(m.date_of(ent, 1, ["P580"], coarse_circa=True)[2] is True, "coarse precision is circa")
check(m.date_of(ent, 1, ["P580"], coarse_circa=True)[0] == -2700000000, "coarse_circa keeps the year")


def dated(time, precision):
    return {"claims": {"P580": [{"rank": "normal", "mainsnak": {"snaktype": "value", "datavalue": {"value":
            {"time": time, "precision": precision}}}}]}}


# Under a million years the device writes a plain year, so anything vaguer than a year is circa there
check(m.date_of(dated("-200000-00-00T00:00:00Z", 4), 1, ["P580"], coarse_circa=True)[2] is True,
      "100,000-year precision under a million years is circa")
check(m.date_of(dated("-200000-00-00T00:00:00Z", 4), 1, ["P580"])[2] is False, "only with coarse_circa")
check(m.date_of(dated("-200000-00-00T00:00:00Z", 9), 1, ["P580"], coarse_circa=True)[2] is False,
      "a year known to the year is not circa")
check(m.date_of(dated("-543000000-01-01T00:00:00Z", 3), 1, ["P580"], coarse_circa=True)[2] is False,
      "deep time known to a million years is not circa")
check(m.date_of(dated("-1900000-00-00T00:00:00Z", 4), 1, ["P580"], coarse_circa=True)[2] is False,
      "from a million years the device writes millions")
check(isinstance(m.date_of({"claims": {"P580": [{"rank": "normal", "mainsnak": {"snaktype": "value", "datavalue": {
    "value": {"time": "-6000000000-00-00T00:00:00Z", "precision": 1}}}}]}}, 1, ["P580"]), str), "beyond YEAR_LIMIT")


class Fake:
    """Stands in for Client: one revision of one page."""

    def __init__(self, title, content):
        self.title, self.content = title, content

    def api(self, base, params):
        page = {"title": self.title, "revisions": [{"slots": {"main": {"content": self.content}}}]}
        return {"query": {"pages": [page]}}


source = {"lang": "en", "title": "Life", "revid": 7, "year": -3500000000,
          "sentence": "Life arose about 3.5 billion years ago."}
fake = Fake("Life", "Intro.<ref>x</ref> Life arose about [[3.5 billion years]] ago. More.")
check(m.revision_plain(fake, "en", "Life", 7) == "Intro. Life arose about 3.5 billion years ago. More.", "revision text")
check(m.revision_plain(fake, "en", "Other", 7) is None, "revision of another page")
check(m.wikipedia_date(fake, source) == (-3500000000, True), "wikipedia date is circa")
check(isinstance(m.wikipedia_date(Fake("Other", "x"), source), str), "wikipedia date, wrong page")
check(isinstance(m.wikipedia_date(fake, dict(source, year=-3000000000)), str), "wikipedia date, wrong number")
check(isinstance(m.wikipedia_date(fake, dict(source, year=-6000000000)), str), "wikipedia date beyond YEAR_LIMIT")

def sentence_ok(sentence, year):
    return m.check_date_sentence(sentence, year, sentence) is None


check(not sentence_ok("about 3.5 million years ago", -35000000), "3.5 million is not 35 million")
check(sentence_ok("about 3.5 million years ago", -3500000), "3.5 million")
check(not sentence_ok("about 300,000 years ago", -300), "300,000 is not 300")
check(sentence_ok("Vor 300.000 Jahren", -300000), "300.000 Jahren")
check(sentence_ok("Vor 3,5 Milliarden Jahren", -3500000000), "3,5 Milliarden")
check(not sentence_ok("Vor 3,5 Milliarden Jahren", -35000000000), "3,5 Milliarden is not 35")
check(sentence_ok("about 3,500 years ago", -3500), "3,500 years")
check(not sentence_ok("about 3,500 years ago", -3), "3,500 years is not 3")
check(m.numbers_in("about 300,000 years ago") == {300000}, "one reading per number")
check(m.wikitext_plain("3.5&nbsp;billion R&amp;D") == "3.5 billion R&D", "entities")
check(sentence_ok(m.wikitext_plain("about 3.5&nbsp;billion years ago"), -3500000000), "nbsp keeps the scale word")


# A Wikipedia date only where Wikidata has no usable one: Wikidata gains dates over time
def item(qid, claims):
    return {"id": qid, "labels": {"en": {"value": "Thing"}, "de": {"value": "Ding"}}, "claims": claims}


wiki_source = {"lang": "en", "title": "Life", "revid": 7, "year": -3500000000,
               "sentence": "Life arose about 3.5 billion years ago."}
life_topic = {"id": "t", "min_precision": 1, "coarse_circa": True, "events": [
    {"wiki": "Dated", "expect": -3500000000, "wikipedia_date": wiki_source},
    {"wiki": "Undated", "expect": -3500000000, "wikipedia_date": wiki_source}]}
entities = {"Dated": item("Q1", dated("-2700000000-00-00T00:00:00Z", 1)["claims"]), "Undated": item("Q2", {})}
printed = io.StringIO()
with contextlib.redirect_stdout(printed):
    events = m.resolve_topic(fake, life_topic, entities)
check([e["qid"] for e in events] == ["Q2"], "an item Wikidata dates gets no Wikipedia date")
check("REJECT t: Dated (Q1)" in printed.getvalue() and "Wikidata" in printed.getvalue(), "the refusal is printed")
check(events and events[0]["prop"] == "wikipedia" and events[0]["circa"], "the undated item takes Wikipedia's date")
coarse_topic = dict(life_topic, min_precision=3, events=life_topic["events"][:1])
with contextlib.redirect_stdout(io.StringIO()):
    events = m.resolve_topic(fake, coarse_topic, entities)
check([e["qid"] for e in events] == ["Q1"], "a Wikidata date too coarse for the topic doesn't count")

check(m.event_flags({"circa": True, "prop": "wikipedia"}) == 5, "flag 4: the date is from Wikipedia")
check(m.event_flags({"circa": False, "prop": "P569"}) == 2, "flag 2: born")
check(m.event_flags({"circa": True, "prop": "P580"}) == 1, "flag 1: circa")

# credits.html is public: Commons' author text goes on it without the makers' e-mail addresses
check(m.without_emails("Emily Willoughby (first.last@example.org)") == "Emily Willoughby", "(address)")
check(m.without_emails("Nobu Tamura email:maker@example.org www.palaeocritti.com") ==
      "Nobu Tamura www.palaeocritti.com", "email: label and address go, the web address stays")
check(m.without_emails("Nobu Tamura (Contact: maker@example.org)") == "Nobu Tamura", "Contact: label")
check(m.without_emails("A (a@b.org, http://a.org )") == "A (http://a.org)", "an address beside a web address")
check(m.without_emails("A (B, a@b.org)") == "A (B)", "an address after a name")
check(m.without_emails("Pavel Riha ( e-mail )") == "Pavel Riha", "the bare ( e-mail )")
check(m.without_emails("The original uploader was HPH at German Wikipedia") ==
      "The original uploader was HPH at German Wikipedia", "no address: unchanged")
check(m.without_emails("") == "" and m.without_emails(None) == "", "nothing")
credit_info = {"page": "https://commons.wikimedia.org/wiki/File:X.jpg", "licence": "CC BY-SA 4.0",
               "artist": "Emily Willoughby (first.last@example.org)"}
page = m.credits_page([("life_history", {"en": "Archaeopteryx"}, "X.jpg", credit_info, "Emily Willoughby, CC BY-SA 4.0")])
check("@" not in page and "<td>Emily Willoughby</td>" in page, "the credits page names the maker, without the address")
generated = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "webpage2", "data", "timeline",
                         "credits.html")
if os.path.exists(generated):
    with open(generated, encoding="utf-8") as f:
        check("@" not in f.read(), "no @ in the generated credits.html")

print("test_make_timeline_data: %s" % ("FAILED" if failures else "ok"))
sys.exit(1 if failures else 0)
