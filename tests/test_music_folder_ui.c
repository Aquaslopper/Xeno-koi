/* Keyboard/paste/apply modal checks; real UI logic, no window or renderer. */
#include <assert.h>
#include "../src/ui_screens.c"

static int typed, key, mouse, down, ctrl, loaded = 3, saved;
static float effects[3];
static const char *clipboard;
static char applied[MUSIC_FOLDER_CAP];
int GetCharPressed(void) { int result = typed; typed = 0; return result; }
bool IsKeyPressed(int k) { return key == k; }
bool IsKeyPressedRepeat(int k) { (void)k; return false; }
bool IsKeyDown(int k) { return ctrl && k == KEY_LEFT_CONTROL; }
bool IsMouseButtonDown(int b) { (void)b; return down; }
bool IsMouseButtonPressed(int b) { (void)b; return mouse; }
const char *GetClipboardText(void) { return clipboard; }
const char *CodepointToUTF8(int cp, int *bytes) {
    assert(cp == 0x00e4); *bytes = 2; return "\xc3\xa4";
}
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
int audio_set_music_folder(AudioAssets *a, const char *path) {
    (void)a; strcpy(applied, path); return loaded;
}
void audio_set_effects(float w, float b, float r) { effects[0] = w; effects[1] = b; effects[2] = r; }
void audio_set_bgm_volume(AudioAssets *a, float v) { a->bgm_volume = v; }
void audio_set_sfx_volume(AudioAssets *a, float v) { a->sfx_volume = v; }
void audio_play_click(AudioAssets *a) { (void)a; }
void theme_set(int theme) { (void)theme; }
void ToggleBorderlessWindowed(void) {}
void storage_save_settings(const GameSettings *s) { (void)s; saved++; }

int main(void) {
    GameSettings s = {0};
    AudioAssets a = {0};
    Vector2 nowhere = {-1, -1};
    strcpy(s.music_folder, "/original");
    music_folder_open = 1;
    music_folder_edit[0] = '\0';
    typed = 0x00e4;
    handle_music_folder_input(&s, &a, nowhere);
    assert(strcmp(music_folder_edit, "ä") == 0);
    key = KEY_BACKSPACE;
    handle_music_folder_input(&s, &a, nowhere);
    assert(!music_folder_edit[0]);
    ctrl = 1; key = KEY_V; clipboard = "\"C:\\My Music\\mix\"";
    handle_music_folder_input(&s, &a, nowhere);
    assert(strcmp(music_folder_edit, clipboard) == 0);
    clipboard = "bad\npath";
    handle_music_folder_input(&s, &a, nowhere);
    assert(strcmp(music_folder_edit, "\"C:\\My Music\\mix\"") == 0);
    ctrl = 0; key = KEY_ENTER; loaded = -1;
    handle_music_folder_input(&s, &a, nowhere);
    assert(music_folder_open && saved == 0 && strcmp(s.music_folder, "/original") == 0);
    loaded = 3;
    handle_music_folder_input(&s, &a, nowhere);
    assert(!music_folder_open && saved == 1);
    assert(strcmp(s.music_folder, "C:\\My Music\\mix") == 0);
    assert(strcmp(applied, s.music_folder) == 0);
    music_folder_open = 1; key = 0; mouse = 1;
    strcpy(music_folder_edit, "/unsaved");
    handle_music_folder_input(&s, &a, (Vector2){800, 440}); /* Cancel */
    assert(!music_folder_open && saved == 1 && strcmp(s.music_folder, applied) == 0);
    music_folder_open = 1;
    handle_music_folder_input(&s, &a, (Vector2){600, 440}); /* Default */
    assert(!music_folder_open && saved == 2 && !s.music_folder[0] && !applied[0]);
    BoardAssets ba = {0};
    s.rounds = 6; s.bgm_volume = 0.42f; s.sfx_volume = 0.43f;
    options_page = 0;
    assert(screens_handle_options_click(&s, &ba, &a, (Vector2){980, 672}) == ACT_NONE);
    assert(options_page == 1 && s.rounds == 6); /* Next page, not an old control. */
    assert(screens_audio_test_state() == -1);
    screens_handle_options_click(&s, &ba, &a, (Vector2){600, 598});
    assert(screens_audio_test_state() == -1 && audio_test_unavailable); /* No silent fake test. */
    a.game_track_count = 2;
    screens_handle_options_click(&s, &ba, &a, (Vector2){600, 598});
    assert(screens_audio_test_state() == 1 && !audio_test_unavailable);
    screens_handle_options_click(&s, &ba, &a, (Vector2){600, 598});
    assert(screens_audio_test_state() == 0); /* Dry A/B stays in the match playlist. */
    screens_handle_options_click(&s, &ba, &a, (Vector2){600, 598});
    assert(screens_audio_test_state() == 1);
    down = 1;
    screens_handle_options_click(&s, &ba, &a, (Vector2){872, 330});
    assert(s.effect_stereo == 1 && effects[0] == 1 && screens_audio_test_state() == 1);
    screens_handle_options_click(&s, &ba, &a, (Vector2){780, 395});
    assert(fabsf(s.effect_bass - 0.5f) < 1e-6f && fabsf(effects[1] - 0.5f) < 1e-6f);
    screens_handle_options_click(&s, &ba, &a, (Vector2){688, 458});
    assert(s.effect_reverb == 0 && effects[2] == 0 && s.sfx_volume == 0.43f);
    down = 0;
    screens_handle_options_click(&s, &ba, &a, (Vector2){890, 330}); /* + at maximum clamps. */
    assert(s.effect_stereo == 1);
    screens_handle_options_click(&s, &ba, &a, (Vector2){665, 458}); /* - at minimum clamps. */
    assert(s.effect_reverb == 0);
    screens_handle_options_click(&s, &ba, &a, (Vector2){700, 140});
    assert(music_folder_open && options_page == 1);
    screens_handle_options_click(&s, &ba, &a, (Vector2){980, 672});
    assert(options_page == 1 && music_folder_open); /* Modal blocks page clicks. */
    screens_handle_options_click(&s, &ba, &a, (Vector2){800, 440});
    assert(!music_folder_open);
    screens_handle_options_click(&s, &ba, &a, (Vector2){860, 672});
    assert(options_page == 0 && screens_audio_test_state() == -1);
    screens_handle_options_click(&s, &ba, &a, (Vector2){980, 672});
    assert(options_page == 1 && screens_audio_test_state() == -1); /* Never auto-start after return. */
    screens_handle_options_click(&s, &ba, &a, (Vector2){600, 598});
    assert(screens_audio_test_state() == 1);
    assert(screens_handle_options_click(&s, &ba, &a, (Vector2){400, 672}) == ACT_GOTO_MENU);
    assert(options_page == 0 && saved == 3 && screens_audio_test_state() == -1);
    assert(s.bgm_volume == 0.42f && s.rounds == 6);
    puts("All music folder / Options pagination / effect slider input checks passed.");
    return 0;
}
