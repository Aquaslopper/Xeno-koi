#!/usr/bin/env python3
"""Static checks for the preview's GLSL, because this sandbox has no GPU to compile on.

Not a compiler -- it cannot tell you a shader is valid. It catches the two mistakes that
actually cost time here, both of which compile fine in the game's own shaders and fail only
when the block is reused in another GLSL version:

  * reserved words. `patch` is legal in `#version 100` and `#version 330`, and reserved in
    `#version 300 es`. Porting depth_fog.fs's fog block verbatim hits exactly that.
  * removed built-ins. `texture2D()`, `gl_FragColor`, `varying` and `attribute` are gone in
    ES 3.00; using them is the other half of the same trap.
  * width mismatches in declarations. `vec3 lightUV = beamLightUV();` where the function
    returns a vec2 is a compile error a keyword scan cannot see; a narrow type table can.

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

# component counts, so a declaration can be compared with the thing assigned to it
WIDTH = {"float": 1, "int": 1, "uint": 1, "bool": 1,
         "vec2": 2, "ivec2": 2, "bvec2": 2,
         "vec3": 3, "ivec3": 3, "bvec3": 3,
         "vec4": 4, "ivec4": 4, "bvec4": 4,
         "mat2": 4, "mat3": 9, "mat4": 16,
         "sampler2D": 0, "sampler3D": 0, "samplerCube": 0, "void": 0}

FUNC_RE = re.compile(r"^\s*(?:(?:highp|mediump|lowp)\s+)?([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\(")
DECL_RE = re.compile(r"^\s*(?:(?:highp|mediump|lowp)\s+)?([A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*=\s*(.+?);\s*$")
ASSIGN_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(.+?);\s*$")
CALL_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*\(")
CONSTRUCTOR_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*\(")


def strip_comments(src):
    """Blank out comments and strings, keeping line numbering intact."""
    out = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), src, flags=re.S)
    out = re.sub(r"//[^\n]*", "", out)
    return out


def check_types(path, body):
    """Narrow type check: declarations and plain assignments only.

    Catches `vec3 x = fn_returning_vec2();` and `vec2 x = someVec3;`. It does not track
    swizzles, arithmetic or call arguments -- it is a smoke alarm, not a compiler.
    """
    returns, variables, problems = {}, {}, []
    for lineno, line in enumerate(body.splitlines(), 1):
        code = line.split("//")[0].strip()
        if not code or code.startswith("#"):
            continue
        fm = FUNC_RE.match(code)
        if fm and fm.group(1) in WIDTH and "=" not in code:
            returns[fm.group(2)] = fm.group(1)
            continue
        dm = DECL_RE.match(code)
        if dm and dm.group(1) in WIDTH:
            declared, name, rhs = dm.group(1), dm.group(2), dm.group(3).strip()
            variables[name] = declared
            inferred = None
            cm = CALL_RE.match(rhs)
            if cm:
                fn = cm.group(1)
                if fn in WIDTH:                       # constructor: vec3(...)
                    inferred = fn
                elif fn in returns:
                    inferred = returns[fn]
            elif re.fullmatch(r"[A-Za-z_]\w*", rhs):
                inferred = variables.get(rhs)
            if inferred and WIDTH.get(inferred) != WIDTH.get(declared):
                problems.append(f"{path}:{lineno}: '{name}' is {declared} but the right side is "
                                f"{inferred}")
            continue
        am = ASSIGN_RE.match(code)
        if am and am.group(1) in variables:
            lhs_t, rhs = variables[am.group(1)], am.group(2).strip()
            inferred = None
            cm = CALL_RE.match(rhs)
            if cm:
                fn = cm.group(1)
                inferred = fn if fn in WIDTH else returns.get(fn)
            elif re.fullmatch(r"[A-Za-z_]\w*", rhs):
                inferred = variables.get(rhs)
            if inferred and WIDTH.get(inferred) != WIDTH.get(lhs_t):
                problems.append(f"{path}:{lineno}: '{am.group(1)}' is {lhs_t} but the right side "
                                f"is {inferred}")
    return problems


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

    problems += check_types(path, body)
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
