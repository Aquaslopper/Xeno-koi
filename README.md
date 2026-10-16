<img src="docs/assets/readme/hero.jpg" alt="[XENO / KOI] Biomechanical Edition — the game's own start screen: teal CRT lettering under a dark arch, over Giger-style biomechanical art of a face merged with the architecture." width="1280" />

<a href="https://aquaslopper.github.io/Xeno-koi/play/"><img src="docs/assets/readme/btn-play.png" alt="Play in the browser" width="300" /></a>&nbsp;
<a href="https://aquaslopper.github.io/Xeno-koi/how-to-play/"><img src="docs/assets/readme/btn-rulebook.png" alt="Read the rulebook" width="300" /></a>&nbsp;
<a href="https://github.com/Aquaslopper/Xeno-koi/releases/latest"><img src="docs/assets/readme/btn-release.png" alt="Windows build" width="300" /></a>

**The browser build is the whole game** — the same C, compiled to WebAssembly. No install, no account, no asset download.

---

## The game in four lines

* **48 cards, twelve months of four.** You capture by **month**, never by picture: any January card matches any January card on the table.
* **Build a set, then choose.** A captured set is a **yaku**. Bank it (**Shobu**) and take the points, or call **Koi-Koi** and keep playing for a bigger award.
* **Pushing cuts both ways.** Every Koi-Koi you call raises your own multiplier; if the opponent calls it and then beats you, their call doubles what *they* take.
* **Five rule sets, twelve opponents, then [DEUS SUPREMUS](https://aquaslopper.github.io/Xeno-koi/reference/deus-supremus/).** He is not an opponent so much as a wall, and he has his own page.

Everything above is explained with real card pictures in the rulebook: **[How to Play](https://aquaslopper.github.io/Xeno-koi/how-to-play/)**.

## Screenshots

<img width="1920" height="1080" src="https://github.com/user-attachments/assets/c48a1bdd-9f0c-4530-ba30-5332710876fd" alt="A match in progress: the player's hand along the bottom, the field in the middle, the stock to the left, and both capture lanes down the left edge, all inside a teal CRT frame." width="1920" />

<img width="1920" height="1080" src="https://github.com/user-attachments/assets/bb6ee598-940e-46f9-ab39-6cf9941d9260" alt="The same table with a card mid-drag, its matching month glowing on the field." width="1920" />

## Where to read what

| You want to | Page |
|---|---|
| Learn the game in one sitting, with card pictures | [How to Play](https://aquaslopper.github.io/Xeno-koi/how-to-play/) |
| Pick a rule set before a match | [Rulesets](https://aquaslopper.github.io/Xeno-koi/rulesets/) |
| Know what a combination is worth | [Yaku & Scoring](https://aquaslopper.github.io/Xeno-koi/scoring/yaku/) |
| Know why a score multiplied the way it did | [Multipliers & Awards](https://aquaslopper.github.io/Xeno-koi/scoring/multipliers/) |
| Look up one of the 48 cards | [Cards](https://aquaslopper.github.io/Xeno-koi/reference/cards/) |
| Know what the CPU is thinking, per difficulty | [Difficulty](https://aquaslopper.github.io/Xeno-koi/reference/difficulty/) |
| Study his atlas, tile by tile | [DEUS SUPREMUS](https://aquaslopper.github.io/Xeno-koi/reference/deus-supremus/) |
| Know what is broken or stale | [Errata](https://aquaslopper.github.io/Xeno-koi/errata/) |

## Overview

**Xeno Koi** is a digital adaptation of the traditional Japanese card game Koi-Koi, wrapped in a dark, HR Giger-inspired biomechanical aesthetic and a 1979 CRT terminal UI.

Designed for both casual play and hardcore competitive strategy, the engine features multiple rule variations, smooth physics-based card interactions, and an atmospheric audiovisual presentation.

## Core Gameplay Features

* **Oya-gime (Dealer Selection):** Interactive pre-match phase where players draw a card to determine the starting dealer based on month order.
* **Cinematic Game Feel:** Heavy pasteboard card physics featuring Y-axis hopping, dynamic drop shadows, and 150ms "Hitlag" (freeze-frame) on card captures to emphasize physical weight.
* **Continuous Audio Flow:** Safe-loading audio engine with crossfaded state transitions and continuous match-music shuffling.
* **Persistent Statistics:** Tracks win rates, total matches, highest round scores, and the player's most frequent/favorite Yaku (combo), displayed via an arcade-style scrolling marquee on the main menu.
* **Koi-Koi Streak Multiplier:** Pushing your luck now pays both ways. Every Koi-Koi call in a round raises the **caller's own** multiplier by +1 (`x1` -> `x2` -> `x3` ... capped at `x6`), stacking on top of the `x2` 7+ Points multiplier and the `x2` Opponent Koi-Koi multiplier. A player who calls Koi-Koi three times and then banks a 6-point hand scores `6 x 4 = 24`.

## Supported Rulesets

The game supports 5 distinct rule variations, selectable in the Options menu to cater to different playstyles:

| Rule set | What it changes |
|---|---|
| **[Nintendo](https://aquaslopper.github.io/Xeno-koi/rulesets/nintendo/)** | The classic, balanced Koi-Koi rule set. One Koi-Koi call per player per round. |
| **[FudaWiki](https://aquaslopper.github.io/Xeno-koi/rulesets/fudawiki/)** | Modern standard rules with slightly adjusted combo values: bigger Goko and Sanko, unlimited Koi-Koi calls. |
| **[KK-ADD-10](https://aquaslopper.github.io/Xeno-koi/rulesets/kk-add-10/)** | A high-scoring variant where fixed sets and counted cards stack additively, and the Sake Cup counts as a plain. The house rule, and a good place for beginners to start. |
| **[Tournament](https://aquaslopper.github.io/Xeno-koi/rulesets/tournament/)** | Disables "Hanami-zake" (Curtain + Cup) and "Tsukimi-zake" (Moon + Cup) Viewings. The Sake Cup is treated strictly as an animal card, eliminating cheap luck-based wins and forcing strategic combo building. |
| **[Yakuza](https://aquaslopper.github.io/Xeno-koi/rulesets/yakuza/)** | Tournament scoring plus "Dealer's Privilege" (Oya-ken). If a round ends in exhaustion with a 0-0 draw, the current Dealer automatically wins the round and is awarded 6 points. |

## Technical Architecture

* **Language:** C99
* **Framework:** Raylib 5.5
* **Graphics:**
  * **GLSL Depth-Fog Shader:** A custom fragment shader that calculates background luminance to map procedural, drifting sine-wave fog exclusively to the dark, distant valleys of the 2D background art.
  * **Native Particle Engine:** Fixed-size memory pools handling ambient phosphor ash and high-velocity capture bursts.
* **Development Methodology:** The core state machine and rendering pipelines were architected using strict, contract-based AI Prompt Engineering (Arena.ai), demonstrating advanced LLM context-retention through isolated logical handoffs.

## Install and run

1. Download the latest `.zip` from the **[Releases](https://github.com/Aquaslopper/Xeno-koi/releases/latest)** tab.
2. Extract the folder to your computer. *(Ensure the `assets/` folder remains in the same directory as the executable).*
3. **Optional Custom Audio:** Add your own `.mp3` and `.wav` files into the `assets/music/` and `assets/sfx/` folders matching the required filenames.
4. Run `KoiKoi.exe`.

## Documentation

The wiki is the reference for the rules, and it is part of the repo: it lives in [`docs/`](docs/) and is published at
**[aquaslopper.github.io/Xeno-koi](https://aquaslopper.github.io/Xeno-koi/)**.
Its scoring tables and its card images are **generated from the C source** by `tools/gen_wiki_tables.py` and
`tools/make_card_images.py`, so a rule change and its documentation cannot drift apart. When you change scoring or
cards, run `python3 tools/gen_wiki_tables.py` and commit the result; CI checks it.

The masthead band and the buttons at the top of this file are generated the same way, from the shipped art, by
`tools/make_readme_art.py`.

## Building from Source

For developers wishing to compile the game from source:
```bash
# Clone the repository
git clone https://github.com/Aquaslopper/Xeno-koi.git
cd xeno-koi

# Compile using MinGW (Windows)
x86_64-w64-mingw32-gcc -std=c99 -O2 src/*.c -I/path/to/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm -o XenoKoi.exe
```
