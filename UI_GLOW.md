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
| the effect | `src/ui_glow.c` (new) |
| the interface | `src/ui_glow.h` (new) - one prototype, points at this file |
| the call | `src/ui_main.c` - one line, at the end of the non-game branch of the canvas draw |

Rules, all of them consequences of the first:

- **One call site.** Not one per screen. `ui_main.c` already decides what a screen draws;
  six call sites would be six places to forget.
- **The module reads two things from the rest of the program:** `g_theme.base` and
  `CANVAS_W` / `CANVAS_H`. It writes nothing outside its own file.
- **If a change to the glow needs a line in any other file, the change is wrong.** Stop,
  write it here first, and let the owner decide.
- **No `storage.c` / `storage.h`, no Options row.** No setting means nothing to save, load,
  validate or paginate.
- **No file under `assets/`.** Nothing is read differently, generated or written.

`build.sh` compiles `src/*.c`, so both new files join the Linux, Windows and web builds with
no edit to the script. The test suite links explicit file lists, never the glob, so nothing
under `tests/` changes.

## 3. What it draws

- **Colour:** `g_theme.base`. TEAL, AMBER, CRIMSON, VIOLET, DARK and GOLD all work with no
  extra state.
- **Motion:** two loops at 0.043 and 0.031 laps per second. They share no small multiple, so
  the pair never settles into the same formation twice.
- **Form:** a stack of circles, rim inwards, one alpha step each. No shader, no texture, no
  allocation, nothing to unload.
- **Strength:** peak additive alpha 48/255 (about 19%) at the centre of a pool, fading to
  nothing at the rim. Additive means it adds light - it does not tint what it crosses.
- **Screens:** UI only - title, main hub, Pancratium, Options, Stats, Plex, campaign. Not the
  game table, which already carries the casing, the card glint, the ash and the embers.

## 4. Boundaries - what it does not touch

- **Cards.** `k_bezel_shine_fs`, `board_draw_table_bezel_shine()` and `board_draw_card_glow()`
  in `src/ui_board.c` belong to the card glint: a specular over a card's own metal. This is
  ambient light in the room. They share no code, no shader and no uniforms, and
  `src/ui_board.c` does not change.
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

`UI_AMBIENT_PEAK`, `UI_AMBIENT_RADIUS` and the two speeds in `src/ui_glow.c`. That is the
whole list. `UI_AMBIENT_PEAK 0` is the off switch.

## 6. Not settled

**Pools or lines.** §3 describes two drifting pools. The alternative is a highlight that
lands only on panel borders, box outlines and text edges, so the UI's structure glints while
flat areas stay dark. Different looks, different code, same module: whichever wins, §3 is
rewritten in the same commit and nothing outside `src/ui_glow.c` moves. That is what the
module boundary is for.

## 7. Verification

- `-fsyntax-only` sweep of `src/*.c` at `-Wall -Wextra -Werror`.
- `./build.sh test`, `./tools/action_guard.sh`, `./tools/tests_guard.sh`,
  `python3 tools/gen_wiki_tables.py --check`.

**Not covered by any test:** the feel. No test asserts that light reads well; that is judged
on a display, by the owner.
