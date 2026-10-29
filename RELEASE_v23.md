# XENO / KOI — Version 23

**The Atrium Plex** · 29 October 2026

Version 23 opens the story. The Atrium Plex is now the spine of the game: forty-eight
cards across twelve months, each one a sector of the vessel with its own record, and
most of them still sealed. Alongside it, the menus that carry you there have been
rebuilt — clearer text, one set of controls instead of six, and a table that finally
keeps a rhythm.

---

## Highlights

**The Atrium Plex.** A twelve-month spine of 48 cards, unlocked one at a time. Cards
begin face down; clearing one turns it over, and completing a month swings the next
month in. This is where the campaign's story lives.

**Three modes inside it.** Speedcoin, Action Card and Training each have their own
screen, their own settings and their own records — no longer reachable only by sitting
down to a campaign match.

**A table with a pulse.** A new travelling token marks whose turn it is to act. It runs
from the player, to the deck, to the CPU and back, and nothing happens at a station
until the token arrives there.

**Menus you can read.** The title, the mode select and every button row were rebuilt
around the type that actually ships — every string measured against its frame — and the
controls now share one size, one gutter and one label style.

---

## The Atrium Plex

- **48 cards, 12 months.** Month 01 is the four tutorials. Months 02–12 hold the story,
  four sectors each, and the atlas tracks every one of them.
- **Two passes.** A standard pass over all 48 cards, then a second, hardcore Deus pass
  with its own atlas and its own unlock state, kept entirely separate.
- **Story on every card.** All 48 sectors carry text in the vessel's voice on the spine
  briefing, and the logfile keeps a written record of each month you have opened.
- **Story protocols.** Fourteen cards deal a real round before their record opens: you
  play it on your ruleset, read one transmission, and the sector clears. The remaining
  cards use the beta instant-clear path.
- **Logfile.** Four tabs — `STORY`, `TRAINING`, `MATCHES`, `ATLAS` — covering the
  twelve monthly documents, your live telemetry, every run you have played, and the
  card atlas with its inspector and achievements.
- **HISCORE boards.** New for this version: Speedcoin and Action Card each have a board
  drawn from the records they already keep — the persisted top eight runs, or your best
  score, run count and recent results.

## The three modes

**Speedcoin** — a solo 180-second saturation clock. Waves of drops, a reaction meter,
serie bonuses, and a top-eight ranklist that survives between sessions. Built to be
played over and over in short bursts.

**Action Card** — the audio-driven duel. Two bio-meters, beat symbols, distraction glow
and a reverb envelope that answers what you do on the table. Runs on KK-ADD-10, always.

**Training** — a full match on any ruleset, with rounds, difficulty, turn timer and
opponent set before you start. Nothing here feeds the campaign.

## The Dealership token

Every two-player table now carries a marker that travels the left margin: player → deck →
CPU → deck → player. Arrival at a station is the permission to act there, and the token
does not leave until that event has finished. It is deliberately short — 50 ms a leg — so
it reads as a moving marker rather than a pause, with a trail and a glow while it travels.

Speedcoin is exempt by design: it is a solo reaction test and a gate would be hostile.
Action Card keeps its own rhythm, which is tied to its audio envelope.

## Menus and readability

- **Title screen** — the brand line and subtitle retuned as a pair, so the two sit close
  together without touching.
- **Mode select** — Pancratium and Atrium Plex are one system of doors: same band, same
  inset, same button row, same label size. Only the width differs, and the names appear
  once each instead of twice.
- **Live read-out** — the Pancratium card carries a rotating line of your combat record;
  the Atrium card says what the vault is for.
- **One control size** — every button in the Plex footer is now identical, and the action
  row above it is a fixed grid.
- **Prologue pacing** — the one-time transmission reveals line by line, and `CONTINUE`
  does not appear until the last line has finished arriving.
- **Logfile text** — larger body type in the story reader, and a tighter briefing card
  with no empty frame inside it.

## Fixes

- **Your records are your own.** The Plex was printing campaign numbers in five places.
  Every one of them now reads Plex data, so a Pancratium win no longer appears as a Plex
  win — and Plex runs no longer appear in the campaign record either.
- **Action Card can only start on KK-ADD-10.** This was true in practice; it is now
  enforced in the code, in one place, so it cannot drift.
- **Text no longer overflows its frames.** Three briefing strings were drawing outside
  the card they belonged to; they were shortened to fit and the type was measured
  against the shipped faces.
- **Button glow is hover-only.** The selected tab used to glow permanently, which made
  the whole footer look active.

## Notes

- **Deus Supremus is open to everyone in this build.** Every player can reach it. That is
  intended for now, not a leftover.
- **Story protocols cover 14 of the 48 cards.** The rest clear immediately; the
  remaining sectors are being written.

## Also in this build

Five rulesets — Nintendo, FudaWiki, Tournament, Yakuza and KK-ADD-10 — three isolated
profile slots with their own settings, stats and history, and a custom music folder that
takes up to 100 tracks alongside the bundled score.

---

*The story continues from here.*
