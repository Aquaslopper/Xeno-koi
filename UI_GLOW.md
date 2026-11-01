# UI glow — where the code goes

A placement note, not a design note. The effect itself is small; the only question worth
settling first is which file owns it, so nothing existing has to grow.

---

## The files

**New: `src/ui_glow.h`** — one line of interface.

```c
void ui_glow_draw_ambient(void);
```

**New: `src/ui_glow.c`** — the whole effect. Constants, one function, about 35 lines
including the comment that explains it. Includes `raylib.h` and `ui_theme.h` (for `g_theme`
and `CANVAS_W/H`) and nothing else.

**One call in `src/ui_main.c`** — at the end of the non-game branch of the canvas draw,
after `draw_hub_screen_state()` (~line 1941 today). One line, and it is the only line any
existing source file gains.

## Why there

- **not `ui_theme.c`** — that file is the themes, the panel flares and the glyph tracing.
  It is already one of the biggest in `src/`, and an ambient light is not a theme. Adding
  to it is exactly the "make the file bigger" that should not happen.
- **not `ui_board.c`** — cards. The card glint stays there and nothing here reaches into it.
- **not one call per screen** — `ui_main.c` already owns the decision of what a screen
  draws. One call covers title, hub, Pancratium, Options, Stats and Plex. Six call sites
  would be six places to forget.
- **no `storage.c` / no Options row** — no setting, so nothing to save, load or paginate.

## Build and tests

`build.sh` compiles `src/*.c` (lines 83, 90, 137), so a new file is picked up by the Linux
build, the Windows cross-compile and the web build with no edits to the script.

The test suite links explicit file lists, never `src/*.c`, so a new module is invisible to
it and nothing under `tests/` changes.

## What does not change

- `src/ui_board.c`, `src/ui_theme.c`, `src/storage.c`, `src/storage.h` — untouched
- every file under `assets/` — untouched, nothing read differently, nothing written
- no shader strings, no textures, no allocation

## The look, in five lines

Two soft pools of `g_theme.base` drifting over the screen at 0.043 and 0.031 laps per second;
additive, so they add light rather than tint what they cross; peak 48/255 at the centre of a
pool; constants at the top of the file; `UI_AMBIENT_PEAK 0` is the off switch.

Settled: UI screens only, not the game table. Unsettled: pools versus a line-catching
highlight — that is a change inside `ui_glow.c` and touches nothing else.

## When it is built

This file has done its job and can go, with its reasoning folded into the comment at the top
of `src/ui_glow.c`.
