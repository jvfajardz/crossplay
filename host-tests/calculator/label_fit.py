#!/usr/bin/env python3
"""Every key label, measured in the cut its skin actually resolves, against the
cell that skin actually produces.

This is the check that nothing else can make. A host test cannot read a font
header, and a screenshot shows one state of one skin -- so a label one glyph too
wide for its key crosses the outline in a skin nobody photographed, and the only
symptom is that it looks wrong. NIGHT shipped exactly that in its first render:
Ubuntu Bold's "DEL" came to within a pixel and a half of the key's border on
both sides, and the three-character keys read as one run of letters.

It also catches the other half, which is worse because it is invisible: a
codepoint the face has no glyph for draws as NOTHING AT ALL on this renderer, so
a plus-minus sign asked of Jersey 25 would be a blank key that still works.

    host-tests/calculator/label_fit.py <table from test_calculator --labels>
"""

import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[1]
FONTS = REPO / "src/apps_local/calculator/fonts"

# The ids are CalcFontIds.h's, read from it rather than copied, so a renumbered
# font cannot make this script quietly measure the wrong face.
IDS = {}
for line in (REPO / "src/apps_local/calculator/CalcFontIds.h").read_text().splitlines():
    m = re.match(r"constexpr int k(\w+)FontId = (0x[0-9A-Fa-f']+);", line.strip())
    if m:
        IDS[int(m.group(2).replace("'", ""), 16)] = m.group(1)

FILES = {
    "JerseyLabel": "calc_jersey_28",
    "JerseySmall": "calc_jersey_20",
    "JerseyNumber": "calc_jersey_56",
    "JerseyMid": "calc_jersey_44",
    "JerseyTiny": "calc_jersey_34",
    "UbuntuLabel": "calc_ubuntu_26",
    "UbuntuSmall": "calc_ubuntu_18",
    "UbuntuNumber": "calc_ubuntu_56",
    "UbuntuMid": "calc_ubuntu_44",
    "UbuntuTiny": "calc_ubuntu_34",
    "SerifLabel": "calc_serif_26",
    "SerifSmall": "calc_serif_16",
    "SerifNumber": "calc_serif_56",
    "SerifMid": "calc_serif_44",
    "SerifTiny": "calc_serif_34",
}

# Every id in CalcFontIds.h has to name a file here, or this gate silently
# stops measuring the face it was pointed at.
_missing = sorted(set(IDS.values()) - set(FILES))
assert not _missing, f"label_fit.py has no file for: {', '.join(_missing)}"

# A label has to sit inside its key with air on both sides. Ten per cent a side
# is the least that still reads as a key with a glyph in it rather than a glyph
# with a box round it.
SIDE_AIR = 0.10
# What the display has to hold. The first is an everyday result, the last two
# are the widest the engine can produce: twelve significant digits fixed, and
# nine plus an exponent once it goes scientific.
# Each sample carries the lowest rung it may land on, because the ladder's job
# is not "everything is big" -- it is that an EVERYDAY result is big and an
# extreme one is merely readable. A seven character result landing three rungs
# down is the defect this catches: it is not clipped, it is just small, and it
# only looks wrong next to the same number in a skin whose ladder has a rung
# there. An exponent is the rarest state a display reaches and may be small.
SAMPLES = (
    ("546.875", 34),          # an everyday result, read at arm's length
    ("17636.5714286", 21),    # twelve significant digits
    ("-999999999999.", 17),   # the widest fixed form
    ("-1.23456789e-15", 12),  # scientific: the rarest state, it only has to read
)


def load_font(name):
    src = (FONTS / f"{name}.h").read_text()
    start = src.index(f"{name}Glyphs[] = {{")
    glyphs = [
        tuple(int(x, 0) for x in m)
        for m in re.findall(
            r"\{\s*(-?\w+),\s*(-?\w+),\s*(-?\w+),\s*(-?\w+),\s*(-?\w+),\s*(-?\w+),\s*(-?\w+)\s*\}",
            src[start : src.index("\n};", start)],
        )
    ]
    start = src.index(f"{name}Intervals[] = {{")
    intervals = [
        tuple(int(x, 0) for x in m)
        for m in re.findall(
            r"\{\s*(0x[0-9A-Fa-f]+|\d+),\s*(0x[0-9A-Fa-f]+|\d+),\s*(0x[0-9A-Fa-f]+|\d+)\s*\}",
            src[start : src.index("\n};", start)],
        )
    ]
    return glyphs, intervals


def cap_height(font):
    """The ink height of a digit. What a reader actually judges the size by --
    the nominal cut size says nothing across faces, because a 56px Ubuntu Bold
    and a 56px Jersey put very different amounts of ink on the panel."""
    glyphs, intervals = font
    for first, last, offset in intervals:
        if first <= ord("8") <= last:
            return glyphs[offset + (ord("8") - first)][1]
    return 0


def measure(text, font):
    """-> (pixels, missing). advanceX is 12.4 fixed point (EpdFontData.h)."""
    glyphs, intervals = font
    total = 0.0
    missing = []
    for ch in text:
        cp = ord(ch)
        for first, last, offset in intervals:
            if first <= cp <= last:
                total += glyphs[offset + (cp - first)][2] / 16.0
                break
        else:
            missing.append(ch)
    return total, missing


def check_cut_metrics():
    """CalcCutMetrics.h against the real font headers.

    Those constants are not decoration: the display's height is DERIVED from
    them, so a regenerated cut that moves a line height by a pixel moves every
    band on the screen and takes the difference out of the key rows. Toybox has
    the same hazard and the same answer; this is the gate that makes the claim
    in CalcCutMetrics.h true instead of hopeful.
    """
    src = (REPO / "src/apps_local/calculator/CalcCutMetrics.h").read_text()
    declared = {
        m[0]: (int(m[1]), int(m[2]))
        for m in re.findall(r"constexpr CutMetrics k(\w+)Cut\{(\d+), (\d+)\}", src)
    }
    checks = failed = 0
    for key, (line_h, cap) in sorted(declared.items()):
        name = FILES.get(key)
        if name is None:
            failed += 1
            print(f"FAIL label_fit  CalcCutMetrics.h declares k{key}Cut and no font is mapped to it")
            continue
        font = load_font(FONTS / f"{name}.h") if False else load_font(name)
        real_cap = cap_height(font)
        real_line = advance_y(name)
        checks += 2
        if real_line != line_h:
            failed += 1
            print(f"FAIL label_fit  k{key}Cut says lineHeight {line_h}, {name}.h has {real_line}")
        if real_cap != cap:
            failed += 1
            print(f"FAIL label_fit  k{key}Cut says capHeight {cap}, {name}.h has {real_cap}")
    for key in sorted(set(FILES) - set(declared)):
        checks += 1
        failed += 1
        print(f"FAIL label_fit  {FILES[key]} has no CutMetrics entry, so any layout using it is guessed")
    return checks, failed


def advance_y(name):
    """EpdFontData's fifth field: the face's own baseline-to-baseline advance."""
    src = (FONTS / f"{name}.h").read_text()
    body = src[src.index(f"static const EpdFontData {name} = {{"):]
    body = body[: body.index("};")]
    fields = [x.strip().rstrip(",") for x in body.splitlines()[1:] if x.strip()]
    return int(fields[4])


def main():
    table = pathlib.Path(sys.argv[1]).read_text().splitlines()
    cache = {}

    def font_for(font_id):
        if font_id not in cache:
            cache[font_id] = load_font(FILES[IDS[font_id]])
        return cache[font_id]

    skins = {}
    ladders = {}
    checks, failed = check_cut_metrics()
    for line in table:
        # Split on the fixed leading fields only: a label may contain a space
        # and must reach `measure` exactly as the panel would draw it.
        parts = line.split(" ")
        if parts[0] == "SKIN":
            skins[parts[1]] = (int(parts[2]), int(parts[3]), int(parts[4]))
        elif parts[0] == "LABEL":
            # The font id is the one calc::labelFontFor resolved, not the skin's
            # headline cut: a word key is set smaller, and measuring the wrong
            # one is how this gate would pass a label the panel clips.
            name, font_id, label = parts[1], int(parts[2]), line.split(" ", 3)[3]
            cell_w = skins[name][1]
            width, missing = measure(label, font_for(font_id))
            budget = cell_w * (1 - 2 * SIDE_AIR)
            checks += 2
            if missing:
                failed += 1
                print(f"FAIL label_fit  {name} key \"{label}\": {IDS[font_id]} has no glyph for "
                      f"{' '.join('U+%04X' % ord(c) for c in missing)} -- it would draw as nothing")
            if width > budget:
                failed += 1
                print(f"FAIL label_fit  {name} key \"{label}\" is {width:.0f}px in a {cell_w}px key "
                      f"(budget {budget:.0f}px with {int(SIDE_AIR*100)}% air a side)")
        elif parts[0] == "NUMBER":
            name, rung, font_id, width_px = parts[1], int(parts[2]), int(parts[3]), int(parts[4])
            ladders.setdefault(name, (width_px, []))[1].append((rung, font_id))

    # The display steps DOWN a ladder as a result gets longer. Two things can go
    # wrong and only one of them is visible in a screenshot: a rung missing
    # (the number drops straight to a label cut and looks broken beside the same
    # number in another skin), and the bottom rung still not fitting (the panel
    # clips a result).
    for name, (width_px, rungs) in sorted(ladders.items()):
        for text, min_cap in SAMPLES:
            checks += 1
            landed = None
            for rung, font_id in sorted(rungs):
                w, missing = measure(text, font_for(font_id))
                if missing:
                    failed += 1
                    print(f"FAIL label_fit  {name}: {IDS[font_id]} has no glyph for "
                          f"{' '.join('U+%04X' % ord(c) for c in missing)}")
                    break
                if w <= width_px:
                    landed = (rung, font_id, w, cap_height(font_for(font_id)))
                    break
            if landed is None:
                failed += 1
                print(f"FAIL label_fit  {name}: \"{text}\" does not fit a {width_px}px display "
                      f"at ANY rung -- the panel would clip it")
            elif landed[3] < min_cap:
                failed += 1
                print(f"FAIL label_fit  {name}: \"{text}\" lands on {IDS[landed[1]]} at rung "
                      f"{landed[0]}, {landed[3]}px of capital against a {min_cap}px floor "
                      f"-- the ladder has a cliff in it")

    print(f"{'FAILED' if failed else 'ok'}: {checks} checks, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
