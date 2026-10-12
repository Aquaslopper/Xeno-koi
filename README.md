# XENO KOI

**A Biomechanical, Atmospheric Hanafuda (Koi-Koi) Engine.**
Built in pure C99 and Raylib, featuring custom GLSL shaders, procedural particle effects, and a rigorous state-machine architecture.

<img width="2112" height="1144" alt="giger set" src="https://github.com/user-attachments/assets/94ff2b69-2f7e-44d1-95e3-5021e011378f" />(giger set.png) 

<img width="1920" height="1080" alt="koi" src="https://github.com/user-attachments/assets/008ea59c-fbd8-4279-a56f-713f5c41383e" />(koi.png)

## Overview
**Xeno Koi** is a digital adaptation of the traditional Japanese card game Koi-Koi, wrapped in a dark, HR Giger-inspired biomechanical aesthetic and a 1979 CRT terminal UI. 

Designed for both casual play and hardcore competitive strategy, the engine features multiple rule variations, smooth physics-based card interactions, and an atmospheric audiovisual presentation.

## Core Gameplay Features
* **Oya-gime (Dealer Selection):** Interactive pre-match phase where players draw a card to determine the starting dealer based on month order.
* **Cinematic Game Feel:** Heavy pasteboard card physics featuring Y-axis hopping, dynamic drop shadows, and 150ms "Hitlag" (freeze-frame) on card captures to emphasize physical weight.
* **Continuous Audio Flow:** Safe-loading audio engine with crossfaded state transitions and continuous match-music shuffling.
* **Persistent Statistics:** Tracks win rates, total matches, highest round scores, and the player's most frequent/favorite Yaku (combo), displayed via an arcade-style scrolling marquee on the main menu.

## Supported Rulesets
The game supports 5 distinct rule variations, selectable in the Options menu to cater to different playstyles:
* **Nintendo (Standard):** The classic, balanced Koi-Koi rule set.
* **FudaWiki:** Modern standard rules with slightly adjusted combo values.
* **KK-ADD-10:** A high-scoring variant where fixed sets and counted cards stack additively.
* **Tournament (Hardcore):** Disables "Hanami-zake" (Curtain + Cup) and "Tsukimi-zake" (Moon + Cup) Viewings. The Sake Cup is treated strictly as an animal card, eliminating cheap luck-based wins and forcing strategic combo building.
* **Yakuza (Oya-ken):** Enables "Dealer's Privilege." If a round ends in exhaustion with a 0-0 draw, the current Dealer automatically wins the round and is awarded 6 points.

## Technical Architecture
* **Language:** C99
* **Framework:** Raylib 5.5
* **Graphics:** 
  * **GLSL Depth-Fog Shader:** A custom fragment shader that calculates background luminance to map procedural, drifting sine-wave fog exclusively to the dark, distant valleys of the 2D background art.
  * **Native Particle Engine:** Fixed-size memory pools handling ambient phosphor ash and high-velocity capture bursts.
* **Development Methodology:** The core state machine and rendering pipelines were architected using strict, contract-based AI Prompt Engineering (Arena.ai), demonstrating advanced LLM context-retention through isolated logical handoffs.

## How to Play
1. Download the latest `.zip` from the **[Releases]** tab.
2. Extract the folder to your computer. *(Ensure the `assets/` folder remains in the same directory as the executable).*
3. **Optional Custom Audio:** Add your own `.mp3` and `.wav` files into the `assets/music/` and `assets/sfx/` folders matching the required filenames.
4. Run `XenoKoi.exe`.

## Building from Source
For developers wishing to compile the game from source:
```bash
# Clone the repository
git clone https://github.com/YourUsername/xeno-koi.git
cd xeno-koi

# Compile using MinGW (Windows)
x86_64-w64-mingw32-gcc -std=c99 -O2 src/*.c -I/path/to/raylib/src -lraylib -lopengl32 -lgdi32 -lwinmm -o XenoKoi.exe
