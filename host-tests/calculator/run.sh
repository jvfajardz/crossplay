#!/bin/sh
# The calculator's arithmetic, its key semantics and the geometry of all five
# candidate pads, checked without a panel. CalcEngine.h and CalcLayout.h are
# freestanding C++17 for exactly this reason.
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
