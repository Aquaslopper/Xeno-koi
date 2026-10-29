#include "ui_dealership.h"

#include "raylib.h"
#include <math.h>
#include <string.h>

extern Font font_body;

/* One leg between neighbouring stations. Two legs (the deck is always between
   the players) is 100 ms - the same budget as the inter-step pause this replaces,
   so the rhythm becomes visible without making the game slower. */
#define DL_LEG_SECONDS 0.050f
#define DL_EPSILON     0.0015f

/* Track positions in the 1280x720 logical space, taken from the HUD:
   right end of the CPU score bar, just left of the stock, right end of the
   player score bar - the vertical run down the left margin in the mock-up. */
static const Vector2 k_anchor[3] = {
    { 279.0f, 693.0f },   /* DL_PLAYER: player score line, beside [DEALER] */
    { 286.0f, 324.0f },   /* DL_DECK:   left of the face-down stock at 330,262 */
    { 279.0f,  27.0f }    /* DL_CPU:    CPU score line */
};

#define DL_TRAIL_MAX  28        /* ghosts kept; a 50 ms leg only gives ~3 samples */
#define DL_TRAIL_LIFE 0.28f     /* how long a ghost lives, seconds */
static Vector2 s_trail_pos[DL_TRAIL_MAX];
static float   s_trail_age[DL_TRAIL_MAX];
static int     s_trail_head;
static int     s_trail_count;

static float s_pos;          /* 0 = player, 1 = deck, 2 = cpu */
static int   s_target;
static int   s_travelling;
static int   s_active;
static int   s_station = DL_NONE;   /* station of the last update */
static float s_glow;                /* 0 idle .. 1 travelling; eases in and out */

static Vector2 station_point(float pos) {
    float clamped = (pos < 0.0f) ? 0.0f : (pos > 2.0f) ? 2.0f : pos;
    int i = (int)clamped;
    if (i > 1) i = 1;
    float t = clamped - (float)i;
    return (Vector2){
        k_anchor[i].x + (k_anchor[i + 1].x - k_anchor[i].x) * t,
        k_anchor[i].y + (k_anchor[i + 1].y - k_anchor[i].y) * t
    };
}

void dealership_reset(int station) {
    s_pos        = (float)station;
    s_target     = station;
    s_travelling = 0;
    s_trail_head = 0;
    s_trail_count = 0;
    s_glow = 0.0f;
    memset(s_trail_age, 0, sizeof(s_trail_age));
}

void dealership_set_active(int active) {
    s_active = active ? 1 : 0;
    if (!s_active) { s_travelling = 0; s_trail_count = 0; s_glow = 0.0f; }
}

int dealership_travelling(void) { return s_travelling; }

int dealership_unlocked(void) {
    if (!s_active) return 1;
    /* Phases with no station (deal, lucky check, round end) gate nothing, even if
       the token was caught mid-flight when the phase changed underneath it. */
    if (s_station == DL_NONE) return 1;
    if (s_travelling) return 0;
    return (fabsf(s_pos - (float)s_target) < DL_EPSILON) ? 1 : 0;
}

static void trail_push(Vector2 p) {
    s_trail_pos[s_trail_head] = p;
    s_trail_age[s_trail_head] = 0.0f;
    s_trail_head = (s_trail_head + 1) % DL_TRAIL_MAX;
    if (s_trail_count < DL_TRAIL_MAX) s_trail_count++;
}

void dealership_update(float dt, int station, int settled) {
    /* Age the trail every frame, travelling or not. */
    for (int i = 0; i < s_trail_count; i++) s_trail_age[i] += dt;

    /* The movement glow: lights up on departure, eases out after arrival so the
       token does not snap back to a dead marker at the station. */
    {
        float target = s_travelling ? 1.0f : 0.0f;
        float rate = s_travelling ? 18.0f : 7.0f;
        float k = rate * dt; if (k > 1.0f) k = 1.0f;
        s_glow += (target - s_glow) * k;
        if (s_glow < 0.0f) s_glow = 0.0f;
        if (s_glow > 1.0f) s_glow = 1.0f;
    }

    if (!s_active) return;
    s_station = station;
    if (station == DL_NONE) { s_travelling = 0; return; }

    if (station != s_target) { s_target = station; s_travelling = 1; }
    if (!s_travelling) return;

    /* Never leave the current station until its event is completely finished. */
    if (!settled) return;

    float dir = (s_target > s_pos) ? 1.0f : -1.0f;
    s_pos += dir * (dt / DL_LEG_SECONDS);

    if ((dir > 0.0f && s_pos >= (float)s_target) ||
        (dir < 0.0f && s_pos <= (float)s_target)) {
        s_pos = (float)s_target;
        s_travelling = 0;
    }
    trail_push(station_point(s_pos));
    /* A 50 ms leg is ~3 frames at 60 Hz; two samples per frame keep the trail
       continuous instead of three beads per leg. */
    trail_push(station_point(s_pos - dir * 0.35f));
}

void dealership_draw(void) {
    if (!s_active) return;

    float g = s_glow;                       /* 0 idle .. 1 travelling */
    float pulse = 1.0f + 0.35f * g;         /* the whole marker swells while moving */

    /* Trail: white ghosts, oldest first, shrinking and dimming. Bright enough to
       read a 50 ms leg as a streak rather than a jump. */
    for (int n = 0; n < s_trail_count; n++) {
        int idx = (s_trail_head - 1 - n + DL_TRAIL_MAX * 2) % DL_TRAIL_MAX;
        float life = 1.0f - s_trail_age[idx] / DL_TRAIL_LIFE;
        if (life <= 0.0f) continue;
        float fade = life * life;
        float r = (2.2f + 5.0f * life) * pulse;
        unsigned char a = (unsigned char)(150.0f * fade);
        DrawCircleV(s_trail_pos[idx], r, (Color){ 255, 255, 255, a });
        /* a soft halo on the freshest ghosts so the head of the trail bleeds */
        if (n < 8) DrawCircleV(s_trail_pos[idx], r * 2.1f, (Color){ 255, 255, 255, (unsigned char)(34.0f * fade) });
    }

    Vector2 p = station_point(s_pos);

    /* Glow: two wide, very faint discs, then the hairline ring. All of it grows
       and brightens while the token is in motion. */
    DrawCircleV(p, (17.0f + 14.0f * g) * pulse, (Color){ 255, 255, 255, (unsigned char)(16.0f + 46.0f * g) });
    DrawCircleV(p, (12.0f +  9.0f * g) * pulse, (Color){ 255, 255, 255, (unsigned char)(26.0f + 58.0f * g) });
    if (g > 0.02f) {
        /* motion bloom: a third, much wider disc that only exists while moving */
        DrawCircleV(p, 34.0f * g * pulse, (Color){ 255, 255, 255, (unsigned char)(14.0f * g) });
    }
    DrawCircleLinesV(p, 8.0f * pulse, (Color){ 255, 255, 255, (unsigned char)(210.0f + 45.0f * g) });
    DrawCircleV(p, 2.6f * pulse, (Color){ 255, 255, 255, (unsigned char)(110.0f + 80.0f * g) });

    /* The badge: a D, the dealer's hand, centred in the token. */
    const char *d = "D";
    float fs = 14.0f * pulse;
    Vector2 sz = MeasureTextEx(font_body, d, fs, 1.0f);
    DrawTextEx(font_body, d,
               (Vector2){ p.x - sz.x * 0.5f, p.y - sz.y * 0.5f },
               fs, 1.0f, (Color){ 255, 255, 255, 235 });
}
