#ifndef UI_FOLDER_PICKER_H
#define UI_FOLDER_PICKER_H

#include <stddef.h>

/* Desktop-only native "choose a folder" window, so a player can point the game
   at any folder on the PC instead of having to copy music under assets/music.
   Returns 1 and writes a UTF-8 path into out on success, 0 when the dialog is
   cancelled, -1 where no native dialog exists (web / non-Windows).
   Deliberately no Raylib types in here: windows.h must not clash with Raylib. */
int ui_pick_music_folder(char *out, size_t cap);

#endif /* UI_FOLDER_PICKER_H */
