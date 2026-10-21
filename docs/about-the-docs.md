# About these docs

This wiki is built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/). The Markdown lives in the repository's `docs/` folder, so every rule change and its documentation can go into the same pull request.

## Which documents are authoritative

1. **The code** in `src/` decides what the game does.
2. **This wiki** (`docs/`) explains the current rules and is the reference for players and contributors.
3. **`HANDOFF_KoiKoi_Giger_v2.pdf`** is the legacy build spec. It is kept for history only. Where it disagrees with the code or the wiki, the handoff is wrong. [Errata & Known Issues](errata.md) lists the differences.
4. **`STATUS.md`** is a dated development handoff. It records completed work, checks that were run, and items still to verify; it is not the rules authority or a release record.

When you change a rule, update the code, the wiki page for that rule, and the generated tables in the same change. Keep [`AGENTS.md`](https://github.com/Aquaslopper/Xeno-koi/blob/main/AGENTS.md) in mind too: it gives future contributors and coding agents the same instructions.

## Two kinds of content

**Hand-written pages** explain rules, strategy, and concepts. Edit these directly.

**Generated blocks** hold the tables that must match the code: card list, yaku values, stacking, award grid, ruleset probes, and the opponent roster. They sit between marker comments:

```markdown
<!-- BEGIN GENERATED: NAME -->
...
<!-- END GENERATED: NAME -->
```

Do not edit text inside a generated block. It is overwritten the next time the generator runs.

## Regenerating the tables

```bash
python3 tools/gen_wiki_tables.py          # rewrite the generated blocks in docs/
python3 tools/gen_wiki_tables.py --check  # exit 1 if docs are out of date
```

The generator compiles `tools/wiki_tables.c` against the real `src/cards.c` and `src/rules.c` and reads the output. It needs `gcc`. The compiled binary is built in a temporary folder and is never committed.

Run the generator whenever you change scoring, cards, or opponents. CI runs `--check` and fails if the docs are stale.

## Card images

The card pictures come from the game's own art (`assets/atlas.png`). They are cut into one small file per card by:

```bash
python3 tools/make_card_images.py          # needs Pillow
python3 tools/make_card_images.py --check  # exit 1 if an image is missing
```

The images live in `docs/assets/cards/` and are named by card ID (`card-00.webp` to `card-47.webp`, plus `back.webp`). Generated tables link them automatically; hand-written pages link them with the `card-thumb`, `card-mini`, or `card-tiny` styles from `docs/stylesheets/cards.css`. Never edit the thumbnails by hand — rerun the script.

## README art

The masthead band and the three buttons at the top of the repository's `README.md` are not hand-made either.
`tools/make_readme_art.py` paints them out of the shipped files: the background is one of the eight frames the
game loads, dimmed and lettered exactly as `screens_draw_start()` does it, then passed through the scanline and
gamma maths of `assets/postprocess.fs` at the default settings. The typefaces are the game's own
(`assets/audiowide.ttf`, `assets/play.ttf`, both OFL with their licence files beside them), so every constant in
the tool names the line of `src/` it came from.

```bash
python3 tools/make_readme_art.py          # needs Pillow
python3 tools/make_readme_art.py --check  # exit 1 if an image is missing
```

They are build products: `docs/assets/readme/hero.jpg` plus three button PNGs. Edit the tool, not the images, if
a button label changes -- and note that the tool refuses to draw a label that would overflow the game's 300 x 40
button rect, because `draw_btn()` has no wrapping to fall back on.

`tools/make_board_figure.py` writes `docs/assets/readme/table-map.png` next to them: the play screen drawn at
1280 x 720 from the layout constants in `src/`, with the field slots from `board_field_slot_xy()`, at one pixel per
game pixel. It takes the same `--check`, and it has the same rule -- if the layout in `src/` changes, the figure is
regenerated rather than repainted.

## Diagrams

Prose about the turn and about the award is hard to hold in your head, so both are also drawn.
`tools/make_flow_figures.py` paints two figures into `docs/assets/figures/`:

```bash
python3 tools/make_flow_figures.py          # needs Pillow
python3 tools/make_flow_figures.py --check  # exit 1 if a figure is missing
```

- `turn-flow.png` -- one turn as [How to Play](how-to-play.md) describes it: hand stage, the four
  match counts, stock stage, the verdict, and which of three endings follows.
- `award-pipeline.png` -- the six gates in `calculate_award()`, in the order the code applies them,
  on [Multipliers & Awards](scoring/multipliers.md).

They carry **no numbers**. A figure that repeats the score table goes stale on the next balance
pass; the numbers are generated into the tables on the same page, and the figure only carries the
shape of the rule. Every box names the `src/` function and line range it was read from, and the
sizes are derived from measured text metrics, because a box sized by eye is a box that overflows
the first time somebody rewords a sentence.

Both figures are drawn 760 px wide -- the width Material gives an article -- and saved at 2x, so
they are sharp on a hi-dpi screen. The `figure-zoom` class in `docs/stylesheets/cards.css` fits one
to the column and blows it up on click.

## Screenshots

Eight pictures of the running game sit in `docs/assets/screenshots/`: the board in play, a
tooltip, the Koi-Koi screen, a transmission from DEUS, the match result, the options screen, the
campaign grid and the stats screen. They and the one reference scan beside them are the **only
images on this site that no tool can produce** - this repository has no GPU and no headless browser,
so nothing here can render the game. A frame is captured by hand and imported once:

```bash
python3 tools/make_screenshots.py --from ~/shots   # re-encode the captures it is given
python3 tools/make_screenshots.py                  # re-derive the seven close-ups
python3 tools/make_screenshots.py --check          # exit 1 if one is missing or hand-cropped
```

Two things about them are not accidents:

- **Nothing is cropped by hand.** The seven close-ups are cut from the committed frames at the
  rectangles the code draws - the capture lanes at `src/ui_board.c:302`, the decide overlay at
  `src/ui_screens.c:466`, the transmission panel at `src/ui_taunt.c:43`, the twelve campaign tiles at
  their `{ 155 + i * 82, 96, 68, 110 }` step from `src/campaign.c:120` - scaled by the frame's own
  ratio (1920 x 1080 of a 1280 x 720 canvas is 1.5, and the tool verifies that against the captures
  rather than trusting the number). Where a picture is not UI pixels - the traditional hanafuda
  sheet on [Cards](reference/cards.md) - it is imported through the same tool at its own size and its
  own quality, and cropped by nobody. If a panel moves in `src/`, the crop moves with it and
  `--check` fails on anything trimmed by eye.
- **They carry their own numbers, and those numbers are the game's.** The Koi-Koi frame says
  "Hanami-zake, 5 pts" because that is what `score_captures()` handed that player in that round.
  Captions quote them as what a picture shows; the rules pages still get their totals from the
  generated tables, which is the difference between a screenshot and a screenshot used as a source.

Some stored captures predate the new combo sidebar and Stats coin rows. Treat them as examples of the pictured match, not a live inventory of every current control; the prose and source describe the current UI. See [Modes & Settings](reference/modes.md) for coin rows and [How to Play](how-to-play.md#controls) for the combo windows.

Capturing a new one: reach the state, screenshot the whole window at any size, save a PNG, drop it
in a folder and run the import with `--from`. Do not crop or export it as JPEG - the crop is the
tool's job, and a second lossy pass over the game's own scanlines is not worth the kilobytes.

## Previewing locally

```bash
pip install mkdocs-material
mkdocs serve
```

Then open the address MkDocs prints, usually `http://127.0.0.1:8000`.

## Publishing

The site is published from `main` by the **Docs** GitHub Actions workflow to GitHub Pages.

## Adding a page

1. Create the Markdown file under `docs/`.
2. Add it to the `nav:` section of `mkdocs.yml`.
3. Link to it from a related page, so nobody has to find it through the sidebar.
4. A figure embedded as raw HTML (a `figure-zoom`/`figure-still` block around an `<img>` tag) must
   have its `src` written from where the page is *published*, not from where the file sits: MkDocs
   rewrites paths written in Markdown and leaves raw HTML alone, and with `use_directory_urls` every
   page is a directory — `docs/tutorial.md` arrives at `/tutorial/index.html`, so its HTML images
   need `../assets/…`, and anything under `docs/reference/` needs `../../assets/…`. A wrong path
   there 404s on the live site while `mkdocs build --strict` stays green, because it never looks
   inside HTML; that is why CI also runs `python3 tools/gen_wiki_tables.py --check`, which resolves
   every raw-HTML `<img>` against the published directory and fails with the corrected path
   (`broken_html_srcs()`).
