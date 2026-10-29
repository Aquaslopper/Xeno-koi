# XENO / KOI — Version 23

**What changed since Version 22** · 27–29 October 2026

---

## Story and the Atrium Plex

- **Every card speaks.** All 48 cards now carry a passage on the briefing panel in the
  vessel's voice — three lines each, except the four Month 01 tutorials, which keep their
  instructional text and get one line underneath it.
- **Story protocols.** Fourteen cards deal a real round before they clear: you play it on
  your ruleset, a keeper transmission opens across the table, and the sector completes
  when you close it.
- **The logfile was rebuilt** into four tabs — `STORY`, `TRAINING`, `MATCHES`, `ATLAS` —
  with the twelve monthly documents, live telemetry, every run you have played, and the
  card atlas with its inspector and achievements. The second, hardcore pass gets its own
  atlas.
- **Plex records are Plex records.** Campaign numbers were being printed in five places
  inside the Atrium Plex; all five now read Plex data. A Pancratium win no longer appears
  as a Plex win, and Plex runs no longer appear in the campaign record.

## Atmosphere

- **The background light moves.** The beam no longer drifts on a fixed left-to-right line:
  it wanders between positions while circling, with a stronger core, a tighter falloff and
  a soft aura instead of hard bands. The ambient vignette over the whole scene is gone.
- **Flash and ash retuned.** Thunder flash is halved to 0.25 s. The ash field runs fewer,
  longer-lived particles with a steady sideways drift, and the ember heat was recalibrated
  so an ordinary capture glows warm and cools slowly instead of jumping to full on the
  first card.

## Audio

- **The reverb envelope breathes.** A capture holds the tail for about four seconds
  instead of two, decays at 0.4/s instead of 1.5/s, and the smoothing was slowed so the
  room opens rather than snaps. A full-strength peak is capped: it stays up for four
  seconds, then settles to 85%, so a stacked round can no longer leave the tail pinned.
- **Room and tone.** The reverb slider now covers a wider space, the pre-delay is fixed at
  about 25 ms, and the right-channel comb and allpass spacing were retuned to match the
  left, which removes the metallic edge at high settings.
- **Tighter effect caps.** The action-driven chorus, bass and reverb mix is hard-limited,
  and the card-impact glow on a moving card is dimmed so it no longer flares over the
  flight.

## Readability

- **Small text is sharper.** Font rasterisation sizes were lowered across all three faces,
  so labels are drawn close to their native resolution instead of being stretched up from
  a large master.
- **Title screen.** The brand line and the subtitle were re-sized as a pair, the subtitle
  now sits in a heavier weight, and the two are close together without touching.
- **Button glow is hover-only.** A selected tab used to glow permanently, which made whole
  footers look active. The drifting text motes were removed from headings and plain
  centred text, and now appear only on hovered buttons.
- **Hearing safety warning** at startup.

## Menus

- **Quit runs the loader** with a `-decoupled-` caption instead of closing the window on
  the click.
- **Mode select is two doors**, each named once on its button, sharing one grid, one inset
  and one label size. The twelve decorative ticks down the Atrium card's edge are gone —
  they repeated a number the card already prints.
- **The Pancratium card** carries a rotating read-out of your combat record; the Atrium
  card states what the vault is for.
- **Options** is reachable from the campaign page.
- **Player-facing labels** were cleaned of leftover internal vocabulary, so every screen now
  says what it is in the game's own words.
- **Atrium Plex screen.** It now names itself at the top left. Every footer button is the
  same size on one gutter, the action row is a fixed three-slot grid, the briefing card
  was tightened with no empty frame inside it, and new information glows for longer. The
  opening transmission reveals line by line, and `CONTINUE` only appears once the last
  line has landed. **HISCORE** boards were added for Speedcoin and Action Card.

## Tables

- **The Dealership.** A marker travels player → deck → CPU → deck → player down the left
  margin. Arriving at a station is the permission to act there, and it does not leave
  until that event has finished, so the table has a visible rhythm instead of an invisible
  pause. Action Card is included; Speedcoin is exempt by design.
- **Action Card is locked to KK-ADD-10** in a single place in the code rather than inferred
  in two.
