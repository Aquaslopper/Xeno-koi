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

4. **Deliverables**
   - All C source files strictly under 500 lines.
   - 15,000 engine test matches passed.
   - Windows 64-bit release binary compiled to `dist/KoiKoi.exe`.
   - Archive packaged at `/home/user/koikoi_project.zip`.
