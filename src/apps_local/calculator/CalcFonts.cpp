#include "CalcFonts.h"

#include "../ui/ToyboxFonts.h"
#include "fonts/calc_jersey_20.h"
#include "fonts/calc_jersey_28.h"
#include "fonts/calc_jersey_34.h"
#include "fonts/calc_jersey_44.h"
#include "fonts/calc_jersey_56.h"
#include "fonts/calc_serif_16.h"
#include "fonts/calc_serif_26.h"
#include "fonts/calc_serif_34.h"
#include "fonts/calc_serif_44.h"
#include "fonts/calc_serif_56.h"
#include "fonts/calc_ubuntu_18.h"
#include "fonts/calc_ubuntu_26.h"
#include "fonts/calc_ubuntu_34.h"
#include "fonts/calc_ubuntu_44.h"
#include "fonts/calc_ubuntu_56.h"

namespace calc {
namespace {

// All six converted at 1 BIT, like the Toybox cuts and for the same reason:
// GfxRenderer's BW path paints a pixel for ANY coverage above zero, so an
// antialiased cut floods -- stems fatten, counters close, and the type turns to
// mush. The two grotesques get --force-autohint as well, which is what keeps a
// 26px stem from going ragged at one bit.
EpdFont jerseyLabel(&calc_jersey_28);
EpdFont jerseyNumber(&calc_jersey_56);
EpdFont ubuntuLabel(&calc_ubuntu_26);
EpdFont ubuntuNumber(&calc_ubuntu_56);
EpdFont serifLabel(&calc_serif_26);
EpdFont serifNumber(&calc_serif_56);
EpdFont jerseySmall(&calc_jersey_20);
EpdFont ubuntuSmall(&calc_ubuntu_18);
EpdFont serifSmall(&calc_serif_16);
EpdFont jerseyMid(&calc_jersey_44);
EpdFont jerseyTiny(&calc_jersey_34);
EpdFont ubuntuMid(&calc_ubuntu_44);
EpdFont ubuntuTiny(&calc_ubuntu_34);
EpdFont serifMid(&calc_serif_44);
EpdFont serifTiny(&calc_serif_34);

EpdFontFamily jerseyLabelFamily(&jerseyLabel);
EpdFontFamily jerseyNumberFamily(&jerseyNumber);
EpdFontFamily ubuntuLabelFamily(&ubuntuLabel);
EpdFontFamily ubuntuNumberFamily(&ubuntuNumber);
EpdFontFamily serifLabelFamily(&serifLabel);
EpdFontFamily serifNumberFamily(&serifNumber);
EpdFontFamily jerseySmallFamily(&jerseySmall);
EpdFontFamily ubuntuSmallFamily(&ubuntuSmall);
EpdFontFamily serifSmallFamily(&serifSmall);
EpdFontFamily jerseyMidFamily(&jerseyMid);
EpdFontFamily jerseyTinyFamily(&jerseyTiny);
EpdFontFamily ubuntuMidFamily(&ubuntuMid);
EpdFontFamily ubuntuTinyFamily(&ubuntuTiny);
EpdFontFamily serifMidFamily(&serifMid);
EpdFontFamily serifTinyFamily(&serifTiny);

bool registered = false;

const EpdFontFamily* familyFor(const int fontId) {
  switch (fontId) {
    case kJerseyLabelFontId: return &jerseyLabelFamily;
    case kJerseyNumberFontId: return &jerseyNumberFamily;
    case kUbuntuLabelFontId: return &ubuntuLabelFamily;
    case kUbuntuNumberFontId: return &ubuntuNumberFamily;
    case kSerifLabelFontId: return &serifLabelFamily;
    case kSerifNumberFontId: return &serifNumberFamily;
    case kJerseySmallFontId: return &jerseySmallFamily;
    case kUbuntuSmallFontId: return &ubuntuSmallFamily;
    case kSerifSmallFontId: return &serifSmallFamily;
    case kJerseyMidFontId: return &jerseyMidFamily;
    case kJerseyTinyFontId: return &jerseyTinyFamily;
    case kUbuntuMidFontId: return &ubuntuMidFamily;
    case kUbuntuTinyFontId: return &ubuntuTinyFamily;
    case kSerifMidFontId: return &serifMidFamily;
    case kSerifTinyFontId: return &serifTinyFamily;
    default: return nullptr;
  }
}

const EpdFontData* dataFor(const int fontId) {
  switch (fontId) {
    case kJerseyLabelFontId: return &calc_jersey_28;
    case kJerseyNumberFontId: return &calc_jersey_56;
    case kUbuntuLabelFontId: return &calc_ubuntu_26;
    case kUbuntuNumberFontId: return &calc_ubuntu_56;
    case kSerifLabelFontId: return &calc_serif_26;
    case kSerifNumberFontId: return &calc_serif_56;
    case kJerseySmallFontId: return &calc_jersey_20;
    case kUbuntuSmallFontId: return &calc_ubuntu_18;
    case kSerifSmallFontId: return &calc_serif_16;
    case kJerseyMidFontId: return &calc_jersey_44;
    case kJerseyTinyFontId: return &calc_jersey_34;
    case kUbuntuMidFontId: return &calc_ubuntu_44;
    case kUbuntuTinyFontId: return &calc_ubuntu_34;
    case kSerifMidFontId: return &calc_serif_44;
    case kSerifTinyFontId: return &calc_serif_34;
    default: return nullptr;
  }
}

}  // namespace

void ensureCalcFonts(GfxRenderer& renderer) {
  if (registered) return;
  renderer.insertFont(kJerseyLabelFontId, jerseyLabelFamily);
  renderer.insertFont(kJerseyNumberFontId, jerseyNumberFamily);
  renderer.insertFont(kUbuntuLabelFontId, ubuntuLabelFamily);
  renderer.insertFont(kUbuntuNumberFontId, ubuntuNumberFamily);
  renderer.insertFont(kSerifLabelFontId, serifLabelFamily);
  renderer.insertFont(kSerifNumberFontId, serifNumberFamily);
  renderer.insertFont(kJerseySmallFontId, jerseySmallFamily);
  renderer.insertFont(kUbuntuSmallFontId, ubuntuSmallFamily);
  renderer.insertFont(kSerifSmallFontId, serifSmallFamily);
  renderer.insertFont(kJerseyMidFontId, jerseyMidFamily);
  renderer.insertFont(kJerseyTinyFontId, jerseyTinyFamily);
  renderer.insertFont(kUbuntuMidFontId, ubuntuMidFamily);
  renderer.insertFont(kUbuntuTinyFontId, ubuntuTinyFamily);
  renderer.insertFont(kSerifMidFontId, serifMidFamily);
  renderer.insertFont(kSerifTinyFontId, serifTinyFamily);
  registered = true;
}

Metrics metricsFor(const int fontId) {
  const EpdFontData* data = dataFor(fontId);
  const EpdFontFamily* family = familyFor(fontId);
  if (!data || !family) {
    // A Toybox id: hand it back to the owner of those cuts rather than keeping
    // a second table that can drift from it.
    const toybox::FontMetrics m = toybox::metricsFor(fontId);
    return Metrics{m.ascender, m.capTop, m.capHeight, m.ascender * 2};
  }
  Metrics metrics;
  metrics.ascender = data->ascender;
  metrics.lineHeight = data->advanceY;
  // '8' rather than 'H': the number cuts carry no letters at all, and a digit
  // is the flat-topped shape these faces actually align to on a display.
  const EpdGlyph* glyph = family->getGlyph('8');
  if (glyph == nullptr) glyph = family->getGlyph('H');
  if (glyph != nullptr) {
    metrics.capTop = glyph->top;
    metrics.capHeight = glyph->height;
  }
  return metrics;
}

void drawCapsCentered(const GfxRenderer& renderer, const int fontId, const int x, const int boxY, const int boxH,
                      const char* text, const bool black) {
  const Metrics m = metricsFor(fontId);
  // Ink top on screen is (y + ascender) - capTop. Solve for the y that puts ink
  // top at the box's centred position.
  const int y = boxY + (boxH - m.capHeight) / 2 - m.ascender + m.capTop;
  renderer.drawText(fontId, x, y, text, black);
}

void drawCapsCenteredIn(const GfxRenderer& renderer, const int fontId, const int boxX, const int boxW, const int boxY,
                        const int boxH, const char* text, const bool black) {
  const int w = renderer.getTextWidth(fontId, text);
  drawCapsCentered(renderer, fontId, boxX + (boxW - w) / 2, boxY, boxH, text, black);
}

}  // namespace calc
