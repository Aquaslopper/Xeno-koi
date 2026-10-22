#!/usr/bin/env python3
"""Static checks for the preview's GLSL, because this sandbox has no GPU to compile on.

Not a compiler -- it cannot tell you a shader is valid. It catches the two mistakes that
actually cost time here, both of which compile fine in the game's own shaders and fail only
when the block is reused in another GLSL version:

  * reserved words. `patch` is legal in `#version 100` and `#version 330`, and reserved in
    `#version 300 es`. Porting depth_fog.fs's fog block verbatim hits exactly that.
  * removed built-ins. `texture2D()`, `gl_FragColor`, `varying` and `attribute` are gone in
    ES 3.00; using them is the other half of the same trap.

    python3 tools/check_shader.py               # checks shaders/pom.frag
    python3 tools/check_shader.py path.frag ...
"""
import re
import sys
import os

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT = os.path.join(os.path.dirname(HERE), "shaders", "pom.frag")

# GLSL ES 3.00 section 3.7: keywords reserved for future use (plus the tessellation /
# compute names desktop GL gained, which ES keeps reserved).
RESERVED_ES300 = """
asm class union enum typedef template this packed goto inline noinline volatile public
static extern external interface long short double half fixed unsigned input output
hvec2 hvec3 hvec4 dvec2 dvec3 dvec4 fvec2 fvec3 fvec4 sampler3DRect filter sizeof cast
namespace using row_major patch sample subroutine common partition active superp
resource coherent restrict readonly writeonly atomic_uint layout
""".split()

# Keywords that exist in ES 1.00 / GLSL 1.50 and were removed or renamed in ES 3.00.
REMOVED_ES300 = ["attribute", "varying", "texture2D", "texture3D", "textureCube",
                 "texture2DProj", "gl_FragColor", "gl_FragData", "gl_FragDepthEXT"]

VERSION_RE = re.compile(r"^\s*#\s*version\s+(\d+)(\s+es)?\s*$", re.MULTILINE)


def strip_comments(src):
    """Blank out comments and strings, keeping line numbering intact."""
    out = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    out = re.sub(r"//[^\n]*", "", out)
    return out


def check(path):
    with open(path) as fh:
        src = fh.read()
    m = VERSION_RE.search(src)
    if not m:
        return [f"{path}: no #version directive on the first line"]
    version, es = int(m.group(1)), bool(m.group(2))
    body = strip_comments(src)
    problems = []

    if version < 300 or not es:
        print(f"{path}: #version {version}{' es' if es else ''} -- "
              f"ES 3.00-only checks skipped")
        return problems

    words = RESERVED_ES300 + REMOVED_ES300
    for lineno, line in enumerate(body.splitlines(), 1):
        if line.lstrip().startswith("#"):
            continue
        for w in re.findall(r"[A-Za-z_][A-Za-z_0-9]*", line):
            if w in words:
                kind = "removed in ES 3.00" if w in REMOVED_ES300 else "reserved in ES 3.00"
                problems.append(f"{path}:{lineno}: '{w}' is {kind}")
    return problems


def main():
    paths = sys.argv[1:] or [DEFAULT]
    problems = []
    for p in paths:
        problems += check(p)
    if problems:
        for p in problems:
            print("FAIL " + p)
        return 1
    print(f"GLSL ES 3.00 keyword check passed ({len(paths)} file"
          f"{'s' if len(paths) != 1 else ''}). This is not a compile.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
