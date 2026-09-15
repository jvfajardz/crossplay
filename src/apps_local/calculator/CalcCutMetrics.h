#pragma once

// The vertical facts about each calculator cut, freestanding.
//
// They are HERE, as constants, rather than read from the font at layout time,
// because the display's height is DERIVED from them -- it is the pending line
// plus the number's band plus the rule, and nothing else -- and a layout derived
// from a number the host suite cannot see is a layout the host suite cannot
// check. Toybox solves the same problem the same way in ToyboxTokens.h.
//
// They are not a second opinion: ensureCalcFonts() checks every one of them
// against the real EpdFontData and logs loudly on a mismatch, so a regenerated
// cut cannot silently move a screen the way a regenerated toybox_20 would.

#include "CalcFontIds.h"

namespace calc {

struct CutMetrics {
  int lineHeight = 0;  // EpdFontData::advanceY
  int capHeight = 0;   // ink height of '8', which is what the eye measures
};

// Measured from the generated headers by tools_local/toybox/gen_calc_fonts.sh.
constexpr CutMetrics kJerseySmallCut{42, 26};
constexpr CutMetrics kJerseyLabelCut{58, 36};
constexpr CutMetrics kJerseyTinyCut{71, 43};
constexpr CutMetrics kJerseyMidCut{92, 57};
constexpr CutMetrics kJerseyNumberCut{117, 71};
constexpr CutMetrics kUbuntuSmallCut{43, 28};
constexpr CutMetrics kUbuntuLabelCut{62, 39};
constexpr CutMetrics kUbuntuTinyCut{81, 51};
constexpr CutMetrics kUbuntuMidCut{105, 67};
constexpr CutMetrics kUbuntuNumberCut{134, 84};
constexpr CutMetrics kSerifSmallCut{45, 23};
constexpr CutMetrics kSerifLabelCut{74, 41};
constexpr CutMetrics kSerifTinyCut{96, 53};
constexpr CutMetrics kSerifMidCut{125, 67};
constexpr CutMetrics kSerifNumberCut{159, 86};

constexpr CutMetrics cutFor(const int fontId) {
  return fontId == kJerseySmallFontId    ? kJerseySmallCut
         : fontId == kJerseyLabelFontId  ? kJerseyLabelCut
         : fontId == kJerseyTinyFontId   ? kJerseyTinyCut
         : fontId == kJerseyMidFontId    ? kJerseyMidCut
         : fontId == kJerseyNumberFontId ? kJerseyNumberCut
         : fontId == kUbuntuSmallFontId  ? kUbuntuSmallCut
         : fontId == kUbuntuLabelFontId  ? kUbuntuLabelCut
         : fontId == kUbuntuTinyFontId   ? kUbuntuTinyCut
         : fontId == kUbuntuMidFontId    ? kUbuntuMidCut
         : fontId == kUbuntuNumberFontId ? kUbuntuNumberCut
         : fontId == kSerifSmallFontId   ? kSerifSmallCut
         : fontId == kSerifLabelFontId   ? kSerifLabelCut
         : fontId == kSerifTinyFontId    ? kSerifTinyCut
         : fontId == kSerifMidFontId     ? kSerifMidCut
                                         : kSerifNumberCut;
}

}  // namespace calc
