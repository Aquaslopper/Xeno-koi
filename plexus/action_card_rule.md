# ATRIUM PLEX — Action Card Ruleset Specification (`action_card_rule.md`)

> **Status:** Finalized Groundwork Specification
> **Ruleset Scope:** Atrium-Plex-exclusive vs-CPU mode locked to `RULESET_KK_ADD_10`
> **Engine Basis:** `src/engine.c` · `src/rules.c` (`KK-ADD-10`) · `src/ui_board.c` · `src/ui_theme.c` · `src/ui_input.c` · `src/ui_speed.c` · `src/ui_audio.c` · `src/ui_main.c` · `src/ui_plexus.c`

---

## 1. Core Concept

1. **Rhythm & Energy vs-CPU Match (`KK-ADD-10`):** Action Card is a full Player-vs-CPU Hanafuda match played strictly under **`RULESET_KK_ADD_10`** (including the bottom-right `1.00 s` Speedbar and `+1 PT` per `10` Speedcoins at match end), augmented by **dual vertical audio/energy meters**, escalating **perimeter distraction glow**, and **5 beat-armed table symbols**.
2. **Options Integration with Locked `KK-ADD-10` Ruleset:**
   - Launching **`ACTION CARD`** from the Atrium Plex bottom bar **always** uses `RULESET_KK_ADD_10`, even when `DEUS` (`RULESET_YAKUZA` in standard play) or another ruleset is selected in Options.
   - Options **Rounds** (`1..12`) and **Difficulty** (`EASY`, `HARD`, `MASTER` `15 s` / `30 s` turn timer, and `DEUS` AI + `15 s` sudden-death timer returning to Atrium Plex + `atlas_deus` card deck + Deus background/theme `5`) apply in full.
3. **Progression & Campaign Isolation:** Unlocks in the Atrium Plex bottom bar after completing all 4 cards of **Month 08** (`ACTION_CARD_UNLOCKED_FOR_TESTING = 1` kept open during Phase 4 testing). Campaign coins, sector unlocks, `stats.txt`, and `history.txt` remain untouched; all Action Card records persist in `plexsave.txt`.

---

## 2. Table Additions (`1280 × 720`)

All standard vs-CPU table zones remain in place, with two vertical meters flanking the center field:

| HUD Element | Coordinates | Function |
| :--- | :--- | :--- |
| **Left Music Meter** | `{414, 170, 14, 264}` | Theme-colored live beat meter (`90%` peak line near top); independent of `bgm_volume` and Options page-2 FX sliders |
| **Beat-Symbol Badge** | `32 × 32` centered at `(421, 148)` | Displays the active symbol (`Skull`, `Hand`, `Reverse Arrow`, `One Eye`, `Gold Coin`); glows bright when armed |
| **Right Wine-Red Meter** | `{1082, 170, 14, 264}` | Soft-wine-red capture/Koi-Koi energy meter (`reverb_synergy`) with 3 horizontal lines at **`25%`**, **`50%` (enlarged `20×3` middle trigger line)**, and **`75%`** |
| **Ceiling Time Counter** | `24 px` centered at `(1089, 140)` | Appears once the right bar hits the ceiling (`>= 98%`) and counts upward at `10 / sec` exclusively while pinned at the ceiling |
| **Card & Panel Distraction** | Field, both hands, stock & HUD | Escalating perimeter border + crisp `<= 1.05 px` specular perimeter dots driven by the right meter's 3 lines (card interiors never fill or whiten) |

---

## 3. Right-Side Wine-Red Meter, Distraction Tiers & Ceiling Numbers

- **Envelope Input (`src/ui_board.c`):** Each captured card adds **`+0.22`** and each **Koi-Koi** call adds **`+0.21`** to `reverb_energy` (clamped at `1.0`), held for **`0.05 s`** before draining at **`-0.26 / s`** through `0.10 s` / `0.19 s` two-stage smoothing and the `/ 0.68` S-curve (`reverb_synergy`, driving both `audio_set_reverb` and `added_lvl`).
- **Three Horizontal Threshold Lines (`ih = 260 px`):**
  1. **Line 1 (`>= 25%`):** Activates Tier-1 perimeter border & flare-dot distraction (`0.30` panel / `0.32` card glow, `1.00×` dot speed) across HUD panels, field cards, both hands, and the stock pile.
  2. **Line 2 — Enlarged Middle Line (`>= 50%`):** Activates Tier-2 distraction (`0.62` panel / `0.65` card glow, `1.70×` dot speed). Rendered thicker and wider (`3 px` tall with `3 px` side ticks overhanging the bar frame) because **dropping below `< 50%` (`right_fh < 130`) fires any armed symbol**.
  3. **Line 3 (`>= 75%`):** Activates Tier-3 maximum distraction (`1.00` glow, `2.15×` dot speed) and **suppresses** Easy-mode match hints, selection highlights, and card tooltips (`board_is_action_distraction_high()`). Rising **above Line 3 (`right_fh > 195`, `> 75%`)** immediately boosts the left music meter so music beats peak.
- **Ceiling (`>= 98%`) & Time Numbers:**
  - **Only** while `added_lvl >= 0.98` (`right_full`), the `24 px` counter above the right bar ticks upward at **`10` ticks/sec** (`1 + floor(accum)`).
  - Draining below `98%` (or pausing) freezes the banked number in place without resetting it; re-hitting the ceiling resumes counting from that value, and unbanked numbers carry across rounds within the match until claimed by **Gold Coin**.

---

## 4. Left-Side Music Rhythm Meter & Beat-Armed Symbols

- **Pure Beat Detection (`music_meter_processor` in `src/ui_main.c`):** Reads pre-mix match-music frames (~`27 ms` RMS tracker normalized against adaptive `hi_rms` / `lo_rms` beat envelopes with `0.035 s` attack / `0.120 s` release), completely unaffected by `bgm_volume` or Options audio FX sliders.
- **3rd-Line Peak Shift:**
  - When the right meter is at or below the 3rd line (`<= 75%`), the left meter swings up to **`0.80 * vis`** (`<= 80%`), never crossing its **`90%`** line.
  - As soon as the right meter rises **above the 3rd line (`> 75%`)**, `s_action_full_boost` ramps (`0.04 s` attack, `0.14 s` release) toward `min(1.0, 0.40 + 0.68 * vis)` so live musical beats swing across **`90%`** up to the ceiling while valleys dip back below **`84%`**.
- **Cycling & Arming Symbols:** Each upward crossing of **`>= 90%`** (after resetting below `< 84%`, `0.12 s` debounce) advances the `32 × 32` badge by 1 step (`Skull -> Hand -> Reverse Arrow -> One Eye -> Gold Coin`) and sets **`armed = 1`**. Repeated beats while above the 3rd line continue cycling which symbol is armed without firing it early.

---

## 5. Middle-Line (`< 50%`) Symbol Fire & the 5 Symbol Effects

Once a symbol is armed (`armed == 1`), it **fires once as soon as the right-side wine-red meter drops below the enlarged middle line (`right_fh < 260 / 2`, `< 50%`)**, then disarms (`armed = 0`) until a future excursion above the 3rd line arms a new beat symbol. When triggered at `PHASE_TURN_HAND`, gameplay freezes (`2.28 s` total): the full center table badge (`{430, 132, 650, 400}`) materializes in place via a `96`-mote reverse-vaporize condensation (`0.38 s`), holds stationary for `1.30 s`, applies its effect at `1.68 s` for the active turn owner (**YOU** or **CPU**) while preserving the 48-card zone invariant, shatters/vaporizes outward (`0.60 s`) while affected cards animate underneath, and resumes gameplay immediately:

1. **Skull (Field Purge):** Returns half of the current field cards (`field_count / 2`, chosen randomly) face-down into `stock`, animates them back to `{330, 262}`, and shuffles `stock`.
2. **Hand (Hand Swap):** Swaps the player's hand with the CPU's hand while preserving each side's current turn-pair card count (`hand_count[current_player]` and `hand_count[1 - current_player]`), so a mid-pair swap never shifts turn parity or grants consecutive turns.
3. **Reverse Arrow (Capture Recall):** Returns half of **each** player's captured cards (`captured_count[0] / 2` and `captured_count[1] / 2`) face-down into `stock`, shuffles `stock`, and resyncs both players' `yaku_snapshot` and `last_base` combo state.
4. **One Eye (Blind Field Cards):** Flips up to **`2`** face-up field cards **face-down** (`0.36 s` 3D turn-around). Blind cards suppress tooltips, `field_month_mask` Easy-mode hints, selection borders, and click-to-play probing — to match a blind card, the player must remember its month and drag-drop onto it directly (or clear it via stock match / Skull).
5. **Gold Coin (Claim Time Numbers):** The active turn owner (`current_player`: **YOU** on your turn, **CPU** on the CPU's turn) claims the entire banked ceiling counter at **`100 = 10 PTS`** rounded half-up (`pts = (count + 5) / 10`), adding `pts` directly to `total_score[who]`, resetting the ceiling counter to `0`, and launching the enlarging strobe + 56-mote directional vaporize stream toward the winner (`(540, 650)` for `YOU`, `(540, 34)` for `CPU`).

---

## 6. Result Screens & Atrium Plex Telemetry (`plexsave.txt`)

- **Between-Round & Match-End Readouts:** Round-result banners/boxes and the Match-Result screen display both **Speedcoins** (`MP` + `+PTS`) and **Time Numbers** (`YOU`, `CPU`, and remaining `ON METER`) whenever `> 0`.
- **Persistent Telemetry (`plexsave.txt`):** Finished Action Card matches record `ac_runs`, `ac_wins`, `ac_losses`, `ac_best_score`, `ac_symbols` (total symbols fired), and `ac_time_total` (lifetime Time Numbers claimed by the player) alongside Speedbar and Audio FX stats in the `ACTION CARD` briefing panel and `LOGFILE` (saved/restored without pollution during scripted tutorials).
