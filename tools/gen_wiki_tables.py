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

    for src in SOURCES:
        if not src.exists():
            raise SystemExit(f"generator: missing source {src}")

    with tempfile.TemporaryDirectory() as tmp:
        fragments = parse_fragments(build_and_run(pathlib.Path(tmp)))

    seen = set()
    stale = []
    for md in sorted(DOCS.rglob("*.md")):
        original = md.read_text(encoding="utf-8")
        updated = splice(original, fragments, seen)
        if updated != original:
            stale.append(md)
            if not check_only:
                md.write_text(updated, encoding="utf-8")

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
