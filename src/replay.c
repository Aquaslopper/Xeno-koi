#include "replay.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One event line per decision. The grammar is deliberately plain text so a
   replay file stays readable and shareable:
     OYA <pick>              dealer draw (0 = left card, 1 = right card)
     PLAY <p> <card> <target>
     PICK <p> <target>       hand-choice or draw-choice target (phase decides)
     CANCEL <p>              cancelled a hand choice
     KOI <p> / SHOBU <p>     Koi-Koi decision
     TIMEOUT <p>             the turn timer ran out and autoplayed
   The engine's own phase machine (deal, stock reveal, next round) is not
   recorded: it is deterministic, so playback calls it exactly like the live
   loop does. */

#define REPLAY_MAX_EVENTS 1024
#define REPLAY_EVENT_LEN  24
#define REPLAY_PACE       0.50f         /* seconds between two playback steps */

/* ---------------- recording ---------------- */

static int  rec_active;
static int  rec_count;
static int  rec_overflow;
static char rec_header[192];
static char rec_events[REPLAY_MAX_EVENTS][REPLAY_EVENT_LEN];
static char rec_blob[REPLAY_MAX_EVENTS * REPLAY_EVENT_LEN + 256];
static char rec_file[REPLAY_MAX_EVENTS * REPLAY_EVENT_LEN + 320];

void replay_rec_begin(uint32_t seed, uint32_t ai_rng, int ruleset, int rounds, int difficulty, int master_timer, int opponent) {
    snprintf(rec_header, sizeof(rec_header),
             "seed=%u\nai_rng=%u\nruleset=%d\nrounds=%d\ndifficulty=%d\nmaster_timer=%d\nopponent=%d\n",
             (unsigned)seed, (unsigned)ai_rng, ruleset, rounds, difficulty, master_timer, opponent);
    rec_active = 1;
    rec_count = 0;
    rec_overflow = 0;
}

int replay_rec_active(void) { return rec_active; }

static void rec_push(const char *line) {
    if (!rec_active || rec_overflow) return;
    if (rec_count >= REPLAY_MAX_EVENTS) { rec_overflow = 1; return; }
    snprintf(rec_events[rec_count], REPLAY_EVENT_LEN, "%s", line);
    rec_count++;
}

void replay_rec_play(int player, int card, int target) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "PLAY %d %d %d", player, card, target);
    rec_push(b);
}

void replay_rec_pick(int player, int target) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "PICK %d %d", player, target);
    rec_push(b);
}

void replay_rec_cancel(int player) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "CANCEL %d", player);
    rec_push(b);
}

void replay_rec_koi(int player, int koi_koi) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "%s %d", koi_koi ? "KOI" : "SHOBU", player);
    rec_push(b);
}

void replay_rec_timeout(int player) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "TIMEOUT %d", player);
    rec_push(b);
}

void replay_rec_oya(int pick) {
    char b[REPLAY_EVENT_LEN];
    if (!rec_active) return;
    snprintf(b, sizeof(b), "OYA %d", pick);
    rec_push(b);
}

void replay_rec_abort(void) {
    rec_active = 0;
    rec_count = 0;
    rec_overflow = 0;
}

const char *replay_rec_blob(void) {
    if (!rec_active || rec_overflow || rec_count == 0) return NULL;
    size_t pos = 0;
    int w = snprintf(rec_blob, sizeof(rec_blob), "%s", rec_header);
    if (w < 0 || (size_t)w >= sizeof(rec_blob)) return NULL;
    pos = (size_t)w;
    for (int i = 0; i < rec_count; i++) {
        w = snprintf(rec_blob + pos, sizeof(rec_blob) - pos, "%s\n", rec_events[i]);
        if (w < 0 || pos + (size_t)w >= sizeof(rec_blob)) return NULL;
        pos += (size_t)w;
    }
    return rec_blob;
}

char *replay_build_file(const char *blob, int auto_flag) {
    if (!blob) return NULL;
    snprintf(rec_file, sizeof(rec_file), "# xenokoi replay %d\nauto=%d\n%s",
             REPLAY_FORMAT, auto_flag ? 1 : 0, blob);
    return rec_file;
}

/* ---------------- playback ---------------- */

static char pb_events[REPLAY_MAX_EVENTS][REPLAY_EVENT_LEN];
static int  pb_count;
static int  pb_i;
static uint32_t pb_seed;
static int  pb_ruleset;
static int  pb_rounds;
static int  pb_difficulty;
static int  pb_active;
static int  pb_status;
static float pb_delay;
static int  pb_hold;        /* a flight is in the air: apply nothing until released */
static int  pb_shown;       /* the step at pb_i has already been handed to the caller */
static const char *pb_error = "";

static int pb_fail(const char *why) {
    pb_error = why;
    pb_status = -1;
    pb_active = 0;
    return -1;
}

static int ev_ok(const char *e) {
    int p = 0, a = 0, b = 0, end = 0;
    if (sscanf(e, "OYA %d%n", &p, &end) == 1 && e[end] == '\0') return (p == 0 || p == 1);
    end = 0;
    if (sscanf(e, "PLAY %d %d %d%n", &p, &a, &b, &end) == 3 && e[end] == '\0')
        return (p == 0 || p == 1) && a >= 0 && a < 48 && b >= -1 && b < 48;
    end = 0;
    if (sscanf(e, "PICK %d %d%n", &p, &a, &end) == 2 && e[end] == '\0')
        return (p == 0 || p == 1) && a >= 0 && a < 48;
    end = 0;
    if (sscanf(e, "CANCEL %d%n", &p, &end) == 1 && e[end] == '\0') return (p == 0 || p == 1);
    end = 0;
    if (sscanf(e, "KOI %d%n", &p, &end) == 1 && e[end] == '\0') return (p == 0 || p == 1);
    end = 0;
    if (sscanf(e, "SHOBU %d%n", &p, &end) == 1 && e[end] == '\0') return (p == 0 || p == 1);
    end = 0;
    if (sscanf(e, "TIMEOUT %d%n", &p, &end) == 1 && e[end] == '\0') return (p == 0 || p == 1);
    return 0;
}

static int parse_int(const char *text, int *out) {
    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX) return 0;
    *out = (int)value;
    return 1;
}

static int parse_u32(const char *text, uint32_t *out) {
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value > UINT32_MAX) return 0;
    *out = (uint32_t)value;
    return 1;
}

int replay_pb_load(const char *text) {
    pb_count = 0; pb_i = 0; pb_active = 0; pb_status = 0; pb_error = ""; pb_hold = 0; pb_shown = 0;
    if (!text) { pb_error = "REPLAY FILE IS EMPTY"; return 0; }

    int version_ok = 0, have_seed = 0, have_ai_rng = 0, have_master_timer = 0;
    int have_difficulty = 0, have_opponent = 0, ruleset = -1, rounds = 0, difficulty = -1;
    int master_timer = 0, opponent = -1, bad_line = 0;
    uint32_t seed = 0, ai_rng = 0;
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[REPLAY_EVENT_LEN];
        if (len >= sizeof(line)) { bad_line = 1; len = sizeof(line) - 1; }
        memcpy(line, p, len);
        line[len] = '\0';
        p = nl ? nl + 1 : p + strlen(p);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = '\0';
        if (len == 0) continue;
        if (line[0] == '#') {
            int v = 0;
            if (sscanf(line, "# xenokoi replay %d", &v) == 1) version_ok = (v == REPLAY_FORMAT);
            continue;
        }
        if (!strncmp(line, "auto=", 5)) continue;
        if (!strncmp(line, "seed=", 5)) { if (!parse_u32(line + 5, &seed)) bad_line = 1; else have_seed = 1; continue; }
        if (!strncmp(line, "ai_rng=", 7)) { if (!parse_u32(line + 7, &ai_rng)) bad_line = 1; else have_ai_rng = 1; continue; }
        if (!strncmp(line, "ruleset=", 8)) { if (!parse_int(line + 8, &ruleset)) bad_line = 1; continue; }
        if (!strncmp(line, "rounds=", 7)) { if (!parse_int(line + 7, &rounds)) bad_line = 1; continue; }
        if (!strncmp(line, "difficulty=", 11)) { if (!parse_int(line + 11, &difficulty)) bad_line = 1; else have_difficulty = 1; continue; }
        if (!strncmp(line, "master_timer=", 13)) { if (!parse_int(line + 13, &master_timer)) bad_line = 1; else have_master_timer = 1; continue; }
        if (!strncmp(line, "opponent=", 9)) { if (!parse_int(line + 9, &opponent)) bad_line = 1; else have_opponent = 1; continue; }
        if (pb_count < REPLAY_MAX_EVENTS && ev_ok(line)) {
            snprintf(pb_events[pb_count], REPLAY_EVENT_LEN, "%s", line);
            pb_count++;
        } else bad_line = 1;
    }

    if (!version_ok) { pb_error = "REPLAY VERSION NOT SUPPORTED"; return 0; }
    if (bad_line) { pb_error = "REPLAY FILE IS CORRUPT"; return 0; }
    if (!have_seed || !have_ai_rng || !have_difficulty || !have_master_timer || !have_opponent ||
        ruleset < 0 || ruleset > RULESET_YAKUZA || rounds < 1 || rounds > 12 ||
        difficulty < 0 || difficulty > 3 || (master_timer != 15 && master_timer != 30) ||
        opponent < 0 || opponent >= 12) {
        pb_error = "REPLAY HEADER IS INCOMPLETE";
        return 0;
    }
    (void)ai_rng; /* recorded as part of the settings snapshot; decisions are replayed verbatim */
    if (pb_count < 1 || strncmp(pb_events[0], "OYA ", 4)) { pb_error = "REPLAY HAS NO DEALER DRAW"; return 0; }

    pb_seed = seed;
    pb_ruleset = ruleset;
    pb_rounds = rounds;
    pb_difficulty = difficulty;
    return 1;
}

void replay_pb_begin(GameState *g) {
    int pick = 0;
    sscanf(pb_events[0], "OYA %d", &pick);
    if (pick != 0 && pick != 1) pick = 0;

    engine_init(g, (Ruleset)pb_ruleset, pb_rounds, pb_seed);
    /* The dealer draw is the one live input that happens before the deal: the
       recorded pick decides which drawn card is the human's. */
    g->oya_player_choice = pick;
    g->oya_winner = engine_oya_winner(g->oya_cards[pick], g->oya_cards[1 - pick]);
    g->dealer = g->oya_winner;
    engine_start_round(g);

    pb_i = 1;
    pb_delay = REPLAY_PACE;
    pb_active = 1;
    pb_status = 0;
    pb_hold = 0;
    pb_shown = 0;
}

PbWant replay_pb_want(const GameState *g, int *player, int *card, int *target) {
    if (player) *player = g ? g->current_player : -1;
    if (card)   *card = -1;
    if (target) *target = -1;
    if (!pb_active || pb_hold || pb_shown || !g) return PB_WANT_NONE;

    PbWant w = PB_WANT_NONE;

    if (g->phase == PHASE_TURN_DRAW) {
        /* engine_reveal_stock() is the deterministic step the live game shows as
           a flip out of the stock pile. */
        int top = (g->stock_count > 0) ? g->stock[g->stock_count - 1] : -1;
        if (top >= 0) {
            if (card) *card = top;
            w = PB_WANT_DRAW;
        }
    } else if ((g->phase == PHASE_TURN_HAND || g->phase == PHASE_HAND_CHOICE) && pb_i < pb_count) {
        /* Same test the tick applies below: anything else is a divergence and
           must stay the tick's job to report. */
        int p = 0, a = 0, b = 0;
        if (sscanf(pb_events[pb_i], "PLAY %d %d %d", &p, &a, &b) == 3 && p == g->current_player) {
            if (player) *player = p;
            if (card)   *card = a;
            if (target) *target = b;
            w = PB_WANT_PLAY;
        }
    }

    /* Handed over once and only once: the tick clears the flag when it applies
       the step, so a flight can never be shown twice for the same event. */
    if (w != PB_WANT_NONE) pb_shown = 1;
    return w;
}

void replay_pb_hold(void) { pb_hold = 1; }

/* The flight has landed, so the step has to be taken on this tick: whatever is
   left of the pacing pause is dropped. Waiting it out would leave the card
   sitting in the hand it has already flown out of, and the board would pull it
   back before the engine moved it. */
void replay_pb_release(void) { pb_hold = 0; pb_delay = 0.0f; }

int replay_pb_tick(GameState *g, float dt) {
    if (!pb_active) return pb_status;
    if (pb_hold) return 0;            /* a card is still in the air */
    if (pb_delay > 0.0f) {
        pb_delay -= dt;
        if (pb_delay > 0.0f) return 0;
    }

    if (g->phase == PHASE_MATCH_END) { pb_active = 0; pb_status = 1; return 1; }
    if (g->phase == PHASE_ROUND_END) { engine_next_round(g); pb_shown = 0; pb_delay = REPLAY_PACE; return 0; }
    if (g->phase == PHASE_TURN_DRAW)  { engine_reveal_stock(g); pb_shown = 0; pb_delay = REPLAY_PACE; return 0; }
    if (pb_i >= pb_count) return pb_fail("REPLAY ENDED WITHOUT A RESULT");

    const char *ev = pb_events[pb_i];
    int p = 0, a = 0, b = 0;

    if (sscanf(ev, "PLAY %d %d %d", &p, &a, &b) == 3) {
        if ((g->phase != PHASE_TURN_HAND && g->phase != PHASE_HAND_CHOICE) || p != g->current_player)
            return pb_fail("REPLAY DIVERGED (PLAY)");
        engine_play_hand(g, a, b);
    } else if (sscanf(ev, "PICK %d %d", &p, &a) == 2) {
        if (p != g->current_player) return pb_fail("REPLAY DIVERGED (PICK)");
        if (g->phase == PHASE_HAND_CHOICE) engine_pick_hand_choice(g, a);
        else if (g->phase == PHASE_DRAW_CHOICE) engine_pick_draw_choice(g, a);
        else return pb_fail("REPLAY DIVERGED (PICK)");
    } else if (sscanf(ev, "CANCEL %d", &p) == 1) {
        if (g->phase != PHASE_HAND_CHOICE || p != g->current_player) return pb_fail("REPLAY DIVERGED (CANCEL)");
        engine_cancel_hand_choice(g);
    } else if (sscanf(ev, "KOI %d", &p) == 1 || sscanf(ev, "SHOBU %d", &p) == 1) {
        if (g->phase != PHASE_DECIDE || p != g->current_player) return pb_fail("REPLAY DIVERGED (DECISION)");
        engine_decide(g, (ev[0] == 'S') ? DECISION_SHOBU : DECISION_KOI_KOI);
    } else if (sscanf(ev, "TIMEOUT %d", &p) == 1) {
        engine_timeout_autoplay(g);
    } else {
        return pb_fail("REPLAY DIVERGED (EVENT)");
    }

    pb_i++;
    pb_shown = 0;       /* the step is spent: the next one may be shown */
    pb_delay = REPLAY_PACE;
    return 0;
}

int replay_pb_active(void) { return pb_active; }
int replay_pb_difficulty(void) { return pb_difficulty; }

void replay_pb_stop(void) {
    if (pb_active) pb_status = 0;
    pb_active = 0;
    pb_hold = 0;
    pb_shown = 0;
}

const char *replay_pb_error(void) { return pb_error; }
