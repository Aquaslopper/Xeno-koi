# Changes since v22

27–29 October 2026. Everything below is a net value change or a net code change in the
tree between the `v22` tag and `main`.

---

## `assets/depth_fog.fs` — background light

- Light position: the fixed left-to-right drift (`0.08 + 0.84 * (0.5 - 0.5*cos`) is replaced
  by a position that steps to a new random point every 10 s and circles it
  (`lightUV = 0.5 + 0.42*(cos/sin)` with a per-step phase nudge).
- Beam spin: `time * 0.215` -> `time * 0.09`.
- Beam reach: a directional `beamReach` term replaced the radial `reach` in the beam sum;
  radius `0.90` -> `0.80`.
- Beam strength: `layers *= 1.0 + 0.40`.
- `flashGain` base: `1.0` -> `0.60`.
- Fog density: `* 0.80` clamped to `0.55` -> `* 0.50` clamped to `0.35`.
- Ambient vignette block: removed.
- Final output: `blended * vignette` -> `blended * 0.8`.
- New `normalMask = smoothstep(0.02, 0.12, map.a)` gating `lightAdd`.

## `src/ui_board.c` — table

- `ASH_COUNT` 155 -> 100, `ASH_BURST_MAX` 420 -> 260.
- Ash wind: added a constant `+30.0` sideways drift.
- Ash particle life: `0.85` -> `0.95` base.
- Ember heat: `EMBER_KICK_CAP` 0.10 -> 0.08, `EMBER_KICK_KOI` 0.30 -> 0.10,
  `EMBER_DECAY` 0.08 -> 0.06, `EMBER_HOT` 0.52 -> 0.50, `EMBER_MIN` 0.07 -> 0.08,
  `EMBER_SMOOTH_ATK` 0.12 -> 0.22, `EMBER_SIZE_BONUS` 0.16 -> 0.12.
- Impact glow on a moving card while it flies: `0.8` -> `0.4`.
- Thunder: `thunder_flash` 1.0 -> 0.25, `thunder_hold` 1.50 -> 0.50, flash decay
  constant `0.80` -> `1.00`.
- Reverb envelope driven from the table: per-capture gain `0.10` -> `0.09`,
  hold `2.00` -> `4.10` (and only re-armed below 0.85), decay `1.5` -> `0.4` per second,
  smoothing `0.10` -> `0.70` and `0.19` -> `0.14`.
- Peak hold: a full-strength peak is capped, held 4.10 s, then forced back to 0.85.

## `src/ui_audio.c` — reverb

- Right-channel comb lengths: `1147,1229,1307,1399,1459,1523,1597,1657` ->
  `1126,1204,1292,1370,1436,1502,1574,1631` (now matched to the left).
- Right-channel allpass lengths: `577,461,359,251` -> `561,446,341,231`.
- Room size: `0.65 + 1.55*room` -> `0.65 + 1.85*room`.
- Diffusion: `0.50 + 0.15*room` -> `0.25 + 0.10*room`.
- Pre-delay: `220 + 1980*room` (5–50 ms) -> fixed `1102.5` (~25 ms).

## `src/ui_theme.c` — panel flare

- Flare alpha: `22 + 233*shine` -> `22 + 200*shine`.
- Flare width: `0.7 + 0.4*shine` -> `0.9 + 0.3*shine`.

## `src/ui_main.c` — pacing

- Card flight duration: `0.40` -> `0.60`.
- CPU think duration: fixed `0.15` -> `0.40 / 0.30 / 0.20` by difficulty.
- `trigger_event_beat` multiplier: initial `0.0` -> `1.0` (the beat was being multiplied
  to zero).
- `trigger_cpu_step_beat` now also skips while the action-card mode is active.

## Also changed

- `src/campaign.c`: comment on `DEUS_UNLOCKED_FOR_TESTING`.
- `build.sh`, `tools/`, `.github/`: contributor tooling.
- Docs: `LEARN_THE_CODE.md`, `AGENTS.md`, `STATUS.md`, `Dealership.md`, `plexus/*`.
