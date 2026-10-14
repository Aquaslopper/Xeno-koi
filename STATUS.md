# XENO/KOI: BIOMECHANICAL EDITION - STATUS REPORT

## Status Summary

1. **Start Menu Order Updated**
   - Configured exact 6-button main menu structure:
     1. `START MATCH`
     2. `OPTION`
     3. `STATS`
     4. `SAVE`
     5. `CREDITS`
     6. `QUIT`
   - Spaced and centered cleanly with active navigation logic.

2. **Wiki Components Fully Removed**
   - Removed `src/ui_wiki.h` and `src/ui_wiki.c`.
   - Removed `SCREEN_WIKI` from `ScreenState` and `ACT_GOTO_WIKI` from `ScreenAction`.
   - Purged all Wiki-specific update, input, drawing, and audio routes from `src/ui_main.c`.

3. **CRT Analog Beam & Clean Scanlines**
   - All legacy software-loop scanlines removed from both board and screens.
   - Clean analog CRT beam shader active with selectable in-menu filter strength (`0: OFF`, `1: LOW`, `2: MID`, `3: HIGH`).

4. **Koi-Koi Streak Multiplier (Caller-Side)**
   - `calculate_award()` now takes both Koi-Koi tallies: the winner's own calls drive an escalating
     multiplier (`koi_caller_multiplier()`, +1 per call, capped at `KOI_CALLER_MULT_MAX = 6`), while the
     loser's calls still grant the flat `x2` Opponent Koi-Koi multiplier.
   - `RoundRecord` carries `mult_opp_koi` (was `mult_koi`), `mult_self_koi`, `self_koi_calls` and
     `opp_koi_calls`, so the ledger and round-result screen can break the award down per multiplier.
   - Decide overlay, round-result screen, side combo panels and the CPU banner all display the live streak.
   - AI decision-making now compares the banked award against the projected streak award instead of using
     a fixed "base < 7" threshold, so the CPU chases (and stops chasing) streaks correctly.

5. **Deliverables**
   - All C source files strictly under 500 lines.
   - 15,000 engine test matches passed (post-change: AI-vs-AI sweep over all 5 rulesets, award/multiplier fields verified against the engine, zero zone violations).
   - Windows 64-bit release binary compiled to `dist/KoiKoi.exe`.
   - Archive packaged at `/home/user/koikoi_project.zip`.
