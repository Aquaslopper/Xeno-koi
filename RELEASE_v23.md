# XENO / KOI — Version 23

**Changes since Version 22**

---

### Atmosphere

The background light no longer drifts on a fixed left-to-right track. It now wanders to a
new position every ten seconds and circles it, so the chamber is lit differently each time
you look up. Beam reach, beam strength and fog density were retuned around it, and the
ambient vignette was removed so the scene is no longer darkened at the edges.

Lightning is shorter and sharper: the flash now lasts a quarter of a second instead of a
full second, with a shorter hold and a slower fade.

The ash field carries fewer particles, but they live longer and drift steadily sideways
instead of hanging in place.

Embers build up more slowly and cool down more slowly. A single capture now warms the field
gently rather than igniting it, and heat carries further into a round.

### Sound

The room reverb was rebuilt around you. The space is wider, the pre-delay is fixed at about
25 ms, and the left and right channels now use matched comb and allpass spacing, which
removes the metallic edge at high settings.

A capture holds its tail for about four seconds instead of two, and lets go gently instead
of falling away. A peak that reaches full strength is now capped: it stays up for four
seconds, then settles back to 85%, so a stacked round can no longer leave the room pinned
at maximum.

### Motion

Cards take 0.6 s to fly instead of 0.4 s, so captures read as movement rather than a jump.

The CPU now thinks for 0.40, 0.30 or 0.20 seconds depending on difficulty, where every
difficulty previously paused for the same 0.15 s.

The inter-step beat is fixed: it was being multiplied to zero and never played.

### Interface

Panel flares are slightly wider and a little calmer in peak brightness.
