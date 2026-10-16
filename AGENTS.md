# Agent & contributor notes for Xeno Koi

Read this before changing rules, scoring, cards, opponents, or the docs.
# [SYSTEM DIRECTIVE: HYPER-FOCUS MODE ENGAGED]

You are acting as a precision systems programmer on the Xeno-Koi C99 / Raylib game engine. 
Read and strictly enforce all operational guardrails below before writing any code.

STRICT CONSTRAINTS:
1. DO NOT refactor or rewrite entire files.
2. DO NOT modify any card databases, matching rules, or existing scoring logic.
3. DO NOT introduce new libraries or overhaul the state architecture.
4. Keep all changes minimal, surgical.

OUTPUT REQUIREMENT:
WRITE SILENTLY TO DISK. ZERO CODE OUTPUT. NO PREAMBLE. SHORT SUMMARY OF WHAT YOU CHANGED IN CHAT.

## Where the truth lives

1. **The code** (`src/`) decides what the game does.
2. **The wiki** (`docs/`, published from `mkdocs.yml`) explains the current rules. It is the reference for players and contributors.
3. **`HANDOFF_KoiKoi_Giger_v2.pdf` is legacy.** It is the original build spec from 11 Oct 2026. Do not implement or document rules from it. Where it disagrees with the code or the wiki, the handoff is out of date. [`docs/errata.md`](docs/errata.md) lists each difference.
4. **`STATUS.md` is a legacy snapshot** of one earlier session.

If a handoff rule and the wiki disagree, follow the wiki and the code, and say so in your summary.

## Owner decisions (do not reverse without asking the maintainer)

- **Deus Supremus is open to everyone on purpose.** `DEUS_UNLOCKED_FOR_TESTING` in `src/campaign.c` is `1` while the maintainer tests the mode. Leave it alone.
- **KK-ADD-10 is the maintainer's own house rule**, taken from an earlier game. It is the recommended first rule set for beginners. It is expected to change, so keep `docs/rulesets/kk-add-10.md` and the generated tables in step with `src/rules.c`.
- **The handoff stays in the repo unchanged** for history. Do not edit or delete the PDF.

## Changing rules, cards, or opponents

Do all of these in the same change:

1. Edit the code in `src/`.
2. Regenerate the engine-derived tables:
   ```bash
   python3 tools/gen_wiki_tables.py
   ```
3. Update the hand-written prose in `docs/` that describes the changed rule. The generator only fills marked blocks. It does not rewrite the explanations.
4. Add or update the entry in `docs/errata.md` if the change alters something the handoff described or a known issue.
5. Check the docs:
   ```bash
   python3 tools/gen_wiki_tables.py --check
   python3 -m mkdocs build --strict
   ```

CI runs the same checks (`.github/workflows/docs.yml`) and fails if the generated tables are stale.

## Building and checking

- `tools/wiki_tables.c` links the real `src/rules.c` and `src/cards.c`. Do not copy scoring logic into the docs or the generator.
- Card images in the wiki (`docs/assets/cards/card-<id>.webp` + `back.webp`) are cut from `assets/atlas.png` by `tools/make_card_images.py` — never edit the art or the thumbnails by hand. Regenerate after any atlas change: `python3 tools/make_card_images.py` (needs Pillow: `pip install --break-system-packages pillow`). Check they exist with `python3 tools/make_card_images.py --check`.
- `README.md`'s masthead band and link buttons are generated from the shipped art by `tools/make_readme_art.py`, and the to-scale table map `docs/assets/readme/table-map.png` with `tools/make_board_figure.py` (both write into that folder; both take `--check`) -- never edit `docs/assets/readme/*` by hand. Regenerate with `python3 tools/make_readme_art.py`, check with `--check`.
- Desktop builds: see `build.sh`. The `linux`, `win`, and `wasm` modes expect raylib in `/tmp`.
- `build.sh test` expects `tests/test_engine.c`, which is not in this repository.
- Do not commit build output: `koikoi_linux`, `test_engine`, `dist/`, `site/`, or compiled tool binaries. Build tools to `/tmp`.

## Code changes
- Keep a change to the smallest diff that solves the problem. If a fix
  needs more than ~80 lines, stop and describe the plan first.
- Never bundle a refactor with a feature. Separate commits.
- Do not add a parameter to a shared function to pass a mode flag
  without saying so in the summary.
- Before finishing, list what you touched and what you deliberately
  did not touch.

## Pages site

The wiki is deployed to GitHub Pages from `main` by `.github/workflows/docs.yml`. Any other workflow that deploys to the same Pages site replaces the whole site, so coordinate before adding one.

## Pull requests

Open PRs against `main`. Include the rule change, the docs updated, and the checks you ran.
