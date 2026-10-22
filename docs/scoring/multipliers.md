# Multipliers & Awards

When a round ends with a winner, the **award** is the base points multiplied by up to three separate multipliers. The formula is the same in every rule set.

## The formula

```
award = base × seven_multiplier × opponent_koi_multiplier × caller_multiplier
```

<div class="figure-zoom" id="award-pipeline"><div class="figure-zoom-scroll"><img src="../../assets/figures/award-pipeline.png" width="1520" height="508" loading="lazy" alt="Six boxes in a row: your pile, its base points from the ruleset, then three gates - a base of 7 or more doubles, an opponent who called Koi-Koi doubles, your own calls multiply by 1 plus the number of calls up to 6 - and the award at the end."></div><a class="figure-zoom-btn" href="#award-pipeline">zoom the diagram</a><a class="figure-zoom-btn" href="#">close</a></div>

| Multiplier | When it applies | Value |
|---|---|---|
| **7+ points** | The base is 7 or more. | ×2 |
| **Opponent called Koi-Koi** | The player who lost the round called Koi-Koi at least once. | ×2 |
| **Your own Koi-Koi streak** | Each Koi-Koi call you make in the round. | ×(1 + calls), capped at ×6 |

The multipliers stack by multiplying together. The 7+ and opponent multipliers are each a fixed ×2. Only your own streak has a cap: it stops at ×6, which you reach after five calls.

### Your own Koi-Koi streak

Each time you call Koi-Koi, your own multiplier goes up by one step:

| Your Koi-Koi calls this round | Caller multiplier |
|---:|---:|
| 0 | ×1 |
| 1 | ×2 |
| 2 | ×3 |
| 3 | ×4 |
| 4 | ×5 |
| 5 or more | ×6 (cap) |

Your own calls never double your opponent's score. They only raise your own award.

## Award tables

Each table shows the award for a given base, with and without the opponent's Koi-Koi.

<!-- BEGIN GENERATED: award-grid -->

**Opponent Koi-Koi calls: none (x1)**

| Base points | no self Koi (x1) | 1 call (x2) | 2 calls (x3) | 3 calls (x4) | 4 calls (x5) | 5+ calls (x6) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1 | 2 | 3 | 4 | 5 | 6 |
| 2 | 2 | 4 | 6 | 8 | 10 | 12 |
| 3 | 3 | 6 | 9 | 12 | 15 | 18 |
| 4 | 4 | 8 | 12 | 16 | 20 | 24 |
| 5 | 5 | 10 | 15 | 20 | 25 | 30 |
| 6 | 6 | 12 | 18 | 24 | 30 | 36 |
| 7 | 14 | 28 | 42 | 56 | 70 | 84 |
| 8 | 16 | 32 | 48 | 64 | 80 | 96 |
| 9 | 18 | 36 | 54 | 72 | 90 | 108 |
| 10 | 20 | 40 | 60 | 80 | 100 | 120 |
| 12 | 24 | 48 | 72 | 96 | 120 | 144 |
| 15 | 30 | 60 | 90 | 120 | 150 | 180 |
| 20 | 40 | 80 | 120 | 160 | 200 | 240 |

**Opponent Koi-Koi calls: 1 or more (x2)**

| Base points | no self Koi (x1) | 1 call (x2) | 2 calls (x3) | 3 calls (x4) | 4 calls (x5) | 5+ calls (x6) |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 2 | 4 | 6 | 8 | 10 | 12 |
| 2 | 4 | 8 | 12 | 16 | 20 | 24 |
| 3 | 6 | 12 | 18 | 24 | 30 | 36 |
| 4 | 8 | 16 | 24 | 32 | 40 | 48 |
| 5 | 10 | 20 | 30 | 40 | 50 | 60 |
| 6 | 12 | 24 | 36 | 48 | 60 | 72 |
| 7 | 28 | 56 | 84 | 112 | 140 | 168 |
| 8 | 32 | 64 | 96 | 128 | 160 | 192 |
| 9 | 36 | 72 | 108 | 144 | 180 | 216 |
| 10 | 40 | 80 | 120 | 160 | 200 | 240 |
| 12 | 48 | 96 | 144 | 192 | 240 | 288 |
| 15 | 60 | 120 | 180 | 240 | 300 | 360 |
| 20 | 80 | 160 | 240 | 320 | 400 | 480 |

*(computed by `calculate_award()` in `src/rules.c`; base <= 0 always awards 0)*

<!-- END GENERATED: award-grid -->

## Worked examples

**Example 1: a bank with a streak.** You call Koi-Koi three times, then bank a round worth 6 base points. The opponent never called. Caller multiplier is ×4, and the base is under 7, so there is no 7+ bonus.

`6 × 4 = 24`

**Example 2: the 7+ bonus plus the opponent's call.** You bank with 8 base points, and the opponent called Koi-Koi once. You made no calls.

`8 × 2 (7+) × 2 (opponent Koi-Koi) × 1 = 32`

**Example 3: a big streak.** You bank with 12 base points after one Koi-Koi call. The opponent never called.

`12 × 2 (7+) × 2 (your streak) = 48`

Check examples 1 and 3 against the tables above: 6 with a ×4 caller multiplier is 24, and 12 with one call is 48.

## Non-scoring awards

Some awards are fixed and skip the formula:

- **Lucky hand (Teshi or Kuttsuki):** 6 points, no multipliers.
- **Yakuza Dealer's Privilege:** 6 points, no multipliers. See [Yakuza](../rulesets/yakuza.md).
- **Round with a base of 0:** no award. A round can only end as a Shobu after the base has gone up, so a Shobu always has a base of at least 1.

## KK-ADD-10 match-end minipoints

The [KK-ADD-10 speed minigame](../rulesets/kk-add-10.md#speed-minigame-the-timer-bar-and-minipoints) is separate from this round-award formula. Its minipoints do not multiply a yaku award and are not added after each round. Once the final round is over, every full 10 minipoints adds 1 point to your match total before the winner and result screen are decided; any remainder is discarded.

## Koi-Koi decisions

The game only offers Koi-Koi or Shobu when your score has gone up since your last call this round. The question is always "stop and bank this, or risk it for more?" The award line on the decision screen shows the exact multiplied value you would get if you stopped now.

On Nintendo, Tournament, and Yakuza, you can only call Koi-Koi once per round. After that, the next score increase is an automatic Shobu. On FudaWiki and KK-ADD-10 you can keep calling. See the [rule set pages](../rulesets/index.md).
