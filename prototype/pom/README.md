# Parallax Occlusion Mapping for the Xeno Koi backgrounds

**Short answer: yes, it is possible, and it is a small, self-contained change — but only the
foreground of the paintings survives it.** The tubes, ribs and skulls that sit close to the
camera get real depth. The skyline and the sky do not: there is no height in a painted wall
of sky, so anything you treat as geometry up there just stretches. The fix for that is a
per-background far-field mask, which is section 4.

This folder is a working prototype, not a patch to the game. Nothing in `src/` or `assets/`
that the game loads was changed.

```
prototype/pom/
├── index.html            live preview (WebGL2), the thing to look at first
├── js/app.js             viewer: pointer-driven eye, sliders, A/B wipe
├── shaders/pom.frag      the game's background shader + the ray march, in GLSL ES 3.0
├── serve.py              static server (document root = repository root)
├── tools/make_height_maps.py    builds assets/height/*.png out of the shipped art
├── tools/render_stills.py       offline stills, numpy port of depth_fog.fs
└── stills/               rendered A/B frames (harvest, castle, face)
```

Run it:

```bash
python3 prototype/pom/serve.py            # http://localhost:8000/prototype/pom/
```

The height maps it needs are already generated, inside this folder at `height/<name>_height.png`
(8 files, 640×360 grey, 488 KiB total). Regenerate any time with
`python3 prototype/pom/tools/make_height_maps.py`; `--check` verifies they match the art.

---

## 1. What the backgrounds are right now

`assets/depth_fog.fs` draws one full-screen quad and does three things to `assets/<bg>.jpg`:

| stage | what it uses | effect |
|---|---|---|
| moving light | `assets/<bg>_normal.png` | a drifting, spinning 4-band beam, specular + bloom over the normal map |
| depth fog | luminance of the blend | sine drift fog pushed into the dark valleys, bottom 62% |
| vignette | screen position | corner falloff |

So the shader already has exactly the inputs POM wants, minus one: **a height field**. The
normal map gives a *slope* per texel but no absolute height, and POM marches against height.
That missing channel is the whole cost of this feature.

While building the generator I noticed the shipped normal maps already carry something that
looks a lot like it: their **alpha channel** correlates 0.83–0.89 with luminance and reads as a
smooth cavity/height map (bright in the seams between plates). If the maintainer generated
those files, it is worth asking what the alpha was meant to be — it may already be the height
data, in which case `make_height_maps.py` gets much simpler.

## 2. What POM adds

For every pixel the shader casts a short ray *into* the screen along the view direction, walks
it down the height field until it goes under the surface, and then samples the colour and
normal maps **at the point it hit** instead of at the pixel itself. Three consequences:

* **Geometry moves, not the picture.** As the eye moves, near surfaces slide against far ones.
* **Self-occlusion comes free.** Whatever the ray hit is in front of whatever lies behind it, so
  a tube edge covers the seam behind it at grazing angles.
* **Two cheap extras.** The distance the ray travelled is a contact-AO term, and an 8-tap march
  toward the same drifting light the beam uses gives soft shadow in the crevices. Both are in
  the prototype; both are what make it look like carved metal rather than a wobbling texture.

Cost, at 1280×720: 22–32 march taps + 8 bloom taps + 8 shadow taps, so roughly 40–48 texture
fetches per pixel over the board quad. On a desktop GPU that is nothing. On Intel integrated
and mobile browsers it is the number to watch, which is why the prototype shows a live
estimate in the corner and lets you drag steps down to 8.

## 3. What it looks like

`stills/*-wipe.jpg` is one frame with the divider at the middle: flat on the left, POM on the
right, both with the game's fog and light intact. `stills/*-crop-2x.jpg` is the same at 2× on
the two frames where the change is easiest to read.

Measured on the three test frames at 18 px depth:

| frame | eye-to-surface shift, mean | max | mean luminance flat → POM |
|---|---:|---:|---|
| harvest | 2.2 px | 35.6 px | 0.100 → 0.095 |
| castle | 3.0 px | 35.7 px | 0.164 → 0.150 |
| face | 3.2 px | 27.6 px | 0.093 → 0.083 |

**Where it wins:**
* The foreground tube bundles in Harvest and Face. Ribs get real roundness, the seams between
  them go dark, and the bundle visibly sits in front of the fog behind it.
* The castle's near ridges and the ring gate on the right.
* Nothing at all happens to the sky, the moon, or the fog — they are sampled through a height
  of ~0, so the march returns immediately. That is why the effect is *safe*: it cannot damage
  the parts of the painting that are pure atmosphere.

**Where it fails, and why:**
* Skyline silhouettes. In `castle-wipe.jpg`, right of the divider, the distant spires have
  been pushed sideways and the sky has smeared into the structures. That is not a bug in the
  march — it is the height field claiming the skyline is solid geometry. In a painted 2D image
  the spires are *background*; in a height field anything not black is a surface.
* The same happens to the figure in Face at higher depth: at 26 px the body slides against the
  tubes behind it, which is physically right and artistically wrong.
* It reads as a **global shift**, not local relief, when the depth is pushed. Local relief needs
  local height contrast, and a JPEG painting only has so much.

Depth is the single most important number. 14–18 px at the frame edge looks like a room behind
the table. 34 px, which is what a naive "let's make it 3D" pass reaches for, starts to warp the
art. The prototype defaults to 14.

## 4. The one design decision that makes or breaks it

**POM needs a far-field mask, per background.** A 640×360 (or even 160×90) greyscale PNG with
`1 = near geometry, 0 = sky/far`, multiplied into the height before the march. Then the skyline
has no height, the march returns instantly, and only the foreground parallaxes. That is the
difference between "the painting moved" and "there is a room in front of the painting".

Cheap first approximation, no new art required, and what I would try first:

```glsl
// treat dark + bright-sky regions as far, keep only the mid-tones that face the camera
float far = smoothstep(0.55, 0.85, lumSky) + (1.0 - smoothstep(0.03, 0.10, lumSky));
float h = heightAt(uv) * (1.0 - clamp(far, 0.0, 1.0));
```

Painted masks would be better; eight of them is maybe an hour of work in an image editor, and
they are tiny files.

## 5. How it would land in the C game

There is no new dependency and no change to game logic. The pattern is the one the shader
already uses:

1. **Shaders.** New `assets/pom.fs` and `assets/pom_web.fs` (the prototype's `pom.frag` is the
   ES 3.0 version; the shipped pair is `#version 330` and `#version 100`). It is
   `depth_fog.fs` plus a `parallax()` function; the moving-light, fog and vignette blocks stay
   byte-identical.
2. **Assets.** `assets/height/<bg>_height.png` ×8, generated by
   `prototype/pom/tools/make_height_maps.py` (they live in `prototype/pom/height/` today; only
   the game's own `assets/` would need a copy). 488 KiB total, so nothing on the wire until
   they are actually used.
3. **`src/ui_board.h`.** `Texture2D bg_heights[8]` in `BoardAssets`, plus
   `ShaderLocation` for `vantage`, `depth`, `steps`, `ao`, `shadow`.
4. **`src/ui_board.c:board_load_assets`.** Load them next to `bg_normals[i]`, same
   `TEXTURE_FILTER_BILINEAR` / `WRAP_CLAMP`.
5. **`src/ui_board.c:board_draw_table`** (and `draw_bg_if_loaded` in `ui_screens.c`). Before
   `BeginShaderMode`, push the uniforms. The vantage is the smoothed virtual mouse the game
   already tracks (`ui_theme_set_mouse`), damped with the same exponential the rest of the UI
   uses, plus a slow idle sine so the room breathes when the player is still.
6. **Options page 2.** One row, `BG PARALLAX: OFF / LOW / HIGH` → `steps` 0 / 18 / 32. OFF must
   be the default on first run so nobody's framerate dies on a weak laptop before they see the
   menu.

Two web-specific constraints the desktop build does not have:

* The `#version 100` path is GLES2. No dynamic loop bounds, so the step count has to be a
  compile-time constant (a `#define` per quality level, or three shader variants). The
  prototype's `for (int i = 1; i <= MAX_STEPS; i++) { if (i > steps) break; }` is the portable
  shape of that.
* The height maps must be power-of-two-friendly and sampled with `LINEAR`/`CLAMP_TO_EDGE`; the
  current 640×360 is not POT. Emscripten + WebGL2 is fine with it, WebGL1 is not, so 512×288 is
  the safer size for the web pair. That is a one-line change in the generator.

## 6. Honest recommendation

Worth doing **if** the game gets an optional graphics row, because this is the single largest
change in perceived depth available without new art. The order I would build it in:

1. Fix the height maps: painted far-field masks, and settle what the normal-map alpha is.
2. Ship OFF by default, HIGH on desktop.
3. Judge it live with the fog on. The fog is a depth cue that already works; POM's job is only
   to make the foreground agree with it. If a frame looks worse with POM than without, the
   mask is wrong, not the march.

Not worth doing if the backgrounds are meant to stay *paintings*. POM will always tell you
that the sky has no volume, and it will always stretch a spire. That is the trade.
