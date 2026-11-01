#include "ui_hub.h"
#include "ui_theme.h"
#include "version.h"
#include <math.h>
#include <stdio.h>

extern Font font_title;
extern Font font_body;
/* Deus ON state is derived from settings; only pre-activation values are cached. */
static StorageDeusSessionSnapshot s_deus_snapshot;

void ui_hub_deus_snapshot_capture(const GameSettings *s) {
    if (!s || s->difficulty == DIFFICULTY_DEUS) {
        s_deus_snapshot.valid = 0;
        return;
    }
    s_deus_snapshot.difficulty = s->difficulty;
    s_deus_snapshot.ruleset = s->ruleset;
    s_deus_snapshot.master_timer = s->master_timer;
    s_deus_snapshot.terminal_color = s->terminal_color;
    s_deus_snapshot.background = s->background;
    s_deus_snapshot.current_opponent = s->current_opponent;
    s_deus_snapshot.valid = 1;
}

void ui_hub_deus_snapshot_invalidate(void) {
    s_deus_snapshot.valid = 0;
}

int ui_hub_deus_snapshot_is_valid(void) {
    return s_deus_snapshot.valid;
}

const StorageDeusSessionSnapshot *ui_hub_deus_snapshot(void) {
    return &s_deus_snapshot;
}

/* --- Geometry (1280 x 720 virtual canvas) -------------------------------- */

static const Rectangle k_title_btns[3] = {
    { 490.0f, 374.0f, 300.0f, 48.0f },
    { 490.0f, 440.0f, 300.0f, 48.0f },
    { 490.0f, 506.0f, 300.0f, 48.0f }
};
static const Rectangle k_options_menu_btns[4] = {
    { 430.0f, 316.0f, 420.0f, 46.0f },
    { 430.0f, 374.0f, 420.0f, 46.0f },
    { 430.0f, 432.0f, 420.0f, 46.0f },
    { 430.0f, 490.0f, 420.0f, 46.0f }
};
static const char *const k_options_menu_labels[4] = {
    "SYSTEM OPTIONS", "LOAD / SAVE", "CREDITS", "BACK"
};
static const HubAction k_options_menu_actions[4] = {
    HUB_ACT_OPTIONS_SYSTEM, HUB_ACT_OPTIONS_PROFILES,
    HUB_ACT_OPTIONS_CREDITS, HUB_ACT_OPTIONS_BACK
};
static const Rectangle k_profile_rows[3] = {
    { 265.0f, 220.0f, 750.0f, 66.0f },
    { 265.0f, 306.0f, 750.0f, 66.0f },
    { 265.0f, 392.0f, 750.0f, 66.0f }
};
static const Rectangle k_profile_actions[5] = {
    { 108.0f, 524.0f, 204.0f, 48.0f }, { 328.0f, 524.0f, 204.0f, 48.0f },
    { 548.0f, 524.0f, 204.0f, 48.0f }, { 768.0f, 524.0f, 204.0f, 48.0f },
    { 988.0f, 524.0f, 184.0f, 48.0f }
};
static const char *const k_profile_action_labels[5] = {
    "CREATE", "LOAD", "ERASE", "SAVE ACTIVE", "BACK"
};


/* Mode select: one button per card plus the copy above it, no heading (the button carries
   the name), and both cards are built the same way - same inset, same
   copy/button/back rows - so only their width differs. Pancratium is the short door and
   Atrium the long one. The copy is drawn straight on the card: it used to sit in a second
   framed strip of its own, which capped it at 15-16 px for no gain. */
static const Rectangle k_main_left_panel    = { 130.0f, 256.0f, 320.0f, 178.0f };
static const Rectangle k_main_right_panel   = { 482.0f, 256.0f, 668.0f, 178.0f };
static const Rectangle k_main_campaign_btn  = { 154.0f, 366.0f, 272.0f,  48.0f };
static const Rectangle k_main_atrium_btn    = { 506.0f, 366.0f, 620.0f,  48.0f };
static const Rectangle k_main_back_btn      = { 510.0f, 594.0f, 260.0f,  48.0f };

static const Rectangle k_camp_panel         = { 280.0f,  96.0f, 720.0f, 548.0f };
static const Rectangle k_camp_select_btn    = { 430.0f, 254.0f, 420.0f,  50.0f };
static const Rectangle k_camp_stats_btn     = { 430.0f, 314.0f, 420.0f,  50.0f };
static const Rectangle k_camp_options_btn   = { 430.0f, 374.0f, 420.0f,  50.0f };
static const Rectangle k_camp_deus_box      = { 430.0f, 434.0f, 420.0f,  56.0f };
static const Rectangle k_camp_back_btn      = { 510.0f, 536.0f, 260.0f,  46.0f };

/* --- Pure readout helpers ------------------------------------------------ */

int ui_hub_unlocked_sectors(const GameSettings *s) {
    if (!s) return 1;
    if (s->unlocked_opponents < 1) return 1;
    if (s->unlocked_opponents > 12) return 12;
    return s->unlocked_opponents;
}

int ui_hub_count_coins(const GameSettings *s) {
    if (!s) return 0;
    int count = 0;
    for (int i = 0; i < 36; i++) {
        if ((s->cleared_mask >> i) & 1ULL) count++;
    }
    return count;
}

static const char *hub_ruleset_label(Ruleset r) {
    switch (r) {
        case RULESET_NINTENDO:   return "NINTENDO";
        case RULESET_FUDAWIKI:   return "FUDAWIKI";
        case RULESET_KK_ADD_10:  return "KK-ADD-10";
        case RULESET_TOURNAMENT: return "TOURNAMENT";
        case RULESET_YAKUZA:     return "YAKUZA";
        default:                 return "NINTENDO";
    }
}

static const char *hub_difficulty_label(Difficulty d) {
    switch (d) {
        case DIFFICULTY_EASY:   return "EASY";
        case DIFFICULTY_HARD:   return "HARD";
        case DIFFICULTY_MASTER: return "MASTER";
        case DIFFICULTY_DEUS:   return "DEUS";
        default:                return "EASY";
    }
}

/* --- Static draw helpers (side-effect free) ------------------------------ */

static void hub_draw_text_snapped(Font font, const char *text, float x, float y, float fsz, float sp, Color col) {
    DrawTextEx(font, text, (Vector2){ floorf(x), floorf(y) }, fsz, sp, col);
}

/* Headings and the brand line draw plain text. The glyph motes live on hovered buttons
   only (hub_draw_button below) - they are a button affordance, not decoration. */
static void hub_draw_centered(Font font, const char *text, float cx, float y, float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    Vector2 pos = { floorf(cx - ts.x * 0.5f), floorf(y) };
    DrawTextEx(font, text, pos, fsz, sp, col);
}

static void hub_draw_backdrop_alpha(const BoardAssets *ba, unsigned char alpha) {
    if (ba && ba->current_bg >= 0 && ba->current_bg < 8 && ba->bgs[ba->current_bg].id > 0) {
        int bg = ba->current_bg;
        if (ba->has_fog_shader) {
            float cur_time = (float)GetTime();
            float light_on = (ba->bg_normals[bg].id > 0 &&
                              ba->light_normal_loc >= 0 &&
                              ba->light_enabled_loc >= 0) ? 1.0f : 0.0f;
            float flash_zero = 0.0f;
            SetShaderValue(ba->fog_shader, ba->fog_time_loc, &cur_time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(ba->fog_shader, ba->light_enabled_loc, &light_on, SHADER_UNIFORM_FLOAT);
            if (ba->light_flash_loc >= 0)
                SetShaderValue(ba->fog_shader, ba->light_flash_loc, &flash_zero, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(ba->fog_shader);
            if (light_on > 0.5f)
                SetShaderValueTexture(ba->fog_shader, ba->light_normal_loc, ba->bg_normals[bg]);
        }
        DrawTexturePro(ba->bgs[bg],
                       (Rectangle){ 0.0f, 0.0f, (float)CANVAS_W, (float)CANVAS_H },
                       (Rectangle){ 0.0f, 0.0f, (float)CANVAS_W, (float)CANVAS_H },
                       (Vector2){ 0.0f, 0.0f }, 0.0f, WHITE);
        if (ba->has_fog_shader) EndShaderMode();
    } else {
        ClearBackground(BLACK);
    }
    DrawRectangle(0, 0, CANVAS_W, CANVAS_H, (Color){ 0, 0, 0, alpha });
}

static void hub_draw_backdrop(const BoardAssets *ba) {
    hub_draw_backdrop_alpha(ba, 168);
}

static void hub_draw_button(Rectangle r, const char *label, int fsz, Vector2 vm) {
    int hov = CheckCollisionPointRec(vm, r);
    DrawRectangleRec(r, hov ? COLOR_BTN_HOVER_BG : COLOR_PANEL_BG);
    ui_draw_panel_border(r, hov ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
    Vector2 ts = MeasureTextEx(font_title, label, (float)fsz, 1.2f);
    Vector2 pos = {
        floorf(r.x + (r.width - ts.x) * 0.5f),
        floorf(r.y + (r.height - (float)fsz) * 0.5f)
    };
    DrawTextEx(font_title, label, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
    if (hov) ui_theme_draw_text_flares(font_title, label, pos, (float)fsz, 1.2f, COLOR_PHOSPHOR);
}

static void hub_record_hit(HubButtonHit *out_hit, Rectangle r, const char *lbl, int fsz) {
    if (!out_hit) return;
    out_hit->rect      = r;
    out_hit->label     = lbl;
    out_hit->font_size = fsz;
}

/* --- Title Screen (SCREEN_START) ----------------------------------------- */

/* No bold face ships with this repo - all four TTFs are weight 400 - so the
   subtitle's weight is synthesised: the line is drawn twice with a sub-pixel
   horizontal offset. At 37 px a 0.9 px offset thickens the stems without
   visibly doubling the edges, and the centring accounts for the extra 0.9 px
   so the line stays optically centred. */
static void hub_draw_centered_bold(Font font, const char *text, float cx, float y,
                                   float fsz, float sp, Color col) {
    Vector2 ts = MeasureTextEx(font, text, fsz, sp);
    float x = floorf(cx - (ts.x + 0.9f) * 0.5f);
    DrawTextEx(font, text, (Vector2){ x, floorf(y) }, fsz, sp, col);
    DrawTextEx(font, text, (Vector2){ x + 0.9f, floorf(y) }, fsz, sp, col);
}

void ui_hub_draw_title(const BoardAssets *ba, Vector2 vm, float saved_timer, int has_active_profile) {
    const char *labels[3] = { has_active_profile ? "CONTINUE" : "START GAME", "OPTION", "QUIT" };
    hub_draw_backdrop_alpha(ba, 95);
    Color wine_red = { 224, 136, 146, 255 };
    /* Owner pass, 29 October 2026. 140.8 = 176 * 0.8 (brand 20 % smaller) and
       37.4 = 34 * 1.1 (subtitle 10 % bigger). The brand's y is ink-to-ink, not an
       em box: at 140.8 px its ink runs +24.9 .. +155.6 below its y, and the
       subtitle's ink starts +10.4 below its own y at 248, so the visible gap is
       exactly 102.8 - y. y = 91 left 11.8 px and still read as a hole, so it comes
       down again to y = 96 for 6.8 px: close enough to touch, far enough not to
       clip the subtitle's ascenders. At the old 176 px the same pair was ~3 px
       apart, which is why shrinking alone opened the hole in the first place.
       Smaller also means less upscaling from the 48 px master (3.67x -> 2.93x),
       so the line is slightly sharper as a side effect. */
    hub_draw_centered(font_title, "[XENO / KOI]", 640.0f, 96.0f, 140.8f, 3.0f, COLOR_PHOSPHOR);
    hub_draw_centered_bold(font_body, "HANAFUDA: ACOUSTICA APEXUS", 640.0f, 248.0f, 37.4f, 4.0f, wine_red);
    for (int i = 0; i < 3; i++) hub_draw_button(k_title_btns[i], labels[i], 18, vm);
    if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "ALL FILES SAVED", 640.0f, 675.0f, 20.0f, 1.5f, COLOR_PHOSPHOR);
    /* Build stamp, bottom-right (see src/version.h). This is the title screen the game
       actually draws - screens_draw_start() in ui_screens.c has no callers and never runs.
       Right-aligned to a 24 px margin on the 1280x720 canvas: the buttons end at y 554 and
       the SAVED note is centred at 675, so 694 is clear of both. It is reference, not a
       control, so it stays small and quiet - but it draws in the same phosphor as the
       menu, not the dim tone: the backdrop under the corner is a painted
       board at 95/255 black, and dim-on-artwork was the difference between "there" and
       "I cannot see it". */
    {
        const float fsz = 15.0f, sp = 1.2f;
        Vector2 vs = MeasureTextEx(font_body, XENO_KOI_VERSION, fsz, sp);
        hub_draw_text_snapped(font_body, XENO_KOI_VERSION,
                              CANVAS_W - 24.0f - vs.x, CANVAS_H - 26.0f,
                              fsz, sp, COLOR_PHOSPHOR);
    }
}

HubAction ui_hub_handle_title_click(Vector2 vm, HubButtonHit *out_hit, int has_active_profile) {
    static const HubAction actions[3] = { HUB_ACT_TITLE_START_GAME, HUB_ACT_TITLE_OPTIONS, HUB_ACT_TITLE_QUIT };
    const char *labels[3] = { has_active_profile ? "CONTINUE" : "START GAME", "OPTION", "QUIT" };
    for (int i = 0; i < 3; i++) {
        if (CheckCollisionPointRec(vm, k_title_btns[i])) {
            hub_record_hit(out_hit, k_title_btns[i], labels[i], 18);
            return actions[i];
        }
    }
    return HUB_ACT_NONE;
}

static void hub_draw_menu_panel(const BoardAssets *ba, const char *title, const char *subtitle) {
    hub_draw_backdrop(ba);
    hub_draw_centered(font_title, title, 640.0f, 118.0f, 30.0f, 1.4f, COLOR_PHOSPHOR);
    hub_draw_centered(font_body, subtitle, 640.0f, 164.0f, 17.0f, 1.0f, COLOR_PHOSPHOR_DIM);
}

void ui_hub_draw_options_menu(const BoardAssets *ba, Vector2 vm, float saved_timer) {
    hub_draw_menu_panel(ba, "OPTIONS", "SYSTEM SETTINGS  //  PROFILE DATA  //  CREDITS");
    for (int i = 0; i < 4; i++) hub_draw_button(k_options_menu_btns[i], k_options_menu_labels[i], 18, vm);
    if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "PROFILE SAVED", 640.0f, 590.0f, 17.0f, 1.0f, COLOR_PHOSPHOR);
}

HubAction ui_hub_handle_options_menu_click(Vector2 vm, HubButtonHit *out_hit) {
    for (int i = 0; i < 4; i++) {
        if (CheckCollisionPointRec(vm, k_options_menu_btns[i])) {
            hub_record_hit(out_hit, k_options_menu_btns[i], k_options_menu_labels[i], 18);
            return k_options_menu_actions[i];
        }
    }
    return HUB_ACT_NONE;
}

static int s_profile_manager_selected_slot = 1;
void ui_hub_profile_manager_reset(int selected_slot) {
    s_profile_manager_selected_slot = (selected_slot >= 1 && selected_slot <= 3) ? selected_slot : 1;
}

int ui_hub_profile_manager_selected_slot(void) { return s_profile_manager_selected_slot; }

void ui_hub_draw_profile_manager(const BoardAssets *ba, const int slots[3], int active_slot,
                                 int selected_slot, Vector2 vm, float saved_timer, int error) {
    hub_draw_menu_panel(ba, "PROFILE LOAD / SAVE", "SELECT A SLOT  //  PROFILES ARE STORED SEPARATELY");
    for (int i = 0; i < 3; i++) {
        Rectangle r = k_profile_rows[i];
        int slot = i + 1;
        int selected = slot == selected_slot;
        DrawRectangleRec(r, selected ? (Color){ 8, 28, 31, 242 } : (Color){ 0, 0, 0, 224 });
        DrawRectangleLinesEx(r, selected ? 2.0f : 1.0f, selected ? COLOR_PHOSPHOR : COLOR_PHOSPHOR_DIM);
        char label[96];
        snprintf(label, sizeof(label), "SLOT %02d  //  %s%s", slot,
                 slots[i] ? "PROFILE" : "EMPTY",
                 (slot == active_slot) ? "  //  ACTIVE" : "");
        hub_draw_text_snapped(font_body, label, r.x + 24.0f, r.y + 21.0f, 19.0f, 1.0f,
                              selected ? COLOR_PHOSPHOR : COLOR_TEXT_INFO);
        if (CheckCollisionPointRec(vm, r)) {
            Color glow = COLOR_PHOSPHOR; glow.a = 45;
            DrawRectangleRec(r, glow);
        }
    }
    for (int i = 0; i < 5; i++) {
        int enabled = 1;
        if (i == 0) enabled = !slots[selected_slot - 1];
        else if (i == 1) enabled = slots[selected_slot - 1];
        else if (i == 2) enabled = slots[selected_slot - 1] && selected_slot != active_slot;
        else if (i == 3) enabled = active_slot > 0;
        if (enabled) hub_draw_button(k_profile_actions[i], k_profile_action_labels[i], 15, vm);
        else {
            DrawRectangleRec(k_profile_actions[i], (Color){ 0, 0, 0, 175 });
            DrawRectangleLinesEx(k_profile_actions[i], 1.0f, COLOR_PHOSPHOR_DIM);
            hub_draw_centered(font_body, k_profile_action_labels[i],
                              k_profile_actions[i].x + k_profile_actions[i].width * 0.5f,
                              k_profile_actions[i].y + 14.0f, 15.0f, 1.0f, COLOR_PHOSPHOR_DIM);
        }
    }
    if (error) hub_draw_centered(font_body, "PROFILE OPERATION FAILED  //  ACTIVE DATA PRESERVED",
                                 640.0f, 610.0f, 16.0f, 1.0f, COLOR_RED);
    else if (saved_timer > 0.0f)
        hub_draw_centered(font_body, "PROFILE SAVED", 640.0f, 610.0f, 16.0f, 1.0f, COLOR_PHOSPHOR);
}

HubAction ui_hub_handle_profile_manager_click(Vector2 vm, HubButtonHit *out_hit, int *out_slot,
                                                const int slots[3], int active_slot) {
    for (int i = 0; i < 3; i++) {
        if (CheckCollisionPointRec(vm, k_profile_rows[i])) {
            s_profile_manager_selected_slot = i + 1;
            if (out_slot) *out_slot = i + 1;
            hub_record_hit(out_hit, k_profile_rows[i], "SLOT", 18);
            return HUB_ACT_PROFILE_SLOT_SELECTED;
        }
    }
    int selected = s_profile_manager_selected_slot - 1;
    for (int i = 0; i < 5; i++) {
        if (!CheckCollisionPointRec(vm, k_profile_actions[i])) continue;
        if ((i == 0 && slots[selected]) ||
            (i == 1 && !slots[selected]) ||
            (i == 2 && (!slots[selected] || s_profile_manager_selected_slot == active_slot)) ||
            (i == 3 && active_slot == 0)) return HUB_ACT_NONE;
        if (out_slot) *out_slot = s_profile_manager_selected_slot;
        hub_record_hit(out_hit, k_profile_actions[i], k_profile_action_labels[i], 15);
        return (HubAction)(HUB_ACT_PROFILE_CREATE + i);
    }
    return HUB_ACT_NONE;
}

/* The second line of the Pancratium card is the only moving copy on this screen: one
   combat line at a time, rotated every few seconds, so the card answers "what have I done
   in here" without a wall of text. Same GameStats the result and stats screens read. */
static void hub_draw_combat_line(const GameStats *st, float cx, float y) {
    char line[96];
    const char *txt = "NO MATCHES YET  //  PLAY ONE";
    Color col = COLOR_AMBER;
    if (st && st->matches_played > 0) {
        col = COLOR_TEXT_INFO_DIM;
        switch ((int)(GetTime() / 5.0f) % 4) {
            case 0:
                snprintf(line, sizeof(line), "W %d / L %d   //   STREAK %d",
                         st->matches_won, st->matches_lost, st->current_streak);
                break;
            case 1: {
                int rate = (st->matches_played > 0)
                               ? (int)((100.0f * (float)st->matches_won) / (float)st->matches_played)
                               : 0;
                snprintf(line, sizeof(line), "WIN RATE %d%%   //   BEST STREAK %d",
                         rate, st->best_streak);
                break;
            }
            case 2:
                snprintf(line, sizeof(line), "TOP YAKU  %s", storage_get_favorite_combo(st));
                break;
            default:
                snprintf(line, sizeof(line), "KOI CALLS %d   //   BEST ROUND %d",
                         st->koi_calls_you, st->highest_round_score);
                break;
        }
        txt = line;
    }
    hub_draw_centered(font_body, txt, cx, y, 16.0f, 1.0f, col);
}

/* --- Main Game Hub (SCREEN_HUB_MAIN) ------------------------------------- */

void ui_hub_draw_main(const BoardAssets *ba, const GameSettings *s, const GameStats *st,
                      Vector2 vm) {
    int sectors = ui_hub_unlocked_sectors(s);
    int coins   = ui_hub_count_coins(s);
    int is_deus = (s && s->difficulty == DIFFICULTY_DEUS);
    char line1[96];

    hub_draw_backdrop(ba);

    hub_draw_centered(font_title, "[XENO / KOI]", 640.0f, 36.0f, 44.0f, 2.2f, COLOR_PHOSPHOR);
    hub_draw_centered(font_body, "MAIN TERMINAL  //  MODE SELECT", 640.0f, 92.0f, 20.0f, 2.0f, COLOR_PHOSPHOR_DIM);

    /* Left Card: CAMPAIGN -- slim, one frame. */
    DrawRectangleRec(k_main_left_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_main_left_panel, COLOR_PHOSPHOR_DIM);
    /* The card holds the button and nothing else framed. The copy above it used to
       sit in its own 58 px box, which bought no legibility and cost the text the
       room to grow; the box and its two constants are gone (owner, 29 Oct 2026). */
    snprintf(line1, sizeof(line1), "SECTORS %02d/12   //   COINS %02d/36", sectors, coins);
    hub_draw_centered(font_body, line1, 290.0f, 286.0f, 17.0f, 1.1f, COLOR_PHOSPHOR);
    /* Deus is a session-long warning, so it outranks the rotating combat line. */
    if (is_deus) {
        hub_draw_centered(font_body, "DEUS SUPREMUS ACTIVE", 290.0f, 314.0f, 16.0f, 1.0f, COLOR_RED);
    } else {
        hub_draw_combat_line(st, 290.0f, 314.0f);
    }

    hub_draw_button(k_main_campaign_btn, "PANCRATIUM", 20, vm);

    /* Right Card: ATRIUM PLEX -- same build as the Pancratium card, only longer; the two
       share one frame style so the screen reads as one system of doors. */
    DrawRectangleRec(k_main_right_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_main_right_panel, COLOR_PHOSPHOR_DIM);

    /* Same as the left card: no box around the copy. 15 px was the smallest
       body text on the screen and this card is the widest surface in the game
       (620 px of button), so it goes to 19 px - it still fits with room to
       spare (516 px and 395 px drawn). */
    hub_draw_centered(font_body, "A monastic vault for the calibration of tactical reflexes",
                      816.0f, 285.0f, 19.0f, 1.0f, COLOR_TEXT_INFO_DIM);
    hub_draw_centered(font_body, "and the accumulation of theater dossiers.",
                      816.0f, 313.0f, 19.0f, 1.0f, COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_main_atrium_btn, "ATRIUM PLEX", 20, vm);

    /* Footer Back Button */
    hub_draw_button(k_main_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_main_click(Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_main_campaign_btn)) {
        hub_record_hit(out_hit, k_main_campaign_btn, "PANCRATIUM", 20);
        return HUB_ACT_GOTO_CAMPAIGN_HUB;
    }
    if (CheckCollisionPointRec(vm, k_main_atrium_btn)) {
        hub_record_hit(out_hit, k_main_atrium_btn, "ATRIUM PLEX", 20);
        return HUB_ACT_GOTO_ATRIUM;
    }
    if (CheckCollisionPointRec(vm, k_main_back_btn)) {
        hub_record_hit(out_hit, k_main_back_btn, "BACK", 20);
        return HUB_ACT_BACK_TO_TITLE;
    }
    return HUB_ACT_NONE;
}

/* --- Campaign Hub (SCREEN_HUB_CAMPAIGN) ---------------------------------- */

void ui_hub_draw_campaign(const BoardAssets *ba, const GameSettings *s, Vector2 vm) {
    int sectors = ui_hub_unlocked_sectors(s);
    int coins   = ui_hub_count_coins(s);
    int rounds  = (s && s->rounds >= 1 && s->rounds <= 12) ? s->rounds : 2;
    int is_deus = (s && s->difficulty == DIFFICULTY_DEUS);
    char line1[96], line2[96];

    hub_draw_backdrop(ba);

    /* One frame here too: the double frame was the only thing that made this page a
       different system from the cards in mode select. */
    DrawRectangleRec(k_camp_panel, COLOR_PANEL_BG);
    ui_draw_panel_border(k_camp_panel, COLOR_PHOSPHOR_DIM);

    hub_draw_centered(font_title, "PANCRATIUM", 640.0f, 118.0f, 28.0f, 1.5f, COLOR_PHOSPHOR);
    DrawLine(320, 160, 960, 160, COLOR_PHOSPHOR_DIM);

    Rectangle info_box = { 320.0f, 176.0f, 640.0f, 62.0f };
    DrawRectangleRec(info_box, (Color){ 4, 8, 10, 210 });
    DrawRectangleLinesEx(info_box, 1.0f, COLOR_PHOSPHOR_DIM);
    snprintf(line1, sizeof(line1), "UNLOCKED SECTORS %02d/12   |   COINS %02d/36", sectors, coins);
    hub_draw_centered(font_body, line1, 640.0f, 184.0f, 18.0f, 1.2f, COLOR_PHOSPHOR);
    snprintf(line2, sizeof(line2), "DIFFICULTY: %s   |   RULESET: %s   |   %d ROUNDS",
             s ? hub_difficulty_label(s->difficulty) : "EASY",
             s ? hub_ruleset_label(s->ruleset) : "KK-ADD-10",
             rounds);
    hub_draw_centered(font_body, line2, 640.0f, 212.0f, 15.0f, 1.0f,
                      is_deus ? COLOR_RED : COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_camp_select_btn, "APEX PANCRATIUM CIRCUIT", 20, vm);
    hub_draw_button(k_camp_stats_btn, "RECORDS & STATS", 20, vm);
    hub_draw_button(k_camp_options_btn, "SYSTEM OPTIONS", 20, vm);

    DrawRectangleRec(k_camp_deus_box, (Color){ 4, 8, 10, 210 });
    ui_draw_panel_border(k_camp_deus_box, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    hub_draw_centered(font_title, is_deus ? "DEUS MODE: ON" : "DEUS MODE: OFF",
                      640.0f, 443.0f, 17.0f, 1.3f, is_deus ? COLOR_RED : COLOR_PHOSPHOR_DIM);
    const char *deus_hint = !is_deus ? "CLICK TO ACTIVATE // SESSION ONLY" :
                            ui_hub_deus_snapshot_is_valid() ? "CLICK TO DEACTIVATE" : "OFF DISABLED // USE OPTIONS";
    hub_draw_centered(font_body, deus_hint, 640.0f, 468.0f, 14.0f, 1.0f, COLOR_TEXT_INFO_DIM);

    hub_draw_button(k_camp_back_btn, "BACK", 20, vm);
}

HubAction ui_hub_handle_campaign_click(const GameSettings *s, Vector2 vm, HubButtonHit *out_hit) {
    if (CheckCollisionPointRec(vm, k_camp_deus_box)) {
        if (s && s->difficulty == DIFFICULTY_DEUS) {
            if (!ui_hub_deus_snapshot_is_valid()) return HUB_ACT_NONE;
            hub_record_hit(out_hit, k_camp_deus_box, "DEUS MODE", 17);
            return HUB_ACT_DEUS_DEACTIVATE;
        }
        hub_record_hit(out_hit, k_camp_deus_box, "DEUS MODE", 17);
        return HUB_ACT_DEUS_ACTIVATE;
    }
    if (CheckCollisionPointRec(vm, k_camp_select_btn)) {
        hub_record_hit(out_hit, k_camp_select_btn, "APEX PANCRATIUM CIRCUIT", 20);
        return HUB_ACT_OPEN_CAMPAIGN_OVERLAY;
    }
    if (CheckCollisionPointRec(vm, k_camp_stats_btn)) {
        hub_record_hit(out_hit, k_camp_stats_btn, "RECORDS & STATS", 20);
        return HUB_ACT_GOTO_STATS;
    }
    if (CheckCollisionPointRec(vm, k_camp_options_btn)) {
        hub_record_hit(out_hit, k_camp_options_btn, "SYSTEM OPTIONS", 20);
        return HUB_ACT_CAMPAIGN_OPTIONS;
    }
    if (CheckCollisionPointRec(vm, k_camp_back_btn)) {
        hub_record_hit(out_hit, k_camp_back_btn, "BACK", 20);
        return HUB_ACT_BACK_TO_MAIN_HUB;
    }
    return HUB_ACT_NONE;
}
