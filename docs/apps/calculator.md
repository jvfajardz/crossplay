# Calculator

A calculator for the X4 Pro. What is settled, what is still Mario's to settle,
and the five candidate pads that exist to settle the biggest of them.

## The panel decides the pad, and it is roomier than it looks

Portrait, 480x800. The Toybox chrome takes the top 83px (band 76 + gap 4 + rule
3) and `toybox::kBodyTop` puts the first content row at 119; with `kMargin`
either side and below, an app owns **448 x 665**.

At a 12px gutter that is generous for keys. Measured off `calc::padGeom`, with
the panel's 220ppi converted to millimetres:

| candidate  | grid | key px  | key mm      |
| ---------- | ---- | ------- | ----------- |
| PHONE      | 4x5  | 103x97  | 11.9 x 11.2 |
| DESKTOP    | 4x6  | 103x78  | 11.9 x 9.0  |
| TAPE       | 4x5  | 103x71  | 11.9 x 8.2  |
| SCIENTIFIC | 5x7  | 80x68   | 9.2 x 7.9   |
| EXPRESSION | 5x5  | 80x85   | 9.2 x 9.8   |

Apple's minimum touch target is 44pt, about **61px** here, and the smallest of
these clears it by 7px in its tightest direction. `host-tests/calculator`
asserts that floor rather than leaving it as a claim, so a pad that grows a row
fails instead of shipping small. **Six columns is where it stops**: 448px over
six columns is 64px, 7.4mm, and on a panel with no tap feedback at all a miss
costs a full refresh to notice and another to undo.

## The keys are drawn, not typed, and that is not a style choice

`toybox_14`, `_20`, `_30`, `_44` and `_64` are Jersey 25 converted from an
**ASCII-only** TTF: `U+0020..U+007E` and nothing else (only `toybox_10` carries
Latin-1). So `÷` `×` `±` `√` and any backspace arrow are not characters to use
carefully -- they are characters that draw as **empty space**, silently, because
a glyph the face lacks is a hole rather than a box. Regenerating a cut with more
glyphs is worse: a regenerated Toybox cut does not reproduce, it moves every
glyph's metrics and reflows every screen in the fork.

Those five are therefore drawn from primitives in
`CalculatorActivity::drawSymbol`, sized off the key so one routine serves a
103px pad and an 80px one. `testEveryKeySaysWhatItIs` refuses any label with a
byte past `U+007E`, so the next key somebody adds cannot reintroduce it.

## The pad is hit-tested against geometry, not registered

`toybox::kMaxInteractions` is 24 and the SCIENTIFIC pad is 35 keys. A screen
that registers more loses the **last** ones registered, on the device only,
silently: they draw, they look live, and they answer nothing. That is how the
Connections archive shipped with every date from the 20th onward dead.

So `calc::keyRect` is the one geometry function, `drawPad` draws from it and
`loop()` hit-tests against it. `host-tests/calculator` probes every key's centre
**and its four corners**, and requires one pixel past each edge to belong to
something else. Centres alone are not enough and this suite said they were until
a mutation run proved otherwise: shifting every hit rect sideways by one gap
left each key's own centre inside its own wrong rect, and the whole pad passed.

## What is settled about the arithmetic

`CalcEngine.h` is freestanding C++17 and every case below is one where
calculators are commonly wrong. All are pinned in the suite.

- **0.1 + 0.2 shows 0.3.** The fix is not decimal arithmetic, it is printing at
  **twelve significant digits** when the double carries about seventeen. That is
  what iOS does, and it is why its calculator looks exact.
- **Percent reads the pending operator.** `200 + 10 %` is 220; `200 x 10 %` is
  20. Not one operation, a convention -- and the most commonly reimplemented
  wrong key on a calculator.
- **`2 + 3 = = =` is 5, 8, 11.** Equals repeats the operator and the operand.
- **Two operators in a row replace**, they do not stack.
- **An error is a wall.** Divide by zero says so and then refuses every key but
  clear, rather than letting a digit land on top of the message.
- **The thirteenth typed digit is refused**, not accepted and silently rounded.

## What is NOT settled, and why each is Mario's

### 1. Which pad

Five are built behind `-DCALC_VARIANT=n` and rendered from the simulator
(`qa-artifacts/calculator-five.png`). Four of the five are deleted in the commit
that picks one.

### 2. The near-zero residue: double, or decimal?

Twelve-digit rounding fixes every case where the error is small against the
result and **none** where the result itself is near zero, because the error is
then the whole answer:

    0.1 + 0.2        ->  0.3                   fixed
    1.1 x 3          ->  3.3                   fixed
    0.1 + 0.2 - 0.3  ->  5.55111512313e-17     NOT fixed

Casio shows `0` there because Casio's arithmetic is decimal (BCD), not because
its display is cleverer. The two honest options are:

- **double + 12-digit display rounding** -- what ships today, ~0 extra flash,
  and `0.1+0.2-0.3` shows the residue. What iOS does.
- **IBM decNumber** (ICU licence, ~25KB flash measured on ESP32-S3, 36 bytes a
  number, no exceptions, no allocation at our precision) -- `0.1+0.2-0.3` is
  exactly `0`.

`testTheKnownLimitOfBinaryArithmetic` pins the residue **as a test**, so the
boundary is documented rather than waiting to be discovered by a user. If that
test ever has to change, the change is decNumber and not a bigger rounding.
There is a popular hack -- snap to zero when the result is tiny against the
operands -- and it is a lie that will eventually give a wrong answer to somebody
doing legitimate small-number arithmetic. Not shipping it.

### 3. Percent on x and /

`500 x 5 %` is **25** on iOS and Casio (percent becomes a plain hundredth) and
**12500** on Windows (the pocket-calculator rule: the two values are multiplied
and divided by 100). The engine does the iOS thing, and
`testPercentReadsThePendingOperator` is the one line that changes if Windows is
wanted.

## What no library does, and what one does

The parser is the part with good off-the-shelf options and it is also the
smallest part of the job.

- **EXPRESSION needs one**: `tinyexpr` (zlib, ~1000 lines, **6.5KB of flash**
  measured on ESP32-S3, C99, no exceptions, no RTTI). Compile it as `.c`, not
  C++ -- as C++ it is 121 `void*` conversion errors -- and with
  `-DTE_POW_FROM_RIGHT -DTE_NAT_LOG`, because its defaults make `-2^2` be 4 and
  `log` be base 10. Its `err` reports syntax position only: `1/0` comes back as
  `inf` with no error flag, so the result must be `isfinite`-checked before it
  is shown. Not vendored yet, because it is dead weight in the other four pads.
- **The other four need none.** They are immediate-execution machines: there is
  no expression to parse, and no library anywhere supplies the input state
  machine, the percent convention, the repeated-equals rule or the display
  formatting. Those are this file's ~350 lines and its test suite.

Rejected, each for a reason rather than taste: **tinyexpr++** (C++20 and 76
`throw` sites; exceptions are off in this build), **muParser** (exceptions are
its only error channel), **ExprTk** (RTTI, and a 1.66MB header), **Windows
Calculator's RatPack** (MIT and genuinely exact -- arbitrary-precision
rationals -- but it throws raw ints and returns `std::wstring`), **Android's
constructive reals** (the most rigorous of the lot, and Java).

## Still to measure

**Every keypress is a whole-screen refresh, and nobody knows what one costs on
an X4 Pro.** `docs/open-items.md` has this open: the SDK says "0.3-2 s" and that
is the whole of our knowledge; `displayWindow` exists, is marked EXPERIMENTAL,
and `HalDisplay` exposes only whole-screen modes. A calculator is the app that
cares most -- a twelve-tap sum is twelve refreshes -- so the number should be
taken off the dev-mode device before the pad is finalised. The existing evidence
that this is tolerable is the on-screen QWERTY keyboard, which people already
type Wi-Fi passwords on at 46px keys; these are twice that.

One thing does already work in our favour: the pad's hit table never changes
between frames, so `RevealedInteractions`' "an unchanged table always routes"
rule keeps taps alive through a repaint. Rapid tapping is not dropped.
