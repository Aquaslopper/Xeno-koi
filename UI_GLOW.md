# UI glow

The cards keep their glint. This is a separate thing for the interface: light that moves on
its own, in the active theme's colour, attached to nothing the player does.

Nothing is built yet. This file is the shape, so the code does not have to be guessed at.

---

## Not the card glint

Cards already have one: `k_bezel_shine_fs` and `board_draw_table_bezel_shine()` in
`src/ui_board.c` - two lights orbiting a card's bezel, a specular over its metal, added when
a card is held or placed.

Nothing here reuses, exports, extends or edits it. If a change to the UI glow touches
`src/ui_board.c`, the change is wrong.

## What it is

Two soft pools of the theme colour, drifting slowly over a UI screen, drawn additively.
Light in the room, not a filter on the art: what it crosses keeps its colour and is simply
brighter while the pool is over it.

- **colour** - `g_theme.base`. TEAL, AMBER, CRIMSON, VIOLET, DARK and GOLD all work with no
  extra state; change the theme and the light changes with it.
- **motion** - two loops at 0.043 and 0.031 laps per second. They share no small multiple,
  so the pair never settles into the same formation twice.
- **softness** - a stack of circles, rim inwards, one alpha step each. No shader, no texture.
- **strength** - peak additive alpha 48/255 (about 19%) at the centre of a pool, fading to
  nothing at the rim.

## Where it draws

UI screens only: title, main hub, Pancratium, Options, Stats, Plex, campaign. Not the game
table - the table already carries the casing, the card glint, the ash and the embers, and
one more light there would be noise.

One call, at the end of the non-game branch of the canvas draw in `src/ui_main.c`.

## What it is not

- no shader, no new texture, no file under `assets/`
- no setting, no Options row: the constants sit at the top of the function, and
  `UI_AMBIENT_PEAK 0` is the off switch
- no hover trigger, no audio tie-in, no per-panel behaviour, no second copy for a second
  screen

## Not settled

1. **Pools or lines?** Two drifting pools (above), or a highlight that lands only on panel
   borders, box outlines and text edges, so the UI's structure glints while flat areas stay
   dark. Different looks, different code.
2. **The game table:** recommendation is no. It already has its own light.
3. **A switch:** recommendation is none for now. If it earns one later, it becomes a real
   setting then, in one place, not a flag threaded through the screens.

## Cost

48 circle draws per frame while a UI screen is up, and one blend-mode change. No allocation,
no load, nothing to unload.

## How it gets judged

On a screen, by eye. The numbers above are a starting point, not a spec - if it reads too
strong or too quick, `UI_AMBIENT_PEAK`, `UI_AMBIENT_RADIUS` and the two speeds are the only
things to touch.
