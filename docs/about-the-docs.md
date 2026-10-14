# About these docs

This wiki is built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/). The Markdown lives in the repository's `docs/` folder, so every rule change and its documentation can go into the same pull request.

## Which documents are authoritative

1. **The code** in `src/` decides what the game does.
2. **This wiki** (`docs/`) explains the current rules and is the reference for players and contributors.
3. **`HANDOFF_KoiKoi_Giger_v2.pdf`** is the legacy build spec. It is kept for history only. Where it disagrees with the code or the wiki, the handoff is wrong. [Errata & Known Issues](errata.md) lists the differences.
4. **`STATUS.md`** is a legacy snapshot of one earlier session.

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
