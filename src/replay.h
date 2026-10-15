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

/* ---- showing the step before it is applied ----
   A replay is a list of engine calls, so on its own every card would simply
   appear in its new place. To show the flight a live match shows, the caller
   asks what the next step is before it happens, holds the engine, plays the
   flight and then releases it:
     want  = replay_pb_want(&g, &player, &card, &target);
     ... start the flight ...            replay_pb_hold();
     ... the flight landed ...           replay_pb_release();
   Nothing is applied while the engine is held, and the step is only ever
   applied once, so the playback cannot drift or double up. */
typedef enum {
    PB_WANT_NONE = 0,   /* no flight needed - apply the next step now */
    PB_WANT_PLAY,       /* a hand card is about to be played onto the table */
    PB_WANT_DRAW        /* the top stock card is about to be revealed */
} PbWant;

/* What the next step needs, or PB_WANT_NONE when the engine is free to run.
   Safe to call every frame: a step is handed over once only, and never while
   another one is still in the air. */
PbWant replay_pb_want(const GameState *g, int *player, int *card, int *target);

/* Freeze / unfreeze the engine side while a flight is in the air. */
void replay_pb_hold(void);
void replay_pb_release(void);

#endif /* REPLAY_H */
