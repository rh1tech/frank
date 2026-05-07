#!/usr/bin/env bash
# Render wireframe-style SVGs of every KiCad board into docs/boards/.
#
# For each board we produce a top and a bottom view by combining:
#   Edge.Cuts + F.Silkscreen + F.Fab  (top, normal)
#   Edge.Cuts + B.Silkscreen + B.Fab  (bottom, mirrored)
#
# F.Fab gives us the rectangular body outline of every component
# (resistors, caps, ICs) while silkscreen carries the human-facing
# labels the designer placed (port names, pin numbers, board title).
# We render the layers in two passes and strip text from the fab pass
# so the silkscreen labels are not duplicated by the fab reference
# designators.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HARDWARE_DIR="$REPO_ROOT/hardware"
OUT_DIR="$REPO_ROOT/docs/boards"

# Locate kicad-cli. Prefer PATH, fall back to the macOS app bundle.
if command -v kicad-cli >/dev/null 2>&1; then
  KICAD_CLI="kicad-cli"
elif [ -x "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli" ]; then
  KICAD_CLI="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
else
  echo "error: kicad-cli not found in PATH or /Applications/KiCad" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

# Run kicad-cli to plot the requested layers to a single SVG.
plot_layers() {
  local pcb="$1"
  local out_svg="$2"
  local layers="$3"
  local mirror="$4"
  local sketch_pads="${5:-0}"

  local args=(
    pcb export svg
    --output "$out_svg"
    --layers "$layers"
    --black-and-white
    --exclude-drawing-sheet
    --fit-page-to-board
    --page-size-mode 2
    --mode-single
  )
  if [ "$mirror" = "1" ]; then
    args+=(--mirror)
  fi
  if [ "$sketch_pads" = "1" ]; then
    # SMD package bodies on F.Fab/B.Fab don't include their leads, so
    # SOIC/QFN/QFP chips render as floating rectangles. This flag adds
    # pad outlines on the fab layer so the leads show up too.
    args+=(--sketch-pads-on-fab-layers)
  fi
  args+=("$pcb")

  "$KICAD_CLI" "${args[@]}" >/dev/null
}

# Build a top or bottom wireframe by combining a silkscreen pass and a
# text-stripped fab pass into a single SVG.
build_view() {
  local pcb="$1"
  local out_svg="$2"
  local silk_layers="$3"
  local fab_layers="$4"
  local mirror="$5"

  local tmp_silk
  local tmp_fab
  tmp_silk="$(mktemp -t kicad-silk.XXXXXX).svg"
  tmp_fab="$(mktemp -t kicad-fab.XXXXXX).svg"
  trap 'rm -f "$tmp_silk" "$tmp_fab"' RETURN

  plot_layers "$pcb" "$tmp_silk" "$silk_layers"            "$mirror" 0
  plot_layers "$pcb" "$tmp_fab"  "Edge.Cuts,${fab_layers}" "$mirror" 1

  python3 - "$tmp_silk" "$tmp_fab" "$out_svg" <<'PY'
import re
import sys
from pathlib import Path

MARGIN = 0.5  # mm

silk_path, fab_path, out_path = (Path(p) for p in sys.argv[1:4])
silk = silk_path.read_text()
fab = fab_path.read_text()

# Strip every text node from the fab pass so it contributes only the
# component body outlines, not the ref-designator labels.
fab = re.sub(r"<text\b[^>]*>.*?</text>", "", fab, flags=re.DOTALL)
fab = re.sub(
    r'<g class="stroked-text">.*?</g>',
    "",
    fab,
    flags=re.DOTALL,
)

# Pull only the body groups (everything inside the outermost <svg>) from
# the fab SVG so we can splice them into the silk SVG.
fab_body_match = re.search(r"<svg\b[^>]*>(.*)</svg>", fab, flags=re.DOTALL)
fab_body = fab_body_match.group(1) if fab_body_match else ""

# Inject fab body just before the closing </svg> of the silk pass.
combined = silk.replace("</svg>", fab_body + "</svg>", 1)

# Widen the 0.05 mm Edge.Cuts hairline so the outline survives at
# README display sizes.
combined = combined.replace("stroke-width:0.0500", "stroke-width:0.3000")

# Expand the viewBox by MARGIN on every side; the Edge.Cuts path runs
# flush to x=0,y=0 in KiCad output, and without margin half the stroke
# is clipped.
def expand(match):
    w = float(match.group("w"))
    h = float(match.group("h"))
    x = float(match.group("vx"))
    y = float(match.group("vy"))
    vw = float(match.group("vw"))
    vh = float(match.group("vh"))
    return (
        f'width="{w + 2 * MARGIN:.4f}mm" '
        f'height="{h + 2 * MARGIN:.4f}mm" '
        f'viewBox="{x - MARGIN:.4f} {y - MARGIN:.4f} '
        f'{vw + 2 * MARGIN:.4f} {vh + 2 * MARGIN:.4f}"'
    )

vb_pattern = re.compile(
    r'width="(?P<w>[\d.]+)mm"\s+height="(?P<h>[\d.]+)mm"\s+'
    r'viewBox="(?P<vx>[-\d.]+)\s+(?P<vy>[-\d.]+)\s+'
    r'(?P<vw>[\d.]+)\s+(?P<vh>[\d.]+)"'
)
vb_match = vb_pattern.search(combined)
if vb_match:
    combined = (
        combined[: vb_match.start()]
        + expand(vb_match)
        + combined[vb_match.end():]
    )
    bg_x = float(vb_match.group("vx")) - MARGIN
    bg_y = float(vb_match.group("vy")) - MARGIN
    bg_w = float(vb_match.group("vw")) + 2 * MARGIN
    bg_h = float(vb_match.group("vh")) + 2 * MARGIN
else:
    bg_x = bg_y = 0.0
    bg_w = bg_h = 0.0

# Inject a white background rect so the black-stroke wireframe is
# visible against dark GitHub themes.
if 'data-board-bg="1"' not in combined:
    open_svg = re.search(r"<svg\b[^>]*>", combined)
    if open_svg:
        end = open_svg.end()
        rect = (
            f'\n<rect data-board-bg="1" '
            f'x="{bg_x:.4f}" y="{bg_y:.4f}" '
            f'width="{bg_w:.4f}" height="{bg_h:.4f}" '
            f'fill="#ffffff"/>'
        )
        combined = combined[:end] + rect + combined[end:]

out_path.write_text(combined)
PY
}

shopt -s nullglob
for pcb in "$HARDWARE_DIR"/*/*.kicad_pcb; do
  board="$(basename "$(dirname "$pcb")")"
  echo "Rendering ${board}..."
  build_view "$pcb" "$OUT_DIR/$board-top.svg" \
    "Edge.Cuts,F.Silkscreen" "F.Fab" 0
  build_view "$pcb" "$OUT_DIR/$board-bottom.svg" \
    "Edge.Cuts,B.Silkscreen" "B.Fab" 1
done

echo "Done. Output in $OUT_DIR"
