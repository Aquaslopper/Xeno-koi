# Agent & contributor notes for Xeno Koi

Read this before changing rules, scoring, cards, opponents, or the docs.

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
- Desktop builds: see `build.sh`. The `linux`, `win`, and `wasm` modes expect raylib in `/tmp`.
- `build.sh test` expects `tests/test_engine.c`, which is not in this repository.
- Do not commit build output: `koikoi_linux`, `test_engine`, `dist/`, `site/`, or compiled tool binaries. Build tools to `/tmp`.

## Pages site

The wiki is deployed to GitHub Pages from `main` by `.github/workflows/docs.yml`. Any other workflow that deploys to the same Pages site replaces the whole site, so coordinate before adding one.

## Protected mechanics (read this before touching rules, cards or settings)

A set of zones is protected: rules/scoring, settings/Deus/difficulties,
cards/atlas art, docs/wiki, the core Koi-Koi screen + points + multiplier code
and the guard configuration itself. They may still change — they just cannot
change *silently*. Two checks enforce that on every PR to `main`:

- **`guard`** (Guard protected mechanics) — red if a non-owner changes a
  protected file, with a sticky comment listing the files per zone. Green for
  the maintainer's own PRs.
- **`stale`** (Guard stale protected revert) — red if protected files changed on
  `main` after your branch's merge base, because merging as-is risks dropping
  that newer work. Fix it by merging or rebasing `main`.

The full zone list, the required checks and the local commands are documented in
[`docs/reference/protected-mechanics.md`](docs/reference/protected-mechanics.md).
The three copies of the zone list (`.github/CODEOWNERS`,
`tools/guard_protected.sh`, `tools/guard_stale.sh`) must agree:
`python3 tools/check_protected_zones.py` fails if they drift, and
`bash tools/test_guards.sh` simulates every guard scenario locally.

## Pull requests

Open PRs against `main`. Include the rule change, the docs updated, and the checks you ran.
Keep `src/rules.c`, `src/engine.c`, `src/cards.c`, `src/campaign.c`, `src/storage.c`,
`src/ai.c`, `assets/atlas.png`, `assets/*.png`, `docs/` and the guard files intact
unless the change deliberately touches them; a WASM/UI/build-only PR must not
rewrite them.
