# ATRIUM PLEX — Story & Per-Card Protocol Roadmap (`roadmap_story.md`)

> **Purpose:** The production plan for the Atrium Plex story: what narrative surface each of the
> 48 spine cards gets, what the "special game" behind a card may legally be, and in what order to
> build it without breaking the table.
> **Companions:** [`story.md`](story.md) (lore notebook) · [`training_games.md`](training_games.md)
> (game archetypes) · [`roadmap_plexus.md`](roadmap_plexus.md) (Phase 5, which this file fills in).
> **Status:** Partly implemented (28 October 2026) — see §8. All 48 cards have SIGNAL text on the
> spine briefing, and 14 cards (all of Month 02, plus each of Months 03-12's FACE card) play a real
> one-round **story protocol** with a frozen keeper panel. Stages B (paged story screen) and D (art)
> are still plan only.

---

## 0. TL;DR

1. **You do not need a new text system for the first pass.** Four text surfaces already exist and
   are already wired to per-card state — the spine briefing panel, the frozen in-match panel, the
   CPU transmission box, and the Logfile story reader. Each has a measured character budget (§5).
2. **The story is already scaffolding.** The Logfile holds a Prologue plus 12 chapter slots with a
   read-mask, and the 12 Monos already exist as named entities in `src/cards.c` — one per month,
   each a `g_opponents[]` record with a lore line, two taunts, a title, and a favourite card that
   is *always* that month's slot-0 spine card. That is a cast; it just has no lines.
3. **Make one card = one "protocol"**: a **SIGNAL** (pre-match briefing, 3 lines), an
   **INTRUSION** (one frozen mid-match panel), and a **TRACE** (archive line). 48 of each is
   ~2,400 words of writing, not 48 essays.
4. **The per-card "special game" must be a script over an existing mode**, never a second board.
   `roadmap_plexus.md` rule 1 already forbids cloning the table. The generic form is a
   `ProtocolScript` table (forced deal, allowed cards, one goal, pass/fail text) run by one small
   runner — today every card above Month 01 bypasses gameplay entirely and marks itself cleared.
5. **Absence is part of the canon.** Month 01 and 03 have no Animal card, Month 08 has no Ribbon,
   Month 12 has neither. Use those gaps: the entity loses appetite, then voice, then everything.
6. **ASCII only.** All three faces are loaded with `LoadFontEx(..., NULL, 0)`, i.e. codepoints
   32–126 (the story face adds `×`). A curly quote or em dash draws as a placeholder box. This is
   the single most common way story text will look broken.
7. **Ship in four increments** (§8): A text-only beats → B paged story screen → C scripted
   protocol games → D art. A alone gives the game a story; C is the expensive one.
8. **Reuse `plexsave.txt`.** It is already in `web/shell.html`'s localStorage sync list, so new
   story masks persist on desktop and web with no shell change. A new save file would need one.

---

## 1. What already exists (audited, not assumed)

| Surface | Code | Geometry | Constraint |
|---|---|---|---|
| Spine briefing | `ui_plexus.c` → briefing block in `plexus_draw_ui_chrome()` | panel `{516, 82, 540, 250}`; text x = 538 | Header 21 px, subtitle 19 px, **exactly 3 body lines** at y+108/142/176. No wrapping, no auto-shrink. Months 02–12 currently print `[PLACEHOLDER]`. |
| In-match frozen panel | `plexus_tutorial_draw_panel()` + `tut_step_is_frozen()` | default `{280, 150, 720, 380}`, per-step overrides | Freezes the table through `plexus_tutorial_update_and_click()` returning 1; `CONTINUE` is `{pr.x+pr.width-186, pr.y+pr.height-56, 164, 40}`. Text is drawn line-by-line, no wrapping. |
| CPU transmission | `ui_taunt.c` → `taunt_trigger()` / `taunt_draw()` | box `{12, 260, 255, 95}`, text `{20, 290, 239, 61}` | 256-byte buffer, word-wraps, 14 px, **~3 visible lines and no clipping if you overflow**. Fed from `g_opponents[i].taunt_text` / `.koi_taunt`. |
| Logfile story reader | `draw_logfile_story_tab()` | doc `{456, 122, 518, 482}` | 6 fixed prose lines (`StoryChapterDoc.p1..p6`, 20.5 px story face) + 4 display-only card nodes at y+318, 115×74, x step 121. |
| One-shot intro | `draw_intro_story_screen()` | `CONTINUE` `{530, 624, 220, 44}` | Single hard-coded screen, 4 fades, `PLEXUS_INTRO_BTN_DELAY 2.65 s`, gated by `intro_seen`. No paging, no skip. |

Also already true, and useful:

* **Cast:** `g_opponents[0..11]` — one entity per month, named after that month's Face card
  (`01: JANUARY CRANE`, `02: FEB. WARBLER`, …, `12: DEC. PHOENIX`), plus `ENTITY 13 // DEUS SUPREMUS`.
  Each has `title`, `lore_text`, `favorite_combo`, `taunt_text`, `koi_taunt`. Nothing else in the
  game speaks.
* **Unlock/progress state:** `s_finished_cards_mask` (48-bit), `s_deus_finished_cards_mask`,
  `s_story_read_mask` (12-bit), `s_intro_seen` — all persisted in `profiles/plexsave.txt`.
* **Art/asset path:** `resolve_asset_path()`; `assets/` is preloaded whole into the WASM bundle
  (`--preload-file assets`), so **every new still is paid for by every web player** (§11).
* **Audio:** `sfx_flip/drop/capture/koi/win/lose/click` are the entire vocabulary. No narration
  bus, no voice files.

What does **not** exist yet, and is therefore on this roadmap:

* Any per-card story text (48 signals, 48 traces, 48 intrusions).
* Any way to *read* a per-card trace: the 4 card nodes in the Logfile are drawn, not clickable.
* Any paged/typewriter story presentation, any skip, any replay-from-archive of a sequence.
* Any gameplay for cards 4–47: `plexus_handle_click()` hard-codes Month 01's four slots and then
  falls through to `mark_active_spine_card_done(cid)` — the card completes on one click.
* A per-card mask (only the 12-bit month mask exists).

---

## 2. Design law (non-negotiable, derived from the code and `AGENTS.md`)

1. **One board.** Every card game runs on `SCREEN_GAME` with the real engine. No second table, no
   second input loop, no second scoring path (`roadmap_plexus.md`, rule 1).
2. **No scoring, rules, or card-database changes for story reasons.** A protocol may restrict what
   the player is *allowed* to play (`plexus_tutorial_allowed_hand_card()` already does) or what
   the deal contains (`plexus_tutorial_setup_round()` already does). It may not change values.
3. **Static text, static memory.** Story lives in `static const` C tables. No file parsing, no
   heap, no dynamic strings; buffer sizes come from the surface (256 bytes for the transmission).
4. **Printable ASCII only** in game strings. Escape `\"`; prefer plain `"` and `-`.
5. **A beat never blocks a returning player.** Every panel is skippable from the first frame; a
   seen beat can be re-read from the Logfile, never re-imposed.
6. **Small diffs.** A stage that exceeds ~80 lines is a plan, not a commit — say so first.

---

## 3. Canon (proposal — replace freely, but keep it one paragraph)

> The Atrium is the inside of a machine that was built to forget what it is. It cannot delete a
> hand it was dealt, so it files each card as a **protocol** and re-runs it, hoping the outcome
> changes. Twelve sectors, twelve keepers, four protocols each. You are the extra token: the
> prologue already says your observation is part of the sequence. Every protocol you clear
> restores one fragment of the machine's memory, and the fragments are not separate — they are
> one sentence, spoken in the wrong order, 48 times.

Voice registers, all already implied by shipped strings:

| Register | Sounds like | Use for |
|---|---|---|
| **THE VESSEL** | clinical, procedural, second person | SIGNAL lines (briefing), Logfile chapter prose |
| **KEEPER n** (`g_opponents[n]`) | declarative, hostile, SHOUTING | INTRUSION panels mid-match, transmissions |
| **DEUS** | the vessel quoting itself | Deus-spine variants, Month 12, `[DEUS MODE]` text |
| **YOU** | one line, only at Month 12 | the last trace |

Rule of thumb: **the keepers threaten, the vessel explains, DEUS repeats.** Never let a keeper
explain the rules — that is the vessel's job, and mixing them is what makes game lore mushy.

### 3.1 The role formula (this is what makes 48 cards writable)

Every month's deck has the same shape, so each card gets a fixed narrative *job* by type, not by
invented backstory:

| Role | Which card | What the beat is |
|---|---|---|
| **FACE** | slot 0 (`card_id = month*4 + 0`) — always the keeper's `favorite_card_id` | The keeper itself. Its name, its demand, its one secret. |
| **VOICE** | the Ribbon / Poetry card | A line of the keeper's poem. Poetry cards are literally poems on the table — use that. |
| **APPETITE** | a second Animal, when the month has one (08, 11) | What the keeper eats, collects, or envies. |
| **BODY** | each Plain (Kasu) | Wear, decay, inventory, maintenance. The unglamorous beats that make the rest feel real. |

And the **gaps are plot**: Month 01 and 03 have no Animal (no appetite), Month 08 has no Ribbon
and Month 12 has neither (a keeper with no voice, and at the apex no body left either). The
sentence being assembled gets quieter as it goes up — that is the ending's setup, and it costs
zero extra cards.

### 3.2 The two passes

`deus_finished_cards_mask` already lets the whole 48 clear a second time. Do **not** write 48
more beats for it. Write **12 alternate chapter codas** (one per month) shown only on the Deus
pass, and reuse the same card beats. Cheap, and it reads as a palimpsest: same memory, different
handwriting.

---

## 4. The narrative unit: SIGNAL → INTRUSION → TRACE

Per card, three pieces of text, in the order the player meets them:

| Piece | Surface | Length budget | When |
|---|---|---|---|
| **SIGNAL** | spine briefing (`line1..line3`) | 3 lines, ≤ 58 chars each | Selecting the card on the spine |
| **INTRUSION** | one frozen panel, `tut_step`-style | ≤ 8 lines, ≤ 76 chars each | Once, triggered by a table event (first capture, first yaku, first miss) |
| **TRACE** | Logfile card node → detail | ≤ 6 lines, ≤ 84 chars each | After the card clears; re-readable forever |

An INTRUSION is *one* panel, not five. Month 01's five-panel walkthrough is a tutorial; a
protocol's beat is a single freeze that says one thing and gets out of the way. If a card needs
more than one panel of story, it needs a better line, not more panels.

---

## 5. Text budgets (measured, not guessed)

All three faces are rasterised once and drawn as a bitmap resize (`LEARN_THE_CODE.md` §7.2:
`font_title` base 48, `font_body` base 26, story face base 32). Widths below are measured the way
raylib measures — `round(advance * base / (hhea ascent - descent)) * size / base`, plus letter
spacing — against the shipped TTFs.

| Surface | Face | Size / spacing | Available | Safe line budget | Hard ceiling |
|---|---|---:|---:|---:|---:|
| Briefing header | Audiowide | 21 / 1.4 | 496 px | **36 chars** | ~41 |
| Briefing subtitle | Play | 19 / 1.2 | 496 px | **50 chars** | ~56 |
| Briefing body line | Play | 18 / 1.2 | 496 px | **56 chars** | ~62 |
| Panel header | Audiowide | 20 / 1.3 | 668 px | **50 chars** | ~54 |
| Panel body line | Play | 18 / 1.2 | 668 px | **74 chars** | ~81 |
| Transmission line | Play | 14 / 2.0 | 239 px | **26 chars** | ~28 |
| Logfile prose line | Saira Cond. | 20.5 / 0.9 | 482 px | **82 chars** | ~88 |
| Logfile card node name | Play | 14 / 1.0 | 99 px | **13 chars** | ~15 |

Two traps worth stating plainly:

* **Nothing clips.** The briefing and panel paths draw one string per `DrawTextEx` call with no
  scissor, so an over-long line simply runs out of the panel and over the background.
* **`MeasureTextEx` is exact only if the same font object measures and draws.** It is here — the
  never-finished font-LOD experiment in `LEARN_THE_CODE.md` §7.2 exists precisely because mixing
  two rasterisations breaks centring.

Use the checker before pasting lines into `src/` (run from the repo root; keep it in `/tmp`
— `tests/` is locked):

```python
#!/usr/bin/env python3
# /tmp/plexus_textcheck.py   stdin: "<surface>\t<text>"
import sys
from fontTools.ttLib import TTFont
FACES = {"title": ("assets/audiowide.ttf", 48),
         "body":  ("assets/play.ttf", 26),
         "story": ("assets/saira_condensed.ttf", 32)}
SURFACES = {"brief_hdr": ("title", 21.0, 1.4, 496.0), "brief_sub": ("body", 19.0, 1.2, 496.0),
            "brief_body": ("body", 18.0, 1.2, 496.0), "panel_hdr": ("title", 20.0, 1.3, 668.0),
            "panel_body": ("body", 18.0, 1.2, 668.0), "taunt": ("body", 14.0, 2.0, 239.0),
            "doc_line": ("story", 20.5, 0.9, 482.0), "node_name": ("body", 14.0, 1.0, 99.0)}
def measure(face, size, spacing, text):
    path, base = FACES[face]
    f = TTFont(path); upm = f["head"].unitsPerEm
    asc, desc = f["hhea"].ascent, -f["hhea"].descent
    scale = base / (asc + desc)
    cmap, hmtx = f.getBestCmap(), f["hmtx"]
    if any(ord(c) < 32 or ord(c) > 126 for c in text):
        raise SystemExit("NON-ASCII in %r" % text)
    adv = sum(round(hmtx[cmap.get(c, ".notdef")][0] * scale) for c in text)
    return adv * (size / base) + spacing * (len(text) - 1)
bad = 0
for raw in sys.stdin:
    raw = raw.rstrip("\n")
    if not raw.strip() or raw.lstrip().startswith("#"):
        continue
    surface, text = raw.split("\t", 1)
    face, size, spacing, avail = SURFACES[surface]
    px = measure(face, size, spacing, text)
    bad += px > avail
    print("%s %-10s %6.1f/%4.0fpx %3d ch  %s" % ("OVER" if px > avail else "ok  ",
          surface, px, avail, len(text), text))
print("\n%d line(s) over budget" % bad)
sys.exit(1 if bad else 0)
```

Example:

```
$ printf 'brief_body\tThe Warbler weaves your name into its nest.\n' | python3 /tmp/plexus_textcheck.py
ok   brief_body   360.6/ 496px  43 ch  The Warbler weaves your name into its nest.
```

---

## 6. The 48-card beat matrix

The shape is fixed (§3.1); the twelve theses are the writing. Each row is a chapter *thesis* — one
sentence that every beat in that month must serve.

| # | Keeper (`cards.c`) | Thesis: the memory this sector holds |
|:--:|---|---|
| 01 | Obsidian Sentinel (Crane) | It was taught to see first and feel later; it never learned the second half. |
| 02 | Avian Construct (Warbler) | It collects ribbons because it cannot remember who wore them. |
| 03 | Sensory Overload Unit (Curtain) | Blossom, neon, overload — it chose brightness over sight. |
| 04 | Parasitic Vine-Mind (Cuckoo) | It keeps what it cannot use, and calls the hoard love. |
| 05 | Structural Intelligence (Bridge) | It is a way across; it has never been anywhere. |
| 06 | Hive-Mind Vector (Swarm) | Many bodies, one appetite, and none of them is allowed to stop. |
| 07 | Heavy-Armor Unit (Boar) | It was built to take the hit so something else did not have to. |
| 08 | Orbital Observer (Moon) | It watches the table it is not allowed to sit at. It has no voice. |
| 09 | Chemical Processing Unit (Sake) | It is the thing that makes the other keepers speak. |
| 10 | Sensory-Stealth Unit (Deer) | It survives by being unseen, and is starving for being seen once. |
| 11 | Atmospheric Anomaly (Rainman) | Rain and lightning are its body; it is the only keeper that leaks. |
| 12 | The Apex Biomech (Phoenix) | It has done this forty-seven times. It does not know that yet. |

Worked example — **Month 02** (copy this shape for the other 44):

| Card | Role | SIGNAL (briefing, 3 lines) |
|---|---|---|
| `04` 02-A Warbler | FACE | `The Warbler has already built this match.` / `Its nest is lined with your previous hands.` / `It wants to know if you still play the same.` |
| `05` 02-RP Poetry | VOICE | `It keeps a poem you have not finished.` / `Capture the slip and it will read you` / `the half you were still deciding.` |
| `06` 02-P1 Plain | BODY | `Nothing in this sector is new.` / `These are the cards it took from you` / `and never bothered to hide.` |
| `07` 02-P2 Plain | BODY | `Count what is left of your hand.` / `The Warbler counts too, and it` / `counts faster.` |

INTRUSION for `04` (fires on the player's first capture): keeper voice, ≤ 6 lines, e.g.
`THIS NEST WAS BUILT FROM A MATCH YOU DO NOT REMEMBER.` / `I HAVE BEEN PLAYING YOU SINCE BEFORE YOU ARRIVED.`
TRACE for `04` (Logfile, 3 short lines): the memory, not the threat — `The nest was not lined with ribbons.`
/ `It was lined with the hands it had been dealt.` / `One of them was still moving.`

---

## 7. Data model & persistence

One static table, one mask pair, no new save file.

```c
/* ui_plexus.c, beside k_story_chapters[] */
typedef struct {
    const char *signal[3];    /* spine briefing body lines            */
    const char *intrusion[6]; /* frozen mid-match panel, keeper voice */
    const char *trace[6];     /* Logfile card detail, vessel voice    */
} StoryBeat;
static const StoryBeat k_story_beats[48]; /* index = card_id = month*4 + slot */
```

* **Masks:** add `s_story_signal_mask` (48-bit) and `s_story_trace_mask` (48-bit). The existing
  `s_story_read_mask` stays 12-bit and keeps meaning "month chapter opened"; do not repurpose it.
* **Persist:** two more `fprintf` lines in `plexus_save()` and two `%llu` parses in `plexus_load()`
  (`finished_cards_mask=` already shows the pattern). Keep them optional on load so old saves stay
  valid — an absent key means `0`.
* **Web:** nothing to do. `plexsave.txt` is already in `web/shell.html`'s sync list (lines ~297
  and ~302–305 for profile paths). If a stage ever needs a *new* file, it must be added there in
  all four forms, or web saves silently drop it.
* **Deus variants:** store only the 12 codas, keyed by month, drawn when
  `is_deus_spine_active()`.
* **Buffer discipline:** all strings are `static const char *`; any runtime composition
  (`snprintf`) stays inside the existing 88–112-byte stack buffers.

---

## 8. Delivery, in four increments

### Stage A — Text beats on existing surfaces (no new UI) · **done 28 Oct 2026**
* [x] `k_story_beats[48]` in `src/ui_plexus.c`; Months 02–12 read `signal[0..2]` into the three
  briefing body lines, Month 01 keeps its tutorial title/description with `signal[0]` beneath it.
* [x] `trace[0]` is written into the MATCHES logfile entry when a protocol card clears.
* [ ] Clickable Logfile card nodes: the four nodes in `draw_logfile_story_tab()` are still drawn,
  not clickable, so the full TRACE is not yet readable in-game. Nodes are at
  `{tx+idx*121, doc.y+318, 115, 74}`; a `[BACK]` control would replace the chapter prose.
* **Gate:** all §5 budgets were measured against the shipped TTFs (widest line 456/496 briefing,
  517/668 panel, 378/482 trace).

### Stage B — A real story screen (paged, skippable) · ~120 lines (needs the "over 80" conversation)
* Generalise `draw_intro_story_screen()` into a small pager: an array of line blocks, a reveal
  timer, `CONTINUE`/`SKIP`, and a "remember as read" flag. Reuse the same fade/blur passes.
* Use it for: the Prologue (replacing the hard-coded screen), Month-12 close, and any protocol
  whose TRACE deserves more than the reader box.
* **Gate:** A + verify the intro can still be re-read from the Logfile and that `intro_seen`
  persistence is unchanged.

### Stage C — Scripted protocol games · **first slice done 28 Oct 2026; the rest is still plan**
* [x] Done without a new module: the generic protocol rides `src/ui_plexus.c`'s tutorial machine via
  `PROTO_WAIT_DEAL` / `PROTO_PANEL_STORY` / `PROTO_PLAY`, discriminated by `s_tut_card_id >= 4`.
  A card with `intrusion[0]` deals one real round on the player's ruleset and freezes once for the
  keeper panel. 14 cards qualify; `story_beat_has_protocol()` is the single switch.
  `./build.sh test`, the table-UI suite and the guard all pass; the live flow is unplayed (no
  display in this environment).
* [ ] Goals and hulls: today a protocol is "play one round, no goal, no hull choice". The sketch
  below is what turns the remaining cards into real challenges:

```c
typedef struct {
    int month, slot;              /* which card this protocol is                    */
    int hull;                     /* HULL_MATCH | HULL_SPEEDCOIN | HULL_ACTION_CARD */
    int rounds, difficulty, timer;
    unsigned int deal_seed;       /* deterministic deal, like plexus_tutorial_setup_round */
    int forced_hand;              /* card_id the player must be able to play, or -1 */
    int goal;                     /* GOAL_CAPTURE_MONTH | GOAL_YAKU | GOAL_CLEAR_ROUND ... */
    int goal_param;
    int intrusion_trigger;        /* INTRUSION_FIRST_CAPTURE | FIRST_YAKU | FIRST_MISS    */
    int time_limit_sec;
} ProtocolScript;
```
* `plexus_handle_click()` replaces the `mark_active_spine_card_done(cid)` fall-through for
  months 02–12 with a table lookup: no script → keep the beta instant-clear; script → launch the
  hull.
* **Hulls are not new games.** Normal match (custom rounds/ruleset/difficulty), Speedcoin, Action
  Card, and Training already exist as isolated modes with their own setup/teardown
  (`plexus_sync_match_settings()` / `plexus_restore_match_settings()`). A protocol is a hull plus a
  goal plus text.
* **Gate:** every hull still exits through the same `SCREEN_ATRIUM` CRT-wipe path; `plexsave.txt`
  round-trips; no scoring file touched; `tests/test_table_ui.sh` still passes.

### Stage D — Art (last, cheapest to cut)
* 12 monthly stills (one per chapter), 1280×720, delivered as `.jpg` like the existing backgrounds,
  drawn behind the Stage-B pager; `assets/` is fully preloaded into the web bundle, so budget for
  it (a 1280×720 JPEG at ~150 KB is 1.8 MB for twelve).
* No per-card art. 48 stills is not a solo-dev budget, and the text carries the card fine.
* Draw them with `resolve_asset_path()` and keep a `FileExists()` guard so a missing still degrades
  to the text screen — never to a crash.

---

## 9. Fixing the "special game per card" question

The honest answer to *"can each card have its own special game?"* is: **yes, but as scripts, not as
48 bespoke games.** The legal design space is:

| Card role | Legit game shape | Reuses |
|---|---|---|
| FACE | A full hull match with one twist: a rule the keeper insists on ("no Koi-Koi", "must reach 2 yaku") | Normal match + one goal flag |
| VOICE | Read-the-board puzzle: capture the Poetry card of the month within N turns | Normal match + `forced_hand` + `goal` |
| APPETITE | Speed run: capture as many Animals as possible before the clock | Speedcoin hull |
| BODY | Endurance / Recall: clear plains under the speedbar, or match from memory (flip-down field) | Speedcoin / Action Card hulls |

Constraints that keep it from becoming 48 games:

1. **90-second rule.** If a protocol cannot be explained in one panel and finished in ~90 s, it is a
   mode, not a card. Modes belong in the bottom bar, not on the spine.
2. **One goal per card.** `goal` + `goal_param`; no compound win conditions.
3. **Deterministic deal.** A fixed `deal_seed` per script means a beat can be tuned, screenshotted,
   and re-verified — and it is the only way to guarantee the tutorial's "you can always play this
   card" promise.
4. **Fail = retry, not loss.** No campaign stats, coins, or replays are touched (the plexus modes
   already isolate this).
5. **The story never gates progress.** Text is always skippable; only the *goal* gates the flip.

---

## 10. Writer's workflow

1. Draft in [`story.md`](story.md) §4 scratchpad — no code, no budgets, just the twelve theses and
   the lines that insist on existing.
2. Move one card at a time into a TSV and run the checker (§5) until it reports `0 line(s) over budget`.
3. Paste into `k_story_beats[]`. Escape `"`. Confirm ASCII (`grep -nP '[^\x00-\x7F]' src/ui_plexus.c`
   should stay empty).
4. Play it: `./build.sh linux` (needs raylib in `/tmp`), or the WASM build for the web path.
5. One sentence of intent per card in `story.md`, so a later edit cannot silently contradict it.

---

## 11. Traps, in the order they will bite

| Trap | Why | Guard |
|---|---|---|
| Non-ASCII punctuation | `LoadFontEx(..., NULL, 0)` loads 32–126 only | checker rejects; keep `"` and `-` |
| Text overflow | Briefing/panel paths have no wrapping or scissor | §5 budgets + checker |
| Transmission overflow | `ui_taunt.c` wraps but does not clip; 4th line leaves the box | ≤ 3 lines / ~78 chars |
| Save-format drift | `plexsave.txt` is parsed key-by-key | new keys optional, absent = 0 |
| Web save loss | `shell.html` syncs an explicit file list | reuse `plexsave.txt` |
| Bundle bloat | whole `assets/` is preloaded for web | 12 stills max, JPEG, guarded load |
| Deus duplication | 48 more beats is 2× the writing for 1× the value | 12 codas (§3.2) |
| Test lock | `tests/` and `tools/tests_guard.*` are locked | scratch checks in `/tmp` only |
| Docs CI | `docs/` is generated + `mkdocs --strict`; `plexus/` is outside `docs_dir` | editing `plexus/` cannot break `docs.yml` |
| Story as a second ruleset | keepers must not explain rules | voice table §3 |

---

## 12. Decisions needed from the maintainer

1. **Canon:** does the "vessel / keeper / extra token" premise in §3 replace the sketch in
   `story.md`, or sit beside it?
2. **Voice:** should keepers shout in caps (consistent with existing taunts) or speak normally, with
   caps reserved for DEUS?
3. **Scope of Stage C:** 12 FACE protocols (one real game per month) or all 48? 12 is a month of
   work; 48 is a year.
4. **Deus pass:** 12 codas as proposed, or a full second pass over all 48 traces?
5. **Art:** are 12 monthly stills affordable (bundle size), or text-only for now?

---

## Appendix — the 48 cards, with role

`card_id = month*4 + slot`; slot 0 is the top of the month's spine after `[NEXT]` and is also the
keeper's `favorite_card_id`.

| id | card | type | role |
|---:|---|---|---|
| 0 | 01-B Crane | Bright 20 | FACE — keeper 01 |
| 1 | 01-RP Poetry | Ribbon 5 | VOICE |
| 2 | 01-P1 Plain | Plain 1 | BODY |
| 3 | 01-P2 Plain | Plain 1 | BODY |
| 4 | 02-A Warbler | Animal 10 | FACE — keeper 02 |
| 5 | 02-RP Poetry | Ribbon 5 | VOICE |
| 6 | 02-P1 Plain | Plain 1 | BODY |
| 7 | 02-P2 Plain | Plain 1 | BODY |
| 8 | 03-B Curtain | Bright 20 | FACE — keeper 03 |
| 9 | 03-RP Poetry | Ribbon 5 | VOICE |
| 10 | 03-P1 Plain | Plain 1 | BODY |
| 11 | 03-P2 Plain | Plain 1 | BODY |
| 12 | 04-A Cuckoo | Animal 10 | FACE — keeper 04 |
| 13 | 04-R Ribbon | Ribbon 5 | VOICE |
| 14 | 04-P1 Plain | Plain 1 | BODY |
| 15 | 04-P2 Plain | Plain 1 | BODY |
| 16 | 05-A Bridge | Animal 10 | FACE — keeper 05 |
| 17 | 05-R Ribbon | Ribbon 5 | VOICE |
| 18 | 05-P1 Plain | Plain 1 | BODY |
| 19 | 05-P2 Plain | Plain 1 | BODY |
| 20 | 06-A Butterflies | Animal 10 | FACE — keeper 06 |
| 21 | 06-RB Blue | Ribbon 5 | VOICE |
| 22 | 06-P1 Plain | Plain 1 | BODY |
| 23 | 06-P2 Plain | Plain 1 | BODY |
| 24 | 07-A Boar | Animal 10 | FACE — keeper 07 |
| 25 | 07-R Ribbon | Ribbon 5 | VOICE |
| 26 | 07-P1 Plain | Plain 1 | BODY |
| 27 | 07-P2 Plain | Plain 1 | BODY |
| 28 | 08-B Moon | Bright 20 | FACE — keeper 08 (no VOICE this month) |
| 29 | 08-A Geese | Animal 10 | APPETITE |
| 30 | 08-P1 Plain | Plain 1 | BODY |
| 31 | 08-P2 Plain | Plain 1 | BODY |
| 32 | 09-A Sake Cup | Animal 10 (cup) | FACE — keeper 09 |
| 33 | 09-RB Blue | Ribbon 5 | VOICE |
| 34 | 09-P1 Plain | Plain 1 | BODY |
| 35 | 09-P2 Plain | Plain 1 | BODY |
| 36 | 10-A Deer | Animal 10 | FACE — keeper 10 |
| 37 | 10-RB Blue | Ribbon 5 | VOICE |
| 38 | 10-P1 Plain | Plain 1 | BODY |
| 39 | 10-P2 Plain | Plain 1 | BODY |
| 40 | 11-B Rainman | Bright 20 | FACE — keeper 11 |
| 41 | 11-A Swallow | Animal 10 | APPETITE |
| 42 | 11-R Ribbon | Ribbon 5 | VOICE |
| 43 | 11-P Lightning | Plain 1 | BODY |
| 44 | 12-B Phoenix | Bright 20 | FACE — keeper 12 |
| 45 | 12-P1 Plain | Plain 1 | BODY |
| 46 | 12-P2 Plain | Plain 1 | BODY |
| 47 | 12-P3 Plain | Plain 1 | BODY |

Note on `09-A Sake Cup`: under `RULESET_KK_ADD_10` the cup counts as a plain (`ui_screens.c`
`draw_yaku_cards()` documents this). Any body/voice framing that depends on it being an Animal must
say so in the beat, or use the KK-ADD-10 framing deliberately.
