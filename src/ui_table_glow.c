#include "ui_table_glow.h"
#include "ui_theme.h"
#include "storage.h"
#include "rlgl.h"
#ifdef __EMSCRIPTEN__
#include <GLES2/gl2.h>
#endif
#include <math.h>
#include <stdio.h>

/* Independent table-only tuning. Shader relief/highlight constants are below. */
#define TABLE_GLOW_LIGHT_COUNT 12 /* 1..16 independent, smoothly roaming lights */
#define TABLE_GLOW_LIGHT_STRENGTH 1.12f
#define TABLE_GLOW_SIZE_A 0.008f /* alternate the two tuned footprint sizes */
#define TABLE_GLOW_SIZE_B 0.006f
#define TABLE_GLOW_HALO_SIZE_A 0.09f
#define TABLE_GLOW_HALO_SIZE_B 0.034f
/* Normalized local relief: flat pixels emit no light. Raise to isolate ridges. */
#define TABLE_GLOW_RELIEF_MIN 0.015f
#define TABLE_GLOW_RELIEF_FULL 0.50f
#if TABLE_GLOW_LIGHT_COUNT < 1 || TABLE_GLOW_LIGHT_COUNT > 16
#error TABLE_GLOW_LIGHT_COUNT must be between 1 and 16
#endif
#define GLOW_STRING_INNER(x) #x
#define GLOW_STRING(x) GLOW_STRING_INNER(x)
#define TABLE_GLOW_SPEED_A 0.33f
#define TABLE_GLOW_SPEED_B -0.25f
#define TABLE_GLOW_IDLE 0.21f
#define TABLE_GLOW_THEME_MIX 0.60f /* 0 = white light, 1 = full terminal color */
#define TABLE_GLOW_MUSIC_GAIN 0.59f
#define TABLE_GLOW_LEVEL_GAIN 1.0f
#define TABLE_GLOW_ATTACK 0.021f
#define TABLE_GLOW_RELEASE 0.64f
/* Table meter only: these never affect sound or the Action Card meter. */
#define TABLE_GLOW_ADAPT_SPEED 1.08f /* >1 adapts faster, <1 more slowly; positive */
#define TABLE_GLOW_BEAT_CONTRAST 1.3f /* 1 = linear; >1 suppresses weaker accents */
#define TABLE_GLOW_BLOOM_GAIN 2.55f /* additive halo strength, 0 disables */
#define TABLE_GLOW_BLOOM_RADIUS 1.1f /* blur spacing in quarter-resolution pixels */
#define TABLE_GLOW_BLOOM_W (CANVAS_W / 4)
#define TABLE_GLOW_BLOOM_H (CANVAS_H / 4)

/* Brightness cutoff per named table, before theme tint and music strength.
   0..1: below cutoff stays unlit; the next 0.10 fades smoothly into light.
   Castle 0.30 is a first-pass value for owner review; Realm stays unchanged.
   The selected normal/CRT artwork uses the same table-specific setting. */
static const float k_table_dark_cutoff[TABLE_DESIGN_COUNT] = {
    [TABLE_DESIGN_CRT]     = 0.28f, /* no overlay; not drawn */
    [TABLE_DESIGN_CITY]    = 0.15f,
    [TABLE_DESIGN_CASTLE]  = 0.30f,
    [TABLE_DESIGN_HOLE]    = 0.30f,
    [TABLE_DESIGN_ARCTIC]  = 0.25f,
    [TABLE_DESIGN_HARVEST] = 0.25f,
    [TABLE_DESIGN_FACE]    = 0.15f,
    [TABLE_DESIGN_REALM]   = 0.15f
};

static Shader s_shader;
static Shader s_blur_shader;
static RenderTexture2D s_bloom[2];
static int s_blur_step_loc = -1, s_blur_gain_loc = -1;
static int s_phase_loc = -1;
static int s_light_locs[TABLE_GLOW_LIGHT_COUNT];
static int s_shape_loc = -1, s_relief_loc = -1;
static int s_cutoff_loc = -1;
static int s_attempted;
static double s_light_started = -1.0;
static float s_level;
static unsigned int s_music_meter_raw, s_music_meter_epoch;
static int s_music_tap_attached;

/* Independent copy of the established Action Card read-only meter pattern.
   Attached from UI after the existing music DSP; no samples are modified. */
static void table_music_meter_processor(void *buffer, unsigned int frames) {
    const float *pcm = (const float *)buffer;
    if (!pcm || frames == 0) return;
    static float fast_ms = 0.0f, lo_rms = 0.12f, hi_rms = 0.24f;
    static unsigned int last_ep = 0;
    unsigned int ep = __atomic_load_n(&s_music_meter_epoch, __ATOMIC_RELAXED);
    if (ep != last_ep) { fast_ms = 0.0f; lo_rms = 0.12f; hi_rms = 0.24f; last_ep = ep; }
    for (unsigned int f = 0; f < frames; f++) {
        float l = pcm[f * 2 + 0], r = pcm[f * 2 + 1], p2 = 0.5f * (l * l + r * r);
        if (!(p2 >= 0.0f) || p2 > 4.0f) p2 = 0.0f;
        fast_ms += (p2 - fast_ms) * 0.00085f;
    }
    if (fast_ms < 1e-12f) fast_ms = 0.0f;
    float rms = sqrtf(fast_ms), c_sec = (float)frames / 44100.0f;
    /* Scale the loud/quiet reference tracking, not the visual attack/release. */
    c_sec *= fmaxf(0.01f, TABLE_GLOW_ADAPT_SPEED);
    if (rms > hi_rms) hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.05f));
    else              hi_rms += (rms - hi_rms) * (1.0f - expf(-c_sec / 0.90f));
    if (rms < lo_rms) lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.06f));
    else              lo_rms += (rms - lo_rms) * (1.0f - expf(-c_sec / 0.90f));
    float gate = (hi_rms > 0.015f) ? fminf(1.0f, (hi_rms - 0.015f) / 0.075f) : 0.0f;
    float floor_r = lo_rms * 0.88f, span = fmaxf(0.025f, hi_rms - floor_r);
    float beat = fminf(1.0f, fmaxf(0.0f, (rms - floor_r) / span));
    float norm = gate * (0.08f + 0.92f * powf(beat, fmaxf(0.01f, TABLE_GLOW_BEAT_CONTRAST)));
    __atomic_store_n(&s_music_meter_raw, (unsigned int)(norm * 1000000.0f), __ATOMIC_RELAXED);
}


void table_glow_detach_music(AudioAssets *audio) {
    if (audio && s_music_tap_attached) {
        for (int i = 0; i < AUDIO_MAX_TRACKS; ++i)
            if (audio->has_music_game[i])
                DetachAudioStreamProcessor(audio->music_game[i].stream, table_music_meter_processor);
    }
    s_music_tap_attached = 0;
    __atomic_add_fetch(&s_music_meter_epoch, 1u, __ATOMIC_RELAXED);
    __atomic_store_n(&s_music_meter_raw, 0u, __ATOMIC_RELAXED);
    s_level = 0.0f;
}

void table_glow_sync_music(AudioAssets *audio, int active, int paused,
                           float transition_alpha, float dt) {
    if (!active) {
        if (s_music_tap_attached) table_glow_detach_music(audio);
        return;
    }
    if (!audio) return;
    if (!s_music_tap_attached) {
        __atomic_add_fetch(&s_music_meter_epoch, 1u, __ATOMIC_RELAXED);
        __atomic_store_n(&s_music_meter_raw, 0u, __ATOMIC_RELAXED);
        for (int i = 0; i < AUDIO_MAX_TRACKS; ++i)
            if (audio->has_music_game[i])
                AttachAudioStreamProcessor(audio->music_game[i].stream, table_music_meter_processor);
        s_music_tap_attached = 1;
    }
    if (paused || dt <= 0.0f) return;
    float target = audio->current_music == 2
        ? __atomic_load_n(&s_music_meter_raw, __ATOMIC_RELAXED) / 1000000.0f : 0.0f;
    target = fminf(1.0f, target * TABLE_GLOW_LEVEL_GAIN) *
             fminf(1.0f, fmaxf(0.0f, 1.0f - transition_alpha));
    float tau = target > s_level ? TABLE_GLOW_ATTACK : TABLE_GLOW_RELEASE;
    s_level += (target - s_level) * (1.0f - expf(-fminf(dt, 0.1f) / tau));
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
    "    float mask = solid * smoothstep(u_dark_cutoff, u_dark_cutoff + 0.10, hc) * smoothstep(0.42, 0.72, hc / peak);\n" \
    "    vec2 g = vec2(hl - hr, hu - hd) / peak;\n" \
    "    float detail = max(length(g), max(max(abs(hc - hl), abs(hc - hr)), max(abs(hc - hu), abs(hc - hd))) / peak);\n" \
    "    mask *= smoothstep(u_relief.x, u_relief.y, detail);\n" \
    "    vec3 n = normalize(vec3(g * 2.8, 1.0));\n" \
    "    float str = fragColor.a * colDiffuse.a;\n" \
    "    float core = 0.0, bloom = 0.0;\n" \
    "    for (int i = 0; i < " GLOW_STRING(TABLE_GLOW_LIGHT_COUNT) "; ++i) {\n" \
    "        vec2 t = (u_lights[i] - fragTexCoord) * vec2(0.70, 1.0);\n" \
    "        float d = dot(t, t);\n" \
    "        vec3 h = normalize(normalize(vec3(t, 0.30)) + vec3(0.0, 0.0, 1.0));\n" \
    "        float nd = max(dot(n, h), 0.0);\n" \
    "        float odd = mod(float(i), 2.0);\n" \
    "        float size = mix(u_shape.x, u_shape.y, odd);\n" \
    "        float halo = mix(u_shape.z, u_shape.w, odd);\n" \
    "        core = max(core, exp(-d / size) * (0.30 + 0.70 * pow(nd, 4.5)));\n" \
    "        bloom = max(bloom, exp(-d / halo) * (0.35 + 0.65 * nd * nd));\n" \
    "    }\n" \
    "    core *= mask * 2.50;\n" \
    "    bloom *= mask * (0.50 + 0.65 * str);\n" \
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
    "uniform vec2 u_lights[" GLOW_STRING(TABLE_GLOW_LIGHT_COUNT) "];\n"
    "uniform vec4 u_shape;\n"
    "uniform vec2 u_relief;\n"
    "uniform float u_dark_cutoff;\n"
    TABLE_GLOW_BODY("texture2D", "gl_FragColor");
#else
static const char *const k_table_glow_fs =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec4 u_phase;\n"
    "uniform vec2 u_lights[" GLOW_STRING(TABLE_GLOW_LIGHT_COUNT) "];\n"
    "uniform vec4 u_shape;\n"
    "uniform vec2 u_relief;\n"
    "uniform float u_dark_cutoff;\n"
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
    "uniform sampler2D texture0;\nuniform vec2 u_step;\nuniform float u_gain;\n"
    "void main() {\n"
    "vec3 c = " BLOOM_SAMPLE "(texture0, fragTexCoord).rgb * 0.227027;\n"
    "c += (" BLOOM_SAMPLE "(texture0, fragTexCoord + u_step * 1.384615).rgb + "
              BLOOM_SAMPLE "(texture0, fragTexCoord - u_step * 1.384615).rgb) * 0.316216;\n"
    "c += (" BLOOM_SAMPLE "(texture0, fragTexCoord + u_step * 3.230769).rgb + "
              BLOOM_SAMPLE "(texture0, fragTexCoord - u_step * 3.230769).rgb) * 0.070270;\n"
    BLOOM_OUTPUT " = vec4(c * u_gain, 1.0);\n}\n";
#undef BLOOM_SAMPLE
#undef BLOOM_OUTPUT
#undef BLOOM_INTERFACE

static void table_glow_bloom(Texture2D overlay, Color tint) {
    if (TABLE_GLOW_BLOOM_GAIN <= 0.0f || s_blur_step_loc < 0 || s_blur_gain_loc < 0 ||
        s_bloom[0].id == 0 || s_bloom[1].id == 0) return;
    /* The board is already inside its 1280x720 canvas. Raylib's texture modes
       do not nest automatically: explicitly restore that framebuffer afterwards. */
#ifdef __EMSCRIPTEN__
    /* raylib 5.5's rlGetActiveFramebuffer() always returns 0 on GLES2/WebGL1. */
    GLint parent_binding = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &parent_binding);
    unsigned int parent = (unsigned int)parent_binding;
#else
    unsigned int parent = rlGetActiveFramebuffer();
#endif
    BeginTextureMode(s_bloom[0]);
    ClearBackground(BLACK);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_shader);
    DrawTexturePro(overlay, (Rectangle){0, 0, overlay.width, overlay.height},
        (Rectangle){0, 0, TABLE_GLOW_BLOOM_W, TABLE_GLOW_BLOOM_H},
        (Vector2){0, 0}, 0, tint);
    EndShaderMode();
    EndBlendMode();
    float unity = 1.0f;
    SetShaderValue(s_blur_shader, s_blur_gain_loc, &unity, SHADER_UNIFORM_FLOAT);
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
    /* Gain is a float in the compositor, not an overflowing 8-bit alpha.
       A zero blur step makes this an identity sample with the requested gain. */
    float zero_step[2] = {0, 0}, gain = fmaxf(0.0f, TABLE_GLOW_BLOOM_GAIN);
    SetShaderValue(s_blur_shader, s_blur_step_loc, zero_step, SHADER_UNIFORM_VEC2);
    SetShaderValue(s_blur_shader, s_blur_gain_loc, &gain, SHADER_UNIFORM_FLOAT);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginShaderMode(s_blur_shader);
    DrawTexturePro(s_bloom[0].texture,
        (Rectangle){0, 0, TABLE_GLOW_BLOOM_W, -TABLE_GLOW_BLOOM_H},
        (Rectangle){0, 0, CANVAS_W, CANVAS_H}, (Vector2){0, 0}, 0,
        WHITE);
    EndShaderMode();
    EndBlendMode();
}

void table_glow_draw(Texture2D overlay, float dt, int table_design) {
    if (overlay.id <= 0 || overlay.width <= 0 || overlay.height <= 0) return;
    if (!s_attempted) {
        s_attempted = 1;
        s_shader = LoadShaderFromMemory(0, k_table_glow_fs);
        if (s_shader.id > 0) {
            s_phase_loc = GetShaderLocation(s_shader, "u_phase");
            for (int i = 0; i < TABLE_GLOW_LIGHT_COUNT; ++i) {
                char name[32];
                snprintf(name, sizeof(name), "u_lights[%d]", i);
                s_light_locs[i] = GetShaderLocation(s_shader, name);
            }
            s_shape_loc = GetShaderLocation(s_shader, "u_shape");
            s_relief_loc = GetShaderLocation(s_shader, "u_relief");
            s_cutoff_loc = GetShaderLocation(s_shader, "u_dark_cutoff");
        }
        s_blur_shader = LoadShaderFromMemory(0, k_blur_fs);
        if (s_blur_shader.id > 0) {
            s_blur_step_loc = GetShaderLocation(s_blur_shader, "u_step");
            s_blur_gain_loc = GetShaderLocation(s_blur_shader, "u_gain");
        }
        for (int i = 0; i < 2; ++i) {
            s_bloom[i] = LoadRenderTexture(TABLE_GLOW_BLOOM_W, TABLE_GLOW_BLOOM_H);
            if (s_bloom[i].id > 0) {
                SetTextureFilter(s_bloom[i].texture, TEXTURE_FILTER_BILINEAR);
                SetTextureWrap(s_bloom[i].texture, TEXTURE_WRAP_CLAMP);
            }
        }
    }
    if (s_shader.id <= 0 || s_phase_loc < 0 || s_cutoff_loc < 0 || s_shape_loc < 0 || s_relief_loc < 0) return;
    if (table_design < 0 || table_design >= TABLE_DESIGN_COUNT) return;
    SetShaderValue(s_shader, s_cutoff_loc, &k_table_dark_cutoff[table_design],
                   SHADER_UNIFORM_FLOAT);
    (void)dt; /* Visual motion follows elapsed time, not gameplay/frame-delta gates. */
    /* Compute paths on CPU in double precision: no random jumps or shader-clock
       precision loss. Different frequencies/phases keep panels from moving in sync. */
    double now = GetTime();
    if (s_light_started < 0.0) s_light_started = now;
    double elapsed = fmax(0.0, now - s_light_started);
    float positions[TABLE_GLOW_LIGHT_COUNT][2];
    for (int i = 0; i < TABLE_GLOW_LIGHT_COUNT; ++i) {
        double seed = i * 2.399963229728653;
        double x = elapsed * TABLE_GLOW_SPEED_A * (1.0 + i * 0.137) + seed;
        double y = elapsed * TABLE_GLOW_SPEED_B * (1.0 + i * 0.173) + seed * 1.71;
        positions[i][0] = (float)(0.5 + 0.44 * sin(x));
        positions[i][1] = (float)(0.5 + 0.50 * sin(y));
    }
    float phase[4] = {0, 0, 1.45f / overlay.width, 1.45f / overlay.height};
    float shape[4] = {TABLE_GLOW_SIZE_A, TABLE_GLOW_SIZE_B,
                      TABLE_GLOW_HALO_SIZE_A, TABLE_GLOW_HALO_SIZE_B};
    SetShaderValue(s_shader, s_phase_loc, phase, SHADER_UNIFORM_VEC4);
    /* Explicit element locations avoid relying on array-base lookup/upload behavior
       across desktop GL and WebGL. Every light is updated on every draw. */
    for (int i = 0; i < TABLE_GLOW_LIGHT_COUNT; ++i)
        if (s_light_locs[i] >= 0)
            SetShaderValue(s_shader, s_light_locs[i], positions[i], SHADER_UNIFORM_VEC2);
    SetShaderValue(s_shader, s_shape_loc, shape, SHADER_UNIFORM_VEC4);
    float relief[2] = {fmaxf(0.0f, TABLE_GLOW_RELIEF_MIN),
                      fmaxf(TABLE_GLOW_RELIEF_MIN + 0.0001f, TABLE_GLOW_RELIEF_FULL)};
    SetShaderValue(s_shader, s_relief_loc, relief, SHADER_UNIFORM_VEC2);
    /* Max-combine footprints in the shader, rather than adding ten light washes. */
    float strength = fminf(1.0f, fmaxf(0.0f,
        (TABLE_GLOW_IDLE + TABLE_GLOW_MUSIC_GAIN * s_level) * TABLE_GLOW_LIGHT_STRENGTH));
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
    for (int i = 0; i < 2; ++i) {
        if (s_bloom[i].id > 0) UnloadRenderTexture(s_bloom[i]);
        s_bloom[i] = (RenderTexture2D){0};
    }
    if (s_blur_shader.id > 0) UnloadShader(s_blur_shader);
    s_blur_shader = (Shader){0};
    s_blur_step_loc = s_blur_gain_loc = -1;
    if (s_shader.id > 0) UnloadShader(s_shader);
    s_shader = (Shader){0};
    s_phase_loc = s_cutoff_loc = s_shape_loc = s_relief_loc = -1;
    for (int i = 0; i < TABLE_GLOW_LIGHT_COUNT; ++i) s_light_locs[i] = -1;
    s_attempted = 0;
    s_light_started = -1.0;
    s_level = 0.0f;
    __atomic_store_n(&s_music_meter_raw, 0u, __ATOMIC_RELAXED);
}
