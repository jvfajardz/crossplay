#pragma once

// Five finished looks for one calculator.
//
// Not five feature sets: the pad, the arithmetic and the hit testing are the
// same underneath all five. What differs is the visual system -- ground, chrome,
// key shape, how an operator is told apart, how the number is presented, and the
// typeface, which is the element that changes a screen most and the one a 1-bit
// panel renders most honestly.
//
// NO RADII ANYWHERE. Mario's call, 2026-09-15: a rounded key is not one of the
// five looks, it is a house style applied to all of them, and it was flattening
// the differences the five exist to show. Weight, fill, lattice, inset and
// typography are what separate them now.
//
// Freestanding data, and that matters more here than it looks: every vertical
// number below is DERIVED from the cut metrics rather than picked, so the host
// suite computes the same layout the panel draws. The unexplained white band
// over every number in the first pass was a guessed `displayHeight` and nothing
// else.

#include <cstdint>

#include "CalcCutMetrics.h"
#include "CalcFontIds.h"
#include "CalcLayout.h"

namespace calc {

// White paper with black ink, or the other way round. Everything a skin draws
// resolves through this: there is no second place that decides a colour.
enum class Ground : uint8_t { Paper, Ink };

// What sits above the pad and says which app this is.
enum class Chrome : uint8_t {
  Band,   // the fork's black header band, full bleed, white title
  Plate,  // a machined strip: heavy rule, title left, a model line right
  Rule,   // a thin rule and a small title, and nothing else
};

enum class KeyShape : uint8_t {
  Outline,  // a bordered rectangle with a gap around it
  Tile,     // a bordered rectangle with NO gap: the pad is one milled part
  Lattice,  // no key border at all: one grid across the whole pad
};

// How the five keys that carry the sum are told apart from the twenty that
// carry the number. Weights have to differ enough to READ as different -- at
// equal weight they compete and neither wins.
enum class Emphasis : uint8_t {
  Weight,  // a heavier border
  Fill,    // solid ground, reversed glyph
  Rule,    // a heavier rule down the column's edge; the lattice answer
};

enum class DisplayStyle : uint8_t {
  Bare,   // the number, flush right, closed by a rule
  Inset,  // a bordered panel the number sits inside, like an instrument's glass
  Tape,   // the sums already finished, above the live one
};

// The air above and below the number inside its own band. One number, so the
// five skins breathe alike even where nothing else about them matches.
constexpr int16_t kNumberAir = 10;

struct Skin {
  const char* name = nullptr;
  const char* subtitle = nullptr;
  Ground ground = Ground::Paper;
  Chrome chrome = Chrome::Band;
  KeyShape shape = KeyShape::Outline;
  Emphasis emphasis = Emphasis::Weight;
  DisplayStyle display = DisplayStyle::Bare;

  int16_t margin = 16;   // side margin for the body
  int16_t gap = 12;      // between keys; Tile and Lattice want 0
  int16_t stroke = 2;    // a digit key's border
  int16_t opStroke = 5;  // an operator key's, when emphasis is Weight
  int16_t chromeH = 76;  // the band, plate or rule strip
  bool equalsFilled = true;

  int labelFont = kJerseyLabelFontId;
  int smallLabelFont = kJerseySmallFontId;
  int numberFont = kJerseyNumberFontId;
  int midNumberFont = kJerseyMidFontId;
  int tinyNumberFont = kJerseyTinyFontId;
  int titleFont = kJerseyLabelFontId;

  // Written by the skin, because the face decides: Jersey 25 has no U+00B1.
  const char* plusMinus = "+/-";
  uint8_t tapeLines = 0;
};

// What the display is TALL, derived from what it holds and nothing else.
//
// This is the whole fix for "the spacing is off". Every skin used to carry a
// guessed height, the number was pinned to the bottom of it, and the difference
// came out as a band of empty panel over every result -- different in each skin,
// explained by nothing. Now: the pending line's own line height, the number's
// own cap plus its air, the rule, and for a tape its lines. Nothing left over.
constexpr int16_t displayHeightFor(const Skin& skin) {
  const int16_t pending = static_cast<int16_t>(cutFor(skin.smallLabelFont).lineHeight);
  const int16_t number = static_cast<int16_t>(cutFor(skin.numberFont).capHeight + 2 * kNumberAir);
  const int16_t rule = 3;
  int16_t tape = 0;
  if (skin.display == DisplayStyle::Tape) {
    tape = static_cast<int16_t>(skin.tapeLines * cutFor(skin.smallLabelFont).lineHeight + 8);
  }
  // An inset panel pays for its own border and the air inside it, twice.
  const int16_t frame = skin.display == DisplayStyle::Inset ? 28 : 0;
  return static_cast<int16_t>(tape + pending + number + rule + frame);
}

namespace skins {

// 1. TOYBOX -- the fork's own language, which is what every other app on this
// shelf speaks: the black header band, Jersey 25, and one filled key. Square
// keys with a light border, and the operator column carried by weight alone.
constexpr Skin kToybox{
    "TOYBOX", nullptr, Ground::Paper, Chrome::Band, KeyShape::Outline, Emphasis::Weight, DisplayStyle::Bare,
    16, 12, 2, 6, 76, true,
    kJerseyLabelFontId, kJerseySmallFontId, kJerseyNumberFontId, kJerseyMidFontId, kJerseyTinyFontId,
    kJerseyLabelFontId, "+/-", 0,
};

// 2. INSTRUMENT -- a machine. No band: a plate with a model line, a display
// recessed behind a border, and keys that tile edge to edge with no daylight
// between them, so the pad reads as one milled part rather than twenty buttons.
// The operator column is solid, the way a function key is dark on real
// equipment, separated by rules in the ground colour so five filled keys do not
// merge into one bar.
constexpr Skin kInstrument{
    "INSTRUMENT", "12 DIGIT", Ground::Paper, Chrome::Plate, KeyShape::Tile, Emphasis::Fill, DisplayStyle::Inset,
    14, 0, 2, 2, 62, true,
    kUbuntuLabelFontId, kUbuntuSmallFontId, kUbuntuNumberFontId, kUbuntuMidFontId, kUbuntuTinyFontId,
    kUbuntuSmallFontId, "\xC2\xB1", 0,
};

// 3. NIGHT -- the same panel, inverted. E-ink holds black with the power off
// just as happily as white, and a calculator is one of the few screens where the
// ink budget survives it: a pad never animates, so the black is paid once.
// Operators come back as solid WHITE, which is the only emphasis left once the
// ground is already ink.
constexpr Skin kNight{
    "NIGHT", nullptr, Ground::Ink, Chrome::Rule, KeyShape::Outline, Emphasis::Fill, DisplayStyle::Bare,
    16, 14, 2, 2, 62, true,
    kUbuntuLabelFontId, kUbuntuSmallFontId, kUbuntuNumberFontId, kUbuntuMidFontId, kUbuntuTinyFontId,
    kUbuntuLabelFontId, "\xC2\xB1", 0,
};

// 4. SWISS -- no key borders at all. One hairline lattice divides the pad, the
// number is flush right and large, and the only heavy marks on the screen are
// the rule under the display and the one down the operator column. The least ink
// of the five, which on this panel is also the crispest.
constexpr Skin kSwiss{
    "SWISS", nullptr, Ground::Paper, Chrome::Rule, KeyShape::Lattice, Emphasis::Rule, DisplayStyle::Bare,
    20, 0, 1, 5, 54, true,
    kJerseyLabelFontId, kJerseySmallFontId, kJerseyNumberFontId, kJerseyMidFontId, kJerseyTinyFontId,
    kJerseySmallFontId, "+/-", 0,
};

// 5. LEDGER -- paper rather than machine. A serif, wide gutters, hairline keys,
// and the panel's own advantage taken: the sums you already finished stay on
// screen, the way a paper tape kept them. Nothing else on this device can hold
// three lines of arithmetic for nothing.
//
// THREE lines, not four: at four the derived display height left 55px key rows,
// and the touch floor is 61. That is the derivation doing its job -- a guessed
// height would have taken the space from the pad without saying so.
constexpr Skin kLedger{
    "LEDGER", "WITH TAPE", Ground::Paper, Chrome::Rule, KeyShape::Outline, Emphasis::Weight, DisplayStyle::Tape,
    22, 12, 1, 4, 58, false,
    kSerifLabelFontId, kSerifSmallFontId, kSerifNumberFontId, kSerifMidFontId, kSerifTinyFontId,
    kSerifLabelFontId, "\xC2\xB1", 3,
};

}  // namespace skins

// The skin this build was compiled with. Temporary: four of these five go away
// in the commit that picks one.
#ifndef CALC_SKIN
#define CALC_SKIN 1
#endif

inline const Skin& activeSkin() {
#if CALC_SKIN == 1
  return skins::kToybox;
#elif CALC_SKIN == 2
  return skins::kInstrument;
#elif CALC_SKIN == 3
  return skins::kNight;
#elif CALC_SKIN == 4
  return skins::kSwiss;
#else
  return skins::kLedger;
#endif
}

// Every skin lays out from here, so "where does the pad start" has one answer.
// The gutter under the chrome is the small cut's own line height rather than a
// round number, so a skin set in a big face gets proportionally more air and a
// skin set in a small one does not float.
inline Rect16 bodyFor(const Skin& skin, const int screenW, const int screenH) {
  const int16_t top = static_cast<int16_t>(skin.chromeH + cutFor(skin.smallLabelFont).lineHeight / 2);
  return Rect16{skin.margin, top, static_cast<int16_t>(screenW - 2 * skin.margin),
                static_cast<int16_t>(screenH - skin.margin - top)};
}

inline PadGeom geomFor(const Skin& skin, const int screenW, const int screenH) {
  return padGeom(kPad, bodyFor(skin, screenW, screenH), skin.gap, displayHeightFor(skin));
}

inline const char* labelFor(const Skin& skin, const KeyDef& key) {
  return key.label ? key.label : skin.plusMinus;
}

// The display's step-down ladder, largest first. Five rungs, because four are
// not enough: the widest result this engine can produce is fifteen characters,
// and the headline cut draws that at nearly twice the panel.
inline int numberFontFor(const Skin& skin, const int rung) {
  switch (rung) {
    case 0: return skin.numberFont;
    case 1: return skin.midNumberFont;
    case 2: return skin.tinyNumberFont;
    case 3: return skin.labelFont;
    default: return skin.smallLabelFont;
  }
}
constexpr int kNumberRungs = 5;

// Which cut a label is drawn in. Structural rather than measured, so the host
// gate can resolve the same face the panel will: one or two BYTES is a digit, an
// operator or a two-letter word, and anything longer is a word key. The math
// signs are two bytes of UTF-8 each, which puts them where they belong -- with
// the digits, at full size.
inline int labelFontFor(const Skin& skin, const char* label) {
  int bytes = 0;
  for (const char* p = label; *p && bytes < 3; ++p) ++bytes;
  return bytes > 2 ? skin.smallLabelFont : skin.labelFont;
}

}  // namespace calc
