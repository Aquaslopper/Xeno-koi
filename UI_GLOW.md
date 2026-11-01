# The UI glow

**This document owns the ambient light over the UI screens.** If you change where it
draws, how fast it moves, how strong it is, or how it is called, you change this file in the
same commit. Nothing about it lives in `STATUS.md`, in a code comment, or in an agent's
memory - `src/ui_glow.h` points here, and this file is the only description.

Why a document for a light: the thing that goes wrong with a small effect is never the
effect, it is the wiring. Written down, the wiring is a list you can check. Left as a habit,
it is whatever the last agent remembered.

---

## 1. Why it exists

The interface is still. The table breathes - ash, embers, the casing, the token's trail -
but a menu sits at one exposure from the moment it opens to the moment it closes. The cards
got a glint. The UI got nothing.

This gives the UI its own slow light: two pools of the active theme colour drifting across
the screen, additive, on their own clock. It is not a response to the player, not a hover
state, not a readout of anything. It carries no information, which is precisely why it is
allowed to be slow.

## 2. Where the code lives - and nowhere else

| Piece | Path |
|---|---|
| the effect and its shader | `src/ui_glow.c` - `ui_glow_draw_ui_pass()` |
| the interface | `src/ui_glow.h` - one prototype, points at this file |
| the call | `src/ui_main.c:2064` - one line, after the canvas blit, `screen != SCREEN_GAME` |

Rules, all of them consequences of the first:

- **One call site.** Not one per screen. `ui_main.c` already decides what a screen draws;
  six call sites would be six places to forget.
- **The module receives the frame and nothing else that matters:** the texture the UI was
  already drawn into (`canvas.texture`), the rectangle it was blitted to - so the pass lands
  exactly on top of it, letterboxing and the canvas's vertical flip included - and
  `g_theme.base` for the colour. It writes nothing outside its own file.
- **If a change to the glow needs a line in any other file, the change is wrong.** Stop,
  write it here first, and let the owner decide.
- **No `storage.c` / `storage.h`, no Options row.** No setting means nothing to save, load,
  validate or paginate.
- **No file under `assets/`.** Nothing is read differently, generated or written.

`build.sh` compiles `src/*.c`, so both new files join the Linux, Windows and web builds with
no edit to the script. The test suite links explicit file lists, never the glob, so nothing
under `tests/` changes.

## 3. What it draws

A single additive pass over the frame the UI is already rendered into. Nothing new is
rendered and no screen is re-drawn: `ui_main.c` hands over `canvas.texture`, the rectangle it
was just blitted to, and the module draws that frame again on top of itself, through its own
shader.

- **The normal is inferred, not modelled.** Five taps give a luminance gradient
  left/right and up/down (`g = (hl - hr, hu - hd) / peak`); that gradient is the normal,
  `n = normalize(vec3(g * 2.2, 1.0))`. This is the card glint's math, not the card glint's
  code - see §8 for what differs and why.
- **Only edges catch it.** `edge = smoothstep(UI_GLOW_EDGE, UI_GLOW_EDGE + 0.30, length(g))`,
  so panel borders, box outlines, rules and text edges glint while painted backgrounds and
  photographs stay dark. The card glint's `solid` gate is alpha-based and cannot be used
  here; §8 explains why.
- **Two lights, two pool widths.** `exp(-dot(t, t) / UI_GLOW_FALLOFF_A)` for the near light
  and `UI_GLOW_FALLOFF_B` for the far one, each multiplied by a half-vector specular
  `0.20 + 0.80 * pow(nd, 3.0)`, then compressed:
  `a = 0.55 * (1 - exp(-edge * core * UI_GLOW_INTENSITY * 2.2))`.
- **Colour:** the tint. The shader emits `vec4(fragColor.rgb, a)` untouched and only decides
  where light lands, so the colour is `g_theme.base` passed from C - the same mechanism the
  card glint uses with its near-white constant.
- **Lighter than the cards, on purpose:** peak alpha lands near 0.35 against the card glint's
  0.88, and the orbit is slower. The cards keep their intensity: it is easy-mode feedback.
- **Screens:** UI only - title, main hub, Pancratium, Options, Stats, Plex, campaign. Not the
  game table, which already carries the casing, the card glint, the ash and the embers.

## 4. Boundaries - what it does not touch

- **Cards.** `k_bezel_shine_fs`, `board_draw_table_bezel_shine()` and `board_draw_card_glow()`
  in `src/ui_board.c` belong to the card glint: a specular over a card's own metal, and an
  easy-mode helper. `src/ui_board.c` does not change, and neither does that shader.
- **No shared shader.** The normal-from-luminance math is re-derived inside `ui_glow.c`
  rather than imported from the card shader - about twenty lines of GLSL, deliberately
  duplicated. Sharing it would make a change to the easy-mode card glint able to move the UI
  light, and a change here able to move the cards. Two systems that look similar stay two
  systems; duplication is the cheaper of the two costs.
- **Themes.** `src/ui_theme.c` owns the themes, the panel flares and the glyph tracing. The
  glow reads `g_theme.base` and is not part of that file.
- **Audio.** Nothing here reads or writes the mixer, the reverb, the beat or any audio state.
  A light that moves to the music is a different feature: it needs its own document and the
  owner asking for it. This is the failure this section exists to prevent - a small visual
  threaded through unrelated systems until it "reacts". The module is called once and draws;
  nothing calls back into it.
- **Gameplay.** No engine, no turn, no gate, no timing, no input.
- **Tests.** Nothing added, nothing changed; `tests/` is locked.

## 5. Tuning surface - how to change it in six months

Every number is a `#define` at the top of `src/ui_glow.c`, compiled in. There is no settings
file and no Options row (§2), so changing the glow means changing one of these and rebuilding.

| Constant | Now | What it does | Sane range |
|---|---|---|---|
| `UI_GLOW_INTENSITY` | `0.22` | overall strength of the pass. **`0.0` switches the whole thing off** | 0.00 - 0.60 |
| `UI_GLOW_SPEED_A` | `0.043` | light A, laps per second | 0.005 - 0.15 |
| `UI_GLOW_SPEED_B` | `0.031` | light B, laps per second. Keep it incommensurate with A | 0.005 - 0.15 |
| `UI_GLOW_EDGE` | `0.10` | gradient at which an edge starts to catch. Higher = fewer edges light up | 0.03 - 0.30 |
| `UI_GLOW_FALLOFF_A` | `0.075` | pool width of the near light, in UV squared. Higher = wider, softer | 0.03 - 0.40 |
| `UI_GLOW_FALLOFF_B` | `0.220` | pool width of the far light, same units | 0.03 - 0.40 |
| `UI_GLOW_SAMPLE_PX` | `1.45` | how far apart the neighbours are sampled for the normal, in pixels | 1.0 - 3.0 |

Recipe, months later:

1. Open `src/ui_glow.c`; the seven defines are the first thing in the file.
2. Change one. If it looks wrong, change it back - nothing else depends on these numbers.
3. `./build.sh` and look at a menu screen.

If a change to the glow needs an edit in any other file, stop: §2 says the change is wrong.

## 6. Not settled

Only judgement: whether the intensity and speed above read right on a display. The form is
decided and built. Anything that would need a change outside `src/ui_glow.c` is a new
decision for the owner, not a tweak.

## 7. Verification

- `-fsyntax-only` sweep of `src/*.c` at `-Wall -Wextra -Werror`.
- `./build.sh test`, `./tools/action_guard.sh`, `./tools/tests_guard.sh`,
  `python3 tools/gen_wiki_tables.py --check`.
- The shader is compiled at runtime by the driver (`LoadShaderFromMemory`). If it fails to
  load, `ui_glow_draw_ui_pass()` returns without drawing anything - the UI is unaffected,
  it simply has no ambient light. No error path can leave the screen half-drawn.

**Not covered by any test:** the feel. No test asserts that light reads well; that is judged
on a display, by the owner.

## 8. Reference: how the card glint actually works

Read from `src/ui_board.c` as it stands, not from memory or from a summary. This section is
read-only - it records the neighbour so the UI pass is understood as a deliberate
re-derivation rather than a copy. Nothing here is a description of `ui_glow.c`.

- **Shader:** `k_bezel_shine_fs` (:529 for `#version 100`, :539 for `#version 330`), body
  `BEZEL_SHINE_BODY` (:490). Uniforms: `texture0`, `colDiffuse`, `u_phase`.
- **The normal comes from luminance, not alpha.** Five taps; brightness is
  `bval(v) = 0.45 * luma + 0.55 * max(r,g,b)` taken from RGB; the gradient
  `g = vec2(hl - hr, hu - hd) / peak` becomes `n = normalize(vec3(g * 2.8, 1.0))`.
- **Alpha decides where it is allowed.** `solid = smoothstep(0.91,0.98,c.a) *
  smoothstep(0.85,0.95,min8)` - opaque pixel, opaque neighbours. Then
  `mask = solid * smoothstep(0.08,0.18,hc) * smoothstep(0.42,0.72,hc/peak)`, so only
  locally-raised areas catch it.
- **Two lights, twice.** Ellipses in UV space; half-vector specular
  `nd = max(dot(n, h), 0)` sharpened by `pow(nd, 4.5)`; per light a tight falloff
  `exp(-d/0.040)` and a wide one `exp(-d/0.085)`, plus a bloom term; compressed into alpha
  by `0.88 * (1 - exp(-raw * 1.65))`.
- **The shader has no colour.** It outputs `vec4(fragColor.rgb, a)`; the RGB is the CPU tint
  at :1051, `Color c = { 250, 246, 238, ... }`. The near-white is a C constant.
- **Motion:** `u_phase = (t * 0.52 + seed, -t * 0.37 + 2.3 * seed, 1.45/width, 1.45/height)`,
  with `seed` from the card's position so two cards never glint in step.
- **Strength is the caller's:** `a = 0.36 + 0.84 * pulse^0.68`; held cards pass 1.0.
- **Without a table skin** `board_draw_card_glow()` is not this at all - it is a plain
  `DrawRectangleLinesEx` in `g_theme.base`.

**What does not transfer.** The frame the UI is drawn into is opaque, so `c.a` is 1 at every
pixel and the `solid` gate would pass all of it, flat backgrounds included. `ui_glow.c`
therefore gates on gradient magnitude instead, and keeps the relative-brightness term. A
literal copy of `solid` lights the whole screen.
