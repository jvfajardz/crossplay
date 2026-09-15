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
#include "CalcLayout.h"

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
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
        e.press(static_cast<Key>(static_cast<int>(Key::D0) + (*p - '0')));
        break;
      case '.': e.press(Key::Dot); break;
      case '+': e.press(Key::Add); break;
      case '-': e.press(Key::Sub); break;
      case 'x': e.press(Key::Mul); break;
      case '/': e.press(Key::Div); break;
      case '=': e.press(Key::Equals); break;
      case '%': e.press(Key::Percent); break;
      case 'n': e.press(Key::Negate); break;
      case 'C': e.press(Key::ClearAll); break;
      case 'E': e.press(Key::ClearEntry); break;
      case '<': e.press(Key::Backspace); break;
      case ' ': break;  // spacing, for runs that read as words
      case 'r': e.press(Key::Sqrt); break;
      case 'q': e.press(Key::Square); break;
      case 'i': e.press(Key::Reciprocal); break;
      default: std::printf("bad key '%c' in \"%s\"\n", *p, keys); break;
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
  CHECK_TEXT(run(e, "007"), "7");        // leading zeros collapse
  CHECK_TEXT(run(e, "1.5"), "1.5");
  CHECK_TEXT(run(e, "1.5.5"), "1.55");   // the second point is refused, not stacked
  CHECK_TEXT(run(e, ".5"), "0.5");       // a bare point opens a fraction
  CHECK_TEXT(run(e, "5n"), "-5");
  CHECK_TEXT(run(e, "5n n"), "5");
  // Thirteen digits typed: the thirteenth is refused rather than accepted and
  // silently rounded away.
  CHECK_TEXT(run(e, "1234567890123"), "123456789012");
  CHECK_TEXT(run(e, "123<"), "12");
  CHECK_TEXT(run(e, "1<<"), "0");
  CHECK_TEXT(run(e, "7E"), "0");         // CE clears what is being typed
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
  CHECK_TEXT(e.tapeLine(1), "250 x 4 = 1000");
}

// --- the pads ---------------------------------------------------------------

const Layout* kAll[] = {&pads::kPhoneLayout, &pads::kDesktopLayout, &pads::kTapeLayout, &pads::kScientificLayout,
                        &pads::kExpressionLayout};

Rect16 body() { return Rect16{16, 119, 448, 800 - 16 - 119}; }

// The floor a finger needs. Apple's is 44pt, which at this panel's 220ppi is
// about 61px; nothing on these pads may come under it in either direction.
constexpr int kMinTouchPx = 61;

void testEveryKeyIsBigEnoughToHit() {
  for (const Layout* layout : kAll) {
    const PadGeom g = padGeom(*layout, body());
    CHECK(g.cellW >= kMinTouchPx);
    CHECK(g.cellH >= kMinTouchPx);
    if (g.cellW < kMinTouchPx || g.cellH < kMinTouchPx) {
      std::printf("  %s keys are %dx%d px\n", layout->name, g.cellW, g.cellH);
    }
  }
}

// The rule three separate bugs in this fork came from breaking: the hit test
// must be the drawing geometry, not a second copy of it.
//
// Centres alone do NOT prove that, and this test said they did until a mutation
// run showed otherwise: shifting every hit rect sideways by one gap still left
// each key's own centre inside its own (wrong) rect, so a systematically
// misplaced pad passed. The corners and the gaps are what can fail -- a key
// whose rect has slid at all loses a corner, and a gap that answers is a key
// that is wider than it looks.
void testEveryKeyAnswersOverItsWholeFaceAndNowhereElse() {
  for (const Layout* layout : kAll) {
    const PadGeom g = padGeom(*layout, body());
    const int cells = layout->cols * layout->rows;
    for (int i = 0; i < cells; ++i) {
      if (layout->keys[i].key == Key::None) continue;
      const Rect16 r = keyRect(*layout, g, i);
      const int probes[5][2] = {{r.x + r.w / 2, r.y + r.h / 2},
                                {r.x + 2, r.y + 2},
                                {r.right() - 3, r.y + 2},
                                {r.x + 2, r.bottom() - 3},
                                {r.right() - 3, r.bottom() - 3}};
      for (const auto& p : probes) {
        const int hit = keyAt(*layout, g, p[0], p[1]);
        CHECK(hit == i);
        if (hit != i) std::printf("  %s key %d: (%d,%d) resolves to %d\n", layout->name, i, p[0], p[1], hit);
      }
      // One pixel past each edge belongs to the gap or a neighbour, never to
      // this key. A pad drawn one place and hit-tested another fails here.
      CHECK(keyAt(*layout, g, r.x - 1, r.y + r.h / 2) != i);
      CHECK(keyAt(*layout, g, r.right(), r.y + r.h / 2) != i);
      CHECK(keyAt(*layout, g, r.x + r.w / 2, r.y - 1) != i);
      CHECK(keyAt(*layout, g, r.x + r.w / 2, r.bottom()) != i);
    }
  }
}

// The gaps refuse rather than round into a neighbour. On a panel that repaints
// in a second, a tap that did the wrong thing costs far more than one that did
// nothing: you have to notice it, wait a refresh, and undo it.
void testTheGapsBetweenKeysAnswerNothing() {
  for (const Layout* layout : kAll) {
    const PadGeom g = padGeom(*layout, body());
    if (layout->cols < 2) continue;
    const Rect16 first = keyRect(*layout, g, 0);
    const int midGapX = first.right() + kKeyGap / 2;
    CHECK(keyAt(*layout, g, midGapX, first.y + first.h / 2) < 0);
  }
}

void testNoTwoKeysOverlapAndNoneLeavesTheBody() {
  const Rect16 b = body();
  for (const Layout* layout : kAll) {
    const PadGeom g = padGeom(*layout, b);
    const int cells = layout->cols * layout->rows;
    for (int i = 0; i < cells; ++i) {
      if (layout->keys[i].key == Key::None) continue;
      const Rect16 a = keyRect(*layout, g, i);
      CHECK(a.x >= b.x && a.right() <= b.right());
      CHECK(a.y >= g.grid.y && a.bottom() <= b.bottom());
      for (int j = i + 1; j < cells; ++j) {
        if (layout->keys[j].key == Key::None) continue;
        const Rect16 c = keyRect(*layout, g, j);
        const bool apart = a.right() <= c.x || c.right() <= a.x || a.bottom() <= c.y || c.bottom() <= a.y;
        CHECK(apart);
        if (!apart) std::printf("  %s keys %d and %d overlap\n", layout->name, i, j);
      }
    }
  }
}

// A wide key swallows the gap it spans, and the cell it covers is None. A span
// left over a real key would put two hit rects on the same pixels, and keyAt
// returns the first -- so the swallowed key would draw and never answer.
void testAWideKeySwallowsAnEmptyCell() {
  for (const Layout* layout : kAll) {
    const int cells = layout->cols * layout->rows;
    for (int i = 0; i < cells; ++i) {
      const uint8_t span = layout->keys[i].span;
      if (span <= 1) continue;
      CHECK(i % layout->cols + span <= layout->cols);
      for (int k = 1; k < span; ++k) CHECK(layout->keys[i + k].key == Key::None);
    }
  }
}

// Whatever else a pad carries, it has to be a calculator: the ten digits, the
// point, the four operators, equals and a way back to zero.
void testEveryPadCanActuallyCalculate() {
  for (const Layout* layout : kAll) {
    bool seen[64] = {};
    const int cells = layout->cols * layout->rows;
    for (int i = 0; i < cells; ++i) seen[static_cast<int>(layout->keys[i].key)] = true;
    for (int d = 0; d < 10; ++d) CHECK(seen[static_cast<int>(Key::D0) + d]);
    const Key required[] = {Key::Dot, Key::Add, Key::Sub, Key::Mul, Key::Div, Key::Equals};
    for (const Key k : required) {
      CHECK(seen[static_cast<int>(k)]);
      if (!seen[static_cast<int>(k)]) std::printf("  %s is missing key %d\n", layout->name, static_cast<int>(k));
    }
    CHECK(seen[static_cast<int>(Key::ClearAll)]);
  }
}

// A key draws either a label or a drawn symbol, never neither: a key with
// nothing in it is a blank slab that still works, which is worse than a key
// that is missing. The Toybox cuts are ASCII-only, so a label with a byte past
// U+007E would draw a HOLE rather than a box and nothing would say why.
void testEveryKeySaysWhatItIs() {
  for (const Layout* layout : kAll) {
    const int cells = layout->cols * layout->rows;
    for (int i = 0; i < cells; ++i) {
      const KeyDef& key = layout->keys[i];
      if (key.key == Key::None) continue;
      CHECK(key.label != nullptr || key.sym != Sym::None);
      if (!key.label) continue;
      for (const char* p = key.label; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        CHECK(c >= 0x20 && c <= 0x7E);
        if (c < 0x20 || c > 0x7E) std::printf("  %s key \"%s\" has a byte no Toybox cut can draw\n", layout->name, key.label);
      }
    }
  }
}

}  // namespace

int main() {
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
  testEveryKeyIsBigEnoughToHit();
  testEveryKeyAnswersOverItsWholeFaceAndNowhereElse();
  testTheGapsBetweenKeysAnswerNothing();
  testNoTwoKeysOverlapAndNoneLeavesTheBody();
  testAWideKeySwallowsAnEmptyCell();
  testEveryPadCanActuallyCalculate();
  testEveryKeySaysWhatItIs();

  std::printf("%s: %d checks, %d failures\n", failures ? "FAILED" : "ok", checks, failures);
  return failures ? 1 : 0;
}
