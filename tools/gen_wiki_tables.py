#!/usr/bin/env python3
"""
Regenerates the engine-derived tables in docs/ from the real C source.

How it works:
  1. Compiles tools/wiki_tables.c together with src/cards.c and src/rules.c
     (the same scoring code the game uses) into a temporary binary.
  2. Runs it and splits its output into named fragments.
  3. Replaces every block in docs/ that looks like:

         <!-- BEGIN GENERATED: yaku-values -->
         ...old content...
         <!-- END GENERATED: yaku-values -->

     with the freshly generated content.

The difficulty curve is the one exception to step 1: ai_get_accuracy() in src/ai.c is
static, so it cannot be linked into the binary. Its branch formulas are read out of the
source and evaluated here instead, and the chart is written to
docs/assets/difficulty-curve.svg. If ai_get_accuracy() is ever reshaped the generator stops
with an error rather than drawing a stale chart, so Deus can be retuned freely.

Usage:
  python3 tools/gen_wiki_tables.py           # rewrite docs/ in place
  python3 tools/gen_wiki_tables.py --check   # exit 1 if docs/ is out of date (for CI)

Requires gcc. The binary is built in a temp directory and never committed.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
SOURCES = [ROOT / "tools" / "wiki_tables.c", ROOT / "src" / "cards.c", ROOT / "src" / "rules.c"]
INCLUDE = ROOT / "src"

BLOCK_RE = re.compile(
    r"(<!-- BEGIN GENERATED: (?P<name>[a-z0-9-]+) -->\n)(?P<body>.*?)(<!-- END GENERATED: (?P=name) -->)",
    re.DOTALL,
)


def build_and_run(tmpdir: pathlib.Path) -> str:
    exe = tmpdir / "wiki_tables"
    cmd = ["gcc", "-std=c99", "-O2", "-Wall", "-Wextra", f"-I{INCLUDE}"]
    cmd += [str(s) for s in SOURCES] + ["-o", str(exe)]
    subprocess.run(cmd, check=True)
    result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
    return result.stdout


def parse_fragments(output: str) -> dict:
    fragments = {}
    current = None
    buf = []
    for line in output.splitlines():
        if line.startswith("@@FRAGMENT "):
            current = line.split(" ", 1)[1].strip()
            buf = []
        elif line.strip() == "@@END":
            if current is None:
                raise SystemExit("generator: @@END without @@FRAGMENT")
            fragments[current] = "\n".join(buf).strip("\n") + "\n"
            current = None
        elif current is not None:
            buf.append(line)
    if current is not None:
        raise SystemExit(f"generator: fragment '{current}' never closed")
    return fragments


# --- difficulty chart -----------------------------------------------------------------
# Written straight from the formulas in src/ai.c into docs/assets/difficulty-curve.svg,
# because ai_get_accuracy() is static and cannot be linked into the table binary.
AI_SRC = ROOT / "src" / "ai.c"
CHART_ASSET = DOCS / "assets" / "difficulty-curve.svg"
CHART_FRAGMENT = "difficulty-curve"
CHART_NODES = 12          # campaign opponents; ai_get_accuracy() clamps boss_id to 0..11
CHART_TOP = 100
CHART_ELSE_BAND = "MASTER"  # the branch with no DIFFICULTY_ constant is the one left over
CHART_W, CHART_H = 1000, 620
CHART_ML, CHART_MR, CHART_MT, CHART_MB = 70, 26, 76, 170
CHART_PLOT_W = CHART_W - CHART_ML - CHART_MR
CHART_PLOT_H = CHART_H - CHART_MT - CHART_MB
CHART_SLOT = CHART_PLOT_W / float(CHART_NODES + 1)
CHART_AXIS_Y = CHART_MT + CHART_PLOT_H
CHART_INK = "#64f0ff"       # the game's TEAL base
CHART_INK_DIM = "#2dafc3"   # ... and its dim
CHART_BG = "#0b0e0b"
CHART_DEUS = "#9aa0a6"      # desaturated: Deus brings its own mono atlas

BRANCH_RE = re.compile(
    r"else\s+if\s*\(\s*diff\s*==\s*DIFFICULTY_(?P<elif>\w+)\s*\)"
    r"|if\s*\(\s*diff\s*==\s*DIFFICULTY_(?P<if>\w+)\s*\)"
    r"|else\s*\{"
)
RETURN_RE = re.compile(r"return\s+([^;]+);")
SAFE_EXPR_RE = re.compile(r"^[\d+\-*/()\s]+$")


def chart_x(node: int) -> float:
    """Column centre; node 12 is the Deus column."""
    return CHART_ML + CHART_SLOT * (node + 0.5)


def chart_y(pct: int) -> float:
    return CHART_MT + CHART_PLOT_H * (1 - pct / float(CHART_TOP))


def chart_branches() -> dict:
    """Every `diff == DIFFICULTY_x` branch of ai_get_accuracy(), mapped to its return text."""
    text = AI_SRC.read_text(encoding="utf-8")
    sig = re.search(r"\bai_get_accuracy\s*\([^)]*\)\s*\{", text)
    if not sig:
        raise SystemExit("generator: ai_get_accuracy() is gone from src/ai.c - fix or drop the chart")
    start = text.index("{", sig.start()) + 1
    depth, i = 1, start
    while depth and i < len(text):
        depth += (text[i] == "{") - (text[i] == "}")
        i += 1
    body = text[start:i - 1]

    branches = {}
    for m in BRANCH_RE.finditer(body):
        band = m.group("elif") or m.group("if") or CHART_ELSE_BAND
        ret = RETURN_RE.search(body, m.end())
        if ret is None:
            raise SystemExit(f"generator: ai_get_accuracy() branch '{band}' returns nothing")
        if band in branches:
            raise SystemExit(f"generator: ai_get_accuracy() has two '{band}' branches")
        branches[band] = ret.group(1).strip()
    missing = {"DEUS", "EASY", "HARD", "MASTER"} - set(branches)
    if missing:
        raise SystemExit("generator: ai_get_accuracy() no longer branches on "
                         + ", ".join(sorted(missing)) + " - fix or drop the chart")
    return branches


def chart_curves() -> dict:
    """{EASY|HARD|MASTER: 12 percentages} plus DEUS: one percentage, from the C formulas.

    boss_id is substituted with the index and the remainder is checked against a whitelist of
    digits and + - * / ( ) before eval runs with no builtins, so nothing but arithmetic is
    possible. C's integer division is Python's //, hence the rewrite.
    """
    curves = {}
    for band, expr in chart_branches().items():
        values = []
        for n in range(CHART_NODES):
            # boss_id becomes the literal index, so only digits and operators can remain.
            expr_n = expr.replace("boss_id", str(n)).replace("/", "//")
            if not SAFE_EXPR_RE.match(expr_n):
                raise SystemExit(f"generator: cannot evaluate ai_get_accuracy() expression {expr!r}")
            values.append(eval(expr_n, {"__builtins__": {}}))  # noqa: S307
        for value in values:
            if not isinstance(value, int) or not 0 <= value <= CHART_TOP:
                raise SystemExit(f"generator: ai_get_accuracy() {band} is not a percentage: {value}")
        if band == "DEUS":
            if len(set(values)) != 1:
                raise SystemExit("generator: DEUS accuracy is not flat across the roster")
            curves["DEUS"] = values[0]
        else:
            curves[band] = values
    return curves


def chart_desc(curves: dict) -> str:
    """The numbers as prose, so the chart reads without being seen."""
    parts = [f"{band.lower()} " + ", ".join(f"{v}%" for v in curves[band])
             for band in ("EASY", "HARD", "MASTER")]
    parts.append(f"deus {curves['DEUS']}% for every opponent")
    return "Opponents 1 to 12, accuracy by difficulty: " + "; ".join(parts) + "."


def render_chart_svg(curves: dict) -> str:
    """One column per opponent: Easy at the bottom, Master at the top, Deus above them all."""
    easy, hard, master, deus = curves["EASY"], curves["HARD"], curves["MASTER"], curves["DEUS"]
    dots = (("EASY", easy, 3.2, "none"), ("HARD", hard, 4.2, CHART_INK_DIM), ("MASTER", master, 5.2, CHART_INK))
    out = []
    add = out.append

    add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {CHART_W} {CHART_H}" role="img" '
        'font-family="monospace">')
    add("<title>AI decision accuracy per opponent</title>")
    add("<desc>" + chart_desc(curves) + "</desc>")
    add(f'<rect x="0.5" y="0.5" width="{CHART_W - 1}" height="{CHART_H - 1}" rx="10" fill="{CHART_BG}" '
        f'stroke="{CHART_INK_DIM}" stroke-opacity="0.45"/>')
    add(f'<text x="{CHART_ML}" y="34" fill="{CHART_INK}" font-size="21">'
        "AI DECISION ACCURACY PER OPPONENT</text>")
    add(f'<text x="{CHART_ML}" y="54" fill="{CHART_INK_DIM}" font-size="12">'
        "chance the CPU plays its best move - from ai_get_accuracy() in src/ai.c</text>")

    for pct in range(0, CHART_TOP + 1, 20):
        y = chart_y(pct)
        add(f'<line x1="{CHART_ML}" y1="{y:.1f}" x2="{CHART_ML + CHART_PLOT_W}" y2="{y:.1f}" '
            'stroke="#ffffff" stroke-opacity="0.07"/>')
        add(f'<text x="{CHART_ML - 10}" y="{y + 4:.1f}" fill="{CHART_INK_DIM}" font-size="12" '
            f'text-anchor="end">{pct}%</text>')
    for pct, label in ((max(easy), f"Easy max {max(easy)}%"),
                       (min(master), f"Master min {min(master)}%")):
        y = chart_y(pct)
        add(f'<line x1="{CHART_ML}" y1="{y:.1f}" x2="{CHART_ML + CHART_PLOT_W}" y2="{y:.1f}" '
            f'stroke="{CHART_INK}" stroke-opacity="0.35" stroke-dasharray="5 5"/>')
        add(f'<text x="{CHART_ML + CHART_PLOT_W - 4}" y="{y - 8:.1f}" fill="{CHART_INK}" font-size="11" '
            f'text-anchor="end">{label}</text>')

    for n in range(CHART_NODES):
        cx = chart_x(n)
        add(f'<rect x="{cx - 15:.1f}" y="{chart_y(master[n]):.1f}" width="30" '
            f'height="{chart_y(easy[n]) - chart_y(master[n]):.1f}" fill="{CHART_INK}" fill-opacity="0.08"/>')
        add(f'<line x1="{cx:.1f}" y1="{chart_y(easy[n]):.1f}" x2="{cx:.1f}" y2="{chart_y(master[n]):.1f}" '
            f'stroke="{CHART_INK_DIM}" stroke-opacity="0.55" stroke-width="1.5"/>')
        for _, values, r, fill in dots:
            add(f'<circle cx="{cx:.1f}" cy="{chart_y(values[n]):.1f}" r="{r}" fill="{fill}" '
                f'stroke="{CHART_INK}" stroke-width="1.6"/>')
        add(f'<text x="{cx:.1f}" y="{CHART_AXIS_Y + 22}" fill="{CHART_INK_DIM}" font-size="12" '
            f'text-anchor="middle">{n + 1:02d}</text>')
        if n == 0:
            # One worked example; the table under the chart carries every number.
            for _, values, _, _ in dots:
                add(f'<text x="{cx + 19:.1f}" y="{chart_y(values[n]) + 4:.1f}" fill="{CHART_INK}" '
                    f'font-size="11">{values[n]}</text>')

    cx = chart_x(CHART_NODES)
    div = cx - CHART_SLOT / 2 - 4
    add(f'<line x1="{div:.1f}" y1="{CHART_MT - 16}" x2="{div:.1f}" y2="{CHART_AXIS_Y + 30}" '
        'stroke="#ffffff" stroke-opacity="0.14" stroke-dasharray="4 6"/>')
    add(f'<line x1="{cx:.1f}" y1="{chart_y(deus) + 7:.1f}" x2="{cx:.1f}" y2="{CHART_AXIS_Y}" '
        f'stroke="{CHART_DEUS}" stroke-opacity="0.3" stroke-dasharray="3 5"/>')
    add(f'<rect x="{cx - 22:.1f}" y="{chart_y(deus) - 5:.1f}" width="44" height="10" rx="2" '
        f'fill="{CHART_DEUS}"/>')
    add(f'<text x="{CHART_ML + CHART_PLOT_W - 4}" y="{chart_y(deus) - 14:.1f}" fill="{CHART_DEUS}" '
        f'font-size="11" text-anchor="end">always {deus}%</text>')
    add(f'<text x="{cx:.1f}" y="{CHART_AXIS_Y + 22}" fill="#c8ccd0" font-size="12" '
        'text-anchor="middle">DEUS</text>')

    tip_x, tip_y = CHART_ML + CHART_PLOT_W + 12, CHART_AXIS_Y
    add(f'<line x1="{CHART_ML}" y1="{tip_y}" x2="{tip_x}" y2="{tip_y}" stroke="{CHART_INK_DIM}" '
        'stroke-opacity="0.6" stroke-width="1.5"/>')
    add(f'<path d="M{tip_x} {tip_y - 6} L{tip_x + 10} {tip_y} L{tip_x} {tip_y + 6} Z" '
        f'fill="{CHART_INK_DIM}" fill-opacity="0.6"/>')
    add(f'<line x1="{CHART_ML}" y1="{CHART_MT - 18}" x2="{CHART_ML}" y2="{CHART_AXIS_Y}" '
        f'stroke="{CHART_INK_DIM}" stroke-opacity="0.6" stroke-width="1.5"/>')
    add(f'<path d="M{CHART_ML - 6} {CHART_MT - 18} L{CHART_ML} {CHART_MT - 28} L{CHART_ML + 6} '
        f'{CHART_MT - 18} Z" fill="{CHART_INK_DIM}" fill-opacity="0.6"/>')

    lx, ly = float(CHART_ML), CHART_AXIS_Y + 58
    for band, _, r, fill in dots:
        add(f'<circle cx="{lx + 5:.1f}" cy="{ly - 4:.1f}" r="{r}" fill="{fill}" stroke="{CHART_INK}" '
            'stroke-width="1.6"/>')
        add(f'<text x="{lx + 14:.1f}" y="{ly}" fill="{CHART_INK_DIM}" font-size="12">{band}</text>')
        lx += 14 + 7.2 * len(band) + 26
    add(f'<rect x="{lx:.1f}" y="{ly - 12:.1f}" width="12" height="14" fill="{CHART_INK}" '
        'fill-opacity="0.12"/>')
    add(f'<text x="{lx + 18:.1f}" y="{ly}" fill="{CHART_INK_DIM}" font-size="12">'
        "spread across that opponent</text>")
    add(f'<text x="{CHART_ML}" y="{ly + 28}" fill="{CHART_INK_DIM}" fill-opacity="0.85" font-size="12">'
        "the 15s/30s timer, the ruleset and the mono Deus atlas change your pressure, not his accuracy"
        "</text>")
    add("</svg>")
    return "\n".join(out) + "\n"


def render_chart_markdown(curves: dict) -> str:
    """The same numbers as a table, so the page still reads if the image does not."""
    easy, hard, master, deus = curves["EASY"], curves["HARD"], curves["MASTER"], curves["DEUS"]
    lines = [
        "![Chart: chance of an optimal CPU move for each opponent on Easy, Hard and Master, "
        "with Deus above them all](../assets/difficulty-curve.svg)",
        "",
        "| Opponent | Easy | Hard | Master |",
        "|---|---:|---:|---:|",
    ]
    lines += [f"| {n + 1:02d} | {easy[n]}% | {hard[n]}% | {master[n]}% |" for n in range(CHART_NODES)]
    lines.append(f"| **13 · Deus** | {deus}% | {deus}% | {deus}% |")
    ramps = [max(v) - min(v) for v in (easy, hard, master)]
    lines.append(f"| *Ramp, 01 to 12* | +{ramps[0]} | +{ramps[1]} | +{ramps[2]} |")
    return "\n".join(lines)


def splice(text: str, fragments: dict, seen: set) -> str:
    def repl(m):
        name = m.group("name")
        if name not in fragments:
            raise SystemExit(f"generator: docs reference unknown fragment '{name}'")
        seen.add(name)
        return m.group(1) + "\n" + fragments[name] + "\n" + m.group(4)

    return BLOCK_RE.sub(repl, text)


def main() -> int:
    check_only = "--check" in sys.argv[1:]

    for src in [*SOURCES, AI_SRC]:
        if not src.exists():
            raise SystemExit(f"generator: missing source {src}")

    with tempfile.TemporaryDirectory() as tmp:
        fragments = parse_fragments(build_and_run(pathlib.Path(tmp)))
    curves = chart_curves()
    fragments[CHART_FRAGMENT] = render_chart_markdown(curves)

    seen = set()
    stale = []
    for md in sorted(DOCS.rglob("*.md")):
        original = md.read_text(encoding="utf-8")
        updated = splice(original, fragments, seen)
        if updated != original:
            stale.append(md)
            if not check_only:
                md.write_text(updated, encoding="utf-8")

    chart = render_chart_svg(curves)
    current = CHART_ASSET.read_text(encoding="utf-8") if CHART_ASSET.exists() else None
    if current != chart:
        stale.append(CHART_ASSET)
        if not check_only:
            CHART_ASSET.parent.mkdir(parents=True, exist_ok=True)
            CHART_ASSET.write_text(chart, encoding="utf-8")

    unused = sorted(set(fragments) - seen)
    if unused:
        print(f"note: generated fragments not placed in any page: {', '.join(unused)}")

    if check_only:
        if stale:
            print("docs are out of date with the engine source. Run: python3 tools/gen_wiki_tables.py")
            for p in stale:
                print(f"  stale: {p.relative_to(ROOT)}")
            return 1
        print("docs are up to date with the engine source.")
        return 0

    print(f"regenerated {len(stale)} file(s)." if stale else "docs already up to date.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
