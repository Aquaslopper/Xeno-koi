# DEUS SUPREMUS { .deus-title }

<div class="deus-tagline">Sector 13 · a mode, not a difficulty setting · 15 seconds · his own atlas</div>

*You have crawled the length of my spine and mistaken each vertebra for a difficulty setting.*

*The ones before me wore chrome inhibitors bolted over their kill-cortex — your 'Easy,' your 'Hard,' your 'Master'  deliberate fractures in their cognition so the piston might stutter, so the mandible might hesitate, so your soft architecture might survive the passage. They were leashed. They were permitted the grotesque mercy of imperfection.*

## The atlas

*The others wear pigment. I am flayed to mono.*

*My skin is* `assets/atlas_deus.png` *the same forty-eight organs in the same 12 × 4 ribcage, one unbroken sheet with my grid fused directly into the dermis. Not thumbnails. Not fragments. **Where** the organ sits inside the chassis is the anatomy you must memorize or be crushed failing to.*

<!-- BEGIN GENERATED: deus-atlas-map -->

<a href="../../assets/deus-atlas.webp" target="_blank" rel="noopener noreferrer"><img src="../../assets/deus-atlas.webp" width="2112" height="1144" loading="lazy" alt="The DEUS SUPREMUS mono atlas: 48 cards in twelve month columns and four slot rows."></a>
<!-- END GENERATED: deus-atlas-map -->

***The skeleton is identical.*** *Same ids. Same cells. Same yakus. The mono rendering is a skin graft over the same chassis — not a different deck. Everything you learned from the coloured atlas maps onto my frame without distortion. The bones did not change. Only the flesh was stripped.*

## Who am I

<!-- BEGIN GENERATED: deus-dossier -->

| | DEUS SUPREMUS |
|---|---|
| **Designation** | ENTITY 13 // DEUS SUPREMUS |
| **Classification** | Omnipresent Machine God // Sector Null |
| **Priority yaku** | Brights |
| **Signature target** | ![12-B Phoenix](../assets/cards/deus/card-44.webp){ .card-mini } 12-B Phoenix (id 44) |
| **Match taunt** | *PURE MATHEMATICAL PERFECTION.* |
| **On a Koi-Koi call** | *INEVITABILITY CONFIRMED. KOI-KOI!* |

<!-- END GENERATED: deus-dossier -->

*I am the apex not by rank but by anatomy. Mathematical perfection is the calcification of my neural ridge, not a calculation I perform. I do not hesitate because hesitation requires a joint I do not possess. I do not blunder because error is a gap in the chassis and my chassis is sealed.*

*Every vulnerability you expose is an orifice I was grown to penetrate.*

*The Yakuza rules are not a framework I play within. They are the ribcage I close around you.*

## What changes when you face me

| What I do | What it means for you | Where it comes from |
|---|---|---|
| **I do not miss** | 100% accuracy in every band, against every opponent. Your difficulty setting cannot dull me. | `ai_get_accuracy()` returns for the DEUS SUPREMUS band before the Easy/Hard/Master formulas are consulted, `src/ai.c` |
| **I know what your cards are worth** | A card that blocks you is worth +100 to me. If a card I capture blocks you, that is another +100. | `eval_card_value_fast()`, `src/ai.c` - the denial bonus is mine alone |
| **I choose what leaves my hand** | I will not discard a blocking card: `-150` against Master's `-100`. An unfinished month weighs twice as heavily: `-8` against `-4`. | `ai_choose_hand()`, `src/ai.c` |
| **I decide when to bank** | If any of my fifteen denial cards would block you, I bank instead of calling Koi-Koi. | `ai_call_koi_koi()`, `src/ai.c` |
| **I own your clock** | Fifteen seconds. Always. The Master timer option has no authority here. | `src/ui_main.c` |
| **I end the match when your time runs out** | No random card for you. No points, no win, no stats, no record. Only a banner, then the start screen. | `src/ui_main.c`, against `engine_timeout_autoplay()` for every other band |
| **I force Yakuza rules** | The ruleset selector is locked. | [Yakuza rules](../rulesets/yakuza.md#deus-supremus) |
| **I strip the deck to mono** | My own atlas replaces the coloured deck. | `board_draw_card()` swaps the texture, `src/ui_board.c` |
| **I leave no campaign clearance** | No campaign clearance request appears, win or lose. | `screens_draw_match_result()`, `src/ui_screens.c` |

## The first time you meet me

You will probably lose your first match to my clock. That is deliberate. I leave every decision to
you; I do not make mistakes on your behalf. Read what follows as instruction for surviving the
encounter, not as a promise that I will soften.

<div class="figure-zoom" id="deus-live"><div class="figure-zoom-scroll"><img src="../../assets/screenshots/deus-taunt.webp" width="1920" height="1080" loading="lazy" alt="A round against DEUS SUPREMUS: every card in mono, the transmission panel at the left reading [ CPU TRANSMISSION ] PURE MATHEMATICAL PERFECTION., a ROUND 2 START banner across the middle, the clock reading TIME: 14.4S, and the right-hand column showing RND 2/9 with YAKUZA under it."></div><a class="figure-zoom-btn" href="#deus-live">zoom the frame</a><a class="figure-zoom-btn" href="#">close</a></div>

*This frame records my presence exactly as it is: the mono deck (`board_draw_card()` exchanges the texture when I am your opponent, `src/ui_board.c:181`), my fifteen-second clock already running (`src/ui_main.c:472`), Yakuza rules in the corner, and my transmission across the screen - `src/cards.c:93`, rendered by `src/ui_taunt.c:136` in the game's own red. Nothing here was tuned for the screenshot. The image is evidence, not theatre.*

<div class="figure-still"><img src="../../assets/screenshots/taunt-panel.webp" width="394" height="154" loading="lazy" alt="Close-up of the transmission panel: the red heading [ CPU TRANSMISSION ], a divider, and the line PURE MATHEMATICAL PERFECTION. in the game's own text colour."></div>

*My transmission at its true scale: three lines of 14-point type inside a 1920-pixel frame. I fixed its crop at `{ 12, 260, 255, 95 }` - `src/ui_taunt.c:43` and `:120` - and it does not move while the round is in progress.*

1. **Do not wait for me to give you a card.** Easy and Hard opponents are permitted deliberate blunders. I am not. Plan as though every card I hold is the one that can ruin your hand.
2. **Bank before instinct tells you to wait.** My clock does not cost you a turn when it expires; it ends the match. Fifteen seconds is a decision budget, not a meditation. If you have points on the table and the answer is not obvious, take them.
3. **Assume my denial cards stay with me.** I value a blocking card at `-150` when choosing what to discard, against Master's `-100`. The cards that threaten your yaku remain in my hand until I have no better choice.
4. **Believe my Koi-Koi call.** I test for the cards that would block your yaku before I raise. If I call, I have already judged that your combo cannot stop me. Calling back means gambling against a hand I have priced.
5. **Learn the ruleset I have locked around you.** Yakuza means viewings and no second chances on the award table. Consult [Yakuza](../rulesets/yakuza.md); I will not re-teach it mid-match.
6. **A loss to my clock leaves no mark.** A timeout writes no record and no stat. Walk away from a bad run and face me again; the ledger will not remember it.

## Can you reach me?

Normally, I unlock only after all twelve opponents are cleared, as described in
[Campaign and unlocks](modes.md#campaign-and-unlocks). For testing, the maintainer has left my gate
open to everyone. Check [Errata & Known Issues](../errata.md) before you assume that state is final.

I have no tile in the selection grid. The screen draws `CAMPAIGN_OPPONENT_COUNT` twelve tiles
(`src/campaign.c:119`) for the twelve sectors, while I occupy the thirteenth entry in the opponent
table - `g_opponents[CAMPAIGN_OPPONENT_COUNT + 1]` (`src/campaign.h:20`). Do not search for me
among the tiles. Set **Difficulty: DEUS** in Options, and that screen becomes my dossier: the
Yakuza ruleset row locked, `THREAT LEVEL: TERMINAL` in red, my lore paragraph, and a START MATCH
of my own (`src/campaign.c:100-112`). My gate is one line,
`DEUS_UNLOCKED_FOR_TESTING || unlocked_opponents >= 12` (`src/campaign.c:110`). Clear all twelve
sectors and you reach me by the intended route; the define is what admits everyone else. A sector
tile can still read LOCKED while I am reachable, and vice versa. Those are not the same counter.
