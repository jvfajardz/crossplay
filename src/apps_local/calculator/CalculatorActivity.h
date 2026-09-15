#pragma once

// Calculator on the device. The thin layer: chrome, drawing, taps.
//
// The pad is drawn by hand into the body rect rather than built from
// fui::keyGrid, and that is not a preference. toybox::kMaxInteractions is 24;
// the SCIENTIFIC pad is 35 keys, and a screen that registers more loses the
// LAST ones registered, on the device only, silently -- they draw, they look
// live, and they answer nothing. The fix for any grid past that ceiling is one
// geometry function used for BOTH the drawing and the hit test, which is what
// calc::keyRect and calc::keyAt are. See the Connections archive, which shipped
// with every date from the 20th onward dead.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "CalcEngine.h"
#include "CalcLayout.h"

class CalculatorActivity final : public Activity {
 public:
  CalculatorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Calculator", renderer, mappedInput) {}
  ~CalculatorActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  calc::Rect16 bodyRect() const;
  void drawDisplay(const calc::PadGeom& g);
  void drawPad(const calc::PadGeom& g);
  void drawKey(const calc::KeyDef& key, const calc::Rect16& r);
  void drawSymbol(calc::Sym sym, const calc::Rect16& r, bool ink);
  // The largest cut the string fits in, walking down. The design language's
  // rule: step the size, never clip the number.
  int fitCut(const char* text, int maxWidth, const int* cuts, int cutCount) const;

  calc::Engine engine;
  // EXPRESSION only: the sum as typed, kept as text because that candidate does
  // not evaluate as it goes. Nothing evaluates it yet -- the parser that would
  // (tinyexpr, zlib, 6.5KB of flash) is deliberately not vendored until the
  // layout is chosen, because it is dead weight in the other four.
  char expr[64] = {};
  void appendExpr(const calc::KeyDef& key);

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
