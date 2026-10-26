#!/bin/bash
set -e

# Secure path away from the git tree
VALIDATION_FILE="/tmp/.xeno_koi_validated"
QA_FILE="LEARN_THE_CODE.md"

if [ "$1" = "--verify" ]; then
    INPUT_FILE="$2"
    
    if [ ! -f "$INPUT_FILE" ]; then
        echo "VALIDATION FAILED: Provide the path to your conceptual answers file."
        exit 1
    fi

    # FIX 1: Prevent the agent from cheating by using LEARN_THE_CODE.md against itself
    if [ "$INPUT_FILE" = "$QA_FILE" ] || [ "$INPUT_FILE" = "./$QA_FILE" ]; then
        echo "VALIDATION FAILED: You cannot pass the documentation file as your answers."
        echo "Please read the questions and write your own unique answers in /tmp/ans.txt."
        exit 1
    fi

    # 1. HARDENING: Verify strict character weight barrier (requires real architectural prose)
    CHAR_COUNT=$(wc -c < "$INPUT_FILE")
    if [ "$CHAR_COUNT" -lt 800 ]; then
        echo "VALIDATION FAILED: Your architectural analysis is too short ($CHAR_COUNT characters)."
        echo "An evaluation of this massive game skeleton requires at least 800 characters of detailed documentation."
        exit 1
    fi

    # 2. HARDENING: Save the content instead of touching an empty file
    cp "$INPUT_FILE" "$VALIDATION_FILE"
    
    # Securely clean up any files accidentally left in the repository workspace
    rm -f ./answers.txt ./answers.md 2>/dev/null || true

    echo "======================================================================"
    echo "SUCCESS: Codebase reading verified. Build unlocked!"
    echo "======================================================================"
    exit 0
fi

# 3. HARDENING: Validate file content size on every execution loop
if [ "${GITHUB_ACTIONS:-}" != "true" ]; then
    if [ ! -f "$VALIDATION_FILE" ] || [ "$(wc -c < "$VALIDATION_FILE" 2>/dev/null || echo 0)" -lt 800 ]; then
        echo "======================================================================"
        echo "ERROR: CODEBASE LOCK ENGAGED."
        echo "You must review the codebase architecture before making modifications."
        echo "======================================================================"
        echo ""
        echo "STEP 1: Read the file '$QA_FILE' completely to understand the skeleton."
        echo "STEP 2: Output your detailed answers to all 6 review questions in chat"
        echo "        AND save them to an external file (e.g., /tmp/ans.txt)."
        echo "STEP 3: Run the validation command to unlock the repository:"
        echo "        ./build.sh --verify /tmp/ans.txt"
        echo ""
        echo "Current Status: Blocked. Please read '$QA_FILE' now."
        echo "======================================================================"
        exit 1
    fi
fi

# ======================================================================
# YOUR NORMAL GAME BUILD CODE STARTS HERE
# ======================================================================
echo "Executing hardened game build sequence..."
# Insert your compilation/testing tools below:

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
        -lopengl32 -lgdi32 -lwinmm -static -mwindows -o dist/KoiKoi.exe
    mkdir -p dist/assets
    cp -rf assets/* dist/assets/
    cp -f assets/small_font.ttf dist/assets/small_font.ttf
    [ -f settings.txt ] && cp -f settings.txt dist/settings.txt || true
    exit 0
fi

# Engine tests (no graphics)
if [ "$1" = "test" ]; then
    ./tools/tests_guard.sh
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o /tmp/test_engine && /tmp/test_engine
    gcc -std=c99 -O2 -Wall -Wextra -Werror -Isrc tools/test_koikoi_trigger.c src/cards.c src/rules.c src/engine.c \
        -o /tmp/test_koikoi_trigger && /tmp/test_koikoi_trigger
    if [ -f "${RAYLIB_SRC:-/tmp/raylib/src}/raylib.h" ]; then
        ./tests/test_table_ui.sh
    fi
    ./tests/test_music_folder.sh
    exit 0
fi

# WebAssembly (Emscripten / HTML5)
if [ "$1" = "wasm" ]; then
    RAYLIB_SRC="${RAYLIB_SRC:-/tmp/raylib/src}"
    RAYLIB_WEB_A="${RAYLIB_WEB_A:-/tmp/libraylib_web.a}"
    if [ ! -f "$RAYLIB_WEB_A" ] && [ -f "$RAYLIB_SRC/libraylib.a" ]; then
        RAYLIB_WEB_A="$RAYLIB_SRC/libraylib.a"
    fi
    mkdir -p dist/web
    emcc -Os -Wall -std=gnu99 -DPLATFORM_WEB src/*.c -Isrc -I"$RAYLIB_SRC" "$RAYLIB_WEB_A" \
        -sUSE_GLFW=3 -sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sTOTAL_MEMORY=134217728 \
        -sEXPORTED_RUNTIME_METHODS='["UTF8ToString","FS"]' \
        --preload-file assets --shell-file web/shell.html -o dist/web/index.html
    exit 0
fi

# Default: run test if exists, or build available targets
if [ -f tests/test_engine.c ]; then
    gcc -std=c99 -O2 -Wall -Wextra -Isrc tests/test_engine.c src/cards.c src/rules.c src/engine.c src/ai.c \
        -o /tmp/test_engine && /tmp/test_engine
fi
if ls src/*.c 1> /dev/null 2>&1; then
    gcc -std=c99 -O2 -Wall -Wextra src/*.c -I/tmp/raylib/src /tmp/libraylib_linux.a \
        -l:libGL.so.1 -lm -lpthread -ldl -lrt -lX11 -o koikoi_linux
    x86_64-w64-mingw32-gcc -std=c99 -O2 src/*.c -I/tmp/raylib/src /tmp/libraylib_win.a \
        -lopengl32 -lgdi32 -lwinmm -static -mwindows -o dist/KoiKoi.exe
    mkdir -p dist/assets
    cp -rf assets/* dist/assets/
    cp -f assets/small_font.ttf dist/assets/small_font.ttf
fi
