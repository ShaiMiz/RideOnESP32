#!/usr/bin/env bash
# Regenerates the schematic, then runs KiCad's ERC, the netlist check and the PDF export.
# Needs Docker (uses the official kicad/kicad:9.0 image), or a local KiCad 9 with KICAD_CLI=kicad-cli.
# Usage: docs/hardware/kicad/check.sh   (from anywhere)
set -euo pipefail
cd "$(dirname "$0")"
python3 gen/generate.py
mkdir -p out
if [ -n "${KICAD_CLI:-}" ]; then
  run() { "$KICAD_CLI" "$@"; }
else
  chmod -R a+rwX . 2>/dev/null || true  # the image runs as its own kicad user, with KiCad's libraries configured
  run() { docker run --rm -v "$PWD":/w -w /w kicad/kicad:9.0 kicad-cli "$@"; }
fi
run sch erc --exit-code-violations -o out/erc.rpt rideonesp32.kicad_sch
run sch erc --exit-code-violations -o out/erc-variant_b.rpt variant_b/variant_b.kicad_sch
run sch export netlist --format kicadsexpr -o out/rideonesp32.net rideonesp32.kicad_sch
python3 gen/check_netlist.py out/rideonesp32.net
run sch export pdf -o rideonesp32.pdf rideonesp32.kicad_sch
run sch export pdf -o variant_b/variant_b.pdf variant_b/variant_b.kicad_sch
echo "ERC clean, netlist matches, PDFs written."
