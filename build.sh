#!/bin/bash
set -e
mkdir -p dist

# Restore static raylib libraries if tmp was cleared
if [ ! -d "/tmp/raylib/src" ] && [ -d "/home/user/raylib_static/src" ]; then
    mkdir -p /tmp/raylib
    cp -r /home/user/raylib_static/src /tmp/raylib/
    cp /home/user/raylib_static/libraylib_linux.a /tmp/
    cp /home/user/raylib_static/libraylib_win.a /tmp/
fi

# Linux (for testing in the sandbox)
if [ "$1" = "linux" ]; then
    gcc -std=c99 -O2 -Wall -Wextra src/*.c -I/tmp/raylib/src /tmp/libraylib_linux.a \
        -l:libGL.so.1 -lm -lpthread -ldl -lrt -lX11 -o koikoi_linux
    exit 0
fi

# Windows (the deliverable). -mwindows hides the console window.
if [ "$1" = "win" ]; then
    x86_64-w64-mingw32-gcc -std=c99 -O2 src/*.c -I/tmp/raylib/src /tmp/libraylib_win.a \
        -lopengl32 -lgdi32 -lwinmm -static -mwindows -o dist/KoiKoi.exe && cp -r assets dist/
    exit 0
fi

# Engine tests (no graphics)
if [ "$1" = "test" ]; then
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o test_engine && ./test_engine
    exit 0
fi

# Default: run test if exists, or build available targets
if [ -f tests/test_engine.c ]; then
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o test_engine && ./test_engine
fi
if ls src/*.c 1> /dev/null 2>&1; then
    gcc -std=c99 -O2 -Wall -Wextra src/*.c -I/tmp/raylib/src /tmp/libraylib_linux.a \
        -l:libGL.so.1 -lm -lpthread -ldl -lrt -lX11 -o koikoi_linux
    x86_64-w64-mingw32-gcc -std=c99 -O2 src/*.c -I/tmp/raylib/src /tmp/libraylib_win.a \
        -lopengl32 -lgdi32 -lwinmm -static -mwindows -o dist/KoiKoi.exe && cp -r assets dist/
fi
