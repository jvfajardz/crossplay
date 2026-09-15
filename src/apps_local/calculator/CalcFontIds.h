#pragma once

// The calculator's font ids, and nothing else.
//
// Split from CalcFonts.h because a Skin names its faces and a Skin has to stay
// freestanding: CalcFonts.h includes GfxRenderer.h, and one include of that in
// the skin table would put the whole renderer on the host suite's include path.
// Ids are numbers; keeping them here costs nothing and keeps the tests able to
// build every skin on a laptop.
//
// Their own 0x70B1 block so they cannot collide with fontIds.h (FNV hashes of
// generated font names) or with Toybox's 0x70B0 block.

namespace calc {

constexpr int kJerseyLabelFontId = 0x70B1'0001;   // Jersey 25 @28, ASCII + U+00D7 U+00F7 U+2212
constexpr int kJerseyNumberFontId = 0x70B1'0002;  // Jersey 25 @56, digits and signs only
constexpr int kUbuntuLabelFontId = 0x70B1'0003;   // Ubuntu Bold @26, and it has U+00B1 and U+221A
constexpr int kUbuntuNumberFontId = 0x70B1'0004;  // Ubuntu Bold @56
constexpr int kSerifLabelFontId = 0x70B1'0005;    // Noto Serif Bold @26
constexpr int kSerifNumberFontId = 0x70B1'0006;   // Noto Serif Bold @56

// The middle rungs. A display steps DOWN a ladder as a result gets longer -- the
// design language's rule for anything that will not fit -- and a ladder with one
// rung is a cliff: a seven digit result was landing on the 26px label cut in
// three of the five skins.
constexpr int kJerseyMidFontId = 0x70B1'000A;    // Jersey 25 @44
constexpr int kJerseyTinyFontId = 0x70B1'000B;   // Jersey 25 @34, digits only
constexpr int kUbuntuMidFontId = 0x70B1'000C;    // Ubuntu Bold @44
constexpr int kUbuntuTinyFontId = 0x70B1'000D;   // Ubuntu Bold @34, digits only
constexpr int kSerifMidFontId = 0x70B1'000E;     // Noto Serif Bold @44
constexpr int kSerifTinyFontId = 0x70B1'000F;    // Noto Serif Bold @34, digits only

// The word keys. A calculator sets AC and DEL smaller than its digits -- look at
// any of them -- and here it is also the only way "DEL" fits a key at all:
// Ubuntu Bold draws it 103px wide at 26px, in a 103px key.
constexpr int kJerseySmallFontId = 0x70B1'0007;  // Jersey 25 @20
constexpr int kUbuntuSmallFontId = 0x70B1'0008;  // Ubuntu Bold @18
constexpr int kSerifSmallFontId = 0x70B1'0009;   // Noto Serif Bold @16

}  // namespace calc
