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
  Negate,       // +/-
  ClearAll,     // AC / C: everything, including a pending operator
  ClearEntry,   // CE: the number being typed, nothing else
  Backspace,    // one digit off the number being typed
  Sqrt,
  Square,
  Reciprocal,   // 1/x
  Pi,
  Euler,        // e
  Ln,
  Log10,
  Sin,
  Cos,
  Tan,
  Power,        // x^y
  Exp10,        // 10^x
  Factorial,
  Abs,
  Mod,
  LParen,
  RParen,
  Second,       // the shift that swaps a pad's second functions in
  Answer,       // the previous result, as an operand
  MemClear,
  MemRecall,
  MemAdd,
  MemSub,
};

// A symbol the Toybox cuts cannot spell.
//
// The cuts are Jersey 25 converted from an ASCII-only TTF: toybox_14, _20, _30,
// _44 and _64 carry U+0020..U+007E and NOTHING ELSE (toybox_10 alone has
// Latin-1). So the divide sign, the multiplication sign, the plus-minus sign,
// the radical and a backspace arrow are not "a character we should be careful
// with" -- they are characters that draw as empty space, silently, because a
// glyph the face lacks is a hole rather than a box. These five are drawn from
// primitives instead, at the ink weight of the cut beside them.
enum class Sym : uint8_t {
  None = 0,  // the label is ASCII and the font can spell it
  Divide,
  Multiply,
  PlusMinus,
  Radical,
  Backspace,
};

struct KeyDef {
  Key key = Key::None;
  const char* label = nullptr;  // ASCII only; nullptr when `sym` carries it
  Sym sym = Sym::None;
  const char* second = nullptr;  // what 2nd turns this key into, printed small
  Key secondKey = Key::None;
  uint8_t span = 1;  // how many columns the key covers (the iPhone's wide zero)
  bool emphasis = false;  // drawn filled: the operator column and =
};

inline bool isDigit(const Key k) { return k >= Key::D0 && k <= Key::D9; }
inline int digitValue(const Key k) { return static_cast<int>(k) - static_cast<int>(Key::D0); }
inline bool isBinaryOp(const Key k) {
  return k == Key::Add || k == Key::Sub || k == Key::Mul || k == Key::Div || k == Key::Power || k == Key::Mod;
}

}  // namespace calc
