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
#include "CalcFormat.h"
#include "CalcStyle.h"

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

// --- the arithmetic is decimal ----------------------------------------------

// The reason lib/decNumber is vendored at all. Rounding a double to twelve
// significant digits hides the first of these and CANNOT hide the second: there
// the error IS the answer, so it comes out as 5.55e-17 whatever the display
// does. Casio and TI show 0 because their arithmetic is decimal, not because
// their displays are cleverer.
void testTheArithmeticIsDecimalRatherThanBinary() {
  Engine e;
  CHECK_TEXT(run(e, "0.1+0.2="), "0.3");
  CHECK_TEXT(run(e, "0.1+0.2-0.3="), "0");
  CHECK_TEXT(run(e, "1.1x3="), "3.3");
  CHECK_TEXT(run(e, "1.1x3-3.3="), "0");
  CHECK_TEXT(run(e, "4.35x100="), "435");
  CHECK_TEXT(run(e, "0.7x10-7="), "0");
  CHECK_TEXT(run(e, "1.03-0.42="), "0.61");
  // Decimal arithmetic preserves scale: 4.35 x 100 is exactly 435.00 and 1.50 is
  // exactly 1.50. A calculator shows neither trailing zero.
  CHECK_TEXT(run(e, "1.5x1="), "1.5");
  CHECK_TEXT(run(e, "2.50+2.50="), "5");
}

// Ten significant digits, which is what kMaxDisplayChars allows -- a sign, ten
// digits and a point -- and what a normal pocket calculator carries.
// The guard digits, which are the difference between decimal arithmetic and a
// decimal CALCULATOR. Sixteen working digits, ten shown: without the six hidden
// ones, a third times three is 0.9999999999 on the panel, which is the answer
// nobody wants and every physical calculator avoids the same way.
void testGuardDigitsMakeAThirdTimesThreeCountAsOne() {
  Engine e;
  CHECK_TEXT(run(e, "1/3x3="), "1");
  CHECK_TEXT(run(e, "2/3x3="), "2");
  CHECK_TEXT(run(e, "1/7x7="), "1");
  CHECK_TEXT(run(e, "10/3x3="), "10");
  // And they do not paper over a real difference: a third is still a third.
  CHECK_TEXT(run(e, "1/3="), "0.3333333333");
}

void testTenSignificantDigits() {
  Engine e;
  CHECK_TEXT(run(e, "1/3="), "0.3333333333");
  CHECK_TEXT(run(e, "2/3="), "0.6666666667");  // rounded half up at the tenth
  CHECK_TEXT(run(e, "9999999999+1="), "10000000000");
  CHECK_TEXT(run(e, "1/8="), "0.125");
  // Past the display's reach it goes to an exponent rather than being clipped.
  CHECK_TEXT(run(e, "9999999999x9999999999="), "9.999999998e+19");
  // Fixed while fixed fits, even at the bottom of the range: a calculator
  // shows 0.0000000001, not 1e-10.
  CHECK_TEXT(run(e, "1/9999999999="), "0.0000000001");
}

// --- the display formatter --------------------------------------------------

void fmt(const char* digits, const int exponent, const bool neg, const int budget, const char* want) {
  uint8_t coeff[40];
  int n = 0;
  for (const char* p = digits; *p; ++p) coeff[n++] = static_cast<uint8_t>(*p - '0');
  char out[64];
  const int len = formatDecimal(Decimal{coeff, n, exponent, neg}, budget, out, sizeof(out));
  ++checks;
  if (std::strcmp(out, want) != 0 || len != static_cast<int>(std::strlen(out)) || len > budget) {
    ++failures;
    std::printf("FAIL %s:%d  %se%d b=%d -> \"%s\" (%d) want \"%s\"\n", __FILE__, __LINE__, digits, exponent, budget,
                out, len, want);
  }
}

void testTheDisplayFormatter() {
  // Fixed whenever fixed fits: a calculator shows 1000, never 1E+3.
  fmt("3", -1, false, 12, "0.3");
  fmt("435", 0, false, 12, "435");
  fmt("15", 2, false, 12, "1500");
  fmt("546875", -3, false, 12, "546.875");
  fmt("9999999999", 0, true, 12, "-9999999999");
  fmt("1234567890", -9, true, 12, "-1.23456789");
  // Decimal arithmetic preserves scale, so 4.35 x 100 is exactly 435.00 and
  // 15.00 is exactly 15 -- the trailing zeros of a FRACTION are noise. The ones
  // to the LEFT of the point are magnitude and stay.
  fmt("43500", -2, false, 12, "435");
  fmt("1500", -2, false, 12, "15");
  fmt("15", 2, false, 12, "1500");
  // Zero is zero. A negative zero is a real decimal value and reads as a bug.
  fmt("0", 0, false, 12, "0");
  fmt("0", -5, true, 12, "0");
  // Scientific only when fixed cannot fit, mantissa cut to what the exponent
  // leaves -- which is what a real calculator does when its exponent field eats
  // into its mantissa field.
  fmt("1", 20, false, 12, "1e+20");
  fmt("123456789", -13, false, 12, "1.2345679e-5");
  fmt("123456789", 95, false, 12, "1.23457e+103");
  fmt("999", 99, false, 12, "9.99e+101");
  // Every kept digit a nine: the mantissa carries to a new power of ten, and the
  // string gets SHORTER rather than longer.
  fmt("99999999", -99, true, 12, "-1e-91");
  fmt("99999999999", -1, true, 12, "-1e+10");
  fmt("999999999", 100, false, 12, "1e+109");
  // A narrow budget still produces something readable rather than a clipped one.
  fmt("99999", -4, false, 6, "9.9999");
  fmt("123456789", -4, false, 8, "12345.68");  // rounded to fit, NOT dropped to an exponent
  fmt("123456789", -8, true, 8, "-1.23457");
}

// The property Mario asked for, checked rather than argued: NOTHING the
// formatter can produce is longer than the budget it was given. Every
// coefficient length, every exponent across the range a ten digit calculator can
// reach, both signs, and every budget from useless to generous -- because the
// one case that overflows will not be the one anybody thought to write down.
void testTheFormatterCanNeverExceedItsBudget() {
  uint8_t coeff[16];
  int over = 0;
  int scanned = 0;
  for (int n = 1; n <= 12; ++n) {
    for (int pattern = 0; pattern < 4; ++pattern) {
      for (int i = 0; i < n; ++i) {
        coeff[i] = static_cast<uint8_t>(pattern == 0   ? 9
                                        : pattern == 1 ? (i == 0 ? 1 : 0)
                                        : pattern == 2 ? (i % 10)
                                                       : (i == n - 1 ? 5 : 9));
      }
      if (coeff[0] == 0) coeff[0] = 1;
      for (int exponent = -120; exponent <= 120; ++exponent) {
        for (int neg = 0; neg < 2; ++neg) {
          for (int budget = 6; budget <= 16; ++budget) {
            char out[64];
            const int len = formatDecimal(Decimal{coeff, n, exponent, neg != 0}, budget, out, sizeof(out));
            ++scanned;
            // A zero return means "cannot be shown at all", which is legitimate
            // at a budget too small for one digit and an exponent -- and which
            // the caller turns into an overflow. What is NOT legitimate is a
            // zero at the budget this app actually uses.
            if (len == 0 && budget >= kMaxDisplayChars) {
              if (++over <= 3) std::printf("  nothing could be shown at budget %d\n", budget);
              continue;
            }
            if (len > budget || len != static_cast<int>(std::strlen(out))) {
              if (++over <= 3) {
                std::printf("  \"%s\" is %d characters in a budget of %d\n", out, len, budget);
              }
            }
          }
        }
      }
    }
  }
  ++checks;
  if (over) {
    ++failures;
    std::printf("FAIL %s:%d  %d of %d formatter results exceeded their budget\n", __FILE__, __LINE__, over, scanned);
  }
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
  CHECK_TEXT(run(e, "1234567890123"), "1234567890");
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

// The presses nobody means to make, which are exactly the ones a calculator has
// to survive: a pad has twenty keys and no grammar, so every sequence is legal
// input and every one of them has to leave a number on the panel.
void testTheSequencesNobodyMeansToPress() {
  Engine e;
  CHECK_TEXT(run(e, "+"), "0");  // an operator first: zero is the operand
  CHECK_TEXT(run(e, "+5="), "5");
  CHECK_TEXT(run(e, "5+="), "10");  // equals with no second operand repeats it
  CHECK_TEXT(run(e, "="), "0");     // equals with nothing at all
  CHECK_TEXT(run(e, "5="), "5");
  CHECK_TEXT(run(e, "..."), "0.");  // one point, and it stays while you type
  CHECK_TEXT(run(e, "5..5"), "5.5");
  CHECK_TEXT(run(e, "n"), "0");  // a sign on nothing is still nothing
  CHECK_TEXT(run(e, "n5"), "5");
  CHECK_TEXT(run(e, "%"), "0");         // percent with nothing pending
  CHECK_TEXT(run(e, "<"), "0");         // backspace on an empty entry
  CHECK_TEXT(run(e, "5+3=<"), "8");     // backspace does NOT eat a result
  CHECK_TEXT(run(e, "5+3=7"), "7");     // a digit after equals starts a new number
  CHECK_TEXT(run(e, "5+3=+2="), "10");  // an operator after equals continues from it
  CHECK_TEXT(run(e, "0n"), "0");        // and there is no negative zero anywhere
  CHECK_TEXT(run(e, "0n5"), "-5");      // the sign is HELD though, not dropped
  CHECK_TEXT(run(e, "0.0n"), "0.0");
  CHECK_TEXT(run(e, "0n+0="), "0");
  CHECK_TEXT(run(e, "5-5=n"), "0");
  CHECK_TEXT(run(e, "CCC"), "0");
  CHECK_TEXT(run(e, "5+++3="), "8");  // operators collapse rather than stack
}

// An error is a wall. Letting a digit land on top of "Cannot divide by zero" is
// how a calculator starts quietly lying: the message goes away, the broken
// state does not.
void testErrorsStopEverythingButClear() {
  Engine e;
  CHECK_TEXT(run(e, "5/0="), "DIVIDE BY 0");
  CHECK(e.hasError());
  type(e, "7");
  CHECK_TEXT(e.display(), "DIVIDE BY 0");
  type(e, "+1=");
  CHECK_TEXT(e.display(), "DIVIDE BY 0");
  type(e, "C");
  CHECK(!e.hasError());
  CHECK_TEXT(e.display(), "0");
  // Zero over zero is INVALID rather than a division by zero, and decNumber
  // reports it on a different status bit.
  CHECK_TEXT(run(e, "0/0="), "BAD INPUT");
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

// --- the pad ----------------------------------------------------------------

PadGeom geom() { return padGeometry(480, 800); }
Rect16 body() { return bodyRect(480, 800); }

// The floor a finger needs. Apple's is 44pt, which at this panel's 220ppi is
// about 61px; nothing on this pad may come under it in either direction.
constexpr int kMinTouchPx = 61;

void testEveryKeyIsBigEnoughToHit() {
  const PadGeom g = geom();
  CHECK(g.cellW >= kMinTouchPx);
  CHECK(g.cellH >= kMinTouchPx);
  if (g.cellW < kMinTouchPx || g.cellH < kMinTouchPx) {
    std::printf("  keys are %dx%d px\n", g.cellW, g.cellH);
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
  const PadGeom g = geom();
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
      if (hit != i) std::printf("  key %d: (%d,%d) resolves to %d\n", i, p[0], p[1], hit);
    }
    CHECK(keyAt(kPad, g, r.x - 1, r.y + r.h / 2) != i);
    CHECK(keyAt(kPad, g, r.right(), r.y + r.h / 2) != i);
    CHECK(keyAt(kPad, g, r.x + r.w / 2, r.y - 1) != i);
    CHECK(keyAt(kPad, g, r.x + r.w / 2, r.bottom()) != i);
  }
}

// The gaps refuse rather than round into a neighbour. On a panel that repaints in
// a second, a tap that did the wrong thing costs far more than one that did
// nothing: you have to notice it, wait a refresh, and undo it.
void testTheGapsBetweenKeysAnswerNothing() {
  const PadGeom g = geom();
  const Rect16 first = keyRect(kPad, g, 0);
  CHECK(keyAt(kPad, g, first.right() + kKeyGapPx / 2, first.y + first.h / 2) < 0);
}

void testNoTwoKeysOverlapAndNoneLeavesTheBody() {
  const Rect16 b = body();
  const PadGeom g = geom();
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
      if (!apart) std::printf("  keys %d and %d overlap\n", i, j);
    }
  }
}

// Nothing left over, anywhere. The only slack allowed is what integer division
// leaves when the grid does not divide by the row or column count. This is the
// check that makes "the spacing is off" a red suite rather than something you
// notice in a render: every band in the display is derived from a cut metric, so
// a leftover here means a band was guessed.
void testNothingIsLeftOver() {
  const Rect16 b = body();
  const PadGeom g = geom();
  CHECK(b.y >= kChromeHeight);
  CHECK(g.display.bottom() <= g.grid.y);
  CHECK(g.grid.bottom() <= b.bottom());
  const Rect16 last = keyRect(kPad, g, kPad.cols * kPad.rows - 1);
  CHECK(b.bottom() - last.bottom() < kPad.rows);
  if (b.bottom() - last.bottom() >= kPad.rows) {
    std::printf("  %d px of unexplained panel under the pad\n", b.bottom() - last.bottom());
  }
  const Rect16 rightmost = keyRect(kPad, g, kPad.cols - 1);
  CHECK(b.right() - rightmost.right() < kPad.cols);
  // And the display's own bands add up to the height it declared.
  CHECK(displayHeight() == kSmallCut.lineHeight + kNumberCut.capHeight + 2 * kNumberAir + kRule);
}

// Whatever else changes, it stays a calculator: the ten digits, the point, the
// four operators, equals, a way back to zero, a way to lose one digit and a sign.
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

// Every key says what it is. The plus-minus key carries no label of its own
// because Jersey 25 has no U+00B1 -- and a glyph the face lacks draws as a HOLE,
// not a box -- so the style supplies the keyboard-era spelling instead.
void testEveryKeyHasALabel() {
  const int cells = kPad.cols * kPad.rows;
  for (int i = 0; i < cells; ++i) {
    if (kPad.keys[i].key == Key::None) continue;
    const char* label = labelFor(kPad.keys[i]);
    CHECK(label != nullptr && label[0] != '\0');
  }
}

}  // namespace

// The facts only the C++ side knows -- which skin uses which cut, how wide its
// cells come out, and what each key says in it -- printed for label_fit.py to
// measure against the real glyph tables. Two processes because neither side can
// do the other's half: a host test cannot parse a font header, and a Python
// script cannot be trusted to re-derive the geometry.
void printLabelTable() {
  const PadGeom g = geom();
  std::printf("PAD %d %d %d\n", g.cellW, g.cellH, g.display.w);
  const int cells = kPad.cols * kPad.rows;
  for (int i = 0; i < cells; ++i) {
    if (kPad.keys[i].key == Key::None) continue;
    const char* label = labelFor(kPad.keys[i]);
    // The RESOLVED cut, through the same function the panel calls. Emitting the
    // headline cut and letting the script assume it would measure a face the
    // device never uses, which is a gate that reports on something else.
    std::printf("LABEL %d %s\n", labelFontFor(label), label);
  }
  for (int rung = 0; rung < kNumberRungs; ++rung) {
    std::printf("RUNG %d %d\n", rung, numberFontFor(rung));
  }

  // The engine's OWN output at its limits, for the script to measure. Not a list
  // of samples somebody thought of: these are what it actually produced, so a
  // formatting change that lengthens a result cannot slip past the gate by being
  // a shape nobody imagined.
  static const char* kLimits[] = {
      "9999999999x9999999999=",              // the largest product it can reach
      "9999999999+9999999999=",              // and the largest sum
      "1/3=",                                // a repeating decimal, filled to capacity
      "2/3=",                                // the same, rounded up at the last digit
      "1/0=",                                // the longest error string
      "0.0000000001/9999999999=",            // driven off the bottom into an exponent
      "9999999999x9999999999=x9999999999=",  // and off the top
      "1234567890n",                         // the widest thing that can be TYPED
      "0.123456789n",                        // a typed fraction with a sign
  };
  Engine engine;
  for (const char* keys : kLimits) {
    run(engine, keys);
    // Which cut this string will really be drawn in, resolved the way the
    // activity resolves it: an error is words and goes in the label cut, a
    // result is digits and walks the number rungs.
    std::printf("WORST %d %s\n", engine.hasError() ? kLabelFontId : 0, engine.display());
    if (engine.pending()[0]) std::printf("PENDING %s\n", engine.pending());
  }
  // And the longest pending line: a full-width operand with an operator after it.
  run(engine, "9999999999x");
  std::printf("PENDING %s\n", engine.pending());
  run(engine, "0.123456789n+");
  std::printf("PENDING %s\n", engine.pending());
}

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--labels") == 0) {
    printLabelTable();
    return 0;
  }
  testTheArithmeticIsDecimalRatherThanBinary();
  testGuardDigitsMakeAThirdTimesThreeCountAsOne();
  testTenSignificantDigits();
  testTheDisplayFormatter();
  testTheFormatterCanNeverExceedItsBudget();
  testTypingANumber();
  testTheFourFunctions();
  testRepeatedEquals();
  testOperatorReplacement();
  testPercentReadsThePendingOperator();
  testNegateAppliesToWhatIsOnScreen();
  testTheSequencesNobodyMeansToPress();
  testErrorsStopEverythingButClear();
  testTheTapeRecordsFinishedSums();
  testEveryKeyIsBigEnoughToHit();
  testEveryKeyAnswersOverItsWholeFace();
  testTheGapsBetweenKeysAnswerNothing();
  testNoTwoKeysOverlapAndNoneLeavesTheBody();
  testNothingIsLeftOver();
  testThePadCanActuallyCalculate();
  testEveryKeyHasALabel();

  // The wording is check.sh's, not a preference: the gate counts sub-suites with
  // grep -c "checks, 0 failed", so a suite that says "failures" runs, passes and
  // is silently left out of the tally -- which looks exactly like a suite nobody
  // ever added. host-tests/checksh enforces it.
  std::printf("%s: %d checks, %d failed\n", failures ? "FAILED" : "ok", checks, failures);
  return failures ? 1 : 0;
}
