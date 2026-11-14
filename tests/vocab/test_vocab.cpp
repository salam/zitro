// Host tests for the vocabulary trainer's pure logic (sketch/sketch_jan12a/vocab_core.h).
// Run with tests/vocab/run.sh; no hardware needed.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../../sketch/sketch_jan12a/vocab_core.h"

static int failures = 0, checks = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    checks++;                                                             \
    if (!(cond)) {                                                        \
      failures++;                                                         \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                                                    \
  do {                                                                                                    \
    checks++;                                                                                             \
    auto va = (a);                                                                                        \
    auto vb = (b);                                                                                        \
    if (!(va == vb)) {                                                                                    \
      failures++;                                                                                         \
      std::ostringstream sa, sb;                                                                          \
      sa << va;                                                                                           \
      sb << vb;                                                                                           \
      std::printf("FAIL %s:%d  %s == %s  (got '%s', want '%s')\n", __FILE__, __LINE__, #a, #b,           \
                  sa.str().c_str(), sb.str().c_str());                                                     \
    }                                                                                                     \
  } while (0)

static const char* verdictName(VocabVerdict v) {
  return v == VOCAB_RIGHT ? "RIGHT" : v == VOCAB_WRONG ? "WRONG" : v == VOCAB_CLOSE ? "CLOSE" : "ASK_AI";
}
#define CHECK_VERDICT(answer, side, want) \
  CHECK_EQ(std::string(verdictName(vocabCheck(answer, side))), std::string(verdictName(want)))

static void testNormalize() {
  CHECK_EQ(vocabNormalize("The timetable."), std::string("the timetable"));
  CHECK_EQ(vocabNormalize("  trousers (pl.)  "), std::string("trousers"));
  CHECK_EQ(vocabNormalize("That\xE2\x80\x99s boring."), std::string("thats boring"));
  CHECK_EQ(vocabNormalize("to swim"), std::string("swim"));
  CHECK_EQ(vocabNormalize("K\xC3\x84SE"), std::string("k\xC3\xA4se"));  // KÄSE -> käse
  CHECK_EQ(vocabNormalize("(Schul-)Klasse"), std::string("klasse"));
}

static void testSolutions() {
  std::vector<std::string> s = vocabSolutions("to the left/right of");
  bool left = false, right = false;
  for (auto& v : s) {
    if (v == "the left of") left = true;
    if (v == "the right of") right = true;
  }
  CHECK(left);
  CHECK(right);
  s = vocabSolutions("Rock, Jupe");
  bool rock = false, jupe = false;
  for (auto& v : s) {
    if (v == "rock") rock = true;
    if (v == "jupe") jupe = true;
  }
  CHECK(rock);
  CHECK(jupe);
  s = vocabSolutions("class/grade");
  bool cls = false, grade = false;
  for (auto& v : s) {
    if (v == "class") cls = true;
    if (v == "grade") grade = true;
  }
  CHECK(cls);
  CHECK(grade);
}

static void testCheck() {
  CHECK_VERDICT("The timetable.", "timetable", VOCAB_RIGHT);
  CHECK_VERDICT("time table", "timetable", VOCAB_RIGHT);  // sounds the same: a recognition detail
  CHECK_VERDICT("table", "timetable", VOCAB_WRONG);
  CHECK_VERDICT("trousers", "trousers (pl.)", VOCAB_RIGHT);
  CHECK_VERDICT("grade", "class/grade", VOCAB_RIGHT);
  CHECK_VERDICT("to the right of", "to the left/right of", VOCAB_RIGHT);
  CHECK_VERDICT("left of", "to the left/right of", VOCAB_RIGHT);  // "the" is an article: leaving it out is fine
  CHECK_VERDICT("left", "to the left/right of", VOCAB_ASK_AI);   // a phrase of three words: the AI decides
  CHECK_VERDICT("Rock", "Rock, Jupe", VOCAB_RIGHT);
  CHECK_VERDICT("Jupe", "Rock, Jupe", VOCAB_RIGHT);
  CHECK_VERDICT("How about pizza?", "How about \xE2\x80\xA6?", VOCAB_RIGHT);
  CHECK_VERDICT("How about?", "How about \xE2\x80\xA6?", VOCAB_ASK_AI);
  CHECK_VERDICT("What is with pizza", "How about \xE2\x80\xA6?", VOCAB_ASK_AI);
  CHECK_VERDICT("story", "history", VOCAB_WRONG);
  CHECK_VERDICT("That is boring", "That\xE2\x80\x99s boring.", VOCAB_ASK_AI);
  CHECK_VERDICT("thats boring", "That\xE2\x80\x99s boring.", VOCAB_RIGHT);
  CHECK_VERDICT("a pupil", "pupil", VOCAB_RIGHT);
  CHECK_VERDICT("pupil", "a pupil", VOCAB_RIGHT);
  CHECK_VERDICT("I live in Zurich", "I live in ...", VOCAB_RIGHT);
  CHECK_VERDICT("", "pupil", VOCAB_WRONG);
  CHECK_VERDICT("Schueler", "Sch\xC3\xBCler/-in", VOCAB_CLOSE);  // the same word without the umlaut
  CHECK_VERDICT("Sch\xC3\xBCler", "Sch\xC3\xBCler/-in", VOCAB_RIGHT);
  CHECK_VERDICT("Sch\xC3\xBClerin", "Sch\xC3\xBCler/-in", VOCAB_RIGHT);
  CHECK_VERDICT("Schulklasse", "(Schul-)Klasse", VOCAB_RIGHT);
  // British and American spellings, digits: never a different word
  CHECK_VERDICT("color", "colour", VOCAB_RIGHT);
  CHECK_VERDICT("my favorite food", "my favourite food", VOCAB_RIGHT);
  CHECK_VERDICT("Maths.", "maths", VOCAB_RIGHT);
  CHECK_VERDICT("math", "maths", VOCAB_RIGHT);
  CHECK_VERDICT("2", "two", VOCAB_RIGHT);
  // Sounds the same, written differently: neither right nor wrong
  CHECK_VERDICT("Jim", "gym", VOCAB_CLOSE);
  CHECK_VERDICT("Thai", "tie", VOCAB_CLOSE);
  CHECK_VERDICT("ship", "sheep", VOCAB_CLOSE);
  CHECK_VERDICT("shirt", "skirt", VOCAB_WRONG);
  CHECK_VERDICT("story", "history", VOCAB_WRONG);
  CHECK_VERDICT("a Jim", "gym", VOCAB_CLOSE);
  CHECK_VERDICT("Klasse", "(Schul-)Klasse", VOCAB_RIGHT);
}

static void testSpellable() {
  CHECK(vocabSpellable("timetable"));
  CHECK(vocabSpellable("T-shirt"));
  CHECK(vocabSpellable("trousers (pl.)"));
  CHECK(!vocabSpellable("How about \xE2\x80\xA6?"));
  CHECK(!vocabSpellable("class/grade"));
  CHECK(!vocabSpellable("teachers\xE2\x80\x99 room"));
}

static void testLetters() {
  CHECK_EQ(vocabLettersFrom("C, H, E, E, S, E.").letters, std::string("cheese"));
  CHECK_EQ(vocabLettersFrom("see aitch ee ee ess ee").letters, std::string("cheese"));
  CHECK_EQ(vocabLettersFrom("Double you, A, T").letters, std::string("wat"));
  CHECK_EQ(vocabLettersFrom("a double p l e").letters, std::string("apple"));
  CHECK(vocabLettersFrom("Cheese.").spokeWord);
  CHECK(!vocabLettersFrom("C-H-E-E-S-E").spokeWord);

  std::ifstream in("fixtures/spike_transcripts.tsv");
  CHECK(in.good());
  std::string line;
  int rows = 0;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream cols(line);
    std::string transcript, letters, word;
    std::getline(cols, transcript, '\t');
    std::getline(cols, letters, '\t');
    std::getline(cols, word, '\t');
    VocabLetters got = vocabLettersFrom(transcript.c_str());
    rows++;
    if (got.letters != letters || got.spokeWord != (word == "1")) {
      failures++;
      std::printf("FAIL fixture '%s': got '%s' word=%d, want '%s' word=%s\n", transcript.c_str(),
                  got.letters.c_str(), got.spokeWord, letters.c_str(), word.c_str());
    }
    checks++;
  }
  CHECK(rows > 100);
}

static std::string marks(const std::vector<VocabCell>& cells) {
  std::string out;
  for (auto& c : cells) out += c.mark == VOCAB_OK ? 'o' : c.mark == VOCAB_BAD ? 'b' : c.mark == VOCAB_MISS ? 'm' : 'x';
  return out;
}

static void testAlign() {
  std::vector<VocabCell> c = vocabAlign("timetable", "timtable");
  CHECK_EQ(marks(c), std::string("ooomooooo"));
  CHECK_EQ(c[3].ch, 'e');
  CHECK(!vocabAllOk(c));
  CHECK_EQ(marks(vocabAlign("choir", "chior")), std::string("oobbo"));
  c = vocabAlign("pupil", "pupill");  // five right letters and one extra l, wherever it is drawn
  CHECK_EQ(c.size(), (size_t)6);
  int extras = 0, oks = 0;
  for (auto& cell : c) {
    if (cell.mark == VOCAB_EXTRA && cell.ch == 'l') extras++;
    if (cell.mark == VOCAB_OK) oks++;
  }
  CHECK_EQ(extras, 1);
  CHECK_EQ(oks, 5);
  CHECK(vocabAllOk(vocabAlign("bread", "pread")));
  CHECK(vocabAllOk(vocabAlign("timetable", "timetable")));
  CHECK(!vocabAllOk(vocabAlign("jumper", "jumbe")));
  CHECK(vocabAllOk(vocabAlign("T-shirt", "tshirt")));  // the hyphen is a fixed box, not spoken
  CHECK(vocabSoundAlike('b', 'p'));
  CHECK(vocabSoundAlike('t', 'p'));
  CHECK(vocabSoundAlike('j', 'g'));
  CHECK(!vocabSoundAlike('a', 'e'));
}

static void testRound() {
  std::vector<bool> eligible(30, true);
  uint16_t cursor = 0;
  std::vector<uint16_t> r = vocabPlanRound({4, 7}, cursor, eligible, 10);
  std::vector<uint16_t> want = {4, 7, 0, 1, 2, 3, 5, 6, 8, 9};
  CHECK(r == want);
  CHECK_EQ(cursor, (uint16_t)10);

  cursor = 28;
  r = vocabPlanRound({}, cursor, eligible, 10);
  want = {28, 29, 0, 1, 2, 3, 4, 5, 6, 7};
  CHECK(r == want);
  CHECK_EQ(cursor, (uint16_t)8);

  eligible[1] = false;
  cursor = 0;
  r = vocabPlanRound({}, cursor, eligible, 3);
  want = {0, 2, 3};
  CHECK(r == want);

  std::vector<bool> few(4, true);
  cursor = 2;
  r = vocabPlanRound({3}, cursor, few, 10);
  CHECK_EQ(r.size(), (size_t)4);
  CHECK_EQ(r[0], (uint16_t)3);

  std::vector<bool> none(5, false);
  cursor = 0;
  CHECK(vocabPlanRound({}, cursor, none, 10).empty());
}

static std::vector<VocabListRef> youngWorld() {
  std::vector<VocabListRef> lists;
  for (int b = 2; b <= 4; b++)
    for (int u = 1; u <= 5; u++)
      lists.push_back({std::to_string(b * 10 + u), "Young World " + std::to_string(b), "Unit " + std::to_string(u)});
  return lists;
}

static void testFind() {
  std::vector<VocabListRef> lists = youngWorld();
  int found = -1;
  std::vector<int> books;
  CHECK_EQ((int)vocabFind(lists, "Young World 3", "", "Unit 2", found, books), (int)VOCAB_FOUND);
  CHECK_EQ(lists[found].id, std::string("32"));
  CHECK_EQ((int)vocabFind(lists, "", "", "unit zwei", found, books), (int)VOCAB_CHOOSE);
  CHECK_EQ(books.size(), (size_t)3);
  CHECK_EQ(lists[books[2]].book, std::string("Young World 4"));
  CHECK_EQ((int)vocabFind(lists, "Young World 3", "", "", found, books), (int)VOCAB_CHOOSE);  // no unit: the book's units
  CHECK_EQ(books.size(), (size_t)5);
  CHECK_EQ((int)vocabFind(lists, "", "young world 4", "2", found, books), (int)VOCAB_FOUND);
  CHECK_EQ(lists[found].id, std::string("42"));
  CHECK_EQ((int)vocabFind(lists, "Young World 3", "", "Unit 7", found, books), (int)VOCAB_NOT_FOUND);
  CHECK_EQ((int)vocabFind({}, "", "", "Unit 1", found, books), (int)VOCAB_NO_LISTS);
  std::vector<VocabListRef> one = {{"a1", "Tiere", "Lektion 2"}, {"a2", "Tiere", "Lektion 3"}};
  CHECK_EQ((int)vocabFind(one, "", "", "Lektion zwei", found, books), (int)VOCAB_FOUND);
  CHECK_EQ(one[found].id, std::string("a1"));
  CHECK_EQ((int)vocabFind(one, "", "", "Unit 3", found, books), (int)VOCAB_FOUND);  // the number decides
  CHECK_EQ(vocabNameKey("Unit Drei!"), std::string("unit 3"));
  CHECK_EQ(vocabNameKey("Young World Four"), std::string("young world 4"));
}

static void testRequest() {
  VocabRequest r;
  CHECK(vocabParseRequest("practice||Unit 3", r));
  CHECK_EQ(r.mode, std::string("practice"));
  CHECK_EQ(r.book, std::string(""));
  CHECK_EQ(r.unit, std::string("Unit 3"));
  CHECK(vocabParseRequest("upload", r));
  CHECK_EQ(r.mode, std::string("upload"));
  CHECK(vocabParseRequest("spell|Young World 4|Unit 2", r));
  CHECK_EQ(r.book, std::string("Young World 4"));
  CHECK(!vocabParseRequest("dance|x|y", r));
  CHECK(!vocabParseRequest("", r));
}

static void testBase32() {
  uint8_t zero[16] = {0};
  CHECK_EQ(vocabBase32(zero, 16), std::string("AAAAAAAAAAAAAAAAAAAAAAAAAA"));
  const uint8_t foobar[6] = {'f', 'o', 'o', 'b', 'a', 'r'};
  CHECK_EQ(vocabBase32(foobar, 6), std::string("MZXW6YTBOI"));  // RFC 4648 test vector without padding
}

int main() {
  testNormalize();
  testSolutions();
  testCheck();
  testSpellable();
  testLetters();
  testAlign();
  testRound();
  testFind();
  testRequest();
  testBase32();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
