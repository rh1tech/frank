#!/usr/bin/env bash
#
# release.sh - Automate a FRANK Next hardware release.
#
# Bumps the board VERSION and DATE (today), gates on a clean ERC and DRC
# (errors, warnings, and schematic/PCB parity - any of it stops the
# release), then produces:
#   gerbers/<project>_<major>_<minor>.zip   (gerbers + drill, JLCPCB-ready)
#   docs/<major>/<minor>/schematics.pdf
#   docs/<major>/<minor>/assembly.pdf       (via Board2Pdf)
#   docs/<major>/<minor>/bom.html           (via InteractiveHtmlBom)
#
# Usage: ./release.sh [version]
#   version   Optional. e.g. "1.02". If omitted you will be prompted,
#             with (latest + 0.01) offered as the default.
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Paths & constants
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PROJECT_PRO="$(ls -1 ./*.kicad_pro 2>/dev/null | head -1 || true)"
if [ -z "$PROJECT_PRO" ]; then
    echo "error: no .kicad_pro found in $SCRIPT_DIR" >&2
    exit 1
fi
PROJECT_NAME="$(basename "$PROJECT_PRO" .kicad_pro)"
PROJECT_SCH="$SCRIPT_DIR/$PROJECT_NAME.kicad_sch"
PROJECT_PCB="$SCRIPT_DIR/$PROJECT_NAME.kicad_pcb"

KICAD_APP="/Applications/KiCad/KiCad.app"
KICAD_CLI="$KICAD_APP/Contents/MacOS/kicad-cli"
KICAD_PYTHON="$KICAD_APP/Contents/Frameworks/Python.framework/Versions/Current/bin/python3"
command -v "$KICAD_CLI" >/dev/null 2>&1 || KICAD_CLI="kicad-cli"

TOOLS_DIR="$SCRIPT_DIR/.release-tools"
VENV_DIR="$TOOLS_DIR/venv"
VENV_MARKER="$VENV_DIR/.ready"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/release-${PROJECT_NAME}.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------
C_RESET=$'\033[0m'; C_BOLD=$'\033[1m'; C_RED=$'\033[31m'; C_GREEN=$'\033[32m'; C_YELLOW=$'\033[33m'; C_BLUE=$'\033[34m'

step()  { printf '\n%s==> %s%s\n' "$C_BOLD$C_BLUE" "$*" "$C_RESET"; }
ok()    { printf '  %s✓%s %s\n' "$C_GREEN" "$C_RESET" "$*"; }
warn()  { printf '  %s!%s %s\n' "$C_YELLOW" "$C_RESET" "$*"; }
fail()  { printf '  %s✗%s %s\n' "$C_RED" "$C_RESET" "$*" >&2; }
die()   { fail "$*"; exit 1; }

# Run kicad-cli quietly; on failure, dump its captured output and abort.
# (kicad-cli is noisy on stderr with harmless Fontconfig warnings, so we
# always capture rather than let it splatter the terminal on success.)
run_kicad_cli() {
    local log="$WORK_DIR/kicad-cli-$$-$RANDOM.log"
    if ! "$KICAD_CLI" "$@" >"$log" 2>&1; then
        fail "kicad-cli $1 failed:"
        cat "$log" >&2
        exit 1
    fi
}

# ---------------------------------------------------------------------------
# 0. Bootstrap the tool venv (InteractiveHtmlBom + Board2Pdf)
#
# Both need KiCad's own `pcbnew` python module, which is a compiled
# extension tied to KiCad's bundled interpreter - it cannot be bridged
# into a different Python via PYTHONPATH. So the venv is built FROM
# KiCad's own python with --system-site-packages.
#
# Board2Pdf 1.9.x (the only version on PyPI at time of writing) targets
# KiCad <=9 and calls two PCB_PLOT_PARAMS methods that KiCad 10 renamed/
# removed (SetPlotInvisibleText, SetPlotViaOnMaskLayer). Both call sites
# are patched to no-op safely when the method is absent. This patch is
# idempotent and re-applied (harmlessly) on every bootstrap.
# ---------------------------------------------------------------------------
bootstrap_tools() {
    step "Preparing release tools"

    if [ ! -x "$KICAD_PYTHON" ]; then
        die "KiCad python not found at $KICAD_PYTHON - is KiCad installed?"
    fi

    if [ ! -f "$VENV_MARKER" ]; then
        warn "first run: building tool venv (this takes a minute)"
        rm -rf "$VENV_DIR"
        "$KICAD_PYTHON" -m venv --system-site-packages "$VENV_DIR"
        "$VENV_DIR/bin/pip" install --quiet --upgrade pip "packaging>=26.2" setuptools wheel setuptools-scm
        "$VENV_DIR/bin/pip" install --quiet InteractiveHtmlBom
        "$VENV_DIR/bin/pip" install --quiet --no-build-isolation board2pdf
        touch "$VENV_MARKER"
    fi

    python3 - "$VENV_DIR/lib/python3.9/site-packages/board2pdf/plot.py" <<'PY'
import re, sys
path = sys.argv[1]
s = open(path).read()
changed = False
for method in ("SetPlotInvisibleText", "SetPlotViaOnMaskLayer"):
    pat = re.compile(r'^([ \t]*)plot_options\.' + method + r'\(([^)]*)\)\s*$', re.M)
    def guard(m):
        indent, args = m.group(1), m.group(2)
        return (f'{indent}if hasattr(plot_options, "{method}"):\n'
                f'{indent}    plot_options.{method}({args})')
    new_s, n = pat.subn(guard, s)
    if n:
        s = new_s
        changed = True
if changed:
    open(path, "w").write(s)
    print("  patched board2pdf for KiCad 10 API compatibility")
PY

    ok "tool venv ready ($VENV_DIR)"
}

# ---------------------------------------------------------------------------
# 1. Ask for the version to release
# ---------------------------------------------------------------------------
ask_version() {
    step "Version"

    local current
    current="$(python3 -c "
import json
print(json.load(open('$PROJECT_PRO'))['text_variables'].get('VERSION','1.00'))
")"

    local major minor default_version
    major="${current%%.*}"
    minor="${current#*.}"
    minor=$((10#$minor + 1))
    if [ "$minor" -gt 99 ]; then
        major=$((major + 1))
        minor=0
    fi
    default_version="$(printf '%s.%02d' "$major" "$minor")"

    echo "  current released version: $current"
    if [ -n "${NEW_VERSION_ARG:-}" ]; then
        NEW_VERSION="$NEW_VERSION_ARG"
        echo "  version to release [$default_version]: $NEW_VERSION (from command line)"
    else
        local input
        read -r -p "  version to release [$default_version]: " input
        NEW_VERSION="${input:-$default_version}"
    fi

    [[ "$NEW_VERSION" =~ ^[0-9]+\.[0-9]{2}$ ]] || die "version must look like 1.02 (major.two-digit-minor)"

    VER_MAJOR="${NEW_VERSION%%.*}"
    VER_MINOR="${NEW_VERSION#*.}"
    GERBER_ZIP="$SCRIPT_DIR/gerbers/${PROJECT_NAME}_${VER_MAJOR}_${VER_MINOR}.zip"
    DOCS_DIR="$SCRIPT_DIR/docs/$VER_MAJOR/$VER_MINOR"

    if [ -e "$GERBER_ZIP" ] || [ -d "$DOCS_DIR" ]; then
        warn "release artifacts for $NEW_VERSION already exist"
        read -r -p "  overwrite? [y/N]: " confirm
        [[ "$confirm" =~ ^[Yy]$ ]] || die "aborted by user"
    fi

    ok "releasing version $NEW_VERSION"
}

# ---------------------------------------------------------------------------
# 2. Write the new version + today's date into the board metadata, verify
#    both show up in the schematic and the PCB before doing anything
#    expensive.
#
# DATE and VERSION are both project text_variables, resolved via the same
# \${...} mechanism in the title_block of every .kicad_sch/.kicad_pcb.
# InteractiveHtmlBom reads the PCB title block's date field through the
# identical pcbnew.ExpandTextVars() path, so fixing this one source also
# fixes the date shown in bom.html - nothing BOM-specific is needed.
# ---------------------------------------------------------------------------
set_metadata() {
    NEW_DATE="$(date +%Y-%m-%d)"
    step "Updating board metadata to $NEW_VERSION, $NEW_DATE"

    python3 - "$PROJECT_PRO" "$NEW_VERSION" "$NEW_DATE" <<'PY'
import json, sys
path, version, today = sys.argv[1], sys.argv[2], sys.argv[3]
d = json.load(open(path))
tv = d.setdefault("text_variables", {})
tv["VERSION"] = version
tv["DATE"] = today
json.dump(d, open(path, "w"), indent=2)
open(path, "a").write("\n")
PY
    ok "$PROJECT_NAME.kicad_pro: text_variables.VERSION = $NEW_VERSION, DATE = $NEW_DATE"

    # Keep the PCB's own cached copies of these variables in sync. This is
    # not what kicad-cli actually reads at plot time (it re-resolves
    # \${VERSION}/\${DATE} live from .kicad_pro), but leaving them stale
    # would be misleading if the .kicad_pcb is ever opened standalone.
    if grep -q '(property "VERSION" "' "$PROJECT_PCB"; then
        sed -i '' -E 's/\(property "VERSION" "[^"]*"\)/(property "VERSION" "'"$NEW_VERSION"'")/' "$PROJECT_PCB"
        ok "$PROJECT_NAME.kicad_pcb: cached VERSION property updated"
    fi
    if grep -q '(property "DATE" "' "$PROJECT_PCB"; then
        sed -i '' -E 's/\(property "DATE" "[^"]*"\)/(property "DATE" "'"$NEW_DATE"'")/' "$PROJECT_PCB"
        ok "$PROJECT_NAME.kicad_pcb: cached DATE property updated"
    fi

    echo "  verifying the new revision and date render in both documents..."

    "$KICAD_CLI" sch export pdf -o "$WORK_DIR/verify_sch.pdf" "$PROJECT_SCH" >/dev/null 2>&1
    "$KICAD_CLI" pcb export pdf --include-border-title -l Edge.Cuts \
        -o "$WORK_DIR/verify_pcb.pdf" "$PROJECT_PCB" >/dev/null 2>&1

    local sch_text pcb_text
    sch_text="$(pdftotext -layout "$WORK_DIR/verify_sch.pdf" - 2>/dev/null)"
    pcb_text="$(pdftotext -layout "$WORK_DIR/verify_pcb.pdf" - 2>/dev/null)"

    # Note: these use a here-string (<<<), not a `echo ... | grep -q` pipe.
    # grep -q/-m exits the instant it finds a match, closing its read end
    # early; under `set -o pipefail` that SIGPIPEs the writer on the other
    # end of a real pipe and the whole pipeline is reported as failed even
    # though grep DID match. A here-string has no second process to SIGPIPE.
    grep -qm1 "Rev: $NEW_VERSION" <<< "$sch_text" || die "schematic title block does not show Rev: $NEW_VERSION after the edit"
    grep -qm1 "Rev: $NEW_VERSION" <<< "$pcb_text" || die "PCB title block does not show Rev: $NEW_VERSION after the edit"
    grep -qm1 "Date: $NEW_DATE"   <<< "$sch_text" || die "schematic title block does not show Date: $NEW_DATE after the edit"
    grep -qm1 "Date: $NEW_DATE"   <<< "$pcb_text" || die "PCB title block does not show Date: $NEW_DATE after the edit"

    ok "schematic title block: Rev: $NEW_VERSION, Date: $NEW_DATE"
    ok "PCB title block:       Rev: $NEW_VERSION, Date: $NEW_DATE"
}

# ---------------------------------------------------------------------------
# 3. ERC gate - stop on ANY error or warning
# ---------------------------------------------------------------------------
check_erc() {
    step "Running ERC"

    local report="$WORK_DIR/erc.json"
    "$KICAD_CLI" sch erc --severity-error --severity-warning --format json \
        -o "$report" "$PROJECT_SCH" >/dev/null 2>&1 || true

    local count
    count="$(python3 -c "
import json
d = json.load(open('$report'))
n = sum(len(sheet.get('violations', [])) for sheet in d.get('sheets', []))
print(n)
")"

    if [ "$count" -gt 0 ]; then
        fail "ERC found $count issue(s) - release stopped"
        python3 -c "
import json
d = json.load(open('$report'))
for sheet in d.get('sheets', []):
    for v in sheet.get('violations', []):
        print(f\"    [{v['severity']}] {v['type']}: {v['description']}\")
"
        exit 1
    fi

    ok "ERC clean (0 errors, 0 warnings)"
}

# ---------------------------------------------------------------------------
# DRC gate - stop on ANY error or warning, including schematic/PCB parity.
#
# Runs with --refill-zones --save-board: DRC checked against a stale zone
# fill is checking the wrong copper, and a stale fill left in place would
# also make its way into the gerbers exported later in this same run. So
# this step both verifies AND leaves the board with a fresh, saved fill -
# by design, not a side effect to be surprised by.
# ---------------------------------------------------------------------------
check_drc() {
    step "Running DRC"

    local report="$WORK_DIR/drc.json"
    "$KICAD_CLI" pcb drc --refill-zones --save-board --schematic-parity \
        --severity-error --severity-warning --format json \
        -o "$report" "$PROJECT_PCB" >/dev/null 2>&1 || true

    local v_count u_count p_count count
    v_count="$(python3 -c "import json; print(len(json.load(open('$report'))['violations']))")"
    u_count="$(python3 -c "import json; print(len(json.load(open('$report'))['unconnected_items']))")"
    p_count="$(python3 -c "import json; print(len(json.load(open('$report'))['schematic_parity']))")"
    count=$((v_count + u_count + p_count))

    if [ "$count" -gt 0 ]; then
        fail "DRC found $count issue(s) - release stopped"
        python3 -c "
import json
d = json.load(open('$report'))
for v in d['violations']:
    print(f\"    [{v['severity']}] {v['type']}: {v['description']}\")
for u in d['unconnected_items']:
    print(f\"    [unconnected] {u.get('description')}\")
for p in d['schematic_parity']:
    print(f\"    [parity] {p.get('description')}\")
"
        exit 1
    fi

    ok "DRC clean (0 violations, 0 unconnected, 0 parity issues, zone fills refreshed)"
}

# ---------------------------------------------------------------------------
# 4. Gerbers + drill -> gerbers/<project>_<major>_<minor>.zip
# ---------------------------------------------------------------------------
build_gerbers() {
    step "Generating gerbers + drill"

    local gdir="$WORK_DIR/gerbers"
    mkdir -p "$gdir"

    # Standard 4-layer fab set: 4 copper, 2 mask, 2 silk, 2 paste, edge cuts.
    local layers="F.Cu,In1.Cu,In2.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,B.SilkS,F.Paste,B.Paste,Edge.Cuts"

    run_kicad_cli pcb export gerbers \
        --layers "$layers" \
        --no-protel-ext \
        --precision 6 \
        -o "$gdir" "$PROJECT_PCB"

    run_kicad_cli pcb export drill \
        --format excellon \
        --excellon-separate-th \
        --excellon-zeros-format decimal \
        --excellon-units mm \
        -o "$gdir" "$PROJECT_PCB"

    local n_gbr n_drl
    n_gbr="$(find "$gdir" -name '*.gbr' | wc -l | tr -d ' ')"
    n_drl="$(find "$gdir" -name '*.drl' | wc -l | tr -d ' ')"
    [ "$n_gbr" -ge 11 ] || die "expected at least 11 gerber layers, got $n_gbr"
    [ "$n_drl" -eq 2 ] || die "expected 2 drill files (PTH+NPTH), got $n_drl"
    ok "$n_gbr gerber layers + $n_drl drill files"

    mkdir -p "$(dirname "$GERBER_ZIP")"
    rm -f "$GERBER_ZIP"
    ( cd "$gdir" && zip -q -X "$GERBER_ZIP" ./*.gbr ./*.drl )
    ok "$(basename "$GERBER_ZIP")"
}

# ---------------------------------------------------------------------------
# 5. Full schematic PDF -> docs/<major>/<minor>/schematics.pdf
# ---------------------------------------------------------------------------
build_schematic_pdf() {
    step "Generating schematic PDF"
    mkdir -p "$DOCS_DIR"
    run_kicad_cli sch export pdf -o "$DOCS_DIR/schematics.pdf" "$PROJECT_SCH"
    ok "docs/$VER_MAJOR/$VER_MINOR/schematics.pdf"
}

# ---------------------------------------------------------------------------
# 6. Assembly PDF via Board2Pdf -> docs/<major>/<minor>/assembly.pdf
# ---------------------------------------------------------------------------
build_assembly_pdf() {
    step "Generating assembly PDF (Board2Pdf)"

    local ini="$SCRIPT_DIR/board2pdf.config.ini"
    [ -f "$ini" ] || die "board2pdf.config.ini not found"

    local log="$WORK_DIR/board2pdf-stdout.log"
    local rc=0
    "$VENV_DIR/bin/board2pdf" "$PROJECT_PCB" \
            --ini "$ini" \
            --output "$DOCS_DIR/assembly.pdf" \
            --log WARN >"$log" 2>&1 || rc=$?

    # Board2Pdf unconditionally writes its own "board2pdf.log" next to the
    # .kicad_pcb (hardcoded to dirname(pcb_path) - unrelated to --log or
    # our cwd), which would otherwise dirty the project directory.
    rm -f "$SCRIPT_DIR/board2pdf.log"

    if [ "$rc" -ne 0 ]; then
        fail "Board2Pdf failed:"
        cat "$log" >&2
        exit 1
    fi

    ok "docs/$VER_MAJOR/$VER_MINOR/assembly.pdf"
}

# ---------------------------------------------------------------------------
# 7. Interactive BOM -> docs/<major>/<minor>/bom.html
#
# Flags are translated live from ibom.config.ini, so changes made through
# the KiCad GUI plugin dialog are picked up automatically on the next
# release without touching this script.
# ---------------------------------------------------------------------------
build_bom() {
    step "Generating interactive BOM"

    local ini="$SCRIPT_DIR/ibom.config.ini"
    [ -f "$ini" ] || die "ibom.config.ini not found"

    local flags_file="$WORK_DIR/ibom_flags.txt"
    python3 - "$ini" > "$flags_file" <<'PY'
import configparser, sys, shlex
c = configparser.ConfigParser()
c.read(sys.argv[1])
h, g = c["html_defaults"], c["general"]
f = c["fields"] if c.has_section("fields") else {}
args = []
if h.getboolean("dark_mode", fallback=False): args.append("--dark-mode")
if not h.getboolean("show_pads", fallback=True): args.append("--hide-pads")
if h.getboolean("show_fabrication", fallback=False): args.append("--show-fabrication")
if not h.getboolean("show_silkscreen", fallback=True): args.append("--hide-silkscreen")
if not h.getboolean("redraw_on_drag", fallback=True): args.append("--no-redraw-on-drag")
args += ["--highlight-pin1", h.get("highlight_pin1", "none")]
args += ["--board-rotation", h.get("board_rotation", "0")]
if h.getboolean("offset_back_rotation", fallback=False): args.append("--offset-back-rotation")
if h.get("checkboxes", ""): args += ["--checkboxes", h.get("checkboxes")]
args += ["--bom-view", h.get("bom_view", "left-right")]
args += ["--layer-view", h.get("layer_view", "FB")]
if not h.getboolean("compression", fallback=True): args.append("--no-compression")
if g.get("component_sort_order", ""): args += ["--sort-order", g.get("component_sort_order")]
if g.get("component_blacklist", ""): args += ["--blacklist", g.get("component_blacklist")]
if not g.getboolean("blacklist_virtual", fallback=True): args.append("--no-blacklist-virtual")
if g.getboolean("blacklist_empty_val", fallback=False): args.append("--blacklist-empty-val")
if g.getboolean("include_tracks", fallback=False): args.append("--include-tracks")
if g.getboolean("include_nets", fallback=False): args.append("--include-nets")
if f and f.get("show_fields", ""): args += ["--show-fields", f.get("show_fields")]
if f and f.get("group_fields", ""): args += ["--group-fields", f.get("group_fields")]
if f and f.getboolean("normalize_field_case", fallback=False): args.append("--normalize-field-case")
if f and f.get("dnp_field", ""): args += ["--dnp-field", f.get("dnp_field")]
print("\n".join(shlex.quote(a) for a in args))
PY

    IBOM_FLAGS=()
    while IFS= read -r line; do IBOM_FLAGS+=("$line"); done < "$flags_file"

    local log="$WORK_DIR/ibom.log"
    if ! "$VENV_DIR/bin/generate_interactive_bom" "$PROJECT_PCB" \
            --dest-dir "$DOCS_DIR" \
            --name-format "bom" \
            --no-browser \
            "${IBOM_FLAGS[@]}" >"$log" 2>&1
    then
        fail "InteractiveHtmlBom failed:"
        cat "$log" >&2
        exit 1
    fi

    ok "docs/$VER_MAJOR/$VER_MINOR/bom.html"
}

# ---------------------------------------------------------------------------
# 8. Verify every artifact exists and is a real, non-corrupt file
# ---------------------------------------------------------------------------
verify_release() {
    step "Verifying release artifacts"

    local all_ok=1

    check_file() {
        local path="$1" min_size="$2" label="$3"
        if [ ! -f "$path" ]; then
            fail "$label: missing ($path)"
            all_ok=0
            return
        fi
        local size
        size="$(wc -c < "$path" | tr -d ' ')"
        if [ "$size" -lt "$min_size" ]; then
            fail "$label: suspiciously small ($size bytes)"
            all_ok=0
            return
        fi
        ok "$label ($size bytes)"
    }

    check_file "$GERBER_ZIP" 10000 "gerbers zip"
    if [ -f "$GERBER_ZIP" ]; then
        if ! unzip -tq "$GERBER_ZIP" >/dev/null 2>&1; then
            fail "gerbers zip: failed integrity test"
            all_ok=0
        else
            local layers
            layers="$(unzip -l "$GERBER_ZIP" | grep -cE '\.(gbr|drl)$')"
            if [ "$layers" -lt 13 ]; then
                fail "gerbers zip: only $layers layer/drill files inside, expected >=13"
                all_ok=0
            else
                ok "gerbers zip: $layers layer/drill files, integrity OK"
            fi
        fi
    fi

    check_file "$DOCS_DIR/schematics.pdf" 20000 "schematics.pdf"
    if [ -f "$DOCS_DIR/schematics.pdf" ]; then
        local sch_final_text rev_count date_count
        sch_final_text="$(pdftotext -layout "$DOCS_DIR/schematics.pdf" - 2>/dev/null)"
        rev_count="$(grep -c "Rev: $NEW_VERSION" <<< "$sch_final_text" || true)"
        date_count="$(grep -c "Date: $NEW_DATE" <<< "$sch_final_text" || true)"
        if [ "$rev_count" -lt 1 ]; then
            fail "schematics.pdf: Rev: $NEW_VERSION not found in rendered text"
            all_ok=0
        elif [ "$date_count" -lt 1 ]; then
            fail "schematics.pdf: Date: $NEW_DATE not found in rendered text"
            all_ok=0
        else
            ok "schematics.pdf: Rev: $NEW_VERSION, Date: $NEW_DATE present on $rev_count sheet(s)"
        fi
    fi

    check_file "$DOCS_DIR/assembly.pdf" 50000 "assembly.pdf"
    if [ -f "$DOCS_DIR/assembly.pdf" ]; then
        local pages
        pages="$(pdfinfo "$DOCS_DIR/assembly.pdf" 2>/dev/null | awk -F': *' '/^Pages/{print $2}')"
        if [ -z "$pages" ] || [ "$pages" -lt 1 ]; then
            fail "assembly.pdf: could not read a valid page count"
            all_ok=0
        else
            ok "assembly.pdf: $pages page(s)"
        fi
    fi

    check_file "$DOCS_DIR/bom.html" 50000 "bom.html"
    if [ -f "$DOCS_DIR/bom.html" ]; then
        if grep -q 'pcbdata' "$DOCS_DIR/bom.html" 2>/dev/null; then
            ok "bom.html: contains embedded board data"
        else
            fail "bom.html: does not look like a real InteractiveHtmlBom output"
            all_ok=0
        fi
    fi

    if [ "$all_ok" -ne 1 ]; then
        die "one or more release artifacts failed verification"
    fi
}

# ---------------------------------------------------------------------------
main() {
    bootstrap_tools
    ask_version
    set_metadata
    check_erc
    check_drc
    build_gerbers
    build_schematic_pdf
    build_assembly_pdf
    build_bom
    verify_release

    step "Release $NEW_VERSION complete"
    echo "  gerbers/$(basename "$GERBER_ZIP")"
    echo "  docs/$VER_MAJOR/$VER_MINOR/schematics.pdf"
    echo "  docs/$VER_MAJOR/$VER_MINOR/assembly.pdf"
    echo "  docs/$VER_MAJOR/$VER_MINOR/bom.html"
    echo
    echo "  Nothing was committed to git. Review with 'git status' / 'git diff' and commit when ready."
}

if [ $# -ge 1 ]; then
    NEW_VERSION_ARG="$1"
fi

main
