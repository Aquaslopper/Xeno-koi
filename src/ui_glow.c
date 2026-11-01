#include "ui_glow.h"
#include "ui_theme.h"

/* Ambient light over the UI screens. UI_GLOW.md owns this: what it draws, what it
   must never touch, and what each constant does. Read that before changing a
   number here.

   What it is, in one paragraph: the frame the UI was already drawn into is drawn a
   second time, additively, through a shader that reads the picture's own luminance
   gradient as a normal and adds a specular where two slow-moving lights line up.
   Edges catch it, flat art does not.

   The tuning surface. Nothing else in this file is meant to be edited. */

#define UI_GLOW_INTENSITY   0.22f   /* overall strength; 0.0 switches the pass off  */
#define UI_GLOW_SPEED_A     0.043f  /* light A, laps per second                     */
#define UI_GLOW_SPEED_B     0.031f  /* light B, laps per second                     */
#define UI_GLOW_EDGE        0.10f   /* gradient magnitude where an edge starts      */
#define UI_GLOW_FALLOFF_A   0.075f  /* light A pool width, in UV squared (bigger = wider) */
#define UI_GLOW_FALLOFF_B   0.220f  /* light B pool width, in UV squared            */
#define UI_GLOW_SAMPLE_PX   1.45f   /* neighbour distance for the normal, in pixels */

/* Why the gate is the gradient and not alpha: the card glint (src/ui_board.c,
   BEZEL_SHINE_BODY) may only light where the texture is opaque, because a card
   bezel is transparent outside its metal. This frame is opaque everywhere, so that
   test would pass every pixel and light the whole screen. See UI_GLOW.md section 8. */
#define UI_GLOW_BODY(TEX_FN, OUT_FRAG) \
    "float bval(vec3 v) { return dot(v, vec3(0.299, 0.587, 0.114)); }\n" \
    "void main() {\n" \
    "    vec2 o = u_phase.zw;\n" \
    "    float hc = bval(" TEX_FN "(texture0, fragTexCoord).rgb);\n" \
    "    float hl = bval(" TEX_FN "(texture0, fragTexCoord - vec2(o.x, 0.0)).rgb);\n" \
    "    float hr = bval(" TEX_FN "(texture0, fragTexCoord + vec2(o.x, 0.0)).rgb);\n" \
    "    float hu = bval(" TEX_FN "(texture0, fragTexCoord - vec2(0.0, o.y)).rgb);\n" \
    "    float hd = bval(" TEX_FN "(texture0, fragTexCoord + vec2(0.0, o.y)).rgb);\n" \
    "    float peak = max(max(max(hl, hr), max(hu, hd)), 0.06);\n" \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n" \
    "    float edge = smoothstep(u_glow.y, u_glow.y + 0.30, length(g));\n" \
    "    if (edge <= 0.001) discard;\n" \
    "    if (hc < 0.02) discard;\n" \
    "    vec3 n = normalize(vec3(g * 2.2, 1.0));\n" \
    "    vec2 t1 = (vec2(0.5 + 0.40 * cos(u_phase.x), 0.5 + 0.38 * sin(u_phase.x)) - fragTexCoord) * vec2(0.80, 1.0);\n" \
    "    vec3 h1 = normalize(normalize(vec3(t1, 0.35)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd1 = max(dot(n, h1), 0.0);\n" \
    "    vec2 t2 = (vec2(0.5 + 0.40 * cos(u_phase.y), 0.5 + 0.38 * sin(u_phase.y * 2.0)) - fragTexCoord) * vec2(0.80, 1.0);\n" \
    "    vec3 h2 = normalize(normalize(vec3(t2, 0.35)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd2 = max(dot(n, h2), 0.0);\n" \
    "    float core = exp(-dot(t1, t1) / u_glow.z) * (0.20 + 0.80 * pow(nd1, 3.0))\n" \
    "               + exp(-dot(t2, t2) / u_glow.w) * (0.20 + 0.80 * pow(nd2, 3.0));\n" \
    "    float raw = edge * core * u_glow.x;\n" \
    "    float a = 0.55 * (1.0 - exp(-raw * 2.2));\n" \
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

void ui_glow_draw_ui_pass(Texture2D frame, Rectangle dest) {
    if (UI_GLOW_INTENSITY <= 0.0f) return;
    if (frame.id <= 0 || frame.width <= 0 || frame.height <= 0) return;
    if (dest.width <= 1.0f || dest.height <= 1.0f) return;
    if (s_state == 0) ui_glow_init();
    if (s_state < 0) return;

    const float t = (float)GetTime();
    const float two_pi = 6.283185307f;
    float phase[4] = {
        t * UI_GLOW_SPEED_A * two_pi,
        t * UI_GLOW_SPEED_B * two_pi + 1.7f,
        UI_GLOW_SAMPLE_PX / (float)frame.width,
        UI_GLOW_SAMPLE_PX / (float)frame.height
    };
    float tune[4] = { UI_GLOW_INTENSITY, UI_GLOW_EDGE, UI_GLOW_FALLOFF_A, UI_GLOW_FALLOFF_B };
    if (s_phase_loc >= 0) SetShaderValue(s_shader, s_phase_loc, phase, SHADER_UNIFORM_VEC4);
    if (s_glow_loc >= 0)  SetShaderValue(s_shader, s_glow_loc, tune, SHADER_UNIFORM_VEC4);

    /* The colour is the tint, exactly as the card glint does it: the shader emits
       fragColor.rgb untouched and only decides where light lands. */
    const Color base = g_theme.base;
    const Color tint = { base.r, base.g, base.b, 255 };

    /* Same vertical flip the canvas blit uses in ui_main.c, or the light lands
       upside down relative to the screen it is lighting. */
    const Rectangle src = { 0.0f, 0.0f, (float)frame.width, -(float)frame.height };

    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexturePro(frame, src, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
    EndShaderMode();
    EndBlendMode();
}
