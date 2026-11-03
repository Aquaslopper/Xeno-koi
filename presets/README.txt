XENO/KOI - TABLE LIGHT PRESETS
=============================

These files tune the 12 little spotlights that roam across a forged card table.
They live in this presets/ folder, right next to the game executable, so you can
edit them with any text editor and swap them with other players. Drop another
.txt file in here and it appears in the MUSIC panel preset row.

IMPORTANT: the spotlights only exist on the forged tables (City, Castle, Hole,
Arctic, Harvest, Face, Realm). The default CRT table has no overlay and shows
no dots. Pick one of those tables in STAGE // MAP SELECT to see your preset.

FORMAT
------
One setting per line:   key = value
# starts a comment. Blank lines and unknown keys are ignored. Every value is
clamped to the range printed in default.txt, so a typo cannot break the shader.
A file without a "name =" line is not a preset (this README is skipped).

Every file starts as a copy of DEFAULT and only overrides the keys it lists, so
you can write a short file with just the three or four things you want changed.

HOW TO USE
----------
1. Copy default.txt, rename it (for example hardcore.txt), edit the numbers.
2. Start the game and open MUSIC (Pancratium: the tab under STAGE; Atrium Plex:
   the MUSIC button in the bottom bar).
3. Step the PRESETS row with < and > to your file. The change is immediate and
   applies to the next match you start.
4. Edited a file while the game is running? Click the preset name to reload the
   folder - no restart needed.

The choice is global and lasts for the session, like the chosen music track. It
is deliberately not saved into settings.txt, so every launch starts on DEFAULT.

WHAT THE PRESETS ARE FOR
------------------------
techno.txt  - fast, flashy, hard reaction to four-on-the-floor kicks: tiny
              attack, short release, high contrast, quick roaming dots.
jazz.txt    - slow bloom that rises with a voice, gentle warm-to-cool colour
              drift, fewer and wider dots, long release.
default.txt - the untouched shipped look, with every knob explained.

The web build has only a couple of bundled tracks and no folder next to an exe,
so it keeps these three presets compiled in and cannot read custom .txt files.

Scope: these files change ONLY src/ui_table_glow.c. They do not touch the
background fog beam, the Action Card meter, the audio DSP, or any rule, card or
score. The dots have their own read-only peak meter on the match music stream;
the meter never modifies what you hear.
