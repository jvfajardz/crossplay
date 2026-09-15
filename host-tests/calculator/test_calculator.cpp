// The calculator, checked without a panel.
//
// Two halves. The first pins the arithmetic and the key semantics -- every case
// here is somewhere a calculator is commonly wrong, and several of them are
// wrong on shipping hardware today. The second walks the geometry of all five
// candidate pads and asserts that every key can be hit, that no two keys
// overlap, and that nothing is drawn where the panel cannot show it; that half
// is what a screenshot cannot prove, because a screenshot shows the pixels and
// says nothing about where a tap would land.

#include <cstdio>
#include <cstring>

#include "CalcEngine.h"
#include "CalcSkin.h"

using namespace calc;

static int checks = 0;
static int failures = 0;

#define CHECK(cond)                                               \
  do {                                                            \
    ++checks;                                                     \
    if (!(cond)) {                                                \
      ++failures;                                                 \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
    }                                                             \
  } while (0)

#define CHECK_TEXT(got, want)                                                             \
  do {                                                                                    \
    ++checks;                                                                             \
    if (std::strcmp((got), (want)) != 0) {                                                \
      ++failures;                                                                         \
      std::printf("FAIL %s:%d  got \"%s\" want \"%s\"\n", __FILE__, __LINE__, got, want); \
    }                                                                                     \
  } while (0)

namespace {

// Press a run of keys written the way a person would say them.
void type(Engine& e, const char* keys) {
  for (const char* p = keys; *p; ++p) {
    switch (*p) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        e.press(static_cast<Key>(static_cast<int>(Key::D0) + (*p - '0')));
        break;
      case '.':
        e.press(Key::Dot);
        break;
      case '+':
        e.press(Key::Add);
        break;
      case '-':
        e.press(Key::Sub);
        break;
      case 'x':
        e.press(Key::Mul);
        break;
      case '/':
        e.press(Key::Div);
        break;
      case '=':
        e.press(Key::Equals);
        break;
      case '%':
        e.press(Key::Percent);
        break;
      case 'n':
        e.press(Key::Negate);
        break;
      case 'C':
        e.press(Key::ClearAll);
        break;
      case 'E':
        e.press(Key::ClearEntry);
        break;
      case '<':
        e.press(Key::Backspace);
        break;
      case ' ':
        break;  // spacing, for runs that read as words
      case 'r':
        e.press(Key::Sqrt);
        break;
      case 'q':
        e.press(Key::Square);
        break;
      case 'i':
        e.press(Key::Reciprocal);
        break;
      default:
        std::printf("bad key '%c' in \"%s\"\n", *p, keys);
        break;
    }
  }
}

const char* run(Engine& e, const char* keys) {
  e.press(Key::ClearAll);
  type(e, keys);
  return e.display();
}

// --- the number on the panel ------------------------------------------------

void testTheDisplayHidesBinaryFloatNoise() {
  char out[64];
  // The canonical one. The double is 0.30000000000000004; a calculator that
  // prints its full precision is the calculator everyone says is broken.
  formatNumber(0.1 + 0.2, out, sizeof(out));
  CHECK_TEXT(out, "0.3");
  formatNumber(1.1 * 3.0, out, sizeof(out));
  CHECK_TEXT(out, "3.3");
  formatNumber(4.35 * 100.0, out, sizeof(out));
  CHECK_TEXT(out, "435");
  formatNumber(0.7 * 10.0 - 7.0, out, sizeof(out));
  CHECK_TEXT(out, "0");
  // Twelve significant digits, so a third is twelve threes and not seventeen.
  formatNumber(1.0 / 3.0, out, sizeof(out));
  CHECK_TEXT(out, "0.333333333333");
  // -0.0 is a real double and reads as a bug on a panel.
  formatNumber(-0.0, out, sizeof(out));
  CHECK_TEXT(out, "0");
  formatNumber(1e20, out, sizeof(out));
  CHECK_TEXT(out, "1e20");
  formatNumber(1.5e-8, out, sizeof(out));
  CHECK_TEXT(out, "1.5e-8");
  formatNumber(999999999999.0, out, sizeof(out));
  CHECK_TEXT(out, "999999999999");
}

// The boundary this design has, stated as a test rather than left to be
// discovered by a user. Twelve-digit rounding fixes every case where the error
// is small against the result and NONE where the result is near zero, because
// the error is then the whole answer. Casio shows 0 here because Casio's
// arithmetic is decimal, not because its display is cleverer. If this test ever
// has to change, the change is decNumber, not a bigger rounding.
void testTheKnownLimitOfBinaryArithmetic() {
  char out[64];
  formatNumber(0.1 + 0.2 - 0.3, out, sizeof(out));
  CHECK(std::strcmp(out, "0") != 0);
  CHECK(std::strncmp(out, "5.55", 4) == 0);
}

// --- typing -----------------------------------------------------------------

void testTypingANumber() {
  Engine e;
  CHECK_TEXT(run(e, "0"), "0");
  CHECK_TEXT(run(e, "007"), "7");  // leading zeros collapse
  CHECK_TEXT(run(e, "1.5"), "1.5");
  CHECK_TEXT(run(e, "1.5.5"), "1.55");  // the second point is refused, not stacked
  CHECK_TEXT(run(e, ".5"), "0.5");      // a bare point opens a fraction
  CHECK_TEXT(run(e, "5n"), "-5");
  CHECK_TEXT(run(e, "5n n"), "5");
  // Thirteen digits typed: the thirteenth is refused rather than accepted and
  // silently rounded away.
  CHECK_TEXT(run(e, "1234567890123"), "123456789012");
  CHECK_TEXT(run(e, "123<"), "12");
  CHECK_TEXT(run(e, "1<<"), "0");
  CHECK_TEXT(run(e, "7E"), "0");  // CE clears what is being typed
}

// --- the four functions -----------------------------------------------------

void testTheFourFunctions() {
  Engine e;
  CHECK_TEXT(run(e, "2+3="), "5");
  CHECK_TEXT(run(e, "9-4="), "5");
  CHECK_TEXT(run(e, "6x7="), "42");
  CHECK_TEXT(run(e, "8/2="), "4");
  // Immediate execution: an operator completes the sum so far, so 2+3x4 is
  // (2+3)x4. This is what a pocket calculator does and what the pads that are
  // not EXPRESSION promise.
  CHECK_TEXT(run(e, "2+3x4="), "20");
}

// Pressing = again repeats the last operator AND the last operand. Every
// physical calculator does this; it is how you step a series without retyping.
void testRepeatedEquals() {
  Engine e;
  CHECK_TEXT(run(e, "2+3="), "5");
  type(e, "=");
  CHECK_TEXT(e.display(), "8");
  type(e, "=");
  CHECK_TEXT(e.display(), "11");
  CHECK_TEXT(run(e, "2x3=="), "18");
}

// Two operators in a row replace. A pad that stacks them computes with an
// operator the user already changed their mind about.
void testOperatorReplacement() {
  Engine e;
  CHECK_TEXT(run(e, "2+x3="), "6");
  CHECK_TEXT(run(e, "10-/+5="), "15");
}

// Percent reads the PENDING operator: additive wants a percentage of the
// running total, multiplicative wants a plain hundredth. This is the rule iOS
// and Casio share, and it is the single most commonly reimplemented-wrong key
// on a calculator. Windows differs on the multiplicative case (it multiplies
// there too, so 500 x 5 % is 12500); if that is the wanted behaviour, this test
// is the one line that changes.
void testPercentReadsThePendingOperator() {
  Engine e;
  CHECK_TEXT(run(e, "200+10%="), "220");
  CHECK_TEXT(run(e, "200-10%="), "180");
  CHECK_TEXT(run(e, "200x10%="), "20");
  CHECK_TEXT(run(e, "200/10%="), "2000");
  CHECK_TEXT(run(e, "50%"), "0.5");  // no pending operator: a plain hundredth
}

void testNegateAppliesToWhatIsOnScreen() {
  Engine e;
  CHECK_TEXT(run(e, "5+3n="), "2");
  CHECK_TEXT(run(e, "2x3=n"), "-6");
}

void testTheUnaryKeys() {
  Engine e;
  CHECK_TEXT(run(e, "9r"), "3");
  CHECK_TEXT(run(e, "7q"), "49");
  CHECK_TEXT(run(e, "4i"), "0.25");
  // A unary result is a result, not something you are still typing: the next
  // digit starts a new number rather than extending it.
  CHECK_TEXT(run(e, "9r5"), "5");
}

// An error is a wall. Letting a digit land on top of "Cannot divide by zero" is
// how a calculator starts quietly lying: the message goes away, the broken
// state does not.
void testErrorsStopEverythingButClear() {
  Engine e;
  CHECK_TEXT(run(e, "5/0="), "Cannot divide by zero");
  CHECK(e.hasError());
  type(e, "7");
  CHECK_TEXT(e.display(), "Cannot divide by zero");
  type(e, "+1=");
  CHECK_TEXT(e.display(), "Cannot divide by zero");
  type(e, "C");
  CHECK(!e.hasError());
  CHECK_TEXT(e.display(), "0");
  CHECK_TEXT(run(e, "5n r"), "Invalid input");
  CHECK_TEXT(run(e, "0i"), "Cannot divide by zero");
  // CE gets out of it too, which is what Windows does and what a hand reaches
  // for first.
  run(e, "5/0=");
  type(e, "E");
  CHECK(!e.hasError());
}

void testTheTapeRecordsFinishedSums() {
  Engine e;
  run(e, "12+34=");
  type(e, "250x4=");
  CHECK(e.tapeCount() == 2);
  CHECK_TEXT(e.tapeLine(0), "12 + 34 = 46");
  // The second sum starts from scratch: after =, a digit opens a new number
  // rather than extending the result, so 250 is an operand and not 46250.
  CHECK_TEXT(e.tapeLine(1), "250 \xC3\x97 4 = 1000");
}

// --- the pads ---------------------------------------------------------------

const Skin* kAll[] = {&skins::kToybox, &skins::kInstrument, &skins::kNight, &skins::kSwiss, &skins::kLedger};

// The activity's own geometry function, not a copy of it: a host suite that
// re-derives the layout checks a layout the panel does not draw.
PadGeom geomFor(const Skin& s) { return calc::geomFor(s, 480, 800); }

// The floor a finger needs. Apple's is 44pt, which at this panel's 220ppi is
// about 61px; no skin may come under it in either direction. A skin is a look,
// and a look is not allowed to cost reachability.
constexpr int kMinTouchPx = 61;

void testEveryKeyIsBigEnoughToHitInEverySkin() {
  for (const Skin* s : kAll) {
    const PadGeom g = geomFor(*s);
    CHECK(g.cellW >= kMinTouchPx);
    CHECK(g.cellH >= kMinTouchPx);
    if (g.cellW < kMinTouchPx || g.cellH < kMinTouchPx) {
      std::printf("  %s keys are %dx%d px\n", s->name, g.cellW, g.cellH);
    }
  }
}

// The rule three separate bugs in this fork came from breaking: the hit test
// must be the drawing geometry, not a second copy of it.
//
// Centres alone do NOT prove that, and this test said they did until a mutation
// run showed otherwise: shifting every hit rect sideways by one gap still left
// each key's own centre inside its own (wrong) rect, so a systematically
// misplaced pad passed. The corners are what can fail.
void testEveryKeyAnswersOverItsWholeFace() {
  for (const Skin* s : kAll) {
    const PadGeom g = geomFor(*s);
    const int cells = kPad.cols * kPad.rows;
    for (int i = 0; i < cells; ++i) {
      if (kPad.keys[i].key == Key::None) continue;
      const Rect16 r = keyRect(kPad, g, i);
      const int probes[5][2] = {{r.x + r.w / 2, r.y + r.h / 2},
                                {r.x + 2, r.y + 2},
                                {r.right() - 3, r.y + 2},
                                {r.x + 2, r.bottom() - 3},
                                {r.right() - 3, r.bottom() - 3}};
      for (const auto& p : probes) {
        const int hit = keyAt(kPad, g, p[0], p[1]);
        CHECK(hit == i);
        if (hit != i) std::printf("  %s key %d: (%d,%d) resolves to %d\n", s->name, i, p[0], p[1], hit);
      }
      CHECK(keyAt(kPad, g, r.x - 1, r.y + r.h / 2) != i);
      CHECK(keyAt(kPad, g, r.right(), r.y + r.h / 2) != i);
      CHECK(keyAt(kPad, g, r.x + r.w / 2, r.y - 1) != i);
      CHECK(keyAt(kPad, g, r.x + r.w / 2, r.bottom()) != i);
    }
  }
}

void testNoTwoKeysOverlapAndNoneLeavesTheBody() {
  for (const Skin* s : kAll) {
    const Rect16 b = bodyFor(*s, 480, 800);
    const PadGeom g = geomFor(*s);
    const int cells = kPad.cols * kPad.rows;
    for (int i = 0; i < cells; ++i) {
      if (kPad.keys[i].key == Key::None) continue;
      const Rect16 a = keyRect(kPad, g, i);
      CHECK(a.x >= b.x && a.right() <= b.right());
      CHECK(a.y >= g.grid.y && a.bottom() <= b.bottom());
      for (int j = i + 1; j < cells; ++j) {
        if (kPad.keys[j].key == Key::None) continue;
        const Rect16 c = keyRect(kPad, g, j);
        const bool apart = a.right() <= c.x || c.right() <= a.x || a.bottom() <= c.y || c.bottom() <= a.y;
        CHECK(apart);
        if (!apart) std::printf("  %s keys %d and %d overlap\n", s->name, i, j);
      }
    }
  }
}

// The pad must not run into the chrome above it or the bezel below it. Every
// skin sets its own chrome height and margins, so this is the one place that
// notices a skin whose numbers do not add up.
void testNoSkinCollidesWithItsOwnChrome() {
  for (const Skin* s : kAll) {
    const Rect16 b = bodyFor(*s, 480, 800);
    CHECK(b.y >= s->chromeH);
    CHECK(b.h > 0 && b.w > 0);
    const PadGeom g = geomFor(*s);
    CHECK(g.display.bottom() <= g.grid.y);
    CHECK(g.grid.bottom() <= b.bottom());
    // Nothing left over at the bottom, in ANY skin. The only slack allowed is
    // what integer division leaves when the grid height does not divide by the
    // row count -- at most one pixel a row. This is the check that makes "the
    // spacing is off" a red suite rather than something you notice in a render:
    // every band in the display is derived from a cut metric, so a leftover
    // here means a band was guessed.
    const Rect16 last = keyRect(kPad, g, kPad.cols * kPad.rows - 1);
    CHECK(b.bottom() - last.bottom() < kPad.rows);
    if (b.bottom() - last.bottom() >= kPad.rows) {
      std::printf("  %s leaves %d px of unexplained panel under its pad\n", s->name, b.bottom() - last.bottom());
    }
    // And nothing left over on the right, for the same reason.
    const Rect16 rightmost = keyRect(kPad, g, kPad.cols - 1);
    CHECK(b.right() - rightmost.right() < kPad.cols);
  }
}

// Whatever else a skin changes, it stays a calculator: the ten digits, the
// point, the four operators, equals and a way back to zero.
void testThePadCanActuallyCalculate() {
  bool seen[64] = {};
  const int cells = kPad.cols * kPad.rows;
  for (int i = 0; i < cells; ++i) seen[static_cast<int>(kPad.keys[i].key)] = true;
  for (int d = 0; d < 10; ++d) CHECK(seen[static_cast<int>(Key::D0) + d]);
  const Key required[] = {Key::Dot,    Key::Add,      Key::Sub,       Key::Mul,    Key::Div,
                          Key::Equals, Key::ClearAll, Key::Backspace, Key::Negate, Key::Percent};
  for (const Key k : required) {
    CHECK(seen[static_cast<int>(k)]);
    if (!seen[static_cast<int>(k)]) std::printf("  the pad is missing key %d\n", static_cast<int>(k));
  }
}

// A subtitle is a second line ABOUT the app, never its name again. SWISS shipped
// a render whose header read "CALCULATOR   CALCULATOR" because its subtitle was
// the title, and the Rule chrome draws both.
void testNoSkinRepeatsItsOwnTitle() {
  for (const Skin* s : kAll) {
    if (!s->subtitle) continue;
    CHECK(std::strcmp(s->subtitle, "CALCULATOR") != 0);
    if (std::strcmp(s->subtitle, "CALCULATOR") == 0) {
      std::printf("  %s subtitle repeats the title\n", s->name);
    }
  }
}

// Every key says what it is, in every skin. The plus-minus key carries no label
// of its own because not every face can spell U+00B1 -- Jersey 25 cannot, and a
// glyph the face lacks draws as a HOLE, not a box -- so the skin supplies it,
// and a skin that forgot to would leave one blank key that still works.
void testEveryKeyHasALabelInEverySkin() {
  for (const Skin* s : kAll) {
    CHECK(s->plusMinus != nullptr && s->plusMinus[0] != '\0');
    const int cells = kPad.cols * kPad.rows;
    for (int i = 0; i < cells; ++i) {
      if (kPad.keys[i].key == Key::None) continue;
      const char* label = labelFor(*s, kPad.keys[i]);
      CHECK(label != nullptr && label[0] != '\0');
    }
  }
}

// Jersey 25 has no U+00B1 and no U+221A. A skin set in it that asked for the
// real sign would draw an invisible key, which is the exact failure the drawn
// glyphs were replaced to avoid -- so the pairing is checked rather than
// remembered.
void testNoSkinAsksItsFaceForAGlyphItLacks() {
  for (const Skin* s : kAll) {
    const bool jersey = s->labelFont == kJerseyLabelFontId;
    const bool asksForPlusMinus = std::strcmp(s->plusMinus, "\xC2\xB1") == 0;
    CHECK(!(jersey && asksForPlusMinus));
    if (jersey && asksForPlusMinus) std::printf("  %s is set in Jersey and asks for U+00B1\n", s->name);
  }
}

}  // namespace

// The facts only the C++ side knows -- which skin uses which cut, how wide its
// cells come out, and what each key says in it -- printed for label_fit.py to
// measure against the real glyph tables. Two processes because neither side can
// do the other's half: a host test cannot parse a font header, and a Python
// script cannot be trusted to re-derive the geometry.
void printLabelTable() {
  for (const Skin* s : kAll) {
    const PadGeom g = geomFor(*s);
    std::printf("SKIN %s %d %d %d\n", s->name, s->labelFont, g.cellW, g.cellH);
    const int cells = kPad.cols * kPad.rows;
    for (int i = 0; i < cells; ++i) {
      if (kPad.keys[i].key == Key::None) continue;
      const char* label = labelFor(*s, kPad.keys[i]);
      // The RESOLVED cut, through the same function the panel calls. Emitting
      // the skin's big cut and letting the script assume it would measure a
      // face the device never uses, which is a gate that reports on something
      // else.
      std::printf("LABEL %s %d %s\n", s->name, labelFontFor(*s, label), label);
    }
    // The whole ladder, in order, so the script can walk it exactly as the
    // activity does. Emitting only the top rung is how a display that quietly
    // falls three rungs to a 26px label cut reports as fitting.
    for (int rung = 0; rung < kNumberRungs; ++rung) {
      std::printf("NUMBER %s %d %d %d\n", s->name, rung, numberFontFor(*s, rung), g.display.w);
    }
  }
}

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--labels") == 0) {
    printLabelTable();
    return 0;
  }
  testTheDisplayHidesBinaryFloatNoise();
  testTheKnownLimitOfBinaryArithmetic();
  testTypingANumber();
  testTheFourFunctions();
  testRepeatedEquals();
  testOperatorReplacement();
  testPercentReadsThePendingOperator();
  testNegateAppliesToWhatIsOnScreen();
  testTheUnaryKeys();
  testErrorsStopEverythingButClear();
  testTheTapeRecordsFinishedSums();
  testEveryKeyIsBigEnoughToHitInEverySkin();
  testEveryKeyAnswersOverItsWholeFace();
  testNoTwoKeysOverlapAndNoneLeavesTheBody();
  testNoSkinCollidesWithItsOwnChrome();
  testThePadCanActuallyCalculate();
  testNoSkinRepeatsItsOwnTitle();
  testEveryKeyHasALabelInEverySkin();
  testNoSkinAsksItsFaceForAGlyphItLacks();

  // The wording is check.sh's, not a preference: the gate counts sub-suites with
  // grep -c "checks, 0 failed", so a suite that says "failures" runs, passes and
  // is silently left out of the tally -- which looks exactly like a suite nobody
  // ever added. host-tests/checksh enforces it.
  std::printf("%s: %d checks, %d failed\n", failures ? "FAILED" : "ok", checks, failures);
  return failures ? 1 : 0;
}
