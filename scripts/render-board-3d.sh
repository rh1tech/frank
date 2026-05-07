#!/usr/bin/env bash
# Render high-quality ray-traced 3D images of every KiCad board into
# docs/boards/3d/. One isometric top render per board is produced for
# README hero shots. Add `--all` to also render flat top + bottom views.
#
# kicad-cli pcb render uses KiCad's built-in ray tracer (the same one
# behind 3D Viewer's "Raytracing" mode). No Blender, no STEP wrangling.
#
# Per-board solder mask color is injected into a temporary copy of the
# board directory before rendering, so the source PCBs stay untouched.
# The renderer is given the colors as explicit RGB hex values rather
# than KiCad's named palette ("Green", "Red", ...) — the named colors
# are translucent in the ray tracer, which lets copper layers show
# through and produces a brown tint on the rendered surface.
#
# After rendering we trim the transparent border with ImageMagick so
# each PNG hugs the board + its shadow.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HARDWARE_DIR="$REPO_ROOT/hardware"
OUT_DIR="$REPO_ROOT/docs/boards/3d"

# 3D models to skip per board, given as a pipe-separated list of
# substrings to match against the model's path (e.g. "ESP-01S" hides
# every model whose path contains that string). The KiCad GUI lets you
# toggle a per-instance "Show" checkbox, but kicad-cli ignores that
# flag, so we strip matching (model ...) blocks from the temp PCB
# before rendering.
hidden_models() {
  case "$1" in
    minifrank)  echo "ESP-01S_RevA" ;;
    *)          echo "" ;;
  esac
}

# Per-board solder-mask color as #RRGGBBAA.
#
# The alpha channel matters: a fully opaque mask hides every trace,
# but a translucent mask over copper (the default "None" finish)
# bleeds brown through and dirties every color — FRANK ends up
# orange-brown, FRANK PGA ends up brown-black, and so on.
#
# We work around this by switching the copper finish to ENIG (gold)
# in prepare_board() below. Gold under a partly-translucent mask
# tints toward yellow-green, which mostly disappears under green and
# stays neutral under red and black. With ENIG underneath we can keep
# the mask translucent enough (~85%) for traces to remain faintly
# visible without dirtying the surface color.
board_color() {
  case "$1" in
    frank)      echo "#B30F0FD9" ;;  # red
    frank_pga)  echo "#1A1A1AFF" ;;  # near-black (fully opaque to avoid ENIG gold bleed-through)
    minifrank)  echo "#0F8A3CD9" ;;  # PCB green
    microfrank) echo "#0F8A3CD9" ;;
    *)          echo "#0F8A3CD9" ;;
  esac
}

if command -v kicad-cli >/dev/null 2>&1; then
  KICAD_CLI="kicad-cli"
elif [ -x "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli" ]; then
  KICAD_CLI="/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
else
  echo "error: kicad-cli not found in PATH or /Applications/KiCad" >&2
  exit 1
fi

if command -v magick >/dev/null 2>&1; then
  MAGICK="magick"
elif [ -x "/opt/homebrew/bin/magick" ]; then
  MAGICK="/opt/homebrew/bin/magick"
else
  MAGICK=""
fi

# Render at moderate resolution; ImageMagick trim removes the
# transparent border so the final PNG ends up well under 1 MB.
WIDTH=1600
HEIGHT=1200
ISO_ROTATE="-25,0,-30"

# Per-board zoom. With auto-trim the zoom only affects "how much air
# around the board" survives before trim, so smaller is safer (just
# avoid going so small the board edges leave the canvas).
iso_zoom() {
  case "$1" in
    frank|frank_pga) echo "0.75" ;;
    minifrank)       echo "0.85" ;;
    microfrank)      echo "0.85" ;;
    *)               echo "0.85" ;;
  esac
}

mkdir -p "$OUT_DIR"

prepare_board() {
  local src_dir="$1"
  local color="$2"
  local tmp_dir="$3"
  local hidden="$4"

  cp -R "$src_dir/." "$tmp_dir/"
  python3 - "$tmp_dir" "$color" "$hidden" <<'PY'
import re
import sys
from pathlib import Path

tmp_dir = Path(sys.argv[1])
color = sys.argv[2]
hidden = [h for h in sys.argv[3].split("|") if h]

for pcb in tmp_dir.glob("*.kicad_pcb"):
    s = pcb.read_text()

    def strip_color(text, mask):
        return re.sub(
            r'(\(layer "' + re.escape(mask) + r'"\s*'
            r'\(type "[^"]+"\)\s*)'
            r'(\(color "[^"]*"\)\s*)?',
            r"\1",
            text,
            count=1,
        )

    def inject(text, mask):
        return re.sub(
            r'(\(layer "' + re.escape(mask) + r'"\s*'
            r'\(type "[^"]+"\))(\s*)(\(thickness)',
            rf'\1\2(color "{color}")\2\3',
            text,
            count=1,
        )

    s = strip_color(s, "F.Mask")
    s = strip_color(s, "B.Mask")
    s = inject(s, "F.Mask")
    s = inject(s, "B.Mask")

    # Switch the copper finish to ENIG (gold). Bleed-through from the
    # translucent mask then tints yellow-green instead of brown, which
    # keeps red boards red, green boards green, and black boards black.
    s = re.sub(
        r'\(copper_finish "[^"]*"\)',
        '(copper_finish "ENIG")',
        s,
        count=1,
    )

    # Strip out (model ...) blocks whose path matches one of the
    # hidden-model substrings. These are typically components the
    # designer disabled in the KiCad GUI ("Show" checkbox off), which
    # kicad-cli does not respect on its own. Walk the file token by
    # token so we can drop balanced s-expressions safely.
    if hidden:
        out = []
        i = 0
        n = len(s)
        while i < n:
            if s.startswith("(model", i):
                # Read up to the closing quote of the path string.
                j = s.find('"', i)
                k = s.find('"', j + 1) if j != -1 else -1
                path = s[j + 1 : k] if (j != -1 and k != -1) else ""
                if any(h in path for h in hidden):
                    # Walk paren depth from this (model ... until it
                    # closes, then drop the whole block (and any
                    # trailing whitespace up to the next newline).
                    depth = 0
                    end = i
                    while end < n:
                        c = s[end]
                        if c == "(":
                            depth += 1
                        elif c == ")":
                            depth -= 1
                            if depth == 0:
                                end += 1
                                break
                        end += 1
                    # Trim leading whitespace from the previously
                    # emitted output so the deleted block does not
                    # leave a blank-indented line behind.
                    while out and out[-1] in " \t":
                        out.pop()
                    # Skip a single trailing newline if present.
                    if end < n and s[end] == "\n":
                        end += 1
                    i = end
                    continue
            out.append(s[i])
            i += 1
        s = "".join(out)

    pcb.write_text(s)
PY
}

render() {
  local pcb="$1"
  local out_png="$2"
  local side="$3"
  local rotate="$4"
  local perspective="$5"
  local zoom="$6"

  local args=(
    pcb render
    --output "$out_png"
    --width "$WIDTH"
    --height "$HEIGHT"
    --side "$side"
    --quality high
    --floor
    --background transparent
  )
  if [ -n "$rotate" ]; then
    args+=(--rotate "$rotate")
  fi
  if [ "$perspective" = "1" ]; then
    args+=(--perspective)
  fi
  if [ -n "$zoom" ]; then
    args+=(--zoom "$zoom")
  fi
  args+=("$pcb")

  "$KICAD_CLI" "${args[@]}"

  # Trim the transparent border so the final image fits the PCB +
  # shadow tightly. A small fuzz tolerance handles antialiased edges
  # and the soft shadow falloff. Skip if ImageMagick is unavailable.
  if [ -n "$MAGICK" ]; then
    "$MAGICK" "$out_png" -bordercolor none -border 1 -trim +repage \
      "$out_png"
  fi
}

render_all=0
if [ "${1:-}" = "--all" ]; then
  render_all=1
fi

TMP_ROOT="$(mktemp -d -t kicad-3d.XXXXXX)"
trap 'rm -rf "$TMP_ROOT"' EXIT

shopt -s nullglob
for pcb_src in "$HARDWARE_DIR"/*/*.kicad_pcb; do
  board="$(basename "$(dirname "$pcb_src")")"
  color="$(board_color "$board")"

  hidden="$(hidden_models "$board")"
  echo "Preparing ${board} (${color}; hide: ${hidden:-none})..."
  tmp_dir="$TMP_ROOT/$board"
  mkdir -p "$tmp_dir"
  prepare_board "$(dirname "$pcb_src")" "$color" "$tmp_dir" "$hidden"
  pcb="$tmp_dir/$(basename "$pcb_src")"

  zoom="$(iso_zoom "$board")"
  echo "Rendering ${board} (isometric, zoom ${zoom})..."
  render "$pcb" "$OUT_DIR/$board-iso.png" "top" "$ISO_ROTATE" 1 "$zoom"

  if [ "$render_all" = "1" ]; then
    echo "Rendering ${board} (top flat)..."
    render "$pcb" "$OUT_DIR/$board-top.png" "top" "" 0 ""

    echo "Rendering ${board} (bottom flat)..."
    render "$pcb" "$OUT_DIR/$board-bottom.png" "bottom" "" 0 ""
  fi
done

echo "Done. Output in $OUT_DIR"
