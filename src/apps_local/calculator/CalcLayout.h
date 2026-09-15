#pragma once

// The five candidate pads, and the one piece of geometry every layer asks.
//
// Freestanding on purpose. The rects here are what the activity DRAWS and what
// its taps are RESOLVED AGAINST -- one function, called twice, never two
// functions that agree today. Three separate bugs in this fork came from
// computing a hit rect a second time; see docs/building-apps.md.

#include <cstdint>

#include "CalcKeys.h"

namespace calc {

// What the top of the screen shows.
enum class DisplayKind : uint8_t {
  Single,      // one number, the way a pocket calculator shows one number
  Tape,        // the last few completed sums above the live one
  Expression,  // the whole sum as typed, with its result under it
};

struct Layout {
  const char* name = nullptr;
  uint8_t cols = 0;
  uint8_t rows = 0;
  const KeyDef* keys = nullptr;  // exactly cols * rows entries, row-major
  DisplayKind display = DisplayKind::Single;
  int16_t displayHeight = 0;
  uint8_t tapeLines = 0;
};

struct Rect16 {
  int16_t x = 0, y = 0, w = 0, h = 0;
  int16_t right() const { return static_cast<int16_t>(x + w); }
  int16_t bottom() const { return static_cast<int16_t>(y + h); }
};

// The gap between keys. One number, so the pads stay a family.
constexpr int16_t kKeyGap = 12;

struct PadGeom {
  Rect16 display{};
  Rect16 grid{};
  int16_t cellW = 0;
  int16_t cellH = 0;
};

// `body` is the activity's content rect: below the chrome, inside the margins.
inline PadGeom padGeom(const Layout& layout, const Rect16& body) {
  PadGeom g{};
  g.display = Rect16{body.x, body.y, body.w, layout.displayHeight};
  const int16_t gridTop = static_cast<int16_t>(body.y + layout.displayHeight + kKeyGap);
  g.grid = Rect16{body.x, gridTop, body.w, static_cast<int16_t>(body.bottom() - gridTop)};
  if (layout.cols == 0 || layout.rows == 0) return g;
  g.cellW = static_cast<int16_t>((g.grid.w - (layout.cols - 1) * kKeyGap) / layout.cols);
  g.cellH = static_cast<int16_t>((g.grid.h - (layout.rows - 1) * kKeyGap) / layout.rows);
  return g;
}

// The rect of the key at `index` into layout.keys. A key wider than one column
// swallows the gap it spans as well, which is why the width is not cellW * span.
inline Rect16 keyRect(const Layout& layout, const PadGeom& g, const int index) {
  const int col = index % layout.cols;
  const int row = index / layout.cols;
  const uint8_t span = layout.keys[index].span < 1 ? 1 : layout.keys[index].span;
  return Rect16{static_cast<int16_t>(g.grid.x + col * (g.cellW + kKeyGap)),
                static_cast<int16_t>(g.grid.y + row * (g.cellH + kKeyGap)),
                static_cast<int16_t>(span * g.cellW + (span - 1) * kKeyGap), g.cellH};
}

// Which key a tap landed on, or -1. Derived from keyRect() and nothing else, so
// a layout change cannot move the pixels without moving the hit box with them.
// The gaps between keys are refused rather than rounded into a neighbour: on a
// panel this slow, a tap that did the wrong thing costs more than one that did
// nothing.
inline int keyAt(const Layout& layout, const PadGeom& g, const int x, const int y) {
  const int cells = layout.cols * layout.rows;
  for (int i = 0; i < cells; ++i) {
    if (layout.keys[i].key == Key::None) continue;
    const Rect16 r = keyRect(layout, g, i);
    if (x >= r.x && x < r.right() && y >= r.y && y < r.bottom()) return i;
  }
  return -1;
}

// ---------------------------------------------------------------------------
// The five candidates. Every one is somebody's shipping layout rather than a
// arrangement of my own: the pad a person already knows is the pad they can use
// on a screen that repaints in a second.
// ---------------------------------------------------------------------------

namespace pads {

// 1. PHONE -- the iOS Calculator's portrait pad. Nineteen keys, the widest keys
// of the five, and the fewest functions. The wide zero is the iPhone's.
constexpr KeyDef kPhone[] = {
    {Key::ClearAll, "AC"},   {Key::Negate, nullptr, Sym::PlusMinus}, {Key::Percent, "%"},  {Key::Div, nullptr, Sym::Divide, nullptr, Key::None, 1, true},
    {Key::D7, "7"},          {Key::D8, "8"},                         {Key::D9, "9"},       {Key::Mul, nullptr, Sym::Multiply, nullptr, Key::None, 1, true},
    {Key::D4, "4"},          {Key::D5, "5"},                         {Key::D6, "6"},       {Key::Sub, "-", Sym::None, nullptr, Key::None, 1, true},
    {Key::D1, "1"},          {Key::D2, "2"},                         {Key::D3, "3"},       {Key::Add, "+", Sym::None, nullptr, Key::None, 1, true},
    {Key::D0, "0", Sym::None, nullptr, Key::None, 2}, {Key::None},   {Key::Dot, "."},      {Key::Equals, "=", Sym::None, nullptr, Key::None, 1, true},
};
constexpr Layout kPhoneLayout{"PHONE", 4, 5, kPhone, DisplayKind::Single, 120, 0};

// 2. DESKTOP -- Windows Calculator's Standard pad, key for key. Twenty-four
// keys: the four-function set plus the five a pocket calculator has always had
// (CE, backspace, reciprocal, square, root) and nothing that needs a parser.
constexpr KeyDef kDesktop[] = {
    {Key::Percent, "%"},     {Key::ClearEntry, "CE"},  {Key::ClearAll, "C"},   {Key::Backspace, nullptr, Sym::Backspace},
    {Key::Reciprocal, "1/x"},{Key::Square, "x2"},      {Key::Sqrt, nullptr, Sym::Radical}, {Key::Div, nullptr, Sym::Divide, nullptr, Key::None, 1, true},
    {Key::D7, "7"},          {Key::D8, "8"},           {Key::D9, "9"},         {Key::Mul, nullptr, Sym::Multiply, nullptr, Key::None, 1, true},
    {Key::D4, "4"},          {Key::D5, "5"},           {Key::D6, "6"},         {Key::Sub, "-", Sym::None, nullptr, Key::None, 1, true},
    {Key::D1, "1"},          {Key::D2, "2"},           {Key::D3, "3"},         {Key::Add, "+", Sym::None, nullptr, Key::None, 1, true},
    {Key::Negate, nullptr, Sym::PlusMinus}, {Key::D0, "0"}, {Key::Dot, "."},   {Key::Equals, "=", Sym::None, nullptr, Key::None, 1, true},
};
constexpr Layout kDesktopLayout{"DESKTOP", 4, 6, kDesktop, DisplayKind::Single, 120, 0};

// 3. TAPE -- the PHONE pad with the panel's own advantage taken: the sums you
// already finished stay on screen. Paper-tape calculators did this and e-ink is
// the one display where keeping them costs nothing to hold.
constexpr Layout kTapeLayout{"TAPE", 4, 5, kPhone, DisplayKind::Tape, 250, 4};

// 4. SCIENTIFIC -- Windows Calculator's Scientific pad, thirty-five keys, and
// the smallest keys of the five. Everything a scientific calculator has, which
// is the question this render exists to answer: whether that is worth 80x67
// instead of 103x89.
constexpr KeyDef kScientific[] = {
    {Key::Second, "2nd"},    {Key::Pi, "pi"},          {Key::Euler, "e"},      {Key::ClearAll, "C"},   {Key::Backspace, nullptr, Sym::Backspace},
    {Key::Square, "x2"},     {Key::Reciprocal, "1/x"}, {Key::Abs, "|x|"},      {Key::Exp10, "exp"},    {Key::Mod, "mod"},
    {Key::Sqrt, nullptr, Sym::Radical}, {Key::LParen, "("}, {Key::RParen, ")"},{Key::Factorial, "n!"}, {Key::Div, nullptr, Sym::Divide, nullptr, Key::None, 1, true},
    {Key::Power, "x^y"},     {Key::D7, "7"},           {Key::D8, "8"},         {Key::D9, "9"},         {Key::Mul, nullptr, Sym::Multiply, nullptr, Key::None, 1, true},
    {Key::Sin, "sin"},       {Key::D4, "4"},           {Key::D5, "5"},         {Key::D6, "6"},         {Key::Sub, "-", Sym::None, nullptr, Key::None, 1, true},
    {Key::Cos, "cos"},       {Key::D1, "1"},           {Key::D2, "2"},         {Key::D3, "3"},         {Key::Add, "+", Sym::None, nullptr, Key::None, 1, true},
    {Key::Tan, "tan"},       {Key::Negate, nullptr, Sym::PlusMinus}, {Key::D0, "0"}, {Key::Dot, "."},  {Key::Equals, "=", Sym::None, nullptr, Key::None, 1, true},
};
constexpr Layout kScientificLayout{"SCIENTIFIC", 5, 7, kScientific, DisplayKind::Single, 104, 0};

// 5. EXPRESSION -- Casio's natural entry. You type the whole sum, you can see
// it, you can back over a mistake, and = evaluates it with brackets and
// precedence. The only one of the five that needs a parser under it.
constexpr KeyDef kExpression[] = {
    {Key::ClearAll, "AC"},   {Key::LParen, "("},       {Key::RParen, ")"},     {Key::Backspace, nullptr, Sym::Backspace}, {Key::Div, nullptr, Sym::Divide, nullptr, Key::None, 1, true},
    {Key::D7, "7"},          {Key::D8, "8"},           {Key::D9, "9"},         {Key::Percent, "%"},    {Key::Mul, nullptr, Sym::Multiply, nullptr, Key::None, 1, true},
    {Key::D4, "4"},          {Key::D5, "5"},           {Key::D6, "6"},         {Key::Power, "x^y"},    {Key::Sub, "-", Sym::None, nullptr, Key::None, 1, true},
    {Key::D1, "1"},          {Key::D2, "2"},           {Key::D3, "3"},         {Key::Sqrt, nullptr, Sym::Radical}, {Key::Add, "+", Sym::None, nullptr, Key::None, 1, true},
    {Key::D0, "0"},          {Key::Dot, "."},          {Key::Negate, nullptr, Sym::PlusMinus}, {Key::Answer, "ANS"}, {Key::Equals, "=", Sym::None, nullptr, Key::None, 1, true},
};
constexpr Layout kExpressionLayout{"EXPRESSION", 5, 5, kExpression, DisplayKind::Expression, 180, 0};

}  // namespace pads

// The candidate this build was compiled with. Temporary: four of these five go
// away in the commit that picks one.
#ifndef CALC_VARIANT
#define CALC_VARIANT 1
#endif

inline const Layout& activeLayout() {
#if CALC_VARIANT == 1
  return pads::kPhoneLayout;
#elif CALC_VARIANT == 2
  return pads::kDesktopLayout;
#elif CALC_VARIANT == 3
  return pads::kTapeLayout;
#elif CALC_VARIANT == 4
  return pads::kScientificLayout;
#else
  return pads::kExpressionLayout;
#endif
}

}  // namespace calc
