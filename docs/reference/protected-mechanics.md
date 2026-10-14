# Protected mechanics

Some parts of Xeno Koi are **protected**: they can still be changed, but a
non-owner change turns the pull request red and asks for the maintainer's
review. The guards exist so contributors find out immediately — and see *why* —
instead of a maintainer noticing a revert months later.

## What is protected

| Zone | Files |
| --- | --- |
| Rules / scoring | `src/rules.*`, `src/engine.*`, `src/cards.*`, `tools/wiki_tables.c`, `tools/test_koikoi_trigger.c`, `docs/rulesets/`, `docs/scoring/` |
| Settings / Deus / difficulties | `src/campaign.*` (`DEUS_UNLOCKED_FOR_TESTING`), `src/storage.*` (`GameSettings`), `src/ai.*` (`DIFFICULTY_*`) |
| Cards / atlas / art | `assets/atlas.png`, `assets/atlas_deus.png`, `assets/back.png`, `assets/deus.png`, `assets/*.jpg`, `docs/assets/cards/`, `docs/assets/deus-bg.webp`, `tools/make_card_images.py` |
| Docs / wiki / rulesets | `docs/`, `mkdocs.yml`, `overrides/` |
| Core Koi-Koi mechanics | `src/ui_screens.c`, `src/ui_board_status.c`, `src/board_layout.*`, `src/rules.c`, `src/engine.c` (combo trigger, stock turn, Shobu, award, multiplier) |
| Guard configuration | `.github/CODEOWNERS`, `.github/workflows/` |

The same list is written down three times, on purpose:

* `.github/CODEOWNERS` — who GitHub asks for a review,
* `.github/workflows/guard-protected.yml` (via `tools/guard_protected.sh`) — which zones the guard reports,
* `.github/workflows/guard-stale.yml` (via `tools/guard_stale.sh`) — which zones must not be silently reverted.

`python3 tools/check_protected_zones.py` fails if the three ever drift apart.

## The two guards

| Check | Workflow | Fails when |
| --- | --- | --- |
| `guard` | [Guard protected mechanics](https://github.com/Aquaslopper/Xeno-koi/blob/main/.github/workflows/guard-protected.yml) | a non-owner changes a protected file |
| `stale` | [Guard stale protected revert](https://github.com/Aquaslopper/Xeno-koi/blob/main/.github/workflows/guard-stale.yml) | protected files changed on `main` after the branch's merge base, so merging as-is risks reverting them |

Both are part of the *Protect core mechanics - main* ruleset, together with
`build` and `build-windows`. Merging needs an approving review, a CODEOWNERS
review and those status checks.

Each guard runs twice:

* **`pull_request_target`** runs the copy of the workflow and of the guard script
  that live on `main` (trusted, read/write token). This run owns the required
  check name (`guard`, `stale`) and it is the one that posts the sticky comment.
* **`pull_request`** runs the copy from the pull request itself and reports
  `guard-preview` / `stale-preview`. It is informational only — a pull request
  can edit its own copy, so it can never satisfy a required check, and it cannot
  quietly switch the guard off.

Neither guard checks out or executes pull request code: they only read git
objects (`git merge-base`, `git diff`) and the pull request head is fetched
through `refs/pull/<number>/head`, which works for fork pull requests too.

## What you will see

* **Protected file changed by a non-owner** — the `guard` check is red, a sticky
  comment lists the files grouped by zone, and each file gets an error annotation
  in the *Files changed* tab. If the change is intentional, say why in the pull
  request and wait for the owner; if it is not, restore the file exactly as
  `main` has it.
* **Stale branch** — the `stale` check is red and the comment lists the protected
  files that changed on `main` since your base. Merge or rebase `main`
  (`git fetch origin main && git merge origin/main`) and push again.
* **Owner pull requests** — green, no comment. The ruleset bypass for the
  maintainer is deliberate; the CODEOWNERS review is still required for everyone
  else.
* **No protected files touched** — both checks are green and silent.

## Check it locally before you push

```bash
# what did my branch change against its own base?
BASE=$(git merge-base HEAD origin/main)
git diff --name-only "$BASE"..HEAD

# would my branch revert protected work? (should print nothing)
git diff --name-only "$BASE"..origin/main -- \
  src/rules.* src/engine.* src/cards.* src/campaign.* src/storage.* src/ai.* \
  src/ui_screens.c src/ui_board_status.c src/board_layout.* \
  tools/wiki_tables.c tools/test_koikoi_trigger.c tools/make_card_images.py \
  assets/atlas.png assets/atlas_deus.png assets/back.png assets/deus.png assets/*.jpg \
  docs/ mkdocs.yml overrides/ .github/CODEOWNERS .github/workflows/
```

The guards themselves can be run locally too:

```bash
# same code path the workflows use (needs a git remote called origin)
PR_NUMBER=<your pr number> EVENT_NAME=pull_request_target \
  BASE_REPO=Aquaslopper/Xeno-koi HEAD_REPO=<your fork>/Xeno-koi \
  bash tools/guard_protected.sh
PR_NUMBER=<your pr number> EVENT_NAME=pull_request_target \
  bash tools/guard_stale.sh
```

`bash tools/test_guards.sh` simulates every guard scenario (owner, non-owner,
fork, stale, up to date) in a temporary repository and asserts the outcomes.

## Changing something protected

Protected does not mean "do not touch". It means: do it in one deliberate
change and expect a review.

1. Change the code and the docs together, as described in
   [`AGENTS.md`](https://github.com/Aquaslopper/Xeno-koi/blob/main/AGENTS.md).
2. Regenerate instead of hand-editing: `python3 tools/gen_wiki_tables.py` for the
   wiki tables, `python3 tools/make_card_images.py` for the card thumbnails.
3. Run `python3 tools/gen_wiki_tables.py --check`, `python3 tools/check_protected_zones.py`
   and `python3 -m mkdocs build --strict`.
4. Explain in the pull request which zone you touched and why.
