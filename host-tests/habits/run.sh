#!/bin/sh
set -eu
cd "$(dirname "$0")"
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-habits-tests-$(cd ../.. && pwd | cksum | cut -d' ' -f1)"
mkdir -p "$BUILD_DIR"
SRC=../../src/apps_local/habits

"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror \
  "$SRC/HabitModel.cpp" "$SRC/HabitStats.cpp" "$SRC/HabitCsvStore.cpp" test_habits.cpp \
  -o "$BUILD_DIR/test_habits"
"$BUILD_DIR/test_habits"
