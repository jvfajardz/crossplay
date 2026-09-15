# Calculator

A calculator for the X4 Pro, in the TOYBOX look. What is settled, and the things
that had to be fixed at the source rather than styled around.

## TOYBOX, and the four that were deleted

Mario chose TOYBOX on 2026-09-15 out of five looks rendered side by side. The
other four -- INSTRUMENT, NIGHT, SWISS, LEDGER -- and the `Skin` struct that
carried them are gone in the same commit, along with the Ubuntu Bold and Noto
Serif cuts only they used. A variant macro that survives a decision is a second
design nobody maintains, and those two faces were 450KB of flash for a look
nothing draws.

Two rules from that pass are settled and stay:

**No rounded corners.** Mario's call, twice.

**No unexplained space.** Every vertical band is DERIVED from a cut metric, not
picked: `displayHeight()` is the pending line's own line height, plus the
number's cap height and its air, plus the rule. Nothing is left over.

That is not tidiness, it is the fix for a real defect. In the first pass every
skin carried a guessed display height, the number was pinned to the bottom of it,
and the remainder came out as a band of empty panel over every result --
different in each skin, explained by nothing. `host-tests/calculator` now refuses
a layout that leaves more than one pixel a row unaccounted for, and
`label_fit.py` checks `CalcCutMetrics.h` against the real font headers, so a
regenerated cut cannot move a band without going red. The derivation earned
itself immediately: LEDGER asked for four tape lines, which left 55px key rows
against a 61px touch floor, and the suite said so.

## Nothing can overflow, and the bound is at the source

Mario, 2026-09-15: *"I NEVER want to see overlapping text with the borders or
numbers that dont read nicely."*

A display cannot promise that by being careful about what it draws. It has to be
impossible, and the only place it can be made impossible is where the string is
produced.

**`kMaxDisplayChars` is 12**, and it is a limit on the ENGINE. Twelve characters
is what the smallest of the three number cuts clears on this panel: twelve times
Jersey 34's widest glyph is 420px of the 448 an app owns. A result that obeys it
cannot be drawn past its box, whatever it is. Twelve characters is a sign, ten
digits and a point -- which is why the engine carries **ten** significant digits
and not twelve. Ten is a normal pocket calculator; TI's display is ten.

Above that bound the display picks the largest of three cuts the string actually
fits in -- 56, 44, 34 -- measured through the renderer's own advance widths
rather than counted, because **Jersey's digits are not tabular**: a `1` is 37px
against a `0` at 57px in the 56 cut, so counting characters would step the number
down a size it did not need and every result would read smaller than it could.

Three rungs and not five, because the worst case is bounded: a result never lands
in a label cut. `label_fit.py` proves it over the strings the ENGINE actually
emits when the suite drives it to its limits, rather than over samples somebody
thought of.

## The symbols are type now, and that was the whole problem

The first five renders had hand-drawn operator glyphs and they looked homemade,
because they were. The cause was one line in `gen_toybox_fonts.sh`: the Toybox
cuts are Jersey 25 **subset to U+0020-007E**, so the division sign, the
multiplication sign and the minus sign draw as NOTHING -- a glyph the face lacks
is a hole, not a box. Jersey has had all three all along.

`tools_local/toybox/gen_calc_fonts.sh` cuts the calculator's own faces with the
math block included. They are NEW files, never wider versions of the Toybox
cuts, because `gen_toybox_fonts.sh` spells out at length that regenerating
`toybox_20` or `_30` today moves every glyph a pixel and silently shifts text in
every app in the fork. A cut nothing else uses cannot do that to anybody.

Five cuts per face, not one: **a calculator sets its word keys smaller than its
digits.** Look at any of them. Here it is also the only way DEL fits a key at
all -- Ubuntu Bold draws it 103px wide at 26px, in a 103px key. `labelFontFor`
picks by label length, structurally, so the host gate can resolve the same face
the panel will.

Jersey has no U+00B1, so the sign key prints **+/-**, which is the spelling a
keyboard-era calculator uses. An invisible key is not a convention.

## The gate that catches a label before a person does

`host-tests/calculator/label_fit.py` measures every key label, in the cut its
skin resolves, against the cell that skin produces. A host test cannot parse a
font header and a screenshot only shows the skin somebody photographed, so this
is the only thing that can see the failure. It found Ubuntu Bold's DEL within a pixel and a half of its border on both sides,
and Noto Serif's **fifteen pixels wider than its key** -- in a look whose render
nobody had looked at yet.

It checks the invisible half too: a codepoint the face has no glyph for is
reported rather than silently costing zero width.

## The pad is hit-tested against geometry, not registered

`toybox::kMaxInteractions` is 24. Twenty keys fits, but a screen that goes past
it loses the **last** ones registered, on the device only, silently -- they
draw, they look live, and they answer nothing. That is how the Connections
archive shipped with every date from the 20th onward dead. So `calc::keyRect` is
the one geometry function, `drawPad` draws from it and `loop()` hit-tests
against it.

The suite probes every key's centre **and its four corners**, and requires one
pixel past each edge to belong to something else. Centres alone are not enough
and this suite said they were until a mutation run proved otherwise: shifting
every hit rect sideways by one gap left each key's own centre inside its own
wrong rect, and the whole pad passed.

## What is settled about the arithmetic

`CalcEngine.h` is freestanding C++17 and every case below is one where
calculators are commonly wrong. All are pinned.

- **0.1 + 0.2 shows 0.3.** The fix is not decimal arithmetic, it is printing at
  twelve significant digits when the double carries about seventeen. That is
  what iOS does, and it is why its calculator looks exact.
- **Percent reads the pending operator.** `200 + 10 %` is 220; `200 x 10 %` is
  20. Not one operation, a convention.
- **`2 + 3 = = =` is 5, 8, 11.** Equals repeats the operator and the operand.
- **Two operators in a row replace**, they do not stack.
- **An error is a wall.** Divide by zero says so and then refuses every key but
  clear, rather than letting a digit land on top of the message.
- **The thirteenth typed digit is refused**, not accepted and silently rounded.

## What is NOT settled

### 1. The near-zero residue: double, or decimal?

Twelve-digit rounding fixes every case where the error is small against the
result and **none** where the result itself is near zero:

    0.1 + 0.2        ->  0.3                   fixed
    0.1 + 0.2 - 0.3  ->  5.55111512313e-17     NOT fixed

Casio shows `0` there because Casio's arithmetic is decimal (BCD), not because
its display is cleverer. The options are double plus display rounding (what
ships, what iOS does, no extra flash) or IBM decNumber (ICU licence, ~25KB
measured on ESP32-S3, no exceptions, no allocation at our precision).
`testTheKnownLimitOfBinaryArithmetic` pins the residue **as a test**, so if that
test ever has to change the change is decNumber and not a bigger rounding.

### 2. Percent on x and /

`500 x 5 %` is **25** on iOS and Casio and **12500** on Windows. The engine does
the iOS thing, and `testPercentReadsThePendingOperator` is the one line that
changes.

## What no library does, and what one does

No parser is vendored. `tinyexpr` (zlib, ~1000 lines, 6.5KB of flash measured on
an ESP32-S3) is the right one if a typed-expression mode is ever wanted -- built
as `.c`, not C++, and with `-DTE_POW_FROM_RIGHT -DTE_NAT_LOG`, because its
defaults make `-2^2` be 4. This pad does not need it: it is an
immediate-execution machine and there is no expression to parse.

What no library anywhere supplies is the input state machine, the percent
convention, repeated equals and the display formatting. That is this app's own
~350 lines and its suites. Rejected with reasons: tinyexpr++ (C++20, 76 `throw`
sites, and exceptions are off here), muParser (exceptions are its only error
channel), ExprTk (RTTI and a 1.66MB header), Windows Calculator's RatPack
(genuinely exact, but throws raw ints and returns `std::wstring`).

## Still to measure

**Every keypress is a whole-screen refresh, and nobody knows what one costs on
an X4 Pro.** `docs/open-items.md` has this open: the SDK says "0.3-2 s" and that
is the whole of our knowledge. A calculator is the app that cares most. The
existing evidence that it is tolerable is the on-screen QWERTY keyboard people
already type Wi-Fi passwords on at 46px keys; these are twice that. One thing
works in our favour: the pad's hit table never changes between frames, so
`RevealedInteractions`' "an unchanged table always routes" rule keeps taps alive
through a repaint.
