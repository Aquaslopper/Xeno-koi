# Xeno Koi Wiki

**Xeno Koi** is a digital Koi-Koi (Hanafuda) game with five rule variations, a CPU opponent campaign, and a biomechanical CRT presentation. This wiki explains how to play, what each ruleset changes, and how every point is scored.

The five **Brights** — the most valuable cards in the deck:

![01-B Crane](assets/cards/card-00.webp){ .card-thumb } ![03-B Curtain](assets/cards/card-08.webp){ .card-thumb } ![08-B Moon](assets/cards/card-28.webp){ .card-thumb } ![11-B Rainman](assets/cards/card-40.webp){ .card-thumb } ![12-B Phoenix](assets/cards/card-44.webp){ .card-thumb }

*Left to right: Crane (January), Curtain (March), Moon (August), Rain Man (November), Phoenix (December). Combos of these are on the [Yaku & Scoring](scoring/yaku.md) page.*

## Start here

- **Never played?** Go through [Play it in three turns](tutorial.md) -- one round played out, one idea per line, real cards at every step.
- **New to Koi-Koi?** Read [How to Play](how-to-play.md) first. It covers the deal, a turn, calling Koi-Koi or Shobu, and how a round ends.
- **Choosing a rule set?** See [Rulesets](rulesets/index.md). The Options menu lets you pick one of five.
- **Want the exact scoring?** See [Yaku & Scoring](scoring/yaku.md) and [Multipliers](scoring/multipliers.md).
- **Looking up a card or opponent?** See [Cards](reference/cards.md) and [Opponents](reference/opponents.md).

## Source of truth

!!! info "This wiki is the current reference"
    The wiki describes the game as it is **now**. Older documents are history, not rules:

    - **This wiki and the code in `src/`** are the source of truth.
    - **`HANDOFF_KoiKoi_Giger_v2.pdf` is legacy.** It was the original build spec. Its rules were later changed, and it has not been updated since. Use it only for history. [Errata & Known Issues](errata.md) lists every place it differs from the game.
    - **`STATUS.md` is a legacy snapshot** of one earlier session.

The scoring tables are **generated from the game's C source** (`src/rules.c`, `src/cards.c`) by `tools/gen_wiki_tables.py`, so they cannot quietly drift from the game. See [About these docs](about-the-docs.md).

## Rule sets at a glance

| Rule set | One-line summary |
|---|---|
| [Nintendo](rulesets/nintendo.md) | The classic balanced baseline. One Koi-Koi call per player per round. |
| [FudaWiki](rulesets/fudawiki.md) | Nintendo with bigger Goko and Sanko, and unlimited Koi-Koi calls. |
| [KK-ADD-10](rulesets/kk-add-10.md) | **The maintainer's house rule, and a good place for beginners to start.** Fixed sets stack with counted cards, and the Sake Cup counts as a plain. |
| [Tournament](rulesets/tournament.md) | Nintendo scoring with both Viewings removed. The Sake Cup is only an animal. |
| [Yakuza](rulesets/yakuza.md) | Tournament scoring plus Dealer's Privilege: a draw pays the dealer 6 points. |
