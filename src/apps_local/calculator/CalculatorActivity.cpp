#include "CalculatorActivity.h"

#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;

namespace {

// The cuts a number may be drawn in, largest first. Stepping down is the design
// language's rule for anything that will not fit: pick the largest cut it fits
// in, walk down, and only give up at the smallest.
constexpr int kNumberCuts[] = {toybox::kHugeFontId, toybox::kLargeFontId, toybox::kDisplayFontId,
                               toybox::kUiFontId};
constexpr int kNumberCutCount = static_cast<int>(sizeof(kNumberCuts) / sizeof(kNumberCuts[0]));

// Key outlines. The weights have to differ enough to read as different -- equal
// weights compete and neither wins -- so a digit is 2 and an operator is 5.
constexpr int kDigitStroke = 2;
constexpr int kOperatorStroke = 5;
constexpr int kKeyRadius = 14;

int textCut(const calc::KeyDef& key, const calc::Rect16& r) {
  if (!key.label) return toybox::kDisplayFontId;
  const size_t len = std::strlen(key.label);
  if (len <= 1 && r.h >= 60) return toybox::kDisplayFontId;
  if (len <= 2 && r.h >= 70) return toybox::kDisplayFontId;
  return toybox::kUiFontId;
}

}  // namespace

std::unique_ptr<Activity> CalculatorActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput);
}

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  requestUpdate();
}

// The body rect, taken from the same constants the chrome reserves rather than
// restated: toybox::kBodyTop IS chromeBelow(band) + the top gutter, and a
// second sum here is exactly the drift card 248 paid for.
calc::Rect16 CalculatorActivity::bodyRect() const {
  const int16_t x = static_cast<int16_t>(toybox::kMargin);
  const int16_t y = static_cast<int16_t>(toybox::kBodyTop);
  const int16_t w = static_cast<int16_t>(renderer.getScreenWidth() - 2 * toybox::kMargin);
  const int16_t h = static_cast<int16_t>(renderer.getScreenHeight() - toybox::kMargin - y);
  return calc::Rect16{x, y, w, h};
}

int CalculatorActivity::fitCut(const char* text, const int maxWidth, const int* cuts, const int cutCount) const {
  for (int i = 0; i < cutCount; ++i) {
    if (renderer.getTextWidth(cuts[i], text) <= maxWidth) return cuts[i];
  }
  return cuts[cutCount - 1];
}

// Five glyphs the Toybox cuts cannot spell, drawn from primitives.
//
// toybox_14/_20/_30/_44/_64 are Jersey 25 converted from an ASCII-only TTF:
// U+0020..U+007E and nothing else. So U+00F7, U+00D7, U+00B1, U+221A and any
// backspace arrow are not characters to be careful with, they are characters
// that draw as EMPTY SPACE -- a missing glyph on this renderer is a hole, not a
// box, so the key would simply look blank and nothing would say why. Regenerating
// a cut with more glyphs is the other way out and is worse: a regenerated Toybox
// cut does not reproduce, it moves every glyph's metrics and reflows every screen
// in the fork.
void CalculatorActivity::drawSymbol(const calc::Sym sym, const calc::Rect16& r, const bool ink) {
  const int cx = r.x + r.w / 2;
  const int cy = r.y + r.h / 2;
  // Sized off the key so one routine serves a 103px pad and an 80px one.
  const int arm = std::min(r.w, r.h) / 5;
  const int weight = std::max(3, arm / 4);
  switch (sym) {
    case calc::Sym::Divide: {
      renderer.drawLine(cx - arm, cy, cx + arm, cy, weight, ink);
      const int dot = std::max(3, weight);
      renderer.fillRect(cx - dot / 2, cy - arm + dot / 2, dot, dot, ink);
      renderer.fillRect(cx - dot / 2, cy + arm - dot - dot / 2 + dot, dot, dot, ink);
      break;
    }
    case calc::Sym::Multiply:
      renderer.drawLine(cx - arm, cy - arm, cx + arm, cy + arm, weight, ink);
      renderer.drawLine(cx - arm, cy + arm, cx + arm, cy - arm, weight, ink);
      break;
    case calc::Sym::PlusMinus:
      renderer.drawLine(cx - arm, cy - arm / 2, cx + arm, cy - arm / 2, weight, ink);
      renderer.drawLine(cx, cy - arm - arm / 2, cx, cy + arm / 2 - arm / 2, weight, ink);
      renderer.drawLine(cx - arm, cy + arm, cx + arm, cy + arm, weight, ink);
      break;
    case calc::Sym::Radical: {
      // The tick, the rise, and the bar over what is under the root.
      renderer.drawLine(cx - arm, cy, cx - arm / 2, cy + arm, weight, ink);
      renderer.drawLine(cx - arm / 2, cy + arm, cx + arm / 4, cy - arm, weight, ink);
      renderer.drawLine(cx + arm / 4, cy - arm, cx + arm, cy - arm, weight, ink);
      break;
    }
    case calc::Sym::Backspace: {
      // A left-pointing tag with a cross in it: the shape every keyboard uses.
      const int h = arm;
      const int w = static_cast<int>(arm * 1.8);
      renderer.drawLine(cx - w, cy, cx - w / 3, cy - h, weight, ink);
      renderer.drawLine(cx - w, cy, cx - w / 3, cy + h, weight, ink);
      renderer.drawLine(cx - w / 3, cy - h, cx + w, cy - h, weight, ink);
      renderer.drawLine(cx - w / 3, cy + h, cx + w, cy + h, weight, ink);
      renderer.drawLine(cx + w, cy - h, cx + w, cy + h, weight, ink);
      const int c = h / 2;
      renderer.drawLine(cx - c / 2 + w / 4, cy - c, cx + c / 2 + w / 4, cy + c, weight, ink);
      renderer.drawLine(cx - c / 2 + w / 4, cy + c, cx + c / 2 + w / 4, cy - c, weight, ink);
      break;
    }
    case calc::Sym::None:
      break;
  }
}

void CalculatorActivity::drawKey(const calc::KeyDef& key, const calc::Rect16& r) {
  // `=` is the only filled key. Filling the whole operator column was the other
  // candidate and it fails the ink-budget rule badly: black is for what changes,
  // and a pad's keys never change, so six permanently black slabs would be the
  // largest standing block of ink in the fork for no information at all.
  const bool filled = key.key == calc::Key::Equals;
  const bool ink = !filled;
  if (filled) {
    renderer.fillRoundedRect(r.x, r.y, r.w, r.h, kKeyRadius, Color::Black);
  } else {
    renderer.drawRoundedRect(r.x, r.y, r.w, r.h, key.emphasis ? kOperatorStroke : kDigitStroke, kKeyRadius, true);
  }

  if (key.sym != calc::Sym::None) {
    drawSymbol(key.sym, r, ink);
    return;
  }
  if (!key.label) return;
  const int cut = textCut(key, r);
  const int w = renderer.getTextWidth(cut, key.label);
  toybox::drawCapsCentered(renderer, cut, r.x + (r.w - w) / 2, r.y, r.h, key.label, ink);
}

void CalculatorActivity::drawPad(const calc::PadGeom& g) {
  const calc::Layout& layout = calc::activeLayout();
  const int cells = layout.cols * layout.rows;
  for (int i = 0; i < cells; ++i) {
    if (layout.keys[i].key == calc::Key::None) continue;
    drawKey(layout.keys[i], calc::keyRect(layout, g, i));
  }
}

void CalculatorActivity::drawDisplay(const calc::PadGeom& g) {
  const calc::Layout& layout = calc::activeLayout();
  const calc::Rect16 d = g.display;
  // A hairline under the number, not a box around it: the number is the content
  // and a box would give it the same weight as a key.
  renderer.drawLine(d.x, d.bottom() - 1, d.right(), d.bottom() - 1, toybox::kHairline, true);

  const int lineInset = 10;
  int numberBottom = d.bottom() - lineInset;

  if (layout.display == calc::DisplayKind::Tape) {
    // The sums already finished, oldest at the top, in the small cut. This is
    // the whole argument for this candidate: on a panel that holds an image
    // with the power off, the last four sums cost nothing to keep and are the
    // thing you actually want when you are adding a column of numbers.
    const int lines = std::min<int>(layout.tapeLines, engine.tapeCount());
    const int lh = 30;
    int y = d.y + 4;
    for (int i = 0; i < layout.tapeLines; ++i) {
      const int idx = engine.tapeCount() - lines + i;
      if (i >= lines || idx < 0) {
        y += lh;
        continue;
      }
      const char* text = engine.tapeLine(idx);
      const int w = renderer.getTextWidth(toybox::kUiFontId, text);
      toybox::drawCapsCentered(renderer, toybox::kUiFontId, d.right() - w, y, lh, text, true);
      y += lh;
    }
    // The sum in progress sits with the finished ones, in the same column, so
    // the tape reads as one running list rather than a list plus a separate
    // field that happens to be above the rule.
    if (engine.pending()[0]) {
      const char* p = engine.pending();
      const int w = renderer.getTextWidth(toybox::kUiFontId, p);
      toybox::drawCapsCentered(renderer, toybox::kUiFontId, d.right() - w, y, lh, p, true);
    }
    numberBottom = d.bottom() - lineInset;
  } else if (layout.display == calc::DisplayKind::Expression) {
    // The sum as typed, above its result. Left-aligned, because you read it
    // forwards; the result is right-aligned under it, because you read a number
    // backwards from its units.
    // The sum as typed, left-aligned because you read it forwards, with a
    // caret where the next key lands. The result line under it stays reserved
    // whether or not there is an answer yet: a box that appears when the first
    // = is pressed would move the whole sum up under your finger.
    char line[80];
    std::snprintf(line, sizeof(line), "%s_", expr);
    const int cut = fitCut(line, d.w, kNumberCuts + 1, kNumberCutCount - 1);
    const toybox::FontMetrics em = toybox::metricsFor(cut);
    toybox::drawCapsCentered(renderer, cut, d.x, d.y + 4, em.capHeight + 10, line, true);
    return;
  } else if (engine.pending()[0]) {
    // The sum so far, small, above the number: the one thing an
    // immediate-execution calculator cannot otherwise tell you, and the reason
    // "why did it say 12" happens on machines that do not show it.
    const char* p = engine.pending();
    const int w = renderer.getTextWidth(toybox::kUiFontId, p);
    toybox::drawCapsCentered(renderer, toybox::kUiFontId, d.right() - w, d.y, 34, p, true);
  }

  const char* text = engine.display();
  const int cut = fitCut(text, d.w, kNumberCuts, kNumberCutCount);
  const toybox::FontMetrics m = toybox::metricsFor(cut);
  const int w = renderer.getTextWidth(cut, text);
  const int boxH = m.capHeight + 8;
  toybox::drawCapsCentered(renderer, cut, d.right() - w, numberBottom - boxH, boxH, text, true);
}

// Text for the EXPRESSION candidate's entry line. ASCII throughout, because the
// line is drawn in a Toybox cut and those cuts have no glyph past U+007E.
void CalculatorActivity::appendExpr(const calc::KeyDef& key) {
  const char* add = nullptr;
  switch (key.key) {
    case calc::Key::ClearAll: expr[0] = '\0'; return;
    case calc::Key::Backspace: {
      const size_t len = std::strlen(expr);
      if (len) expr[len - 1] = '\0';
      return;
    }
    case calc::Key::Add: add = "+"; break;
    case calc::Key::Sub: add = "-"; break;
    case calc::Key::Mul: add = "x"; break;
    case calc::Key::Div: add = "/"; break;
    case calc::Key::Power: add = "^"; break;
    case calc::Key::Sqrt: add = "sqrt("; break;
    case calc::Key::LParen: add = "("; break;
    case calc::Key::RParen: add = ")"; break;
    case calc::Key::Percent: add = "%"; break;
    case calc::Key::Dot: add = "."; break;
    case calc::Key::Answer: add = "ANS"; break;
    case calc::Key::Equals: return;  // nothing evaluates this yet
    default:
      if (calc::isDigit(key.key)) {
        static char one[2] = {};
        one[0] = static_cast<char>('0' + calc::digitValue(key.key));
        add = one;
      }
      break;
  }
  if (!add) return;
  const size_t len = std::strlen(expr);
  if (len + std::strlen(add) + 1 >= sizeof(expr)) return;
  std::snprintf(expr + len, sizeof(expr) - len, "%s", add);
}

void CalculatorActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen surface(frame);

  fui::HeaderProps header;
  header.title = "CALCULATOR";
  header.rightLabel = calc::activeLayout().name;  // temporary: names the candidate
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(surface);
  toybox::headerBand(surface, header);

  const calc::PadGeom g = calc::padGeom(calc::activeLayout(), bodyRect());
  drawDisplay(g);
  drawPad(g);

  interactionsReady = true;
  noteSurfaceBuilt();
  toybox::reportOverflow(interactions, "Calculator");
  renderer.displayBuffer();
}

void CalculatorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;

  const calc::Layout& layout = calc::activeLayout();
  const calc::PadGeom g = calc::padGeom(layout, bodyRect());
  const int index = calc::keyAt(layout, g, tapX, tapY);
  if (index < 0) return;
  if (layout.display == calc::DisplayKind::Expression) {
    appendExpr(layout.keys[index]);
  } else {
    engine.press(layout.keys[index].key);
  }
  requestUpdate();
}
