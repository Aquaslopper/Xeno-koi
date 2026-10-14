#!/usr/bin/env python3
"""
Checks that the protected zones agree everywhere they are written down.

The zones live in three places:

  * .github/CODEOWNERS         - who GitHub itself asks for review
  * tools/guard_protected.sh   - zones the "Guard protected mechanics" check reports
  * tools/guard_stale.sh       - zones the "Guard stale protected revert" check reports

They are worded differently (CODEOWNERS patterns vs. git pathspecs), but they
must cover the same files:

  * every file CODEOWNERS protects must match a zone in both guard scripts,
    otherwise a contributor can change it without the guard saying anything;
  * every file matched by a guard zone must be protected by CODEOWNERS,
    otherwise the guard blocks files nobody protected.

Usage:
  python3 tools/check_protected_zones.py            # report, exit 1 on drift
  python3 tools/check_protected_zones.py --verbose  # list every match
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CODEOWNERS = ROOT / ".github" / "CODEOWNERS"
GUARD_PROTECTED = ROOT / "tools" / "guard_protected.sh"
GUARD_STALE = ROOT / "tools" / "guard_stale.sh"

BEGIN = "# BEGIN PROTECTED ZONES"
END = "# END PROTECTED ZONES"


def to_regex(pattern: str) -> re.Pattern[str]:
    """Turn a CODEOWNERS pattern or a git pathspec into a regex.

    `*` and `?` do not cross `/`; a trailing `/` means "everything below";
    a leading `/` (CODEOWNERS) anchors the pattern at the repository root.
    """
    pattern = pattern.strip().strip("'\"")
    if pattern.startswith("/"):
        pattern = pattern[1:]
    if pattern.endswith("/"):
        pattern += "**"
    out, i = "", 0
    while i < len(pattern):
        c = pattern[i]
        if c == "*":
            if pattern.startswith("**", i):
                out += ".*"
                i += 2
                continue
            out += "[^/]*"
        elif c == "?":
            out += "[^/]"
        else:
            out += re.escape(c)
        i += 1
    return re.compile("^" + out + "$")


def zone_block(path: pathlib.Path) -> list[tuple[str, str]]:
    """Read the `name: pathspec pathspec` lines between the ZONES markers."""
    lines = path.read_text(encoding="utf-8").splitlines()
    try:
        start = next(i for i, l in enumerate(lines) if l.strip().startswith(BEGIN))
        end = next(i for i, l in enumerate(lines) if l.strip().startswith(END))
    except StopIteration:  # pragma: no cover - layout error
        sys.exit(f"{path.relative_to(ROOT)}: missing '{BEGIN}' / '{END}' markers")
    zones = []
    for line in lines[start + 1 : end]:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            sys.exit(f"{path.relative_to(ROOT)}: cannot parse zone line: {line!r}")
        name, _, spec = line.partition(":")
        zones.append((name.strip(), spec.split()))
    return zones


def codeowners_patterns() -> list[str]:
    patterns = []
    for line in CODEOWNERS.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        patterns.append(line.split()[0])
    return patterns


def tracked_files() -> list[str]:
    out = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files"], capture_output=True, text=True, check=True
    ).stdout
    return [f for f in out.split("\n") if f]


def main() -> int:
    verbose = "--verbose" in sys.argv
    files = tracked_files()
    owner_patterns = codeowners_patterns()
    owner_rules = [(p, to_regex(p)) for p in owner_patterns]
    owner_protected = {
        f: [p for p, rx in owner_rules if rx.match(f)] for f in files
    }
    owner_protected = {f: ps for f, ps in owner_protected.items() if ps}

    problems: list[str] = []
    for script in (GUARD_PROTECTED, GUARD_STALE):
        zones = zone_block(script)
        name = str(script.relative_to(ROOT))
        if not zones:
            problems.append(f"{name}: no zones found")
            continue
        rules = [(z, p, to_regex(p)) for z, paths in zones for p in paths]

        for f, why in sorted(owner_protected.items()):
            if not any(rx.match(f) for _, _, rx in rules):
                problems.append(
                    f"{name}: {f} is protected by CODEOWNERS ({', '.join(why)}) "
                    f"but matches no zone in this script"
                )
        for f in sorted(files):
            if f in owner_protected:
                continue
            hit = sorted({z for z, _, rx in rules if rx.match(f)})
            if hit:
                problems.append(
                    f"{name}: zone {', '.join(hit)} matches {f}, but CODEOWNERS "
                    f"does not protect it"
                )

        print(f"{name}: {len(zones)} zone(s), {len(rules)} path pattern(s)")
        if verbose:
            for f in sorted(owner_protected):
                hit = sorted({z for z, _, rx in rules if rx.match(f)})
                print(f"  {f} -> {', '.join(hit)}")

    print(f"CODEOWNERS: {len(owner_patterns)} pattern(s) protecting {len(owner_protected)} file(s)")
    if problems:
        print("\nDRIFT DETECTED:")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("\nprotected zones agree (CODEOWNERS, guard_protected.sh, guard_stale.sh).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
