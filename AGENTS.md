Learn the code before you code — what you have not read and verified is only an assumption, and a change built on an assumption risks breaking code it never understood. Your definitive architectural map is LEARN_THE_CODE.md. Read the architectural map and answer the 6 questions, verify how the code actually behaves.

Agent & contributor notes for Xeno Koi
Read this before changing rules, scoring, cards, opponents, or the docs.

[SYSTEM DIRECTIVE: HYPER-FOCUS MODE ENGAGED]
You are acting as a precision systems programmer on the Xeno-Koi C99 / Raylib game engine. Read and strictly enforce all operational guardrails below before writing any code.

STRICT CONSTRAINTS:
- DO NOT assume how any system works. Consult LEARN_THE_CODE.md for exact phase machines, audio DSP paths, and UI state triggers before writing a line.
- DO NOT refactor or rewrite entire files.
- DO NOT modify any card databases, matching rules, or existing scoring logic.
- DO NOT introduce new libraries or overhaul the state architecture.
- Keep all changes minimal, surgical.
- NEVER create, edit, or delete files in tests/ (or touch tools/tests_guard.*) without explicit prior approval from the maintainer. Run any scratch checks in /tmp and delete them before finishing.
- NEVER create, overwrite, or delete anything under assets/ without explicit prior approval from the maintainer. Art there is authored, not generated: a file that exists is the maintainer's, even when a tool could produce something similar. Automation may write into assets/ only after the maintainer has seen the exact list of files and said yes. A generator run on a copy in /tmp is the way to show a proposal without touching the tree.
- OUTPUT REQUIREMENT: WRITE SILENTLY TO DISK. ZERO CODE OUTPUT. NO PREAMBLE. SHORT SUMMARY OF WHAT YOU CHANGED IN CHAT.

STRICT OUTPUT RULE WHEN CODING:
Output format (fill 1:1):

=========================================

re-read AGENTS.md
Confirm to follow SYSTEM DIRECTIVE
Confirmed architecture via LEARN_THE_CODE.md

What the request is, in the agent's words
Awaiting approval. Nothing written to disk.

SHORT SUMMARY OF WHAT YOU CHANGED IN CHAT.
Touched: files
Not touched: files / areas
Checks run: checks
=========================================

Where the truth lives
- The Architecture Map: LEARN_THE_CODE.md is the strict, zero-fluff guide to the current C/raylib architecture. It holds the absolute truth for phase states, AI heuristic logic, UI boundaries, and memory constraints. 
- The Code: src/ decides what the game actually does.
- The Feature Documents: one file per system at the repo root. Dealership.md owns the turn rhythm, UI_GLOW.md owns the light over the table casing overlay. Each says which files hold the code, what the system may never touch, and its tuning surface. A new system gets its own document in the same commit it is built, and the description lives there - not in a code comment, not in STATUS.md. A new feature does not grow an existing source file to fit: it gets its own module and its own document, and it is never threaded into unrelated systems to make it work.
- The Forge (card-table skins): tables/ is its own world with its own law. Any agent building or porting a card-table skin starts at tables/AGENT_START.md — read order, the forge→beta-port→live-test loop, and the hard rules (anchors from src/ only, uniform scaling, one owner-provided transparent card bezel, verify on the produced file). WORKFLOW.md in that folder is the canon; nothing in tables/ touches src/ until a beta port.
- The Wiki: docs/ (published from mkdocs.yml) explains the current rules. It is the reference for players and contributors.
- The Legacy Contract: HANDOFF_KoiKoi_Giger_v2.pdf is legacy. It is the original build spec from 11 Oct 2026. Do not implement or document rules from it. Use LEARN_THE_CODE.md instead. Where the handoff disagrees with the code or the wiki, it is out of date (see docs/errata.md).
- The Status: STATUS.md is the current development handoff, not a rulebook or deployment record. Update its date, completed work, checks actually run, and remaining verification when handing off a meaningful change. Its explicitly marked historical section is not current status.

If a rule in LEARN_THE_CODE.md and the wiki disagree, follow the wiki and the code, and say so in your summary.

Owner decisions (do not reverse without asking the maintainer)
Deus Supremus is open to everyone on purpose. DEUS_UNLOCKED_FOR_TESTING in src/campaign.c is 1 while the maintainer tests the mode. Leave it alone. The intended end state is that Deus unlocks only once the story has been completed; do not add that gate on your own initiative, and do not treat the current open access as a bug or a TODO.
KK-ADD-10 is the maintainer's own house rule, taken from an earlier game. It is the recommended first rule set for beginners. It is expected to change, so keep docs/rulesets/kk-add-10.md and the generated tables in step with src/rules.c.
The handoff stays in the repo unchanged for history. Do not edit or delete the PDF.
tests/ is locked (tools/tests_guard.sh + tools/tests_guard.manifest). Do not add new test files or update tools/tests_guard.manifest unless the maintainer explicitly asks for a new test.
THE ACTION CARD MODE IS FROZEN. No agent may change any Action Card code, for any reason, without the maintainer asking for that specific change in that specific session. src/ui_action.c and src/ui_action.h are hash-locked by tools/action_guard.sh (baseline in tools/action_guard.sha256); build.sh test runs it, CODEOWNERS requires owner review, and .github/workflows/guard-protected.yml fails any non-owner PR that touches them. tools/action_guard.manifest additionally pins 11 influence points outside those files - CPU think durations and event beats in src/ui_main.c, the KK-ADD-10 ruleset lock, the meter input and freeze gates and the 85% peak clamp in src/ui_board.c, the difficulty source in src/ui_plexus.c, the hint-suppression gate in src/ui_input.c. The mode's ruleset is fixed: it is KK-ADD-10 only, pinned in begin_new_match() in src/ui_main.c both on the engine_init() call and by overwriting settings.ruleset, so it cannot inherit a campaign ruleset, the Deus -> Yakuza override or an Options change. Do not make it configurable. Anything that changes how the mode plays must be REPORTED, never fixed: state which lines differ and what the difference does to the mode, then stop. Do not run action_guard.sh --record, do not edit the guard, the manifest or the recorded hash, and do not describe the mode's behaviour as a bug, a leftover, an oddity or an improvement. Twice on 29 October an agent wired the music tap and a CPU-pacing delay into this mode unasked; one of those was reverted by the maintainer in df4f6d7 ("revert unrequested turn-pacing delays breaking reverb/meter timing"). Run ./tools/action_guard.sh --report to see the influence surface. The Dealership (Dealership.md) covers this mode's turn rhythm by owner decision of 29 October 2026 ("action card has to be included"): the token gates its badge events and its turns like every other mode. That coverage is NOT permission to tune the mode - if its pace reads wrong under the token, report it.
Glyph motes are a hover-only button affordance. ui_theme_draw_text_flares() has exactly three call sites, each behind a button hover test (hub_draw_button, draw_btn, draw_plexus_btn). Do not add motes to headings, the title brand line, centred text or anything drawn on a non-hovered element, and do not re-add a music pulse or a per-size density/speed boost - all three were built, looked over the top in play and removed on the maintainer's instruction. There is one Audiowide face (font_title at base 48); a second 176 px face existed briefly and was removed because a 1024x1024 atlas (~4 MB VRAM + mipmaps) is too much to pay for one line of text.

Changing rules, cards, or opponents
Do all of these in the same change:
- Edit the code in src/ (Check LEARN_THE_CODE.md first).
- Regenerate the engine-derived tables: python3 tools/gen_wiki_tables.py
- Update the hand-written prose in docs/ that describes the changed rule. The generator only fills marked blocks. It does not rewrite the explanations.
- Add or update the entry in docs/errata.md if the change alters something the handoff described or a known issue.
- Check the docs: 
  python3 tools/gen_wiki_tables.py --check
  python3 -m mkdocs build --strict
CI runs the same checks (.github/workflows/docs.yml) and fails if the generated tables are stale.

Atrium Plex story beats and story protocols (src/ui_plexus.c)
Text lives in one table. k_story_beats[48], indexed by card_id = month * 4 + slot. Do not add a story file format, a parser, a loader or story strings scattered into draw functions; add a table row. A beat is three fields: signal[] is the spine briefing (3 short lines), intrusion[] is the keeper's voice on one frozen panel, trace[] is the vessel's voice and trace[0] doubles as the MATCHES logfile detail.
A card becomes a playable protocol only when intrusion[0] is non-NULL. story_beat_has_protocol() is the only test. Do not branch on card_id in plexus_handle_click() to make a card "special" -- give it intrusion text instead, and it becomes a strip of one round on the real table. Rewriting a card should always be a string edit, never a code edit.
Protocols reuse the Month 01 tutorial machine: PROTO_WAIT_DEAL -> PROTO_PANEL_STORY -> PROTO_PLAY, discriminated by s_tut_card_id >= 4. Do not add a story state machine, a second freeze flag, a second match launcher, a second "is active" getter or a parallel panel renderer. The tutorial already owns the frozen board, the input gate, the abort path, telemetry isolation and the clear/flip, which is exactly why src/ui_main.c needs no edit for story content (LEARN_THE_CODE.md 10.6). If a story change makes you edit src/ui_main.c, stop: you are probably rebuilding a gate that already exists.
Branch order matters. Month 01's branches are s_tut_card_id == 1 and == 2 and are checked before the generic >= 4 path. A new special card needs its own branch ABOVE that test, or the generic protocol path will swallow it.
Text rules. ASCII only: all three faces are loaded with LoadFontEx(..., NULL, 0), i.e. codepoints 32-126 (the story face adds the multiply sign). No curly quotes, no em dashes, no accented names, no box drawing. Nothing in the briefing or frozen-panel paths wraps or clips, so measure every line before pasting it; the per-surface character budgets and a checker live in plexus/roadmap_story.md section 5. Long prose belongs in the Logfile reader, not inside a draw call.
Isolation is intentional, not an oversight. Protocols are tutorial-active, so they skip replay recording, campaign stats, clearance coins and the Deus ruleset override, and they save/restore speedbar and audio telemetry around the match. Do not "fix" that by launching campaign matches: story must never touch stats, unlocks, scoring or the deck databases, and a story beat is never a reason to change rules.
One panel per card, on purpose. If a beat needs more than one frozen panel, the answer is a better line or a Logfile trace, not more states in the enum. The Month 01 five-panel walkthrough is a tutorial teaching a mode, not the template for story.
Not built, do not improvise. Per-card save keys, a clickable Logfile trace reader, protocol goals/hulls and story art are all unbuilt and planned in plexus/roadmap_story.md. Stage them there first, then extend k_story_beats -- do not extend the architecture.
Two saves, two records. The Pancratium writes profiles/stats.txt; the Plex writes profiles/plexsave.txt. Plex screens show Plex numbers only. GameStats is the Pancratium career record and must not be printed inside the Atrium -- ui_plexus.c carries no st-> access, and the public signatures keep the parameter marked (void)st (LEARN_THE_CODE.md 10.7). The single agreed exception is the training opponent list, which stays capped by campaign unlocks; do not add a second one without asking.

Building and checking
tools/wiki_tables.c links the real src/rules.c and src/cards.c. Do not copy scoring logic into the docs or the generator.
Card images in the wiki (docs/assets/cards/card-<id>.webp + back.webp, the DEUS variants, and the DEUS background/atlas) are cut from the shipped art by tools/make_card_images.py — never edit the art or the thumbnails by hand. Regenerate after any source-art or generator change: python3 tools/make_card_images.py (needs Pillow: pip install --break-system-packages pillow). This also writes tools/card_images.sha256; check source/output freshness with python3 tools/make_card_images.py --check. Afterwards rerun python3 tools/gen_wiki_tables.py, because the difficulty chart embeds card thumbnails.
README.md's masthead band and link buttons are generated from the shipped art by tools/make_readme_art.py, and the to-scale table map docs/assets/readme/table-map.png with tools/make_board_figure.py (both write into that folder; both take --check) -- never edit docs/assets/readme/* by hand. Regenerate with python3 tools/make_readme_art.py, check with --check.
The flow diagrams in docs/assets/figures/ (turn-flow.png on How to Play, award-pipeline.png on Multipliers & Awards) are drawn by tools/make_flow_figures.py from the stage order in src/engine.c and the gates in calculate_award() -- never edit the PNGs, and never put numbers in them, because totals belong in the generated tables. Regenerate with python3 tools/make_flow_figures.py, check with --check.
The game screenshots in docs/assets/screenshots/ are hand-captured (the only images here no tool can make) and imported with python3 tools/make_screenshots.py --from <dir>; their seven close-ups are cropped from them at the rectangles in src/, so never trim a PNG by hand. Re-derive with python3 tools/make_screenshots.py, check with --check.
Desktop builds: see build.sh. The linux, win, and wasm modes expect raylib in /tmp.
./build.sh test runs tests/test_engine.c (500 simulated matches plus deck and scoring checks). ./tests/test_table_ui.sh runs headless table input and combo-panel checks; it needs Raylib headers at RAYLIB_SRC (default /tmp/raylib/src), but not the Raylib library.
Do not commit build output: koikoi_linux, test_engine, dist/, site/, or compiled tool binaries. Build tools to /tmp.

Code changes
Keep a change to the smallest diff that solves the problem. If a fix needs more than ~80 lines, stop and describe the plan first.
Never bundle a refactor with a feature. Separate commits.
Do not add a parameter to a shared function to pass a mode flag without saying so in the summary.
Before finishing, list what you touched and what you deliberately did not touch.

Pages site
The wiki is deployed to GitHub Pages from main by .github/workflows/docs.yml. Any other workflow that deploys to the same Pages site replaces the whole site, so coordinate before adding one.

Pull requests
Open PRs against main. Include the rule change, the docs updated, and the checks you ran.
