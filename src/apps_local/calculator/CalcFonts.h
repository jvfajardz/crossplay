#pragma once

// The calculator's own cuts, and the two measurements every skin needs.
//
// New files rather than wider versions of the Toybox cuts, and that is not
// tidiness: tools_local/toybox/gen_toybox_fonts.sh spells out that regenerating
// toybox_20 or _30 today moves every glyph a pixel, because the current freetype
// is not the one that produced the committed headers, and ToyboxTokens.h's
// centring constants are solved against exactly those ink heights. A cut nothing
// else uses cannot shift anybody's text.
//
// What they carry that no Toybox cut can: the Toybox cuts are subset to
// U+0020-007E, so U+00D7, U+00F7, U+2212 and U+00B1 draw as NOTHING -- a glyph
// the face lacks is a hole, not a box. See tools_local/toybox/gen_calc_fonts.sh.

#include <GfxRenderer.h>

#include "CalcFontIds.h"

namespace calc {

struct Metrics {
  int ascender = 0;
  int capTop = 0;
  int capHeight = 0;
  // The face's own advance between baselines. Stacking lines on a literal is
  // how LEDGER's tape came to draw four sums through each other: 32px looked
  // generous beside a 26px cut and is not, because a cut's ink is taller than
  // its nominal size.
  int lineHeight = 0;
};

// Call from onEnter() before drawing. Idempotent.
void ensureCalcFonts(GfxRenderer& renderer);

// Understands the calculator's ids AND Toybox's, because a skin mixes them:
// a Toybox-chromed header is drawn in a Toybox cut whatever the pad is set in.
Metrics metricsFor(int fontId);

// Capital ink centred in [boxY, boxY + boxH), the way a typesetter centres it.
// getTextHeight() reports the ASCENDER and drawText() takes the top of the
// ascender box, so centring on either puts the letters visibly low.
void drawCapsCentered(const GfxRenderer& renderer, int fontId, int x, int boxY, int boxH, const char* text,
                      bool black);

// The same, centred horizontally in a box as well. Every key label wants this
// and every one of them computing it again is how two labels come to disagree.
void drawCapsCenteredIn(const GfxRenderer& renderer, int fontId, int boxX, int boxW, int boxY, int boxH,
                        const char* text, bool black);

}  // namespace calc
