#ifndef UI_REPLAY_H
#define UI_REPLAY_H

#include "raylib.h"
#include "engine.h"
#include "storage.h"

/* Stats-screen replay list: the ten log lines, the three buttons under them
   ([START REPLAY] [SAVE REPLAY] [LOAD REPLAY]) and the two small windows
   (save-as name field, load list).  Records live in replays/ next to the exe:
   the ten most recent finished matches plus whatever the player saved by name. */

void ui_replay_draw_rows(const MatchHistory *h, Vector2 vm);  /* the log lines, clickable */
void ui_replay_draw_controls(Vector2 vm);                     /* buttons, status line, windows */
int  ui_replay_click(const MatchHistory *h, Vector2 vm);      /* 1 = the click was ours */
void ui_replay_keys(const MatchHistory *h);                   /* typing in the save field */
int  ui_replay_take_start(GameState *g);                      /* 1 = a replay was started */
void ui_replay_commit_auto(const MatchHistory *h);            /* save + rotate after a match */
void ui_replay_set_status(const char *msg);                   /* short message on the stats screen */

#endif /* UI_REPLAY_H */
