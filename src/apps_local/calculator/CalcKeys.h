#pragma once

// What a calculator key IS, independent of where it sits or how it is drawn.
//
// Freestanding: no renderer, no FreeInkUI, no Arduino. The engine consumes
// these, the layouts arrange them and the activity draws them, so a key's
// meaning has exactly one definition and the three layers cannot disagree
// about it.

#include <cstdint>

namespace calc {

enum class Key : uint8_t {
  None = 0,  // an empty cell, or the continuation cell of a wide key
  D0,
  D1,
  D2,
  D3,
  D4,
  D5,
  D6,
  D7,
  D8,
  D9,
  Dot,
  Add,
  Sub,
  Mul,
  Div,
  Equals,
  Percent,
  Negate,      // +/-
  ClearAll,    // AC / C: everything, including a pending operator
  ClearEntry,  // CE: the number being typed, nothing else
  Backspace,   // one digit off the number being typed
  Sqrt,
  Square,
  Reciprocal,  // 1/x
  Pi,
  Euler,  // e
  Ln,
  Log10,
  Sin,
  Cos,
  Tan,
  Power,  // x^y
  Exp10,  // 10^x
  Factorial,
  Abs,
  Mod,
  LParen,
  RParen,
  Second,  // the shift that swaps a pad's second functions in
  Answer,  // the previous result, as an operand
  MemClear,
  MemRecall,
  MemAdd,
  MemSub,
};

// One key: what it means, what it says, and whether it is one of the five that
// carry the sum rather than the number.
//
// `label` is nullptr for exactly one key, plus-minus, because not every face
// can spell it: Jersey 25 has no U+00B1 and a glyph the face lacks draws as a
// HOLE rather than a box. The skin supplies "+/-" there and the real sign
// everywhere else, which is two correct calculator conventions rather than one
// convention and one invisible key.
struct KeyDef {
  Key key = Key::None;
  const char* label = nullptr;
  bool emphasis = false;  // an operator or equals: the keys a skin sets apart
};

inline bool isDigit(const Key k) { return k >= Key::D0 && k <= Key::D9; }
inline int digitValue(const Key k) { return static_cast<int>(k) - static_cast<int>(Key::D0); }
inline bool isBinaryOp(const Key k) {
  return k == Key::Add || k == Key::Sub || k == Key::Mul || k == Key::Div || k == Key::Power || k == Key::Mod;
}

}  // namespace calc
