#ifndef REPLAY_H
#define REPLAY_H

#include <stdint.h>
#include "engine.h"
#include "storage.h"

/* Replays are faithful re-simulations: a replay file holds the match seed, the
   settings snapshot and every decision both players made. Playing it back feeds
   the same calls into the same engine, so the deals, the captures and the scores
   come out exactly as they did live. The engine stays the only owner of rules -
   this module only writes decisions down and repeats them.
   Storage, listing and the stats-screen widgets live in ui_replay.c. */

/* Bump when the replay file grammar or engine behavior becomes incompatible. */
#define REPLAY_FORMAT 1

/* ---- recording a live match (no-op while recording is inactive) ---- */
void replay_rec_begin(uint32_t seed, uint32_t ai_rng, int ruleset, int rounds, int difficulty, int master_timer, int opponent);
int  replay_rec_active(void);
void replay_rec_play(int player, int card, int target);
void replay_rec_pick(int player, int target);
void replay_rec_cancel(int player);
void replay_rec_koi(int player, int koi_koi);   /* 0 = SHOBU */
void replay_rec_timeout(int player);
void replay_rec_oya(int pick);
void replay_rec_abort(void);                    /* restart / abandon / Deus loss: drop it */

/* The recorded body: "seed=...\n...\nOYA 1\nPLAY 0 12 -1\n", or NULL when there
   is nothing to save. */
const char *replay_rec_blob(void);

/* Full file text: "# xenokoi replay 1\nauto=<auto_flag>\n" + body. */
char *replay_build_file(const char *blob, int auto_flag);

/* ---- playback ---- */
int  replay_pb_load(const char *text);          /* 1 = ok, 0 = unsupported or corrupt */
void replay_pb_begin(GameState *g);
int  replay_pb_tick(GameState *g, float dt);    /* 0 running, 1 finished, -1 diverged */
int  replay_pb_active(void);
int  replay_pb_difficulty(void);
void replay_pb_stop(void);
const char *replay_pb_error(void);

#endif /* REPLAY_H */
