#!/usr/bin/env bash
# Builds and runs every native test suite with g++ directly, with the
# strict flags CI uses on top of `pio test -e native`:
#   -Werror, AddressSanitizer + UndefinedBehaviorSanitizer, and gcov
#   coverage of lib/rideon_core (checked against a floor by gcovr).
#
#   tools/run_native_tests.sh [build-dir]
#
# Needs g++ and Unity's source (UNITY_SRC, default: fetched into the build dir
# at the tag pinned below). See docs/VERIFICATION.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/.pio/verify}"
UNITY_TAG="v2.6.1"
mkdir -p "$BUILD"

if [ -z "${UNITY_SRC:-}" ]; then
  UNITY_SRC="$BUILD/Unity-$UNITY_TAG/src"
  if [ ! -f "$UNITY_SRC/unity.c" ]; then
    git clone --quiet --depth 1 --branch "$UNITY_TAG" https://github.com/ThrowTheSwitch/Unity.git "$BUILD/Unity-$UNITY_TAG"
  fi
fi

CXX="${CXX:-g++}"
FLAGS=(-std=c++17 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer
       -fsanitize=address,undefined -fno-sanitize-recover=all --coverage
       -I"$ROOT/lib/rideon_core/src" -I"$ROOT/include" -I"$UNITY_SRC")

CC="${CC:-gcc}"
"$CC" -O1 -g -fsanitize=address,undefined -c "$UNITY_SRC/unity.c" -o "$BUILD/unity.o"

# Each suite builds into its own folder so gcov data from different suites
# does not collide; gcovr merges them.
failed=0
for dir in "$ROOT"/test/test_*/; do
  name="$(basename "$dir")"
  out="$BUILD/$name"
  mkdir -p "$out"
  rm -f "$out"/*.gcda
  srcs=("$dir"*.cpp "$ROOT"/lib/rideon_core/src/*.cpp)
  echo "=== $name"
  (cd "$out" && "$CXX" "${FLAGS[@]}" "${srcs[@]}" "$BUILD/unity.o" -o "$out/run")
  if ! (cd "$out" && ./run); then
    failed=1
  fi
done
exit $failed
