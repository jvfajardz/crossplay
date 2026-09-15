#!/bin/sh
# The calculator, checked without a panel: its arithmetic, its key semantics,
# the geometry of all five skins, and every label measured in the cut its skin
# actually resolves. CalcEngine.h, CalcLayout.h and CalcSkin.h are freestanding
# C++17 for exactly this reason -- CalcFontIds.h exists so a Skin can name a
# face without dragging GfxRenderer onto this include path.
#
#   host-tests/calculator/run.sh
set -e
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-calculator-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/calculator
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -O2 -I$SRC \
  test_calculator.cpp -o "$BUILD_DIR/test_calculator"
"$BUILD_DIR/test_calculator"

# The half a C++ host test cannot do: a test cannot parse a font header, so the
# widths every label will really be drawn at are measured here instead. The
# binary emits which cut each label resolves to; the script does the arithmetic.
"$BUILD_DIR/test_calculator" --labels > "$BUILD_DIR/labels.txt"
python3 ./label_fit.py "$BUILD_DIR/labels.txt"
