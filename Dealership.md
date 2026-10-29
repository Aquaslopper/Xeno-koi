# The Dealership

**This document owns the turn rhythm of the table.** If you change how a turn
paces, how long the token takes, where it travels or what is allowed to act when,
you change this file in the same commit. Nothing about the dealership lives in
`STATUS.md`, in a code comment, or in an agent's memory.

Owner decision. The token is not decoration and not a suggestion: it is the
thing that decides who may act.

---

## 1. Why it exists

In the physical game nothing appears on the table by itself. A hand moves it, and
you can follow a turn because you *watch* the movement — the stock is touched, a
card is turned, it lands, and only then does the other player answer. The eye
paces itself to what it can see.

A videogame compresses all of that into state changes. A card landing, a capture
and a draw can happen inside one 16 ms frame, several at once, so the player
cannot attribute cause to effect: *was that mine? did it match? why did the CPU
do that?* — and because events can overlap, the order is ambiguous too.

The token fixes exactly that. Every event gets a visible actor and a visible
arrival, and the same object that shows where the action is also guarantees that
nothing else acts:

- **No dealership, no movement.** The token is not one more condition next to the
  others — it is *the* key. Nothing that changes the game state runs unless
  `dealership_unlocked()` is true. Every bypass in `src/ui_main.c` is audited in
  §5 and each one may bypass `board_busy` (so a choice stays clickable during
  capture hitlag) but never the token.
- **Arrival is permission.** An event may not begin until the token has arrived
  at that event's station.
- **Departure waits for completion.** The token may not leave a station until
  that station's event has completely finished.
- One authority for "whose move is it", so two events can never be unlocked
  simultaneously — enforced by construction, not by convention.

A hidden delay is experienced as lag and cannot be judged by eye. The same
duration, shown, reads as composed. That is the whole trade.

---

## 2. Stations and the route

Three stations on a vertical run down the left margin, in the 1280x720 logical
space, taken from the real HUD geometry:

| Station | Point | What sits there |
|---|---|---|
| `DL_PLAYER` | `(279, 693)` | right end of the `YOU SCORE` bar, beside `[DEALER]` |
| `DL_DECK` | `(286, 324)` | immediately left of the face-down stock at `330, 262` |
| `DL_CPU` | `(279, 27)` | right end of the `CPU SCORE` bar |

The deck is **between** the two players, so it is always passed through:

```
PLAYER ──50ms──▶ DECK ──50ms──▶ CPU ──50ms──▶ DECK ──50ms──▶ PLAYER
   ▲             ▲                      ▲             ▲
   │             │                      │             │
player plays   deck turns its         cpu plays   deck turns its
a hand card    card onto the table    a hand card card onto the table
```

**The deck is a station, not a pipe.** The draw is the only event in Koi-Koi with
no player as its cause; a card that materialises with no actor is the least
legible thing in the game. The deck must be seen to do it.

**One token serves both draws**, because in the real game one hand deals the
stock for both players. The dealer turns your card and the opponent's. The `D` on
the token is the dealer's hand, whoever's turn it is — which is why the badge does
not change per player.

---

## 3. Timing

`DL_LEG_SECONDS = 0.050f` — one leg between neighbouring stations.

- Two legs per transfer = **100 ms**, which is the same budget as the hidden
  inter-step pause this replaces. The rhythm becomes visible; the game does not
  get slower.
- Below roughly 40 ms a hop reads as a teleport and the eye cannot track it,
  which defeats the point. Above roughly 150 ms it stops being a beat and becomes
  a wait. 50 ms is the fast edge of trackable.
- Motion is integrated from `dt`, never counted in frames, so it is the same
  duration at 60 Hz and at 144 Hz.

---

## 4. State machine

The station the event is waiting on is **derived** from the engine phase — there
is no second copy of the turn state:

| Phase | Station |
|---|---|
| `PHASE_TURN_HAND`, `PHASE_HAND_CHOICE` | `DL_PLAYER` if `current_player == 0`, else `DL_CPU` |
| `PHASE_TURN_DRAW`, `PHASE_DRAW_CHOICE` | `DL_DECK` |
| `PHASE_DECIDE` | the current player's station (the token holds) |
| anything else (deal, lucky check, round end, match end) | `DL_NONE` — nothing to gate |

Per frame:

1. `dealership_set_active(...)` — the mode gate (§6).
2. On `PHASE_DEAL` / `PHASE_DEALER_SELECT` the token is parked on the dealer.
3. `dealership_update(dt, station, settled)` where
   **`settled` means the previous event's own animation and motion are finished**:
   `!badge_freezing && !motion_is_reflowing(&g) && !anim_is_blocking(&anim) && !cinematic_is_active()`.
4. While `!settled` the token holds its station. Once settled it travels one leg
   at a time, snapping on arrival.
5. `dealership_unlocked()` is true only when the token has arrived at the station
   the phase demands.

The existing `event_beat_timer` is deliberately **not** part of `settled`. Travel
and the old beat therefore run concurrently rather than stacking: the player
waits about `max(beat, travel)`, not `beat + travel`.

---

## 5. The gates

Three places consult `dealership_unlocked()`:

| Where | Effect |
|---|---|
| player input (`input_unlocked`) | the player cannot act until the token is at `DL_PLAYER` |
| the CPU think/fly block | the CPU cannot think until the token is at `DL_CPU` |
| the stock-fly start in `PHASE_TURN_DRAW` | the deck cannot deal until the token is at `DL_DECK` |
| the Action Card badge-event trigger | a badge event cannot start until the token is at that player's station |
| the hand-choice click, and a live drag | neither may resolve an engine call without the token at the player's station |

**On the freeze.** A badge event sets `badge_freezing`, and while it is 1 nothing
moves — the token stays put, and no gate needs to know about the freeze, because
`settled` is false and the token simply holds. The gate on the badge trigger is
therefore not about the frozen state: it is about the instant *before* it, where
`badge_freezing` is still 0 and the badge is about to fire. Without that gate a
badge could start while the marker was still in flight, which is movement before
arrival — the one thing the token exists to forbid. Once the freeze is on, the
marker's job is already done and there is nothing left to care about.

Everything else stays as it was. Choice overlays (`PHASE_HAND_CHOICE`,
`PHASE_DRAW_CHOICE`) resolve at the station that owns them, so a choice never
triggers travel.

---

## 6. Scope

**Active:** campaign, Training, Deus — every mode where turns alternate.

**Not active — `Speedcoin`.** It is a solo, timed, wave mode:
`plexus_speedcoin_setup_wave()` forces `dealer = 0` and `current_player = 0` and
it runs against `speed_mode_clock()`. There is no CPU station, so half the route
does not exist, and its premise is speed. Injecting a 100 ms gate into a reaction
test would sabotage the mode it decorates. (`trigger_cpu_step_beat()` already
excluded Speedcoin for the same reason.)

**Active — `Action Card`, by owner decision (29 October 2026): *"action card has
to be included."*** An earlier draft of this document excluded the mode; that
exclusion is reversed and must not be re-derived from the caution below.

The caution still stands as a constraint, not as a reason to exclude:

- The mode is frozen by the owner. Covering it with the dealership is *not*
  permission to tune it. If its pace reads wrong under the token, that is
  **reported**, not adjusted.
- Its rhythm is tied to the meter and the reverb envelope. Added gating delay
  there broke the audio/meter timing once before (`df4f6d7`: *"revert
  unrequested turn-pacing delays breaking reverb/meter timing"*), which is why
  `settled` includes `badge_freezing`: the token never departs in the middle of a
  badge event, so the envelope and the meter keep their own timing and the token
  waits for them rather than the reverse.
- The token therefore adds no delay of its own to Action Card. It travels inside
  the window the mode already spends settling, and its arrival is the release.

**Not active — replay playback**, which paces itself from the recording.

Menus are unaffected: the token only exists while a table is live.

---

## 7. Files

| File | Role |
|---|---|
| `src/ui_dealership.h` | the entire public surface (§8) |
| `src/ui_dealership.c` | geometry, state machine, trail, drawing |
| `src/ui_main.c` | `dealership_station_for()` maps phase → station; the three gates; the draw call |

The module is engine-free on purpose: it knows stations, a position and a `dt`.
It cannot drift into gameplay because it is not given the game state.

## 8. API

```c
void dealership_reset(int station);                    /* park on the dealer */
void dealership_set_active(int active);                /* off = every gate open */
void dealership_update(float dt, int station, int settled);
int  dealership_unlocked(void);                        /* arrival == permission */
int  dealership_travelling(void);
void dealership_draw(void);
```

`station` is `DL_PLAYER`, `DL_DECK`, `DL_CPU` or `DL_NONE`. When the module is
inactive every gate reads unlocked, so a mode that does not use the dealership
behaves exactly as before.

---

## 9. Look

Tuned on the owner's display, 29 October 2026. All sizes below are radius in the
1280x720 logical space and all of them scale by `pulse = 1 + 0.35 * glow`.

- **Size.** Hairline ring at radius **8** (was 5), bright core **2.6** (was 1.5),
  `D` at **14 pt** (was 9). The marker reads at a glance from across the table;
  the previous one was small enough to be mistaken for a cursor.
- **Trail.** Up to **28** ghosts living **280 ms** (`DL_TRAIL_LIFE`), radius
  `2.2 + 5.0 * life`, alpha `150 * life^2`, and a wide soft halo on the freshest
  eight so the head of the trail bleeds. Two samples are pushed per travelling
  frame, because a 50 ms leg is only about three frames at 60 Hz and one sample
  per frame would draw beads instead of a streak. The trail is why the leg reads
  as movement rather than as a jump — it is not decoration, it is the thing that
  makes a 50 ms transfer legible.
- **Movement glow.** `glow` eases toward 1 while travelling (rate 18/s) and back
  to 0 after arrival (rate 7/s), so departure lights up and arrival settles
  instead of snapping. While lit it adds a third, much wider bloom (radius
  `34 * glow`), widens the two base discs, and pushes the ring toward full white.
- Glow stays transparent throughout — never a filled white blob.
- No spin, no bounce, no idle pulse. The only animation is the one tied to
  movement. (Body metaphors and decorative motion were removed from this project
  before; do not add them back here.)

These numbers, `DL_TRAIL_MAX`, `DL_TRAIL_LIFE` and the rates above are the whole
tuning surface for the token's appearance. Nothing in section 4 or 5 depends on
them: resizing the marker or changing the trail cannot move a gate.

---

## 10. Verification

- `./build.sh test` — engine suite, table UI, music/settings/Options.
- `-fsyntax-only` sweep of all of `src/*.c` at `-Wall -Wextra` against the raylib
  headers; the desktop binary is not linked in this environment.
- `./tools/action_guard.sh` — the Action Card pins must stay green.

**Not covered by any test:** the feel. No test asserts that the rhythm reads
well; that is judged on a display. `tests/` is locked, so no test was added for
this module by design, not by omission.
