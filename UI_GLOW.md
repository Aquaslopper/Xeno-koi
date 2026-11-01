# The overlay light

**This document owns the light over the table casing overlay.** If you change where it
draws, how fast it moves, how strong it is, or how it is called, you change this file in the
same commit. Nothing about it lives in `STATUS.md`, in a code comment, or in an agent's
memory - `src/ui_glow.h` points here, and this file is the only description.

---

## 1. What it is, and what it is not

The table wears an overlay: a transparent PNG per map with the casing drawn on it - the
painted pair, or the CRT pair when `CRT CASING` is on. This feature puts slow light in that
casing. The overlay is drawn a second time, additively, through its own shader; the art's own
alpha says where the casing is, its own luminance gradient says which way the metal faces,
and two drifting lights add a specular along it. The casing catches light and the light
moves; the transparent parts of the overlay stay transparent.

**It is not the card glint.** Cards keep theirs unchanged: `BEZEL_SHINE_BODY`,
`board_draw_table_bezel_shine()` and `board_draw_card_glow()` in `src/ui_board.c`. That one
is easy-mode feedback and stays exactly as strong as it needs to be. This one never touches
it, shares no shader with it, and cannot move it (§4).

**It is not the menus.** An earlier version of this document described a pass over the menu
canvas. That was wrong and was removed: the light belongs on the table casing, not on the
title screen.

**It is not a choice of art.** The module is handed whatever overlay the table drew this
frame and lights that texture. It never picks a file, never prefers one pair over the other,
and does not know or care whether the casing came from a bake or from the owner's hand.

## 2. Where the code lives - and nowhere else

| Piece | Path |
|---|---|
| the effect and its shader | `src/ui_glow.c` - `ui_glow_draw_table_overlay()` |
| the interface | `src/ui_glow.h` - one prototype, points at this file |
| the call | `src/ui_board.c:1285` - one line, immediately after the overlay is drawn |

Rules, all of them consequences of the first:

- **One call site.** The light is drawn where the overlay is drawn, with the same texture
  and the same rectangle, so it can never land somewhere the casing is not.
- **The module receives one texture and one rectangle.** No access to settings, storage,
  themes beyond `g_theme.base` for the tint, the table index, or the map name.
- **If a change to this light needs a line in any other file, the change is wrong.** Stop,
  write it here first, and let the owner decide.
- **No `storage.c` / `storage.h`, no Options row.** No setting means nothing to save, load,
  validate or paginate.
- **No file under `assets/`.** Nothing is read differently, generated or written.

`build.sh` compiles `src/*.c`, so both files join the Linux, Windows and web builds with no
edit to the script. The test suite links explicit file lists, never the glob, so nothing under
`tests/` changes.

## 3. What it draws

- **Mask:** the overlay's own alpha, `smoothstep(UI_GLOW_ALPHA_LO, UI_GLOW_ALPHA_HI, c.a)`.
  This is the same idea as the card glint's `solid` gate and it works here for the same
  reason: overlay art is transparent where there is no casing. Measured on the shipped art,
  `city/overlay.png` is 29% fully clear, `city/crt_overlay.png` 10%.
- **Normal:** five luminance taps, `g = (hl - hr, hu - hd) / peak`, read as
  `n = normalize(vec3(g * 2.2, 1.0))`. The same inference the card glint makes, written
  here rather than imported - see §4.
- **Lights:** two points on wide ellipses, `cos/sin` of a time phase; half-vector specular
  `0.20 + 0.80 * pow(nd, 3.0)`; pool falloffs `exp(-d/F)` for the near light and
  `exp(-d/(2.4F))` for the far one.
- **Colour:** `g_theme.base`, passed as the tint. The shader emits `fragColor.rgb` untouched
  and only decides where the light lands, exactly as the card glint does with its
  near-white.
- **Output:** additive, so it adds light and never tints the casing art.

## 4. Boundaries - what it does not touch

- **Cards.** `src/ui_board.c`'s glint shader and `board_draw_card_glow()` are not modified,
  wrapped, exported or referenced. If a change to this light touches them, it is wrong.
- **No shared shader.** The normal-from-luminance maths is re-derived inside `ui_glow.c`
  rather than imported from the card shader - about twenty lines of GLSL, deliberately
  duplicated. Sharing it would let a change to easy-mode card feedback move this light, and
  a change here move the cards. Two systems that look similar stay two systems.
- **Which overlay wins** stays with `ui_board.c` and `settings.crt_tables`. This module
  lights the texture it is given.
- **Menus.** No call from `ui_main.c`, no pass over the canvas blit, nothing on the title,
  hub, Pancratium, Options, Stats, Plex or campaign screens.
- **Audio.** Nothing here reads or writes the mixer, the reverb, the beat or any audio state.
  A light that moves to the music is a different feature and needs the owner asking for it.
  This is the failure this section exists to prevent: a small visual threaded through
  unrelated systems until it reacts.
- **Gameplay.** No engine, no turn, no gate, no timing, no input.
- **Tests.** Nothing added, nothing changed; `tests/` is locked.

## 5. Tuning surface - how to change it in six months

Every number is a `#define` at the top of `src/ui_glow.c`, compiled in. There is no settings
file and no Options row (§2), so changing the light means changing one of these and
rebuilding.

| Constant | Now | What it does | Sane range |
|---|---|---|---|
| `UI_GLOW_INTENSITY` | `0.55` | overall strength. **`0.0` switches the whole thing off** | 0.00 - 1.00 |
| `UI_GLOW_SPEED_A` | `0.043` | light A, laps per second | 0.005 - 0.15 |
| `UI_GLOW_SPEED_B` | `0.031` | light B, laps per second; keep it incommensurate with A | 0.005 - 0.15 |
| `UI_GLOW_ALPHA_LO` | `0.30` | overlay alpha where light starts to appear | 0.00 - 0.60 |
| `UI_GLOW_ALPHA_HI` | `0.85` | overlay alpha at full strength | 0.40 - 1.00 |
| `UI_GLOW_FALLOFF` | `0.075` | pool width of the near light, UV squared; the far light is 2.4x wider | 0.02 - 0.40 |
| `UI_GLOW_SAMPLE_PX` | `1.45` | neighbour distance for the normal, in pixels | 1.0 - 3.0 |

Recipe, months later:

1. Open `src/ui_glow.c`; the seven defines are the first thing in the file.
2. Change one. Nothing else depends on these numbers.
3. `./build.sh`, pick a table, look at the casing.

If a change needs an edit in any other file, stop: §2 says the change is wrong.

## 6. Not settled

Only judgement: whether the intensity and the speeds read right on a display, and whether
they suit both the painted and the CRT casing. The form is decided and built. Anything that
would need a change outside `src/ui_glow.c` is a new decision for the owner.

## 7. Verification

- `-fsyntax-only` sweep of `src/*.c` at `-Wall -Wextra -Werror`.
- `./build.sh test`, `./tools/action_guard.sh`, `./tools/tests_guard.sh`,
  `python3 tools/gen_wiki_tables.py --check`.
- The shader compiles at runtime (`LoadShaderFromMemory`). If it fails to load,
  `ui_glow_draw_table_overlay()` returns without drawing: the table renders exactly as it
  does today, just without the light. No half-drawn frame.

**Not covered by any test:** the feel. No test asserts that light reads well; that is judged
on a display, by the owner.

## 8. Reference: how the card glint works

Read from `src/ui_board.c` as it stands, not from memory. Read-only - it records the
neighbour so this light is understood as a deliberate re-derivation, not a copy.

- **Shader:** `k_bezel_shine_fs` (:529 `#version 100`, :539 `#version 330`), body
  `BEZEL_SHINE_BODY` (:490). Uniforms: `texture0`, `colDiffuse`, `u_phase`.
- **The normal comes from luminance, not alpha:** five taps; `bval(v) = 0.45 * luma +
  0.55 * max(r,g,b)` from RGB; `g = vec2(hl - hr, hu - hd) / peak`;
  `n = normalize(vec3(g * 2.8, 1.0))`.
- **Alpha decides where it may appear:** `solid = smoothstep(0.91,0.98,c.a) *
  smoothstep(0.85,0.95,min8)`, then `mask = solid * smoothstep(0.08,0.18,hc) *
  smoothstep(0.42,0.72,hc/peak)`.
- **Two lights, twice:** tight `exp(-d/0.040)` and wide `exp(-d/0.085)` per light, half-vector
  specular `pow(nd, 4.5)`, compressed by `0.88 * (1 - exp(-raw * 1.65))`.
- **The shader has no colour:** `vec4(fragColor.rgb, a)`; the RGB is the CPU tint at :1051,
  `Color c = { 250, 246, 238, ... }`.
- **Motion:** `u_phase = (t * 0.52 + seed, -t * 0.37 + 2.3 * seed, 1.45/width, 1.45/height)`,
  `seed` from the card's position.
- **Strength is the caller's:** `a = 0.36 + 0.84 * pulse^0.68`; held cards pass 1.0.
- **Without a table skin** `board_draw_card_glow()` is a plain `DrawRectangleLinesEx` in
  `g_theme.base`.

**Where this light deliberately differs:** its own shader and constants; a softer alpha gate
(0.30-0.85) so semi-transparent casing edges still catch light; one falloff constant with the
second pool derived; theme colour instead of near-white; and no requirement that neighbours
be opaque, because casing strokes are thin and that test would erase them.
