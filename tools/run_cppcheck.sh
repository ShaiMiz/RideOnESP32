#!/usr/bin/env bash
# Static analysis of our own code (lib/rideon_core and src/) with cppcheck.
# Any finding fails CI. C-style casts, "use an STL algorithm" and "member
# function can be static" (hardware classes and stubs that will hold state)
# are style preferences, not defects, so they are off. A deliberate exception
# is marked in the code with `// cppcheck-suppress <id>` and a reason.
#
# CI installs cppcheck from the runner's Ubuntu, so a runner upgrade can
# bring new checks; tools/run_cppcheck.sh prints the version it ran.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
cppcheck --version
cppcheck --enable=warning,style,performance,portability --std=c++17 --language=c++ \
  --inline-suppr --error-exitcode=1 --quiet \
  --suppress=missingIncludeSystem --suppress=missingInclude \
  --suppress=cstyleCast --suppress=useStlAlgorithm --suppress=functionStatic \
  -Ilib/rideon_core/src -Iinclude \
  lib/rideon_core/src src
echo "cppcheck: no findings"
