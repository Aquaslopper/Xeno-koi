# Errata & Known Issues

!!! warning "The handoff is legacy"
    `HANDOFF_KoiKoi_Giger_v2.pdf` (11 Oct 2026) was the original build spec for the first version of the game. **It is no longer the rulebook.** The rules have been changed and extended since, and the handoff has not been updated.

    This page lists where the handoff and the current game disagree. **Where they differ, the wiki and the code are correct.** Do not follow the handoff's rules.

## Rules that changed since the handoff

| Topic | Handoff says | Code does now | Where in code |
|---|---|---|---|
| Caller's own Koi-Koi | "Your own Koi-Koi never doubles your own score." (§7b) | Each of your Koi-Koi calls raises your own multiplier by one step (×1 → ×6, capped). | `koi_caller_multiplier()` in `src/rules.c` |
| Koi-Koi limit | Nintendo: one call per player. FudaWiki and KK-ADD-10: unlimited. | Tournament and Yakuza also allow one call, like Nintendo. | `finish_stock_turn()` in `src/engine.c` |
| KK-ADD-10 Tanzaku | Counts from 5 ribbons, as n − 4. Test case: all six ribbons = 12. | Counts from the 4th ribbon, as n − 3. All six ribbons now score **13** (10 for the combined set + 3). | `score_captures()` in `src/rules.c` (commented "was >= 5, n-4") |
| Viewings | Always on. | Disabled in Tournament and Yakuza. | `score_captures()` in `src/rules.c` |
| Yakuza ruleset | Not in the handoff. | Added: Tournament scoring plus Dealer's Privilege. | `apply_dealer_privilege()` in `src/engine.c` |
| First-dealer tie | §7a: "same month: redraw." The appendix uses rank 20/10/5/1 "for the first-dealer tie-break." | Same month: the higher card type deals. Only an identical month and type forces a redraw. | `engine_determine_first_dealer()` in `src/engine.c` |

## Defaults that differ from the handoff

| Setting | Handoff says | Code does now | Note |
|---|---|---|---|
| Rounds | 6 | `storage_load_settings()` sets **2** rounds when there is no `settings.txt`. | Fresh installs start at 2 rounds. |
| Ruleset | Nintendo | `storage_load_settings()` sets **KK-ADD-10** when there is no `settings.txt`. | Fresh installs start on KK-ADD-10, the maintainer's house rule. |

Both defaults only apply when `settings.txt` is missing. Once the file is saved, your chosen values are used.

## Files that changed since the handoff

| Topic | Handoff says | Code does now | Where in code |
|---|---|---|---|
| Saved files | "Saving: Settings and stats only, in text files next to the exe. No mid-match save." | Finished matches are recorded as well, in a `replays/` folder next to the exe (the ten newest automatic records plus copies the player saved by name). There is still **no mid-match save** - a match has to be played to the end to get a record, and unfinished matches get none. | `src/replay.c`, `src/ui_replay.c` |

## Known issues

1. **[DEUS SUPREMUS](reference/deus-supremus.md) is open to everyone, on purpose.** `src/campaign.c` defines `DEUS_UNLOCKED_FOR_TESTING` as `1`, so the final opponent does not require unlocking the twelfth sector. The maintainer keeps it open while testing the mode. This is intentional, not a bug. Revisit it when the maintainer decides the mode is ready.
2. **FudaWiki's Koi-Koi take-back is not implemented.** The handoff mentions it. The game does not offer it.
3. **[DEUS SUPREMUS](reference/deus-supremus.md) logic.** The DEUS SUPREMUS difficulty uses the Master decision logic with stronger card evaluation and 100% accuracy. It is not a separate AI.
4. **Fullscreen on the browser build belongs to the page, not the game.** The web shell owns it: the `[ FULLSCREEN ]` button and the in-game Options row both toggle the canvas, and `settings.fullscreen` is ignored and cleared on web. Browsers only grant fullscreen from a user gesture, so the page button always works while Safari may ignore the in-game row. Player-facing detail: [Modes & Settings](reference/modes.md).

5. **Audio DSP regression corrected (22 October 2026).** The former unequal left/right reverb decode failed the presence-band check at −18.7 dB Side/Mid. The page-2 audio controls now use symmetric Mid/Side decoding and linked compression/limiting. The unchanged regression passes at approximately −11.1 dB, with additional mono compatibility, channel-balance, bass endpoint/compression and late-bass reverb tests. [Design, sources and measured checks](reference/audio-dsp.md) distinguish numerical verification from live listening, which remains to verify.

## Stereo widening retired; chorus and space reverb (22 October 2026)

The maintainer stopped the stereo-widening work after repeated left-pulled/moving-image feedback. Investigation also reproduced 5.24 dB of Side pumping from its content-driven guards with fixed controls. The width code and its obsolete alignment buffer are now **removed**, not relabelled. Options page 2 offers **Chorus**, **Bass boost**, and **Space reverb**. Chorus is a separate two-voice modulated-delay effect with identical channel modulation, fixed channel gains and no feedback or auto-pan; modulation is intentional. The room slider now changes reflection spacing, diffusion and decay, with measured impulse T60 estimates from ~0.6 s at 25% to ~6 s at maximum at 44.1 kHz. Deep reverb bass stays centred; bass admission still opens over the last 20%. Bass processing and action triggers are unchanged. The new `effect_chorus` setting defaults off; old `effect_stereo` values are ignored. All-off bypass is again bit-identical with zero latency. See [current DSP design and checks](reference/audio-dsp.md). The stereo-fix entries below are **superseded history**, not current behaviour. Live listening is still required.

## Width delay and bass loudness correction (22 October 2026)

Listening feedback identified two problems not caught by the original stereo-energy tests: the width stage used delayed reverb output, and the bass compressor reduced the entire track. Width was first moved to a separate short phase path with no reverb/slapback feed; the subsequent spectral-balance correction below replaces those all-passes. Bass now compresses only the added low-frequency component (up to 4:1), mixed over the original music, with more output headroom. New regressions check width impulses for echoes, positive loud-bass gain, and the level of higher-frequency music immediately after a kick. See [measured verification](reference/audio-dsp.md).

## Stereo spectral-balance correction (22 October 2026)

The independent-all-pass width path could sound left-heavy in the treble: a simultaneous six-tone test measured +13.388 dB L/R at 8 kHz, despite a balanced overall RMS meter and passing isolated-tone checks. It was replaced by a symmetric Hilbert FIR with fixed dry alignment, making the added Side quadrature to the centre at each frequency. New tests measure every tone separately while all play together, including onset and settled windows; the initial 17-tap correction measured below 0.09 dB on onset and 0.0121 dB settled. The stronger revision below retains per-frequency checks. Mono comparison and bypass checks explicitly account for the tiny fixed alignment; there is no zero-latency bypass claim. Bass/reverb control tuning is unchanged. See [DSP design and checks](reference/audio-dsp.md).

## Stereo strength correction (22 October 2026)

The balanced 17-tap version still sounded too centred: on bundled game3, maximum width only reduced correlation from 0.936 to 0.860. That revision used 129 taps with 64-sample fixed dry/bypass alignment (~1.45 ms at 44.1 kHz), a stronger upper slider range, and up to equal Side/Mid power rather than a 0.49 power cap. The mono centre, source-aware guard and separate reverb tank remain. That revision reached 0.183 correlation on the same track, about 41% Side energy versus the previous 7%. A new bass-heavy regression requires strong, increasing width, not merely nonzero Side. Per-frequency balance measures <0.027 dB settled and <0.143 dB on onset; the settled bound stays 0.1 dB, while the explicitly transient bound is 0.2 dB for this longer/stronger path. Impulse output after 5 ms is still zero. These are offline checks, not a headphone listening claim; see [DSP verification](reference/audio-dsp.md).

## Fixed-ear phase widening removed (22 October 2026)

Further listening feedback reported a broken/left-pulled image even though the RMS tests passed. The synthetic Hilbert Side kept a fixed phase polarity: exchanging input L/R did not exchange output L/R. A new regression reproduced this with 0.228065 maximum sample error at only 25% width. The synthetic path is now removed. Width expands existing dry/reverb Side, retains the centre and source-aware guard, and leaves mono mono when Reverb is zero. Channel-exchange error is now zero at 25/50/100% width, bass off/on and 44.1/48/96 kHz. The stereo strength fixture still reaches equal Mid/Side power; game3 reaches correlation 0.214 using its actual stereo. Both dry and bypass retain the prior 64-sample timing alignment; bass processing and the reverb tank/filter parameters are unchanged. This corrects a demonstrable orientation flaw, but does not claim that live listening has been verified. [Current DSP design and checks](reference/audio-dsp.md).

## Campaign progression correction

A clearance previously advanced the saved opponent count directly, bypassing the dossier's silver/gold coin requirements; old saves also kept that inflated count. Unlocks are now recalculated from earned coins, so progress beyond bronze-only sectors requires the displayed silver and gold totals.

## Table UI updates

- **Alternative card control:** click a hand card, then a matching field card (or empty field if unmatched). Dragging remains available; matching and scoring are unchanged.
- **Combo-panel overflow corrected:** the right-column summaries now have fixed bounds and shortened names. Click either panel for paginated details, with play and the turn timer paused until dismissal. Existing gameplay screenshots predate this panel layout; the generated table map reflects it.

## Verified against the code

The following handoff test cases were checked against the current code and still hold:

- Goko: 10 (Nintendo), 15 (FudaWiki), 10 (KK-ADD-10)
- Rain Man + 3 other Brights = Ame-shiko 7
- Rain Man + 2 other Brights = 0
- Curtain + Moon + Sake Cup = 10 (Nintendo)
- KK-ADD-10: Ino-shika-cho trio + 2 animals = 6
- Nintendo: all six ribbons = 10
- Nintendo: Ino-shika-cho trio + 2 animals = 7 (not 7 + 1)
- KK-ADD-10: nine Plains + Sake Cup = 1
- Teshi and Kuttsuki award 6 points
- Both players lucky = 0–0 draw (non-Yakuza)
- Exhaustion = 0–0 draw, and the dealer stays

The one handoff test case that no longer holds is KK-ADD-10 with all six ribbons, which is now 13.
