# XENO / KOI — Version 23

**What changed since Version 22 (27–29 October 2026)**

---

## The story

- **All 48 spine cards now carry story text.** Months 02–12 were placeholders; each sector
  has its own passage in the vessel's voice on the briefing panel and in the logfile.
- **Story protocols.** Fourteen cards (all of Month 02, plus each month's face card) now
  deal a real round on your ruleset and open one transmission before the sector clears.
  The remaining cards still use the instant-clear path.
- **Logfile rebuilt** into four tabs — `STORY`, `TRAINING`, `MATCHES`, `ATLAS` — with the
  twelve monthly documents, live telemetry, every run, and the card atlas with its
  inspector. The Deus second pass gets its own atlas.
- **Plex records are Plex records.** Campaign numbers were being printed in five places
  inside the Atrium Plex; all five now read Plex data. A Pancratium win no longer shows up
  as a Plex win, and Plex runs no longer appear in the campaign record.

## Atmosphere

- **The background light moves.** Instead of a fixed left-to-right drift, the beam now
  wanders between positions while circling, with a stronger core and a tighter falloff.
  The ambient vignette over the whole scene is gone.
- **Flash and ash retuned.** Thunder flash is halved to 0.25 s. The ash field runs fewer,
  longer-lived particles, and the ember heat was recalibrated so an ordinary capture glows
  warm and cools slowly instead of jumping to full on the first card.

## Audio

- **The reverb envelope breathes properly now.** A capture holds the tail for ~4 s instead
  of 2, decays at 0.4/s instead of 1.5/s, and the smoothing was slowed so the room opens
  rather than snaps. A full-strength peak is capped: it stays up for ~4 s, then drops back
  to 85%, so a stacked round no longer leaves the tail pinned.
- **Room and tone.** The reverb slider now spans a wider space, the pre-delay is fixed at
  ~25 ms, and the right-channel comb/allpass spacing was retuned to match the left, which
  removes the metallic edge at high settings.

## Readability

- **Small text is sharper.** Font rasterisation sizes were lowered across all three faces,
  so labels are drawn closer to their native resolution instead of being stretched up
  from a large master.
- **Title screen.** The brand line and subtitle were re-sized as a pair — the subtitle now
  sits in a heavier weight, and the two are close together without touching.
- **Button glow is hover-only.** A selected tab used to glow permanently, which made whole
  footers look active. Screen-wide text motes were removed; they now appear only on
  hovered buttons.
- **Hearing safety warning** at startup.

## Menus

- **Quit runs the loader** with a `-decoupled-` caption instead of closing the window on
  the click.
- **Mode select is two doors**, each named once on its button, on one shared grid. The
  twelve-tick Spine ladder is gone — it repeated a number the card already prints.
- **The Pancratium card** carries a rotating read-out of your combat record; the Atrium
  card says what the vault is for.
- **Options** is reachable from the campaign page.
- **The word "spine" is gone** from every player-facing label.
- **Atrium Plex screen:** now titled, footer buttons all one size, action row on a fixed
  grid, briefing card tightened with no empty frame inside it, longer glow when new
  information arrives, and the opening transmission reveals line by line — `CONTINUE`
  only appears once the last line has landed. **HISCORE** boards were added for Speedcoin
  and Action Card.

## Tables

- **The Dealership.** A marker travels player → deck → CPU → deck → player down the left
  margin. Arriving at a station is the permission to act there, and it does not leave
  until that event finishes — the table now has a visible rhythm instead of an invisible
  pause. Action Card is included; Speedcoin is exempt by design.
- **Action Card is locked to KK-ADD-10** in one place in the code, not inferred in two.

---

## Known

- **Deus Supremus is open to every player in this build.** Intended for now, not a leftover.
- **Story protocols cover 14 of the 48 cards.** The rest clear immediately while the
  remaining sectors are written.
