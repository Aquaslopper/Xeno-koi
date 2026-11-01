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
| the effect and its shader | `src/ui_glow.c` (new) |
| the interface | `src/ui_glow.h` (new) - one prototype, points at this file |
| the call | `src/ui_main.c` - one line, after the canvas blit, UI screens only |

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
rendered and no screen is re-drawn: `ui_main.c` hands over `canvas.texture` and the module
draws it a second time, on top of itself, through its own shader.

- **The normal is inferred, not modelled.** Four extra taps around each pixel give a
  luminance gradient left/right and up/down; that gradient is the normal. Two light points
  drift on wide, slow ellipses, and where the normal lines up with a light, a specular is
  added. This is the card glint's math, not the card glint's code (see §4).
- **Only edges catch it.** A sharpness threshold drops anything with a soft gradient, so
  panel borders, box outlines, rules and text edges glint while painted backgrounds and
  photographs stay dark. That is what makes it read as structure rather than a wash.
- **Lighter than the cards, on purpose.** The cards need their glint as an easy-mode helper
  and keep it untouched. The UI light runs at roughly a third of that intensity, slower than
  the card's orbit, with wider and softer falloff - it should be noticeable when you look for
  it and never in the way when you do not.
- **Colour:** `g_theme.base`, additive, so it adds light rather than tinting what it crosses.
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

## 5. Tuning surface

`UI_GLOW_INTENSITY`, `UI_GLOW_SHARPNESS`, `UI_GLOW_RADIUS` and the two orbit speeds in
`src/ui_glow.c`. That is the whole list. `UI_GLOW_INTENSITY 0` is the off switch.

## 6. Not settled

**Intensity and speed**, and only those. The form is decided: an inferred-normal specular
pass, the same idea the cards use, lighter. What remains is judgement on a display - if it
reads too strong or too quick, it is two constants and nothing else.

## 7. Verification

- `-fsyntax-only` sweep of `src/*.c` at `-Wall -Wextra -Werror`.
- `./build.sh test`, `./tools/action_guard.sh`, `./tools/tests_guard.sh`,
  `python3 tools/gen_wiki_tables.py --check`.

**Not covered by any test:** the feel. No test asserts that light reads well; that is judged
on a display, by the owner.
