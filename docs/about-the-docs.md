# About these docs

This wiki is built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/). The Markdown lives in the repository's `docs/` folder, so every rule change and its documentation can go into the same pull request.

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
