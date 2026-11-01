#include "ui_glow.h"
#include "ui_theme.h"

/* Light over the table casing overlay. UI_GLOW.md owns this: what it draws, what it
   must never touch, and what each constant does. Read that before changing a number.

   What it is, in one paragraph: the overlay the table is already wearing is drawn a
   second time, additively, through a shader that reads the art's own luminance
   gradient as a normal and adds a specular where two slow lights line up. The casing
   lines catch the light; the transparent parts of the overlay stay transparent,
   because the art's alpha gates the pass.

   The overlay is the map's art - the painted pair or the CRT pair, whichever the
   table drew this frame. This module is handed that texture and never chooses it.

   The tuning surface. Nothing else in this file is meant to be edited. */

#define UI_GLOW_INTENSITY   0.55f   /* overall strength; 0.0 switches the pass off  */
#define UI_GLOW_SPEED_A     0.043f  /* light A, laps per second                     */
#define UI_GLOW_SPEED_B     0.031f  /* light B, laps per second                     */
#define UI_GLOW_ALPHA_LO    0.30f   /* overlay alpha where the light starts         */
#define UI_GLOW_ALPHA_HI    0.85f   /* overlay alpha where it is at full strength   */
#define UI_GLOW_FALLOFF     0.075f  /* light A pool width, UV squared; B is 2.4x it */
#define UI_GLOW_SAMPLE_PX   1.45f   /* neighbour distance for the normal, in pixels */

/* The mask is the art's own alpha - the same idea as the card glint's solid gate
   (src/ui_board.c, BEZEL_SHINE_BODY), which works because overlay art, like a card
   bezel, is transparent where there is no casing. */
#define UI_GLOW_BODY(TEX_FN, OUT_FRAG) \
    "float bval(vec3 v) { return dot(v, vec3(0.299, 0.587, 0.114)); }\n" \
    "void main() {\n" \
    "    vec4 c = " TEX_FN "(texture0, fragTexCoord);\n" \
    "    float mask = smoothstep(u_glow.y, u_glow.z, c.a);\n" \
    "    if (mask <= 0.001) discard;\n" \
    "    vec2 o = u_phase.zw;\n" \
    "    float hl = bval(" TEX_FN "(texture0, fragTexCoord - vec2(o.x, 0.0)).rgb);\n" \
    "    float hr = bval(" TEX_FN "(texture0, fragTexCoord + vec2(o.x, 0.0)).rgb);\n" \
    "    float hu = bval(" TEX_FN "(texture0, fragTexCoord - vec2(0.0, o.y)).rgb);\n" \
    "    float hd = bval(" TEX_FN "(texture0, fragTexCoord + vec2(0.0, o.y)).rgb);\n" \
    "    float peak = max(max(max(hl, hr), max(hu, hd)), 0.06);\n" \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n" \
    "    vec3 n = normalize(vec3(g * 2.2, 1.0));\n" \
    "    vec2 t1 = (vec2(0.5 + 0.40 * cos(u_phase.x), 0.5 + 0.38 * sin(u_phase.x)) - fragTexCoord) * vec2(0.80, 1.0);\n" \
    "    vec3 h1 = normalize(normalize(vec3(t1, 0.35)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd1 = max(dot(n, h1), 0.0);\n" \
    "    vec2 t2 = (vec2(0.5 + 0.40 * cos(u_phase.y), 0.5 + 0.38 * sin(u_phase.y * 2.0)) - fragTexCoord) * vec2(0.80, 1.0);\n" \
    "    vec3 h2 = normalize(normalize(vec3(t2, 0.35)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd2 = max(dot(n, h2), 0.0);\n" \
    "    float core = exp(-dot(t1, t1) / u_glow.w) * (0.20 + 0.80 * pow(nd1, 3.0))\n" \
    "               + exp(-dot(t2, t2) / (u_glow.w * 2.4)) * (0.20 + 0.80 * pow(nd2, 3.0));\n" \
    "    float raw = mask * core * u_glow.x;\n" \
    "    float a = 0.85 * (1.0 - exp(-raw * 2.2));\n" \
    "    if (a < 0.003) discard;\n" \
    "    " OUT_FRAG " = vec4(fragColor.rgb, a);\n" \
    "}\n"

#ifdef __EMSCRIPTEN__
static const char *const k_ui_glow_fs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "uniform vec4 u_glow;\n"
    UI_GLOW_BODY("texture2D", "gl_FragColor");
#else
static const char *const k_ui_glow_fs =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "uniform vec4 u_glow;\n"
    "out vec4 finalColor;\n"
    UI_GLOW_BODY("texture", "finalColor");
#endif
#undef UI_GLOW_BODY

static Shader s_shader = { 0 };
static int s_phase_loc = -1;
static int s_glow_loc = -1;
static int s_state = 0;   /* 0 = not tried yet, 1 = ready, -1 = unavailable */

static void ui_glow_init(void) {
    s_shader = LoadShaderFromMemory(0, k_ui_glow_fs);
    if (s_shader.id <= 0) { s_state = -1; return; }
    s_phase_loc = GetShaderLocation(s_shader, "u_phase");
    s_glow_loc  = GetShaderLocation(s_shader, "u_glow");
    s_state = 1;
}

void ui_glow_draw_table_overlay(Texture2D overlay, Rectangle dest) {
    if (UI_GLOW_INTENSITY <= 0.0f) return;
    if (overlay.id <= 0 || overlay.width <= 0 || overlay.height <= 0) return;
    if (dest.width <= 1.0f || dest.height <= 1.0f) return;
    if (s_state == 0) ui_glow_init();
    if (s_state < 0) return;

    const float t = (float)GetTime();
    const float two_pi = 6.283185307f;
    float phase[4] = {
        t * UI_GLOW_SPEED_A * two_pi,
        t * UI_GLOW_SPEED_B * two_pi + 1.7f,
        UI_GLOW_SAMPLE_PX / (float)overlay.width,
        UI_GLOW_SAMPLE_PX / (float)overlay.height
    };
    float tune[4] = { UI_GLOW_INTENSITY, UI_GLOW_ALPHA_LO, UI_GLOW_ALPHA_HI, UI_GLOW_FALLOFF };
    if (s_phase_loc >= 0) SetShaderValue(s_shader, s_phase_loc, phase, SHADER_UNIFORM_VEC4);
    if (s_glow_loc >= 0)  SetShaderValue(s_shader, s_glow_loc, tune, SHADER_UNIFORM_VEC4);

    /* The tint is the theme's colour: the shader emits fragColor.rgb untouched and
       only decides where the light lands, exactly as the card glint does. */
    const Color tint = g_theme.base;

    /* Same orientation as the overlay's own draw (DrawTexture at 0,0) - no flip. */
    const Rectangle src = { 0.0f, 0.0f, (float)overlay.width, (float)overlay.height };

    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexturePro(overlay, src, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
    EndShaderMode();
    EndBlendMode();
}
