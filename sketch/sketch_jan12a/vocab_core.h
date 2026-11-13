// Pure logic of the vocabulary trainer: checking spoken answers against a word list, reading spelled letters from
// a transcript, marking spelling mistakes, planning rounds and finding the list a child asks for.
// No Arduino includes, tested on the host: tests/vocab/run.sh. Design: docs/superpowers/specs/2026-11-13-vocab-trainer-design.md
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

static const size_t VOCAB_ROUND_SIZE = 10;

// ---------- Text normalization ----------

static inline bool vocabIsAsciiAlnum(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// Lower case for ASCII and the German capitals Ä Ö Ü (UTF-8); other bytes stay as they are
static inline std::string vocabLower(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') {
      out += (char)(c - 'A' + 'a');
    } else if (c == 0xC3 && i + 1 < s.size()) {
      unsigned char d = (unsigned char)s[i + 1];
      if (d == 0x84 || d == 0x96 || d == 0x9C) d += 0x20;  // Ä Ö Ü -> ä ö ü
      out += (char)c;
      out += (char)d;
      i++;
    } else {
      out += (char)c;
    }
  }
  return out;
}

// True at s[i] for the ellipsis "…" (E2 80 A6) or "..."; len gets its byte length
static inline bool vocabGapAt(const std::string& s, size_t i, size_t& len) {
  if (i + 2 < s.size() + 0 && (unsigned char)s[i] == 0xE2 && (unsigned char)s[i + 1] == 0x80 &&
      (unsigned char)s[i + 2] == 0xA6) {
    len = 3;
    return true;
  }
  if (i + 2 < s.size() + 0 && s[i] == '.' && s[i + 1] == '.' && s[i + 2] == '.') {
    len = 3;
    return true;
  }
  return false;
}

static inline bool vocabHasGap(const std::string& s) {
  size_t len;
  for (size_t i = 0; i < s.size(); i++)
    if (vocabGapAt(s, i, len)) return true;
  return false;
}

// Lower case, apostrophes and quotes dropped, text in parentheses dropped, every other sign a space, one space
// between words, and a leading "to " removed (verbs are listed as "to swim" and said as "swim").
static inline std::string vocabNormalize(const char* in) {
  std::string s = vocabLower(in ? in : "");
  std::string out;
  int depth = 0;
  for (size_t i = 0; i < s.size(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '(') {
      depth++;
      out += ' ';
      continue;
    }
    if (c == ')') {
      if (depth > 0) depth--;
      out += ' ';
      continue;
    }
    if (depth > 0) continue;
    if (c == 0xE2 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80) {
      unsigned char d = (unsigned char)s[i + 2];
      i += 2;
      if (d == 0x98 || d == 0x99) continue;  // ‘ ’
      out += ' ';                            // “ ” … – — and the like
      continue;
    }
    if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xB4) {  // ´
      i++;
      continue;
    }
    if (c == '\'' || c == '`') continue;
    if (c == '-') continue;  // "T-shirt" = "tshirt", "x-ray" = "xray"
    if (c >= 0x80 || vocabIsAsciiAlnum(c)) {
      out += (char)c;
    } else {
      out += ' ';
    }
  }
  // Fold spaces
  std::string folded;
  bool space = false;
  for (char c : out) {
    if (c == ' ') {
      space = !folded.empty();
      continue;
    }
    if (space) folded += ' ';
    space = false;
    folded += c;
  }
  if (folded.compare(0, 3, "to ") == 0) folded.erase(0, 3);
  return folded;
}

static inline int vocabWordCount(const std::string& normalized) {
  if (normalized.empty()) return 0;
  int n = 1;
  for (char c : normalized)
    if (c == ' ') n++;
  return n;
}

static inline std::string vocabTrim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
  return s.substr(a, b - a);
}

// Splits on a separator at the top level (outside parentheses)
static inline std::vector<std::string> vocabSplitTop(const std::string& s, const char* seps) {
  std::vector<std::string> parts;
  std::string cur;
  int depth = 0;
  for (char c : s) {
    if (c == '(') depth++;
    if (c == ')' && depth > 0) depth--;
    if (depth == 0 && strchr(seps, c)) {
      parts.push_back(vocabTrim(cur));
      cur.clear();
      continue;
    }
    cur += c;
  }
  parts.push_back(vocabTrim(cur));
  return parts;
}

static inline void vocabAddUnique(std::vector<std::string>& v, const std::string& s) {
  if (s.empty()) return;
  for (auto& x : v)
    if (x == s) return;
  v.push_back(s);
}

// One alternative of a list side, expanded at its first slash: "left/right" inside a phrase gives one phrase per
// word, "Schüler/-in" gives "Schüler" and "Schülerin". Recurses for further slashes, at most 8 variants.
static inline void vocabExpandSlashes(const std::string& raw, std::vector<std::string>& out, int depth) {
  size_t slash = raw.find('/');
  if (slash == std::string::npos || depth > 3 || out.size() >= 8) {
    out.push_back(raw);
    return;
  }
  size_t start = slash, end = slash + 1;
  while (start > 0 && raw[start - 1] != ' ') start--;
  while (end < raw.size() && raw[end] != ' ') end++;
  std::string left = raw.substr(start, slash - start), right = raw.substr(slash + 1, end - slash - 1);
  std::string head = raw.substr(0, start), tail = raw.substr(end);
  if (!right.empty() && right[0] == '-') {
    vocabExpandSlashes(head + left + tail, out, depth + 1);
    vocabExpandSlashes(head + left + right.substr(1) + tail, out, depth + 1);
  } else {
    vocabExpandSlashes(head + left + tail, out, depth + 1);
    vocabExpandSlashes(head + right + tail, out, depth + 1);
  }
}

// "(Schul-)Klasse" also stands for "Schulklasse": the brackets go, the word part stays
static inline std::string vocabOpenPrefixBrackets(const std::string& raw) {
  std::string out;
  for (size_t i = 0; i < raw.size(); i++) {
    if (raw[i] == '(') {
      size_t close = raw.find(')', i);
      if (close != std::string::npos && close > i + 1 && raw[close - 1] == '-') {
        out += raw.substr(i + 1, close - i - 2);
        i = close;
        continue;
      }
    }
    out += raw[i];
  }
  return out;
}

// Every accepted form of a list side, normalized
static inline std::vector<std::string> vocabSolutions(const char* side) {
  std::string raw = side ? side : "";
  std::vector<std::string> out;
  if (!vocabHasGap(raw)) vocabAddUnique(out, vocabNormalize(raw.c_str()));  // gaps are checked by vocabFillsGap
  std::vector<std::string> alts = vocabSplitTop(raw, ",;");
  // " / " with spaces separates whole alternatives too
  std::vector<std::string> more;
  for (auto& a : alts) {
    size_t from = 0, at;
    while ((at = a.find(" / ", from)) != std::string::npos) {
      more.push_back(vocabTrim(a.substr(from, at - from)));
      from = at + 3;
    }
    more.push_back(vocabTrim(a.substr(from)));
  }
  for (auto& a : more) {
    if (vocabHasGap(a)) continue;
    std::vector<std::string> forms = {a};
    std::string opened = vocabOpenPrefixBrackets(a);
    if (opened != a) forms.push_back(opened);
    for (auto& f : forms) {
      std::vector<std::string> expanded;
      vocabExpandSlashes(f, expanded, 0);
      for (auto& e : expanded) vocabAddUnique(out, vocabNormalize(e.c_str()));
    }
  }
  return out;
}

// ---------- Checking an answer ----------

enum VocabVerdict { VOCAB_RIGHT, VOCAB_WRONG, VOCAB_ASK_AI };

static inline std::string vocabNoSpaces(const std::string& s) {
  std::string out;
  for (char c : s)
    if (c != ' ') out += c;
  return out;
}

static inline std::string vocabNoArticle(const std::string& s) {
  static const char* const ARTICLES[] = {"a ", "an ", "the "};
  for (const char* a : ARTICLES)
    if (s.compare(0, strlen(a), a) == 0) return s.substr(strlen(a));
  return s;
}

// Same words, spaces aside ("time table" sounds like "timetable"), with or without a leading article
static inline bool vocabSameAnswer(const std::string& a, const std::string& b) {
  if (vocabNoSpaces(a) == vocabNoSpaces(b)) return true;
  return vocabNoSpaces(vocabNoArticle(a)) == vocabNoSpaces(vocabNoArticle(b));
}

// "How about …?" accepts "How about pizza?": the answer starts with the part before the gap, ends with the part
// after it, and has at least one word in between
static inline bool vocabFillsGap(const std::string& answer, const std::string& rawAlt) {
  size_t len = 0, at = std::string::npos;
  for (size_t i = 0; i < rawAlt.size(); i++)
    if (vocabGapAt(rawAlt, i, len)) {
      at = i;
      break;
    }
  if (at == std::string::npos) return false;
  std::string before = vocabNormalize(rawAlt.substr(0, at).c_str());
  std::string after = vocabNormalize(rawAlt.substr(at + len).c_str());
  std::string a = answer;
  if (!before.empty()) {
    if (a.compare(0, before.size(), before) != 0) return false;
    a = a.substr(before.size());
    if (a.empty() || a[0] != ' ') return false;
    a = a.substr(1);
  }
  if (!after.empty()) {
    if (a.size() < after.size() || a.compare(a.size() - after.size(), after.size(), after) != 0) return false;
    a = a.substr(0, a.size() - after.size());
    if (a.empty() || a[a.size() - 1] != ' ') return false;
    a = a.substr(0, a.size() - 1);
  }
  return !vocabTrim(a).empty();
}

// A sentence or phrase where other wording may be right too: the AI is asked when the plain check fails
static inline bool vocabIsSentence(const std::string& rawSide) {
  if (vocabHasGap(rawSide)) return true;
  std::string t = vocabTrim(rawSide);
  if (!t.empty()) {
    char last = t[t.size() - 1];
    if ((last == '.' || last == '?' || last == '!') && vocabWordCount(vocabNormalize(t.c_str())) >= 2) return true;
  }
  return vocabWordCount(vocabNormalize(rawSide.c_str())) >= 3;
}

static inline VocabVerdict vocabCheck(const char* answer, const char* side) {
  std::string a = vocabNormalize(answer);
  std::string raw = side ? side : "";
  if (a.empty()) return VOCAB_WRONG;
  for (auto& s : vocabSolutions(side))
    if (vocabSameAnswer(a, s)) return VOCAB_RIGHT;
  for (auto& alt : vocabSplitTop(raw, ",;"))
    if (vocabFillsGap(a, alt)) return VOCAB_RIGHT;
  return vocabIsSentence(raw) ? VOCAB_ASK_AI : VOCAB_WRONG;
}

// ---------- Spelling ----------

// The word a child spells for this list side, lower case, or "" when the side isn't one word of letters.
// Hyphens and apostrophes stay: they become fixed boxes nobody has to say.
static inline std::string vocabSpellWord(const char* side) {
  std::string raw = side ? side : "";
  std::string noBrackets;
  int depth = 0;
  for (char c : raw) {
    if (c == '(') depth++;
    else if (c == ')') {
      if (depth > 0) depth--;
    } else if (depth == 0) {
      noBrackets += c;
    }
  }
  std::string w = vocabLower(vocabTrim(noBrackets));
  if (w.empty()) return "";
  for (char c : w)
    if (!((c >= 'a' && c <= 'z') || c == '-' || c == '\'')) return "";
  return w;
}

static inline bool vocabSpellable(const char* side) { return !vocabSpellWord(side).empty(); }

struct VocabLetters {
  std::string letters;
  bool spokeWord = false;  // the transcript has a word that isn't a letter name: the child said it instead
};

struct VocabLetterName {
  const char* name;
  char letter;
};

// English letter names as the speech recognition writes them (from the 2026-11-13 spike)
static const VocabLetterName VOCAB_LETTER_NAMES[] = {
    {"a", 'a'},     {"ay", 'a'},    {"ey", 'a'},    {"eh", 'a'},     {"bee", 'b'},  {"be", 'b'},   {"b", 'b'},
    {"see", 'c'},   {"sea", 'c'},   {"cee", 'c'},   {"si", 'c'},     {"dee", 'd'},  {"di", 'd'},   {"ee", 'e'},
    {"e", 'e'},     {"ef", 'f'},    {"eff", 'f'},   {"gee", 'g'},    {"ji", 'g'},   {"aitch", 'h'}, {"haitch", 'h'},
    {"eye", 'i'},   {"i", 'i'},     {"aye", 'i'},   {"jay", 'j'},    {"kay", 'k'},  {"k", 'k'},    {"el", 'l'},
    {"ell", 'l'},   {"em", 'm'},    {"en", 'n'},    {"oh", 'o'},     {"o", 'o'},    {"pee", 'p'},  {"pea", 'p'},
    {"pi", 'p'},    {"cue", 'q'},   {"queue", 'q'}, {"ar", 'r'},     {"are", 'r'},  {"es", 's'},   {"ess", 's'},
    {"tee", 't'},   {"tea", 't'},   {"ti", 't'},    {"you", 'u'},    {"u", 'u'},    {"yu", 'u'},   {"vee", 'v'},
    {"vi", 'v'},    {"ex", 'x'},    {"why", 'y'},   {"wye", 'y'},    {"zed", 'z'},  {"zee", 'z'},  {"doubleyou", 'w'},
};

static inline char vocabLetterOf(const std::string& tok) {
  for (const auto& n : VOCAB_LETTER_NAMES)
    if (tok == n.name) return n.letter;
  if (tok.size() == 1 && tok[0] >= 'a' && tok[0] <= 'z') return tok[0];
  return 0;
}

static inline VocabLetters vocabLettersFrom(const char* transcript) {
  std::string t = vocabLower(transcript ? transcript : "");
  std::vector<std::string> toks;
  std::string cur;
  for (size_t i = 0; i <= t.size(); i++) {
    unsigned char c = i < t.size() ? (unsigned char)t[i] : ' ';
    unsigned char next = i + 1 < t.size() ? (unsigned char)t[i + 1] : 0;
    if (c >= 'a' && c <= 'z') {
      cur += (char)c;
    } else if (c == 0xC3 && (next == 0xA4 || next == 0xB6 || next == 0xBC || next == 0x9F)) {  // ä ö ü ß
      cur += (char)c;
      cur += (char)next;
      i++;
    } else if (!cur.empty()) {  // any other sign, also "、" or "。", ends a token
      toks.push_back(cur);
      cur.clear();
    }
  }
  VocabLetters out;
  for (size_t i = 0; i < toks.size(); i++) {
    const std::string& tok = toks[i];
    if (tok == "double" && i + 1 < toks.size() && (toks[i + 1] == "u" || toks[i + 1] == "you")) {
      out.letters += 'w';  // the letter W, not "uu"
      i++;
      continue;
    }
    if ((tok == "double" || tok == "doppel") && i + 1 < toks.size()) {
      char l = vocabLetterOf(toks[i + 1]);
      if (l) {
        out.letters += l;
        out.letters += l;
        i++;
        continue;
      }
    }
    char l = vocabLetterOf(tok);
    if (l) {
      out.letters += l;
    } else {
      out.spokeWord = true;
    }
  }
  return out;
}

// Letters the speech recognition mixes up (spike: p/t, b/p, g/j); a child is never marked wrong for them
static inline bool vocabSoundAlike(char a, char b) {
  static const char* const PAIRS[] = {"bp", "pt", "dt", "bd", "gj", "mn"};
  for (const char* p : PAIRS)
    if ((a == p[0] && b == p[1]) || (a == p[1] && b == p[0])) return true;
  return false;
}

enum VocabMark : uint8_t { VOCAB_OK, VOCAB_BAD, VOCAB_MISS, VOCAB_EXTRA };

struct VocabCell {
  char ch;         // the word's letter (OK, BAD, MISS) or the child's extra letter (EXTRA)
  VocabMark mark;
};

// Lines up what the child spelled with the word (edit distance) and marks every box
static inline std::vector<VocabCell> vocabAlign(const char* wordIn, const char* saidIn) {
  std::string word = vocabLower(wordIn ? wordIn : ""), said;
  for (char c : vocabLower(saidIn ? saidIn : ""))
    if (c >= 'a' && c <= 'z') said += c;
  std::string letters;  // the word without its fixed signs
  for (char c : word)
    if (c >= 'a' && c <= 'z') letters += c;
  const size_t n = letters.size(), m = said.size();
  std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
  for (size_t i = 0; i <= n; i++) d[i][0] = (int)i;
  for (size_t j = 0; j <= m; j++) d[0][j] = (int)j;
  auto subCost = [&](size_t i, size_t j) {
    char a = letters[i - 1], b = said[j - 1];
    return (a == b || vocabSoundAlike(a, b)) ? 0 : 1;
  };
  for (size_t i = 1; i <= n; i++)
    for (size_t j = 1; j <= m; j++) {
      int best = d[i - 1][j - 1] + subCost(i, j);
      if (d[i - 1][j] + 1 < best) best = d[i - 1][j] + 1;
      if (d[i][j - 1] + 1 < best) best = d[i][j - 1] + 1;
      d[i][j] = best;
    }
  std::vector<VocabCell> rev;
  size_t i = n, j = m;
  while (i > 0 || j > 0) {
    if (i > 0 && j > 0 && d[i][j] == d[i - 1][j - 1] + subCost(i, j)) {
      rev.push_back({letters[i - 1], subCost(i, j) == 0 ? VOCAB_OK : VOCAB_BAD});
      i--;
      j--;
    } else if (i > 0 && d[i][j] == d[i - 1][j] + 1) {
      rev.push_back({letters[i - 1], VOCAB_MISS});
      i--;
    } else {
      rev.push_back({said[j - 1], VOCAB_EXTRA});
      j--;
    }
  }
  // Put the fixed signs (hyphen, apostrophe) back where the word has them
  std::vector<VocabCell> out;
  size_t w = 0;
  for (size_t k = rev.size(); k-- > 0;) {
    const VocabCell& c = rev[k];
    if (c.mark != VOCAB_EXTRA) {
      while (w < word.size() && !(word[w] >= 'a' && word[w] <= 'z')) out.push_back({word[w++], VOCAB_OK});
      w++;
    }
    out.push_back(c);
  }
  while (w < word.size()) out.push_back({word[w++], VOCAB_OK});
  return out;
}

static inline bool vocabAllOk(const std::vector<VocabCell>& cells) {
  for (auto& c : cells)
    if (c.mark != VOCAB_OK) return false;
  return true;
}

// ---------- Rounds ----------

// Up to `want` entries: first those answered wrong last time, then the next ones from the cursor, which walks the
// list so every entry comes up over a few rounds. `cursor` moves past the last entry taken from the walk.
static inline std::vector<uint16_t> vocabPlanRound(const std::vector<uint16_t>& lastWrong, uint16_t& cursor,
                                                   const std::vector<bool>& eligible, size_t want) {
  std::vector<uint16_t> out;
  const size_t n = eligible.size();
  auto taken = [&](uint16_t idx) {
    for (auto x : out)
      if (x == idx) return true;
    return false;
  };
  for (auto idx : lastWrong) {
    if (out.size() >= want) break;
    if (idx < n && eligible[idx] && !taken(idx)) out.push_back(idx);
  }
  if (n == 0) return out;
  size_t start = cursor < n ? cursor : 0;
  for (size_t k = 0; k < n && out.size() < want; k++) {
    uint16_t idx = (uint16_t)((start + k) % n);
    if (!eligible[idx] || taken(idx)) continue;
    out.push_back(idx);
    cursor = (uint16_t)((idx + 1) % n);
  }
  return out;
}

// ---------- Finding a list ----------

// Like recipe names: lower case, letters and digits, one space; number words become digits
static inline std::string vocabNameKey(const char* s) {
  static const char* const NUMBERS[][2] = {
      {"eins", "1"},  {"one", "1"},  {"zwei", "2"},   {"two", "2"},     {"drei", "3"},   {"three", "3"},
      {"vier", "4"},  {"four", "4"}, {"fuenf", "5"},  {"f\xC3\xBCnf", "5"}, {"five", "5"}, {"sechs", "6"},
      {"six", "6"},   {"sieben", "7"}, {"seven", "7"}, {"acht", "8"},    {"eight", "8"},  {"neun", "9"},
      {"nine", "9"},  {"zehn", "10"}, {"ten", "10"},  {"elf", "11"},    {"eleven", "11"}, {"zwoelf", "12"},
      {"zw\xC3\xB6lf", "12"}, {"twelve", "12"}};
  std::string low = vocabLower(s ? s : "");
  std::string out, tok;
  auto flush = [&]() {
    if (tok.empty()) return;
    for (auto& nw : NUMBERS)
      if (tok == nw[0]) {
        tok = nw[1];
        break;
      }
    if (!out.empty()) out += ' ';
    out += tok;
    tok.clear();
  };
  for (size_t i = 0; i <= low.size(); i++) {
    unsigned char c = i < low.size() ? (unsigned char)low[i] : ' ';
    if (vocabIsAsciiAlnum(c) || c >= 0x80) {
      tok += (char)c;
    } else {
      flush();
    }
  }
  return out;
}

// The number of a unit: after a unit word, or a name that is only a number. -1 if none.
static inline int vocabUnitNumber(const std::string& key) {
  static const char* const UNIT_WORDS[] = {"unit", "lektion", "lesson", "kapitel", "chapter", "teil", "part", "units"};
  std::vector<std::string> toks;
  size_t from = 0, at;
  while ((at = key.find(' ', from)) != std::string::npos) {
    toks.push_back(key.substr(from, at - from));
    from = at + 1;
  }
  toks.push_back(key.substr(from));
  auto number = [](const std::string& t) {
    if (t.empty() || t.size() > 3) return -1;
    for (char c : t)
      if (c < '0' || c > '9') return -1;
    return atoi(t.c_str());
  };
  for (size_t i = 0; i + 1 < toks.size(); i++)
    for (const char* w : UNIT_WORDS)
      if (toks[i] == w && number(toks[i + 1]) >= 0) return number(toks[i + 1]);
  if (toks.size() == 1) return number(toks[0]);
  return -1;
}

struct VocabListRef {
  std::string id, book, name;
};

enum VocabFindResult { VOCAB_FOUND, VOCAB_CHOOSE, VOCAB_NOT_FOUND, VOCAB_NO_LISTS };

static inline bool vocabSameBook(const std::string& listBook, const std::string& wanted) {
  std::string a = vocabNameKey(listBook.c_str()), b = vocabNameKey(wanted.c_str());
  if (a.empty() || b.empty()) return false;
  return a == b || a.find(b) != std::string::npos || b.find(a) != std::string::npos;
}

// Finds the list for "<book> <unit>". FOUND sets `found`. CHOOSE fills `choices` with list indices to pick from:
// one per book when the unit is known but several books have it, or every list of the book when no unit was said.
static inline VocabFindResult vocabFind(const std::vector<VocabListRef>& lists, const char* currentBook,
                                        const char* book, const char* unit, int& found, std::vector<int>& choices) {
  found = -1;
  choices.clear();
  if (lists.empty()) return VOCAB_NO_LISTS;
  const std::string current = currentBook ? currentBook : "", wantedBook = book ? book : "";
  const std::string unitKey = vocabNameKey(unit);
  const int unitNum = vocabUnitNumber(unitKey);

  std::vector<int> cand;
  for (size_t i = 0; i < lists.size(); i++) {
    if (unitKey.empty()) {
      cand.push_back((int)i);
      continue;
    }
    std::string nameKey = vocabNameKey(lists[i].name.c_str());
    if (nameKey == unitKey || (unitNum >= 0 && vocabUnitNumber(nameKey) == unitNum)) cand.push_back((int)i);
  }
  auto filterBook = [&](const std::string& b) {
    std::vector<int> kept;
    for (int i : cand)
      if (vocabSameBook(lists[i].book, b)) kept.push_back(i);
    return kept;
  };
  if (!wantedBook.empty()) {
    cand = filterBook(wantedBook);
  } else if (!current.empty()) {
    std::vector<int> kept = filterBook(current);
    if (!kept.empty()) cand = kept;
  }
  if (cand.empty()) return VOCAB_NOT_FOUND;
  if (cand.size() == 1) {
    found = cand[0];
    return VOCAB_FOUND;
  }
  if (unitKey.empty()) {
    choices = cand;
    return VOCAB_CHOOSE;
  }
  // The unit is known: one choice per book, the first list of each
  for (int i : cand) {
    bool seen = false;
    for (int c : choices)
      if (vocabNameKey(lists[c].book.c_str()) == vocabNameKey(lists[i].book.c_str())) seen = true;
    if (!seen) choices.push_back(i);
  }
  if (choices.size() == 1) {
    found = choices[0];
    choices.clear();
    return VOCAB_FOUND;
  }
  return VOCAB_CHOOSE;
}

// ---------- The answer field and the stick's code ----------

struct VocabRequest {
  std::string mode, book, unit;  // mode: upload | practice | spell | print
};

static inline bool vocabParseRequest(const char* value, VocabRequest& out) {
  std::vector<std::string> parts = vocabSplitTop(value ? value : "", "|");
  out = VocabRequest();
  if (parts.empty()) return false;
  std::string mode = vocabLower(parts[0]);
  if (mode != "upload" && mode != "practice" && mode != "spell" && mode != "print") return false;
  out.mode = mode;
  if (parts.size() > 1) out.book = parts[1];
  if (parts.size() > 2) out.unit = parts[2];
  return true;
}

// RFC 4648 base32, no padding
static inline std::string vocabBase32(const uint8_t* bytes, size_t n) {
  static const char* const ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  std::string out;
  uint32_t buffer = 0;
  int bits = 0;
  for (size_t i = 0; i < n; i++) {
    buffer = (buffer << 8) | bytes[i];
    bits += 8;
    while (bits >= 5) {
      out += ALPHABET[(buffer >> (bits - 5)) & 31];
      bits -= 5;
    }
  }
  if (bits > 0) out += ALPHABET[(buffer << (5 - bits)) & 31];
  return out;
}
