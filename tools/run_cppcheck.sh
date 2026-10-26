#!/usr/bin/env bash
# Static analysis of our own code (lib/rideon_core and src/) with cppcheck.
# Any finding fails CI. C-style casts and "use an STL algorithm" are style
# preferences, not defects, so they are off. A deliberate exception is
# marked in the code with `// cppcheck-suppress <id>` and a reason.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
cppcheck --enable=warning,style,performance,portability --std=c++17 --language=c++ \
  --inline-suppr --error-exitcode=1 --quiet \
  --suppress=missingIncludeSystem --suppress=missingInclude \
  --suppress=cstyleCast --suppress=useStlAlgorithm \
  -Ilib/rideon_core/src -Iinclude \
  lib/rideon_core/src src
echo "cppcheck: no findings"
