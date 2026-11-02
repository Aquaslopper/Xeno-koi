#ifndef UI_TABLE_GLOW_H
#define UI_TABLE_GLOW_H
/* Keep audio-only callers independent of Raylib graphics declarations.
   Raylib defines Texture2D as an alias of struct Texture. */
struct Texture;

void table_glow_draw(struct Texture overlay, float dt, int table_design);
void table_glow_unload(void);
/* Read-only music stream tap; audio owns attachment after the music DSP. */
void table_glow_music_tap(void *buffer, unsigned int frames);

#endif
