# Xeno Koi Wiki

**Xeno Koi** is a digital Koi-Koi (Hanafuda) game with five rule variations, a CPU opponent campaign, and a biomechanical CRT presentation. This wiki explains how to play, what each ruleset changes, and how every point is scored.

## Start here

- **New to Koi-Koi?** Read [How to Play](how-to-play.md) first. It covers the deal, a turn, calling Koi-Koi or Shobu, and how a round ends.
- **Choosing a rule set?** See [Rulesets](rulesets/index.md). The Options menu lets you pick one of five.
- **Want the exact scoring?** See [Yaku & Scoring](scoring/yaku.md) and [Multipliers](scoring/multipliers.md).
- **Looking up a card or opponent?** See [Cards](reference/cards.md) and [Opponents](reference/opponents.md).

## Source of truth

These pages describe the **current code**, not the older handoff document. Where the handoff PDF and the code disagree, the code wins and the difference is listed in [Errata & Known Issues](errata.md).

The scoring tables are **generated from the game's C source** (`src/rules.c`, `src/cards.c`) by `tools/gen_wiki_tables.py`, so they cannot quietly drift from the game. See [About these docs](about-the-docs.md).

## Rule sets at a glance

| Rule set | One-line summary |
|---|---|
| [Nintendo](rulesets/nintendo.md) | The classic balanced baseline. One Koi-Koi call per player per round. |
| [FudaWiki](rulesets/fudawiki.md) | Nintendo with bigger Goko and Sanko, and unlimited Koi-Koi calls. |
| [KK-ADD-10](rulesets/kk-add-10.md) | High-scoring: fixed sets stack with counted cards, and Sake Cup counts as a plain. |
| [Tournament](rulesets/tournament.md) | Nintendo scoring with both Viewings removed. The Sake Cup is only an animal. |
| [Yakuza](rulesets/yakuza.md) | Tournament scoring plus Dealer's Privilege: a draw pays the dealer 6 points. |
