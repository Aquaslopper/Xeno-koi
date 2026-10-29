# Examples — inspiration, never evidence

This folder holds **examples**: pictures of what the table could feel like. They are not the game,
not a spec, and not a measurement. Nothing in this folder may be used to decide where something goes.

| file | what it is |
|---|---|
| `city-example-01.jpg` | softer restyle of the city table |
| `city-example-02.jpg` | stronger restyle, same layout |

Both are stylised mockups. The owner made them by feeding an example through an image-to-image
generator to see how a metal UI could *connect* to the city backdrop's art style. They came back with
"this is not the direction to go, this is the way to see how the UI can best connect to the art style
of the background image" — i.e. **read them for the idea, not for the implementation.**

## The four words, kept apart

| word | what it is | what I may infer from it |
|---|---|---|
| **example** / **mockup** | a picture someone made to show a direction | a *feeling*: metal instead of wireframe, capsules instead of rectangles, connected instead of floating. **No numbers. No positions. No claim about the build.** |
| **reference** | a real-world photo/artwork for texture and language | shapes and palette only, and the palette is sampled from it, not eyeballed |
| **the game** | what `src/` does, today, at these anchors | everything measured. `anchors.json`, verified by `./tables/build.sh` |
| **the design** | what we choose to build, expressed as numbers | a table spec in this folder, with its anchors, and its own checklist run |

An example is **upstream of the design**, not a substitute for it. The design may end up far from
the example; that is not a failure, the example was a question.

## The distinction, in one line

> An example tells you what to aim at. It never tells you where anything is, and it never tells you
> what the code currently does.

## Paid-for trap, do not repeat

This was got wrong twice by treating an example as if it were a statement about the current build —
first as a layout to copy, then as "this is what the game looks like today, so the render must be
wrong". Both times the honest answer was simply: *it is an example.*

- It is **not** a bug report that the game does not look like the example.
- It is **not** a measurement that the panels are 12 px here and 14 px there.
- It is **not** a promise that the example's every ornament will be implemented.

**If a question needs a fact, the answer lives in `src/` or in `tables/anchors.json`, never here.**
Recorded as trap 15 in `../WORKFLOW.md`.
