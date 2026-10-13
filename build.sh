#!/bin/bash
# XENO/KOI build script
#   ./build.sh test   - engine integrity suite (15,000 headless sim matches)
#   ./build.sh linux  - native Linux binary (needs X11/GL dev libs)
#   ./build.sh win    - Windows release via MinGW, falling back to zig cc
#   ./build.sh        - test, then build whatever this machine can build
set -e
mkdir -p dist

RAYLIB_DIR=/tmp/raylib
WIN_LIB=/tmp/libraylib_win.a
LINUX_LIB=/tmp/libraylib_linux.a

# Locate a Windows cross toolchain: prefer MinGW, fall back to zig cc
WINCC=""
WINSUB="-mwindows"
ZIG_BIN=""
find_zig() {
    [ -n "$ZIG_BIN" ] && return 0
    for cand in /home/user/.zigvenv/lib/python*/site-packages/ziglang/zig "$(command -v zig 2>/dev/null)"; do
        [ -x "$cand" ] && { ZIG_BIN="$cand"; return 0; }
    done
    return 1
}
if command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1; then
    WINCC="x86_64-w64-mingw32-gcc"
elif find_zig; then
    WINCC="$ZIG_BIN cc -target x86_64-windows-gnu"
    WINSUB="-Wl,--subsystem,windows"
fi

# Fetch raylib 5.5 sources if missing (tmp may have been cleared)
ensure_raylib_src() {
    [ -d "$RAYLIB_DIR/src" ] && return 0
    if [ -d "/home/user/raylib_static/src" ]; then
        mkdir -p "$RAYLIB_DIR"
        cp -r /home/user/raylib_static/src "$RAYLIB_DIR/"
        [ -f /home/user/raylib_static/libraylib_linux.a ] && cp /home/user/raylib_static/libraylib_linux.a /tmp/
        [ -f /home/user/raylib_static/libraylib_win.a ]   && cp /home/user/raylib_static/libraylib_win.a /tmp/
        return 0
    fi
    echo "--> Downloading raylib 5.5 sources..."
    curl -sL -o /tmp/raylib-5.5.tar.gz https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5
    tar xzf /tmp/raylib-5.5.tar.gz -C /tmp
    mv /tmp/raylib-5.5 "$RAYLIB_DIR"
    rm -f /tmp/raylib-5.5.tar.gz
}

# Build the Windows static raylib with zig cc if it is missing
ensure_win_lib() {
    [ -f "$WIN_LIB" ] && return 0
    ensure_raylib_src
    [ -z "$ZIG_BIN" ] && find_zig
    echo "--> Building raylib 5.5 (Windows x64 static) with zig cc..."
    mkdir -p /tmp/raylib/win_obj
    (
        cd "$RAYLIB_DIR/src"
        for f in rcore rglfw rshapes rtextures rtext rmodels raudio utils; do
            "$ZIG_BIN" cc -c -target x86_64-windows-gnu -std=c99 -O2 \
                -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33 -D_GNU_SOURCE \
                -Wno-missing-braces -fno-strict-aliasing -I. -Iexternal/glfw/include \
                "$f.c" -o "/tmp/raylib/win_obj/$f.o"
        done
        "$ZIG_BIN" ar rcs "$WIN_LIB" /tmp/raylib/win_obj/*.o
    )
}

package_dist() {
    mkdir -p dist/assets
    cp -rf assets/* dist/assets/
    [ -f settings.txt ] && cp -f settings.txt dist/settings.txt || true
}

# Linux (for headless screenshot testing; needs libGL + X11 dev files)
if [ "$1" = "linux" ]; then
    ensure_raylib_src
    if [ ! -f "$LINUX_LIB" ]; then
        echo "ERROR: $LINUX_LIB missing and X11/GL dev headers are required to build it." >&2
        echo "       This sandbox has no libGL/X11 dev packages; use './build.sh win' or CI." >&2
        exit 1
    fi
    gcc -std=c99 -O2 -Wall -Wextra src/*.c -I"$RAYLIB_DIR/src" "$LINUX_LIB" \
        -l:libGL.so.1 -lm -lpthread -ldl -lrt -lX11 -o koikoi_linux
    exit 0
fi

# Windows (the deliverable). GUI subsystem hides the console window.
if [ "$1" = "win" ]; then
    if [ -z "$WINCC" ]; then
        echo "ERROR: no Windows cross toolchain found (install gcc-mingw-w64-x86-64 or pip install ziglang)." >&2
        exit 1
    fi
    ensure_raylib_src
    ensure_win_lib
    $WINCC -std=c99 -O2 src/*.c -I"$RAYLIB_DIR/src" "$WIN_LIB" \
        -lopengl32 -lgdi32 -lwinmm -static $WINSUB -o dist/KoiKoi.exe
    rm -f dist/KoiKoi.pdb
    package_dist
    echo "--> dist/KoiKoi.exe built OK"
    exit 0
fi

# Engine tests (no graphics)
if [ "$1" = "test" ]; then
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o test_engine && ./test_engine
    exit 0
fi

# Default: always run the integrity suite, then build available targets
if [ -f tests/test_engine.c ]; then
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o test_engine && ./test_engine
fi
if [ -f "$LINUX_LIB" ]; then
    ensure_raylib_src
    gcc -std=c99 -O2 -Wall -Wextra src/*.c -I"$RAYLIB_DIR/src" "$LINUX_LIB" \
        -l:libGL.so.1 -lm -lpthread -ldl -lrt -lX11 -o koikoi_linux
fi
if [ -n "$WINCC" ]; then
    ./build.sh win
fi
