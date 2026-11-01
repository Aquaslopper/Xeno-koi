#include "ui_table_glow.h"
#include "ui_theme.h"
#include "rlgl.h"
#include <math.h>

/* Independent table-only tuning. Shader relief/highlight constants are below. */
#define TABLE_GLOW_SPEED_A 0.52f
#define TABLE_GLOW_SPEED_B -0.37f
#define TABLE_GLOW_IDLE 0.36f
#define TABLE_GLOW_THEME_MIX 0.40f /* 0 = white light, 1 = full terminal color */
#define TABLE_GLOW_MUSIC_GAIN 0.64f
#define TABLE_GLOW_LEVEL_GAIN 3.0f
#define TABLE_GLOW_ATTACK 0.04f
#define TABLE_GLOW_RELEASE 0.30f
#define TABLE_GLOW_BLOOM_GAIN 0.65f /* additive halo strength, 0 disables */
#define TABLE_GLOW_BLOOM_RADIUS 1.5f /* blur spacing in quarter-resolution pixels */
#define TABLE_GLOW_BLOOM_W (CANVAS_W / 4)
#define TABLE_GLOW_BLOOM_H (CANVAS_H / 4)

static Shader s_shader;
static Shader s_blur_shader;
static RenderTexture2D s_bloom[2];
static int s_blur_step_loc = -1;
static int s_phase_loc = -1;
static int s_attempted;
static float s_phase_a, s_phase_b, s_level;
static unsigned int s_music_peak;
static int s_tap_attached;

/* Read the final mix (music plus SFX), without changing the audio engine.
   Raylib mixed processors receive interleaved stereo float device frames.
   Observe only: never alter samples or allocate on the audio thread. */
static void table_glow_music_tap(void *buffer, unsigned int frames) {
    const float *pcm = buffer;
    float peak = 0.0f;
    for (unsigned int i = 0; i < frames * 2; ++i) {
        float v = fabsf(pcm[i]);
        if (v > peak) peak = v;
    }
    unsigned int value = (unsigned int)(fminf(peak, 1.0f) * 1000000.0f);
    unsigned int old = __atomic_load_n(&s_music_peak, __ATOMIC_RELAXED);
    while (value > old && !__atomic_compare_exchange_n(&s_music_peak, &old, value,
           0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}

#define TABLE_GLOW_BODY(TEX_FN, OUT_FRAG) \
    "float bval(vec3 v) { return 0.45 * dot(v, vec3(0.299, 0.587, 0.114)) + 0.55 * max(max(v.r, v.g), v.b); }\n" \
    "void main() {\n" \
    "    vec4 c = " TEX_FN "(texture0, fragTexCoord);\n" \
    "    vec4 cl = " TEX_FN "(texture0, fragTexCoord - vec2(u_phase.z, 0.0));\n" \
    "    vec4 cr = " TEX_FN "(texture0, fragTexCoord + vec2(u_phase.z, 0.0));\n" \
    "    vec4 cu = " TEX_FN "(texture0, fragTexCoord - vec2(0.0, u_phase.w));\n" \
    "    vec4 cd = " TEX_FN "(texture0, fragTexCoord + vec2(0.0, u_phase.w));\n" \
    "    float diag_a = min(min(" TEX_FN "(texture0, fragTexCoord - u_phase.zw).a,\n" \
    "                           " TEX_FN "(texture0, fragTexCoord + vec2(u_phase.z, -u_phase.w)).a),\n" \
    "                       min(" TEX_FN "(texture0, fragTexCoord + vec2(-u_phase.z, u_phase.w)).a,\n" \
    "                           " TEX_FN "(texture0, fragTexCoord + u_phase.zw).a));\n" \
    "    float min8 = min(min(min(cl.a, cr.a), min(cu.a, cd.a)), diag_a);\n" \
    "    float solid = smoothstep(0.91, 0.98, c.a) * smoothstep(0.85, 0.95, min8);\n" \
    "    float hc = bval(c.rgb);\n" \
    "    float hl = bval(cl.rgb), hr = bval(cr.rgb), hu = bval(cu.rgb), hd = bval(cd.rgb);\n" \
    "    float peak = max(max(max(hl, hr), max(hu, hd)), 0.12);\n" \
    "    float mask = solid * smoothstep(0.08, 0.18, hc) * smoothstep(0.42, 0.72, hc / peak);\n" \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n" \
    "    vec3 n = normalize(vec3(g * 2.8, 1.0));\n" \
    "    vec2 t1 = (vec2(0.5 + 0.44 * cos(u_phase.x), 0.5 + 0.45 * sin(u_phase.x)) - fragTexCoord) * vec2(0.70, 1.0);\n" \
    "    float d1 = dot(t1, t1);\n" \
    "    vec3 h1 = normalize(normalize(vec3(t1, 0.30)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd1 = max(dot(n, h1), 0.0);\n" \
    "    vec2 t2 = (vec2(0.5 + 0.44 * cos(u_phase.y), 0.5 + 0.45 * sin(u_phase.y * 2.0)) - fragTexCoord) * vec2(0.70, 1.0);\n" \
    "    float d2 = dot(t2, t2);\n" \
    "    vec3 h2 = normalize(normalize(vec3(t2, 0.30)) + vec3(0.0, 0.0, 1.0));\n" \
    "    float nd2 = max(dot(n, h2), 0.0);\n" \
    "    float str = fragColor.a * colDiffuse.a;\n" \
    "    float s1 = 0.30 + 0.70 * pow(nd1, 4.5);\n" \
    "    float s2 = 0.30 + 0.70 * pow(nd2, 4.5);\n" \
    "    float core = mask * (exp(-d1 / 0.040) * s1 + exp(-d2 / 0.085) * s2) * 2.50;\n" \
    "    float bloom = mask * (exp(-d1 / 0.038) * (0.35 + 0.65 * nd1 * nd1) + exp(-d2 / 0.028) * (0.35 + 0.65 * nd2 * nd2)) * (0.50 + 0.65 * str);\n" \
    "    float raw = (core + bloom * 0.60) * str;\n" \
    "    float a = 0.88 * (1.0 - exp(-raw * 1.65));\n" \
    "    " OUT_FRAG " = vec4(fragColor.rgb, a);\n" \
    "}\n"

#ifdef __EMSCRIPTEN__
static const char *const k_table_glow_fs =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    TABLE_GLOW_BODY("texture2D", "gl_FragColor");
#else
static const char *const k_table_glow_fs =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "out vec4 finalColor;\n"
    TABLE_GLOW_BODY("texture", "finalColor");
#endif
#undef TABLE_GLOW_BODY

/* Separable five-fetch Gaussian blur; desktop and browser share the kernel.
   The source contains light energy on opaque black, not the table artwork. */
#ifdef __EMSCRIPTEN__
#define BLOOM_SAMPLE "texture2D"
#define BLOOM_OUTPUT "gl_FragColor"
#define BLOOM_INTERFACE "#version 100\nprecision mediump float;\nvarying vec2 fragTexCoord;\n"
#else
#define BLOOM_SAMPLE "texture"
#define BLOOM_OUTPUT "finalColor"
#define BLOOM_INTERFACE "#version 330\nin vec2 fragTexCoord;\nout vec4 finalColor;\n"
#endif
static const char *const k_blur_fs = BLOOM_INTERFACE
    "uniform sampler2D texture0;\nuniform vec2 u_step;\n"
    "void main() {\n"
    "vec3 c = " BLOOM_SAMPLE "(texture0, fragTexCoord).rgb * 0.227027;\n"
    "c += (" BLOOM_SAMPLE "(texture0, fragTexCoord + u_step * 1.384615).rgb + "
              BLOOM_SAMPLE "(texture0, fragTexCoord - u_step * 1.384615).rgb) * 0.316216;\n"
    "c += (" BLOOM_SAMPLE "(texture0, fragTexCoord + u_step * 3.230769).rgb + "
              BLOOM_SAMPLE "(texture0, fragTexCoord - u_step * 3.230769).rgb) * 0.070270;\n"
    BLOOM_OUTPUT " = vec4(c, 1.0);\n}\n";
#undef BLOOM_SAMPLE
#undef BLOOM_OUTPUT
#undef BLOOM_INTERFACE

static void table_glow_bloom(Texture2D overlay, Color tint) {
    if (TABLE_GLOW_BLOOM_GAIN <= 0.0f || s_blur_step_loc < 0 ||
        s_bloom[0].id == 0 || s_bloom[1].id == 0) return;
    /* The board is already inside its 1280x720 canvas. Raylib's texture modes
       do not nest automatically: explicitly restore that framebuffer afterwards. */
    unsigned int parent = rlGetActiveFramebuffer();
    BeginTextureMode(s_bloom[0]);
    ClearBackground(BLACK);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexturePro(overlay, (Rectangle){0, 0, overlay.width, overlay.height},
        (Rectangle){0, 0, TABLE_GLOW_BLOOM_W, TABLE_GLOW_BLOOM_H},
        (Vector2){0, 0}, 0, tint);
    EndShaderMode();
    EndBlendMode();
    for (int pass = 0; pass < 2; ++pass) {
        int source = pass == 0 ? 0 : 1, dest = 1 - source;
        BeginTextureMode(s_bloom[dest]);
        ClearBackground(BLACK);
        float step[2] = {pass == 0 ? TABLE_GLOW_BLOOM_RADIUS / TABLE_GLOW_BLOOM_W : 0,
                         pass == 1 ? TABLE_GLOW_BLOOM_RADIUS / TABLE_GLOW_BLOOM_H : 0};
        SetShaderValue(s_blur_shader, s_blur_step_loc, step, SHADER_UNIFORM_VEC2);
        BeginShaderMode(s_blur_shader);
        DrawTextureRec(s_bloom[source].texture,
            (Rectangle){0, 0, TABLE_GLOW_BLOOM_W, -TABLE_GLOW_BLOOM_H},
            (Vector2){0, 0}, WHITE);
        EndShaderMode();
    }
    EndTextureMode();
    if (parent != 0) {
        RenderTexture2D canvas = {0};
        canvas.id = parent;
        canvas.texture.width = CANVAS_W;
        canvas.texture.height = CANVAS_H;
        BeginTextureMode(canvas);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTexturePro(s_bloom[0].texture,
        (Rectangle){0, 0, TABLE_GLOW_BLOOM_W, -TABLE_GLOW_BLOOM_H},
        (Rectangle){0, 0, CANVAS_W, CANVAS_H}, (Vector2){0, 0}, 0,
        (Color){255, 255, 255, (unsigned char)(255 * TABLE_GLOW_BLOOM_GAIN)});
    EndBlendMode();
}

void table_glow_draw(Texture2D overlay, float dt) {
    if (overlay.id <= 0 || overlay.width <= 0 || overlay.height <= 0) return;
    if (!s_tap_attached && IsAudioDeviceReady()) {
        AttachAudioMixedProcessor(table_glow_music_tap);
        s_tap_attached = 1;
    }
    if (!s_attempted) {
        s_attempted = 1;
        s_shader = LoadShaderFromMemory(0, k_table_glow_fs);
        if (s_shader.id > 0) s_phase_loc = GetShaderLocation(s_shader, "u_phase");
        s_blur_shader = LoadShaderFromMemory(0, k_blur_fs);
        if (s_blur_shader.id > 0)
            s_blur_step_loc = GetShaderLocation(s_blur_shader, "u_step");
        for (int i = 0; i < 2; ++i) {
            s_bloom[i] = LoadRenderTexture(TABLE_GLOW_BLOOM_W, TABLE_GLOW_BLOOM_H);
            if (s_bloom[i].id > 0) {
                SetTextureFilter(s_bloom[i].texture, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(s_bloom[i].texture, TEXTURE_WRAP_CLAMP);
            }
        }
    }
    if (s_shader.id <= 0 || s_phase_loc < 0) return;
    dt = fminf(fmaxf(dt, 0.0f), 0.1f);
    float target = __atomic_exchange_n(&s_music_peak, 0, __ATOMIC_RELAXED) / 1000000.0f;
    target = fminf(1.0f, target * TABLE_GLOW_LEVEL_GAIN);
    float tau = target > s_level ? TABLE_GLOW_ATTACK : TABLE_GLOW_RELEASE;
    s_level += (target - s_level) * (1.0f - expf(-dt / tau));
    s_phase_a = fmodf(s_phase_a + dt * TABLE_GLOW_SPEED_A, 6.2831853f);
    s_phase_b = fmodf(s_phase_b + dt * TABLE_GLOW_SPEED_B, 6.2831853f);
    float phase[4] = { s_phase_a, s_phase_b,
                      1.45f / overlay.width, 1.45f / overlay.height };
    SetShaderValue(s_shader, s_phase_loc, phase, SHADER_UNIFORM_VEC4);
    float strength = TABLE_GLOW_IDLE + TABLE_GLOW_MUSIC_GAIN * s_level;
    Color theme = COLOR_PHOSPHOR;
    Color tint = {
        (unsigned char)(255.0f + (theme.r - 255.0f) * TABLE_GLOW_THEME_MIX),
        (unsigned char)(255.0f + (theme.g - 255.0f) * TABLE_GLOW_THEME_MIX),
        (unsigned char)(255.0f + (theme.b - 255.0f) * TABLE_GLOW_THEME_MIX),
        (unsigned char)(255 * strength)
    };
    table_glow_bloom(overlay, tint);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexture(overlay, 0, 0, tint);
    EndShaderMode();
    EndBlendMode();
}

void table_glow_unload(void) {
    if (s_tap_attached && IsAudioDeviceReady())
        DetachAudioMixedProcessor(table_glow_music_tap);
    s_tap_attached = 0;
    for (int i = 0; i < 2; ++i) {
        if (s_bloom[i].id > 0) UnloadRenderTexture(s_bloom[i]);
        s_bloom[i] = (RenderTexture2D){0};
    }
    if (s_blur_shader.id > 0) UnloadShader(s_blur_shader);
    s_blur_shader = (Shader){0};
    s_blur_step_loc = -1;
    if (s_shader.id > 0) UnloadShader(s_shader);
    s_shader = (Shader){0};
    s_phase_loc = -1;
    s_attempted = 0;
    s_phase_a = s_phase_b = s_level = 0.0f;
    __atomic_store_n(&s_music_peak, 0, __ATOMIC_RELAXED);
}
