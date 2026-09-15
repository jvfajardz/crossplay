#pragma once

// The vertical facts about each cut, freestanding.
//
// They are HERE, as constants, rather than read from the font at layout time,
// because the display's height is DERIVED from them -- it is the pending line
// plus the number's band plus the rule, and nothing else -- and a layout derived
// from a number the host suite cannot see is a layout the host suite cannot
// check. Toybox solves the same problem the same way in ToyboxTokens.h.
//
// They are not a second opinion: host-tests/calculator/label_fit.py checks every
// one against the real generated header, so a regenerated cut cannot silently
// move a band and take the difference out of the key rows.

#include "CalcFontIds.h"

namespace calc {

struct CutMetrics {
  int lineHeight = 0;  // EpdFontData::advanceY
  int capHeight = 0;   // ink height of '8', which is what the eye measures
  // The widest glyph a NUMBER can contain, in whole pixels, rounded up. Jersey's
  // digits are not tabular -- a '1' is 37px against a '0' at 57px in the 56 cut
  // -- so a display budget counted in characters has to count the worst one.
  int widestDigit = 0;
};

constexpr CutMetrics kSmallCut{42, 26, 21};
constexpr CutMetrics kLabelCut{58, 36, 29};
constexpr CutMetrics kFinestCut{54, 33, 27};
constexpr CutMetrics kTinyCut{71, 43, 35};
constexpr CutMetrics kMidCut{92, 57, 45};
constexpr CutMetrics kNumberCut{117, 71, 57};

constexpr CutMetrics cutFor(const int fontId) {
  return fontId == kSmallFontId    ? kSmallCut
         : fontId == kLabelFontId  ? kLabelCut
         : fontId == kFinestFontId ? kFinestCut
         : fontId == kTinyFontId   ? kTinyCut
         : fontId == kMidFontId    ? kMidCut
                                   : kNumberCut;
}

constexpr int numberFontFor(const int rung) {
  return rung <= 0 ? kNumberFontId : (rung == 1 ? kMidFontId : (rung == 2 ? kTinyFontId : kFinestFontId));
}

}  // namespace calc
