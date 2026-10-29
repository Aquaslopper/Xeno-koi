# XENO / KOI — Version 23

**Everything that changed since Version 22.**

---

## Title screen

- The brand line is smaller and sits lower, closing the gap to the subtitle — the two now
  read as one block instead of a line with a hole under it.
- The subtitle "HANAFUDA: ACOUSTICA APEXUS" is 10 % larger and set in a heavier weight,
  synthesised by drawing it twice with a sub-pixel offset.
- The screen-wide text shimmer is gone. It had grown from a few slow dots on button text
  into a fast shimmer across every centred line — title, headings, credits, results — and
  is now reduced to a hover-only affordance on buttons.
- The selected menu tab no longer glows permanently.

## Mode select

- The two doors are built the same way — same inset, same copy/button/back rows — and
  differ only in width. Pancratium is the short door, Atrium the long one, and its copy is
  drawn directly on the door instead of in a framed strip of its own.
- One name per door, one grid for both.
- The old ladder diagram between the cards is removed.
- Each door now carries a live read-out: the combat figures rotate every few seconds so
  the card answers "what have I done in here" without a wall of text.
- Campaign options are reachable directly from the mode select.
- Quitting returns through the card loader instead of cutting to black.

## Atrium Plex

- The one-time prologue wall reveals about 1.5× slower, with the CONTINUE prompt derived
  from the last line so it cannot appear while text is still fading in.
- The prologue is labelled: TRANSMISSION 00 // ATRIUM PLEX ENTRY, with a rule under it.
- THE ATRIUM PLEX is printed top-left of the screen.
- The bottom bar is six identical buttons on an even gutter; the old row had four different
  widths, which made OPTION read as fatter than its neighbours.
- The action row is a three-slot grid instead of a ragged line.
- New HISCORE board for the two modes that keep a score, showing the saved Speedcoin top
  eight and the Action Card match rows. Read-out only — no new save data.
- The briefing card is shorter, with its copy and dividers re-spaced and its glow held
  longer before fading. The row and the settings drawer follow it.
- The Logfile, the month buttons and the label text are sharper: faces are now rasterised
  at sizes close to how they are drawn, instead of being downscaled to about a tenth of
  their master size.
- The 48-card progression is called STORY throughout the interface, and the Deus tier is
  called PASS. No identifiers or save keys changed.
- The Pancratium career record no longer bleeds into the Plex screens.

## Table

- New dealership: a single travelling token that moves PLAYER ↔ DECK ↔ CPU and owns the
  rhythm of the round. Nothing at a station may act until the token arrives, and the token
  does not leave until that station's event has finished — so the draw is now visibly its
  own beat between the two players. It has a trail, a glow while moving and a D badge.
- The token is a gate, not a condition: shortcuts in the round loop cannot skip past it.
- Action Card is fixed to its own ruleset (KK-ADD-10). It no longer inherits the campaign
  ruleset, the Deus override or an Options change, so the mode always plays as designed.

## Atmosphere

- The background light no longer follows a fixed left-to-right drift; it wanders to a new
  position roughly every ten seconds and circles it. This is still being tuned — the light
  is not where it should be yet.
- Beam reach, beam strength, flash gain and fog density were retuned around that, and the
  ambient vignette was removed from both the desktop and web shaders so the scene is no
  longer darkened at the edges.
- Lightning is shorter and sharper: a quarter second instead of a full second, shorter
  hold, slower fade.
- The ash field carries fewer particles, but they live longer and drift steadily sideways.
- Embers build and cool more slowly, so a single capture warms the table gently instead of
  igniting it, and heat carries further into a round.

## Sound

- The room is wider, the pre-delay is fixed at about 25 ms, and the left and right
  channels now use matched comb and allpass spacing, removing the metallic edge at high
  settings.
- A capture holds its tail around four seconds instead of two, and releases it gently.
- A peak that reaches full strength is capped: it holds for four seconds, then settles back
  to 85 %, so a stacked round cannot leave the room pinned at maximum.
- A hearing-safety warning now plays at startup.
- Reverb and mix settings are under continuous tuning.

## Motion

- Pieces fly in 0.6 s instead of 0.4 s, so captures read as movement rather than a jump.
- The CPU thinks for 0.40, 0.30 or 0.20 seconds depending on difficulty instead of a flat
  0.15 s for every opponent.
- The inter-step beat is fixed — it was being multiplied to zero and never played.
- Motion timing is under continuous tuning.

## Interface

- Panel flares are slightly wider and calmer at peak brightness.
