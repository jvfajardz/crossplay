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
#include "CalcFonts.h"

namespace fui = freeink::ui;

namespace {

const calc::Skin& skin() { return calc::activeSkin(); }

// True where the skin's foreground is black. Every colour decision on the
// screen goes through this one expression, so an inverted skin cannot end up
// half inverted -- which is what a second opinion about "is this dark" always
// produces.
bool fg() { return skin().ground == calc::Ground::Paper; }
bool bg() { return !fg(); }

// The step-down ladder for a number, largest first. Both cuts are the skin's
// own face; the small one is its label cut, which is the right last resort
// because a fourteen-character result is a rare state and staying in the
// family matters more than staying big.
int numberCuts(const int i) { return calc::numberFontFor(skin(), i); }
constexpr int kNumberCutCount = calc::kNumberRungs;

}  // namespace

std::unique_ptr<Activity> CalculatorActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<CalculatorActivity>(renderer, mappedInput);
}

void CalculatorActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  calc::ensureCalcFonts(renderer);
  requestUpdate();
}

calc::Rect16 CalculatorActivity::body() const {
  return calc::bodyFor(skin(), renderer.getScreenWidth(), renderer.getScreenHeight());
}

int CalculatorActivity::fitNumberCut(const char* text, const int maxWidth) const {
  for (int i = 0; i < kNumberCutCount; ++i) {
    if (renderer.getTextWidth(numberCuts(i), text) <= maxWidth) return numberCuts(i);
  }
  return numberCuts(kNumberCutCount - 1);
}

void CalculatorActivity::paintGround() {
  renderer.clearScreen();
  if (skin().ground == calc::Ground::Ink) {
    renderer.fillRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight(), true);
  }
}

// The chrome paints from the panel's physical row 0 whatever the glass hides --
// paint may bleed under the bezel, ink may not -- so a band's fill starts at 0
// and its title centres between the bezel's safe top and the band's bottom.
// A covered row is not an invisible row: the eye sees past the bezel from below
// and reads a white strip above any header that started at the safe top.
void CalculatorActivity::drawChrome() {
  const calc::Skin& s = skin();
  const int w = renderer.getScreenWidth();
  int safeTop = 0, safeRight = 0, safeBottom = 0, safeLeft = 0;
  renderer.getOrientedViewableTRBL(&safeTop, &safeRight, &safeBottom, &safeLeft);

  switch (s.chrome) {
    case calc::Chrome::Band: {
      renderer.fillRect(0, 0, w, s.chromeH, true);
      calc::drawCapsCentered(renderer, s.titleFont, s.margin, safeTop, s.chromeH - safeTop, "CALCULATOR", false);
      renderer.fillRect(0, s.chromeH + 4, w, 3, true);
      break;
    }
    case calc::Chrome::Plate: {
      // A machined strip: two rules of different weight with the name between
      // them, and the model line hard right. The weights differ by more than a
      // pixel on purpose -- at equal weight two rules read as a mistake.
      const int top = safeTop + 6;
      const int titleW = renderer.getTextWidth(s.titleFont, "CALCULATOR");
      calc::drawCapsCentered(renderer, s.titleFont, s.margin, top, 30, "CALCULATOR", fg());
      // The model line is dropped rather than drawn through the title. Two
      // labels sized independently and both anchored to an edge is exactly how
      // a header comes to read as one run of overlapping letters, and there is
      // no width at which that is better than one label.
      if (s.subtitle) {
        const int sw = renderer.getTextWidth(s.smallLabelFont, s.subtitle);
        const int subX = w - s.margin - sw;
        if (subX > s.margin + titleW + 24) {
          calc::drawCapsCentered(renderer, s.smallLabelFont, subX, top, 30, s.subtitle, fg());
        }
      }
      renderer.fillRect(s.margin, top + 36, w - 2 * s.margin, 5, fg());
      renderer.fillRect(s.margin, top + 45, w - 2 * s.margin, 1, fg());
      break;
    }
    case calc::Chrome::Rule: {
      // The app's name on the left, always. The subtitle is a second line ABOUT
      // the app, not a replacement for its name: LEDGER's header read "TAPE"
      // for as long as this branch treated one as the other, so the screen
      // never said what it was.
      const int top = safeTop + 6;
      const int titleW = renderer.getTextWidth(s.titleFont, "CALCULATOR");
      calc::drawCapsCentered(renderer, s.titleFont, s.margin, top, 28, "CALCULATOR", fg());
      if (s.subtitle) {
        const int sw = renderer.getTextWidth(s.smallLabelFont, s.subtitle);
        const int subX = w - s.margin - sw;
        if (subX > s.margin + titleW + 24) {
          calc::drawCapsCentered(renderer, s.smallLabelFont, subX, top, 28, s.subtitle, fg());
        }
      }
      renderer.fillRect(s.margin, top + 34, w - 2 * s.margin, 3, fg());
      break;
    }
  }
}

// Laid out TOP DOWN from the height calc::displayHeightFor derived, band by
// band, so there is nothing left over at the end. The first pass pinned the
// number to the bottom of a guessed height and the remainder came out as a strip
// of empty panel over every result -- a different strip in each skin, explained
// by nothing. If a band moves here it has to move there too, and the host suite
// checks the sum.
void CalculatorActivity::drawDisplay(const calc::PadGeom& g) {
  const calc::Skin& s = skin();
  const calc::Rect16 d = g.display;
  const int pendingH = calc::cutFor(s.smallLabelFont).lineHeight;
  const int numberH = calc::cutFor(s.numberFont).capHeight + 2 * calc::kNumberAir;
  const int frame = s.display == calc::DisplayStyle::Inset ? 14 : 0;

  int y = d.y + frame;
  int left = d.x + (frame ? 22 : 0);
  int right = d.right() - (frame ? 22 : 0);

  if (s.display == calc::DisplayStyle::Inset) {
    // The glass of an instrument: a heavy border with a light rule inside it.
    renderer.drawRect(d.x, d.y, d.w, d.h, 4, fg());
    renderer.drawRect(d.x + 8, d.y + 8, d.w - 16, d.h - 16, 1, fg());
  }

  if (s.display == calc::DisplayStyle::Tape) {
    const int lh = calc::cutFor(s.smallLabelFont).lineHeight;
    const int lines = std::min<int>(s.tapeLines, engine.tapeCount());
    for (int i = 0; i < s.tapeLines; ++i) {
      const int idx = engine.tapeCount() - lines + i;
      if (i < lines && idx >= 0) {
        const char* text = engine.tapeLine(idx);
        const int tw = renderer.getTextWidth(s.smallLabelFont, text);
        calc::drawCapsCentered(renderer, s.smallLabelFont, right - tw, y, lh, text, fg());
      }
      y += lh;
    }
    // A hairline between what is done and what is live, so the running sum does
    // not read as one more finished line.
    renderer.fillRect(d.x, y + 3, d.w, 1, fg());
    y += 8;
  }

  // The pending sum's band, reserved whether or not there is one: a display that
  // grew a line when you pressed an operator would shift the number under your
  // finger between one refresh and the next.
  if (engine.pending()[0]) {
    const char* p = engine.pending();
    const int pw = renderer.getTextWidth(s.smallLabelFont, p);
    calc::drawCapsCentered(renderer, s.smallLabelFont, right - pw, y, pendingH, p, fg());
  }
  y += pendingH;

  const char* text = engine.display();
  const int cut = fitNumberCut(text, right - left);
  const int tw = renderer.getTextWidth(cut, text);
  calc::drawCapsCentered(renderer, cut, right - tw, y, numberH, text, fg());
  y += numberH;

  // A rule under the number, not a box around it: the number is the content, and
  // a box would give it a key's weight. An inset skin already has its border.
  if (s.display != calc::DisplayStyle::Inset) renderer.fillRect(d.x, y, d.w, 3, fg());
}

void CalculatorActivity::drawKey(const calc::KeyDef& key, const calc::Rect16& r) {
  const calc::Skin& s = skin();
  const bool filled =
      (key.key == calc::Key::Equals && s.equalsFilled) || (key.emphasis && s.emphasis == calc::Emphasis::Fill);
  // A filled key reverses its glyph; everything else draws in the foreground.
  const bool glyphInk = filled ? bg() : fg();
  const int stroke = (key.emphasis && s.emphasis == calc::Emphasis::Weight) ? s.opStroke : s.stroke;

  switch (s.shape) {
    case calc::KeyShape::Outline:
      if (filled) {
        renderer.fillRect(r.x, r.y, r.w, r.h, fg());
      } else {
        renderer.drawRect(r.x, r.y, r.w, r.h, stroke, fg());
      }
      break;
    case calc::KeyShape::Tile:
      if (filled) {
        renderer.fillRect(r.x, r.y, r.w, r.h, fg());
        // A tiling pad has no gap, so five filled keys in a column merge into
        // one solid bar and stop reading as keys at all. The separator is a rule
        // in the GROUND colour, inside the fill: the only mark that can show on
        // top of solid ink.
        renderer.fillRect(r.x, r.y, r.w, s.stroke, bg());
      } else {
        renderer.drawRect(r.x, r.y, r.w, r.h, stroke, fg());
      }
      break;
    case calc::KeyShape::Lattice:
      // Nothing per key: the lattice is drawn once for the whole pad, so the
      // lines between cells are single weight instead of two borders touching.
      if (filled) renderer.fillRect(r.x, r.y, r.w, r.h, fg());
      break;
  }

  const char* label = calc::labelFor(s, key);
  calc::drawCapsCenteredIn(renderer, calc::labelFontFor(s, label), r.x, r.w, r.y, r.h, label, glyphInk);
}

void CalculatorActivity::drawPad(const calc::PadGeom& g) {
  const calc::Skin& s = skin();
  const calc::Layout& layout = calc::kPad;
  const int cells = layout.cols * layout.rows;

  if (s.shape == calc::KeyShape::Lattice) {
    // One grid, drawn from the same cell arithmetic the keys use, so a line can
    // never sit anywhere but on a cell edge.
    const calc::Rect16 first = calc::keyRect(layout, g, 0);
    const calc::Rect16 last = calc::keyRect(layout, g, cells - 1);
    for (int c = 1; c < layout.cols; ++c) {
      const calc::Rect16 cell = calc::keyRect(layout, g, c);
      // The operator column's edge is the one heavy mark on this pad: it is how
      // the sum keys are told from the number keys with no outlines anywhere.
      const int weight = (c == layout.cols - 1) ? s.opStroke : s.stroke;
      renderer.fillRect(cell.x - weight / 2, first.y, weight, last.bottom() - first.y, fg());
    }
    for (int rIdx = 1; rIdx < layout.rows; ++rIdx) {
      const calc::Rect16 cell = calc::keyRect(layout, g, rIdx * layout.cols);
      renderer.fillRect(first.x, cell.y - s.stroke / 2, last.right() - first.x, s.stroke, fg());
    }
  }

  for (int i = 0; i < cells; ++i) {
    if (layout.keys[i].key == calc::Key::None) continue;
    drawKey(layout.keys[i], calc::keyRect(layout, g, i));
  }
}

void CalculatorActivity::render(RenderLock&&) {
  paintGround();
  // A Frame is still built, so the interaction table has a generation the touch
  // router can read even though this screen registers nothing in it: the pad is
  // hit-tested against geometry.
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);

  drawChrome();
  const calc::PadGeom g = calc::geomFor(skin(), renderer.getScreenWidth(), renderer.getScreenHeight());
  drawDisplay(g);
  drawPad(g);

  interactionsReady = true;
  noteSurfaceBuilt();
  renderer.displayBuffer();
}

void CalculatorActivity::loop() {
  // Read on the per-frame path and ABOVE the "nothing to do unless a tap
  // arrived" return: a swipe is not a tap, and a Back read below that line is
  // on the frame path in name only. host-tests/backgesture enforces exactly
  // this, after Trivia shipped with no way out at all.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady) return;

  const calc::PadGeom g = calc::geomFor(skin(), renderer.getScreenWidth(), renderer.getScreenHeight());
  const int index = calc::keyAt(calc::kPad, g, tapX, tapY);
  if (index < 0) return;
  engine.press(calc::kPad.keys[index].key);
  requestUpdate();
}
