#pragma once

// The calculator itself: what a key press does to the numbers.
//
// Immediate execution, the way every pocket calculator since the 1970s works
// and the way four of the five candidate pads expect: an operator key completes
// the sum so far and shows it, rather than waiting for a closing bracket. The
// EXPRESSION pad is the one that does not, and it needs a parser this does not
// contain.
//
// Freestanding C++17: <cmath>, <cstdio>, <cstring> and nothing else, so
// host-tests can build it with no panel and no Arduino. Everything here is
// somewhere a calculator is commonly WRONG, which is why it is one file with
// one test suite rather than sprinkled through an activity:
//
//   * 0.1 + 0.2 must print 0.3. Binary doubles say 0.30000000000000004, and
//     the fix is not decimal arithmetic -- it is printing at twelve significant
//     digits when the double carries about seventeen. That is what iOS and
//     Windows both do, and it is why their calculators look exact.
//   * `200 + 10 %` is 220 and `200 x 10 %` is 20. Percent is not one operation;
//     it reads the pending operator. Getting this wrong is the single most
//     common calculator bug.
//   * `2 + 3 = = =` is 5, 8, 11. The equals key repeats the last operator and
//     the last operand, forever.
//   * Two operators in a row replace, they do not stack.
//   * Divide by zero says so and then refuses every key but clear, instead of
//     showing `inf` or a blank.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "CalcKeys.h"

namespace calc {

// Twelve significant digits. The double holds about seventeen; showing all of
// them is what puts 0.30000000000000004 on the panel. Twelve is wide enough
// that a real sum is never silently rounded and narrow enough that the number
// fits the display cut without stepping down two sizes.
constexpr int kSignificantDigits = 12;
// What a person may type. Not the same number: the display can show a
// twelve-digit RESULT, and letting someone type thirteen digits that then round
// is worse than refusing the thirteenth keypress.
constexpr int kMaxEntryDigits = 12;
constexpr size_t kTextMax = 32;

// Twelve-significant-digit rendering, trailing zeros and all, with the C
// library doing the hard part. %g already picks fixed or scientific by
// magnitude and strips trailing zeros; what it does not do is normalise the
// exponent width, which differs between platforms (e+20 against e+020).
inline void formatNumber(const double v, char* out, const size_t n) {
  if (!out || n == 0) return;
  if (std::isnan(v) || std::isinf(v)) {
    std::snprintf(out, n, "Error");
    return;
  }
  // -0 is a real double and reads as a bug on a calculator panel.
  const double value = (v == 0.0) ? 0.0 : v;
  std::snprintf(out, n, "%.*g", kSignificantDigits, value);
  char* e = std::strchr(out, 'e');
  if (!e) return;
  const bool negExp = e[1] == '-';
  const char* digits = e + ((e[1] == '+' || e[1] == '-') ? 2 : 1);
  while (digits[0] == '0' && digits[1] != '\0') ++digits;
  char tail[kTextMax];
  std::snprintf(tail, sizeof(tail), "e%s%s", negExp ? "-" : "", digits);
  std::snprintf(e, n - static_cast<size_t>(e - out), "%s", tail);
}

class Engine {
 public:
  Engine() { clearAll(); }

  // --- what the screen asks -------------------------------------------------

  // The big number.
  const char* display() const { return display_; }
  // The sum so far, small, above it: "12 +" while an operator is pending.
  const char* pending() const { return pending_; }
  bool hasError() const { return error_; }
  // The finished sums, newest last. Only the TAPE pad draws these; the others
  // record them anyway, because which pad is on screen is not the engine's
  // business.
  int tapeCount() const { return tapeCount_; }
  const char* tapeLine(const int i) const {
    if (i < 0 || i >= tapeCount_) return "";
    return tape_[(tapeStart_ + i) % kTapeMax];
  }
  double value() const { return entryLive_ ? entryValue() : acc_; }

  // --- what the pad does ----------------------------------------------------

  void press(const Key k) {
    // An error is a wall: only clear gets through. Letting a digit land on top
    // of "Cannot divide by zero" is how a calculator starts lying quietly.
    if (error_ && k != Key::ClearAll && k != Key::ClearEntry) return;

    if (isDigit(k)) return pressDigit(digitValue(k));
    switch (k) {
      case Key::Dot: return pressDot();
      case Key::Add:
      case Key::Sub:
      case Key::Mul:
      case Key::Div: return pressOperator(k);
      case Key::Equals: return pressEquals();
      case Key::Percent: return pressPercent();
      case Key::Negate: return pressNegate();
      case Key::ClearAll: return clearAll();
      case Key::ClearEntry: return clearEntry();
      case Key::Backspace: return pressBackspace();
      case Key::Sqrt: return pressUnary(k);
      case Key::Square: return pressUnary(k);
      case Key::Reciprocal: return pressUnary(k);
      case Key::Abs: return pressUnary(k);
      case Key::Ln: return pressUnary(k);
      case Key::Log10: return pressUnary(k);
      case Key::Exp10: return pressUnary(k);
      case Key::Sin: return pressUnary(k);
      case Key::Cos: return pressUnary(k);
      case Key::Tan: return pressUnary(k);
      case Key::Factorial: return pressUnary(k);
      case Key::Pi: return pressConstant(3.14159265358979323846);
      case Key::Euler: return pressConstant(2.71828182845904523536);
      default: return;  // 2nd, ANS, brackets: the EXPRESSION pad's, not this one
    }
  }

  void clearAll() {
    entry_[0] = '0';
    entry_[1] = '\0';
    entryDigits_ = 0;
    entryLive_ = false;
    entryHasDot_ = false;
    entryNegative_ = false;
    acc_ = 0.0;
    pendingOp_ = Key::None;
    repeatOp_ = Key::None;
    repeatOperand_ = 0.0;
    error_ = false;
    pending_[0] = '\0';
    refresh();
  }

 private:
  static constexpr int kTapeMax = 8;

  void clearEntry() {
    error_ = false;
    entry_[0] = '0';
    entry_[1] = '\0';
    entryDigits_ = 0;
    entryLive_ = false;
    entryHasDot_ = false;
    entryNegative_ = false;
    refresh();
  }

  double entryValue() const {
    const double v = std::atof(entry_);
    return entryNegative_ ? -v : v;
  }

  void setEntryFrom(const double v) {
    formatNumber(v, entry_, sizeof(entry_));
    entryNegative_ = entry_[0] == '-';
    if (entryNegative_) std::memmove(entry_, entry_ + 1, std::strlen(entry_));
    entryHasDot_ = std::strchr(entry_, '.') != nullptr;
    entryDigits_ = 0;
    for (const char* p = entry_; *p; ++p)
      if (*p >= '0' && *p <= '9') ++entryDigits_;
  }

  void pressDigit(const int d) {
    if (!entryLive_) {
      entry_[0] = '\0';
      entryDigits_ = 0;
      entryHasDot_ = false;
      entryNegative_ = false;
      entryLive_ = true;
    }
    // Refusing the thirteenth digit rather than accepting and rounding it: a
    // key that silently does nothing is better than a number that silently
    // changes.
    if (entryDigits_ >= kMaxEntryDigits) return;
    if (entryDigits_ == 0 && d == 0 && !entryHasDot_) {
      std::snprintf(entry_, sizeof(entry_), "0");
      refresh();
      return;
    }
    if (std::strcmp(entry_, "0") == 0 && !entryHasDot_) entry_[0] = '\0';
    const size_t len = std::strlen(entry_);
    if (len + 2 >= sizeof(entry_)) return;
    entry_[len] = static_cast<char>('0' + d);
    entry_[len + 1] = '\0';
    ++entryDigits_;
    refresh();
  }

  void pressDot() {
    if (!entryLive_) {
      std::snprintf(entry_, sizeof(entry_), "0");
      entryDigits_ = 0;
      entryHasDot_ = false;
      entryNegative_ = false;
      entryLive_ = true;
    }
    if (entryHasDot_) return;
    const size_t len = std::strlen(entry_);
    if (len + 2 >= sizeof(entry_)) return;
    entry_[len] = '.';
    entry_[len + 1] = '\0';
    entryHasDot_ = true;
    refresh();
  }

  void pressBackspace() {
    // Only the number being typed. Backspacing a RESULT would have to undo the
    // sum that produced it, and there is no sensible answer to what `5` means
    // after you back a digit off 25 that arrived from 5 x 5.
    if (!entryLive_) return;
    size_t len = std::strlen(entry_);
    if (len == 0) return;
    if (entry_[len - 1] == '.') entryHasDot_ = false;
    if (entry_[len - 1] >= '0' && entry_[len - 1] <= '9' && entryDigits_ > 0) --entryDigits_;
    entry_[len - 1] = '\0';
    if (entry_[0] == '\0') {
      entry_[0] = '0';
      entry_[1] = '\0';
      entryDigits_ = 0;
      entryNegative_ = false;
    }
    refresh();
  }

  void pressNegate() {
    if (entryLive_ || pendingOp_ == Key::None) {
      entryNegative_ = !entryNegative_;
      if (!entryLive_) {
        acc_ = -acc_;
        setEntryFrom(acc_);
      }
    } else {
      entryNegative_ = !entryNegative_;
    }
    refresh();
  }

  void pressConstant(const double v) {
    setEntryFrom(v);
    entryLive_ = true;
    refresh();
  }

  bool apply(const Key op, const double lhs, const double rhs, double& out) {
    switch (op) {
      case Key::Add: out = lhs + rhs; break;
      case Key::Sub: out = lhs - rhs; break;
      case Key::Mul: out = lhs * rhs; break;
      case Key::Div:
        if (rhs == 0.0) return fail("Cannot divide by zero");
        out = lhs / rhs;
        break;
      default: out = rhs; break;
    }
    if (std::isnan(out) || std::isinf(out)) return fail("Overflow");
    return true;
  }

  void pressOperator(const Key op) {
    // Two operators in a row replace rather than stack: the second one is what
    // you meant, and every calculator on every desk behaves this way.
    if (!entryLive_ && pendingOp_ != Key::None) {
      pendingOp_ = op;
      writePending();
      refresh();
      return;
    }
    const double rhs = entryLive_ ? entryValue() : acc_;
    if (pendingOp_ == Key::None) {
      acc_ = rhs;
    } else {
      double out = 0.0;
      if (!apply(pendingOp_, acc_, rhs, out)) return;
      acc_ = out;
    }
    pendingOp_ = op;
    entryLive_ = false;
    setEntryFrom(acc_);
    repeatOp_ = Key::None;
    writePending();
    refresh();
  }

  void pressEquals() {
    double rhs;
    Key op;
    if (pendingOp_ != Key::None) {
      rhs = entryLive_ ? entryValue() : acc_;
      op = pendingOp_;
    } else if (repeatOp_ != Key::None) {
      // `2 + 3 =` then `=` again: repeat the operator AND the operand.
      rhs = repeatOperand_;
      op = repeatOp_;
    } else {
      entryLive_ = false;
      acc_ = entryValue();
      pending_[0] = '\0';
      refresh();
      return;
    }
    char lhsText[kTextMax];
    char rhsText[kTextMax];
    formatNumber(acc_, lhsText, sizeof(lhsText));
    formatNumber(rhs, rhsText, sizeof(rhsText));
    double out = 0.0;
    if (!apply(op, acc_, rhs, out)) return;
    acc_ = out;
    repeatOp_ = op;
    repeatOperand_ = rhs;
    pendingOp_ = Key::None;
    entryLive_ = false;
    setEntryFrom(acc_);
    pending_[0] = '\0';
    char result[kTextMax];
    formatNumber(acc_, result, sizeof(result));
    pushTape(lhsText, opGlyph(op), rhsText, result);
    refresh();
  }

  // `200 + 10 %` is 220 and `200 x 10 %` is 20, because percent reads the
  // pending operator: additive operators want a percentage OF the running
  // total, multiplicative ones want a plain hundredth. This is the rule iOS,
  // Windows and every desk calculator share, and it is the one most
  // reimplementations get wrong.
  void pressPercent() {
    const double x = entryLive_ ? entryValue() : acc_;
    double v;
    if (pendingOp_ == Key::Add || pendingOp_ == Key::Sub) {
      v = acc_ * x / 100.0;
    } else {
      v = x / 100.0;
    }
    setEntryFrom(v);
    entryLive_ = true;
    refresh();
  }

  void pressUnary(const Key k) {
    const double x = entryLive_ ? entryValue() : acc_;
    double v = 0.0;
    switch (k) {
      case Key::Sqrt:
        if (x < 0.0) { fail("Invalid input"); return; }
        v = std::sqrt(x);
        break;
      case Key::Square: v = x * x; break;
      case Key::Reciprocal:
        if (x == 0.0) { fail("Cannot divide by zero"); return; }
        v = 1.0 / x;
        break;
      case Key::Abs: v = std::fabs(x); break;
      case Key::Ln:
        if (x <= 0.0) { fail("Invalid input"); return; }
        v = std::log(x);
        break;
      case Key::Log10:
        if (x <= 0.0) { fail("Invalid input"); return; }
        v = std::log10(x);
        break;
      case Key::Exp10: v = std::pow(10.0, x); break;
      case Key::Sin: v = std::sin(x); break;
      case Key::Cos: v = std::cos(x); break;
      case Key::Tan: v = std::tan(x); break;
      case Key::Factorial: {
        if (x < 0.0 || x != std::floor(x) || x > 170.0) { fail("Invalid input"); return; }
        v = 1.0;
        for (int i = 2; i <= static_cast<int>(x); ++i) v *= i;
        break;
      }
      default: return;
    }
    if (std::isnan(v) || std::isinf(v)) {
      fail("Overflow");
      return;
    }
    setEntryFrom(v);
    entryLive_ = true;
    // The entry is a result now, not something typed: another digit starts a
    // new number rather than extending this one.
    entryLive_ = false;
    acc_ = v;
    refresh();
  }

  bool fail(const char* why) {
    error_ = true;
    std::snprintf(display_, sizeof(display_), "%s", why);
    pending_[0] = '\0';
    return false;
  }

  static const char* opGlyph(const Key op) {
    switch (op) {
      case Key::Add: return "+";
      case Key::Sub: return "-";
      // `x` and `/`, not the multiplication and division signs: the Toybox
      // cuts are ASCII-only and U+00D7 would draw as nothing at all. The KEYS
      // get the real signs because a key is drawn from primitives; a line of
      // text cannot be.
      case Key::Mul: return "x";
      case Key::Div: return "/";
      default: return "?";
    }
  }

  void writePending() {
    char lhs[kTextMax];
    formatNumber(acc_, lhs, sizeof(lhs));
    std::snprintf(pending_, sizeof(pending_), "%s %s", lhs, opGlyph(pendingOp_));
  }

  void pushTape(const char* lhs, const char* op, const char* rhs, const char* result) {
    const int slot = (tapeStart_ + tapeCount_) % kTapeMax;
    std::snprintf(tape_[slot], kTextMax * 2, "%s %s %s = %s", lhs, op, rhs, result);
    if (tapeCount_ < kTapeMax) {
      ++tapeCount_;
    } else {
      tapeStart_ = (tapeStart_ + 1) % kTapeMax;
    }
  }

  void refresh() {
    if (error_) return;
    if (entryLive_) {
      std::snprintf(display_, sizeof(display_), "%s%s", entryNegative_ ? "-" : "", entry_);
    } else {
      formatNumber(entryNegative_ ? -std::atof(entry_) : std::atof(entry_), display_, sizeof(display_));
    }
  }

  char entry_[kTextMax] = {};
  char display_[kTextMax * 2] = {};
  char pending_[kTextMax * 2] = {};
  char tape_[kTapeMax][kTextMax * 2] = {};
  int tapeStart_ = 0;
  int tapeCount_ = 0;
  int entryDigits_ = 0;
  bool entryLive_ = false;
  bool entryHasDot_ = false;
  bool entryNegative_ = false;
  bool error_ = false;
  double acc_ = 0.0;
  Key pendingOp_ = Key::None;
  Key repeatOp_ = Key::None;
  double repeatOperand_ = 0.0;
};

}  // namespace calc
