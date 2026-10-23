/* Headless end-to-end verification + optional software-rasterized frame strip
 * for the 360 ms 2-stage CRT hub transition (200 ms wild button strobe +
 * 160 ms top-to-bottom CRT phosphor scan-beam wipe) in src/ui_main.c.
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__has_include) && __has_include("external/stb_image.h") && \
    __has_include("external/stb_truetype.h") && __has_include("external/stb_image_write.h")
#define HAVE_RAYLIB_STB 1
#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "external/stb_image.h"
#include "external/stb_truetype.h"
#include "external/stb_image_write.h"
#else
#define HAVE_RAYLIB_STB 0
#endif

#include "raylib.h"

/* ---------------- Software OpenGL-equivalent FBO + Font Rasterizer -------- */

#define SW_MAX_TEX 32
typedef struct {
    int used, w, h, fbo_flip; /* fbo_flip=1 mimics OpenGL FBO bottom-left UV */
    uint8_t *rgba;            /* always stored top-down in memory: y=0 is top */
} SwTex;

static SwTex g_tex[SW_MAX_TEX];
static int g_active_rt = 0; /* 0 = window backbuffer, >0 = RenderTexture id */
static int g_scissor_on = 0, g_sc_x = 0, g_sc_y = 0, g_sc_w = 1280, g_sc_h = 720;
static int g_win_w = 1280, g_win_h = 720;
static float g_dt = 1.0f / 60.0f;
static double g_time_s = 1.0;
static Vector2 g_mouse = { -500.0f, -500.0f };
static int g_mouse_pressed = 0, g_mouse_down = 0;
static char g_repo_root[512] = ".";

/* Frame telemetry recorded per UpdateDrawFrame() call */
static int g_overlay_rt_id = -1;
static int g_rec_scissor_calls = 0;
static int g_rec_last_split_y = -1;
static int g_rec_overlay_draws = 0;

#if HAVE_RAYLIB_STB
typedef struct {
    int loaded;
    stbtt_fontinfo info;
    unsigned char *ttf_buf;
} SwFont;
static SwFont g_fonts[8];
#endif

static int alloc_sw_tex(int w, int h, int fbo_flip) {
    for (int i = 1; i < SW_MAX_TEX; i++) {
        if (!g_tex[i].used) {
            g_tex[i].used = 1;
            g_tex[i].w = w;
            g_tex[i].h = h;
            g_tex[i].fbo_flip = fbo_flip;
            g_tex[i].rgba = (uint8_t *)calloc((size_t)w * (size_t)h * 4u, 1);
            return i;
        }
    }
    return 0;
}

static SwTex *cur_target(void) {
    if (g_active_rt > 0 && g_active_rt < SW_MAX_TEX && g_tex[g_active_rt].used)
        return &g_tex[g_active_rt];
    return &g_tex[0];
}

static inline void blend_px(SwTex *t, int x, int y, Color c) {
    if (!t || !t->rgba || c.a == 0) return;
    if (x < 0 || y < 0 || x >= t->w || y >= t->h) return;
    if (g_scissor_on) {
        if (x < g_sc_x || y < g_sc_y || x >= g_sc_x + g_sc_w || y >= g_sc_y + g_sc_h)
            return;
    }
    uint8_t *d = &t->rgba[((size_t)y * (size_t)t->w + (size_t)x) * 4u];
    if (c.a == 255) {
        d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = 255;
    } else {
        uint32_t a = c.a, ia = 255u - a;
        d[0] = (uint8_t)((c.r * a + d[0] * ia + 127u) / 255u);
        d[1] = (uint8_t)((c.g * a + d[1] * ia + 127u) / 255u);
        d[2] = (uint8_t)((c.b * a + d[2] * ia + 127u) / 255u);
        d[3] = 255;
    }
}

static void resolve_asset_path(const char *fileName, char *out, size_t cap) {
    if (access(fileName, R_OK) == 0) {
        snprintf(out, cap, "%s", fileName);
    } else {
        snprintf(out, cap, "%s/%s", g_repo_root, fileName);
    }
}

/* ---------------- Raylib Window / Timing / Input / Audio Stubs ------------ */

void SetConfigFlags(unsigned int flags) { (void)flags; }
void InitWindow(int width, int height, const char *title) {
    (void)title;
    g_win_w = width; g_win_h = height;
    g_tex[0].used = 1; g_tex[0].w = width; g_tex[0].h = height;
    g_tex[0].fbo_flip = 0;
    g_tex[0].rgba = (uint8_t *)calloc((size_t)width * (size_t)height * 4u, 1);
}
void SetWindowMinSize(int width, int height) { (void)width; (void)height; }
void CloseWindow(void) {}
bool WindowShouldClose(void) { return false; }
void SetTargetFPS(int fps) { (void)fps; }
void SetExitKey(int key) { (void)key; }
int GetScreenWidth(void) { return g_win_w; }
int GetScreenHeight(void) { return g_win_h; }
bool IsWindowFullscreen(void) { return false; }
void ToggleFullscreen(void) {}
void ToggleBorderlessWindowed(void) {}
bool ChangeDirectory(const char *dir) { (void)dir; return true; }
bool IsWindowFocused(void) { return true; }
bool IsCursorOnScreen(void) { return true; }
float GetFrameTime(void) { g_time_s += g_dt; return g_dt; }
double GetTime(void) { return g_time_s; }

Vector2 GetMousePosition(void) { return g_mouse; }
Vector2 GetMouseDelta(void) { return (Vector2){ 0.0f, 0.0f }; }
bool IsMouseButtonPressed(int button) { return button == MOUSE_BUTTON_LEFT && g_mouse_pressed != 0; }
bool IsMouseButtonDown(int button) { return button == MOUSE_BUTTON_LEFT && g_mouse_down != 0; }
bool IsMouseButtonReleased(int button) { (void)button; return false; }
float GetMouseWheelMove(void) { return 0.0f; }
bool IsKeyPressed(int key) { (void)key; return false; }
bool IsKeyPressedRepeat(int key) { (void)key; return false; }
bool IsKeyDown(int key) { (void)key; return false; }
int GetCharPressed(void) { return 0; }
const char *GetClipboardText(void) { return ""; }

bool CheckCollisionPointRec(Vector2 point, Rectangle rec) {
    return (point.x >= rec.x) && (point.x < (rec.x + rec.width)) &&
           (point.y >= rec.y) && (point.y < (rec.y + rec.height));
}

bool FileExists(const char *fileName) {
    char p[768];
    resolve_asset_path(fileName, p, sizeof(p));
    return access(p, R_OK) == 0;
}
bool DirectoryExists(const char *dirPath) { return access(dirPath, R_OK) == 0; }
FilePathList LoadDirectoryFiles(const char *dirPath) {
    (void)dirPath;
    FilePathList l = { 0 };
    return l;
}
FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter, bool scanSubdirs) {
    (void)basePath; (void)filter; (void)scanSubdirs;
    FilePathList l = { 0 };
    return l;
}
void UnloadDirectoryFiles(FilePathList files) { (void)files; }
const char *GetFileName(const char *filePath) {
    const char *s = strrchr(filePath, '/');
    return s ? (s + 1) : filePath;
}
int MakeDirectory(const char *dirPath) { (void)dirPath; return 0; }
char *LoadFileText(const char *fileName) { (void)fileName; return NULL; }
void UnloadFileText(char *text) { free(text); }
bool SaveFileText(const char *fileName, char *text) { (void)fileName; (void)text; return true; }
unsigned int TextLength(const char *text) { return text ? (unsigned int)strlen(text) : 0u; }
int GetCodepointNext(const char *text, int *codepointSize) {
    if (!text || !text[0]) { if (codepointSize) *codepointSize = 0; return 0; }
    if (codepointSize) *codepointSize = 1;
    return (unsigned char)text[0];
}
const char *CodepointToUTF8(int codepoint, int *utf8Size) {
    static char c[8];
    c[0] = (char)codepoint; c[1] = '\0';
    if (utf8Size) *utf8Size = 1;
    return c;
}
int GetGlyphIndex(Font font, int codepoint) { (void)font; (void)codepoint; return 0; }
void TakeScreenshot(const char *fileName) { (void)fileName; }
const char *GetApplicationDirectory(void) { return g_repo_root; }
const char *TextFormat(const char *text, ...) {
    static char buf[4][512];
    static int idx = 0;
    idx = (idx + 1) & 3;
    va_list args;
    va_start(args, text);
    vsnprintf(buf[idx], sizeof(buf[idx]), text, args);
    va_end(args);
    return buf[idx];
}

void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}
bool IsAudioDeviceReady(void) { return false; }
Sound LoadSound(const char *fileName) { (void)fileName; Sound s = { 0 }; return s; }
Sound LoadSoundFromWave(Wave wave) { (void)wave; Sound s = { 0 }; return s; }
bool IsSoundValid(Sound sound) { (void)sound; return false; }
void UnloadSound(Sound sound) { (void)sound; }
void PlaySound(Sound sound) { (void)sound; }
void SetSoundVolume(Sound sound, float volume) { (void)sound; (void)volume; }
void SetSoundPitch(Sound sound, float pitch) { (void)sound; (void)pitch; }
Music LoadMusicStream(const char *fileName) { (void)fileName; Music m = { 0 }; return m; }
bool IsMusicValid(Music music) { (void)music; return false; }
void UnloadMusicStream(Music music) { (void)music; }
void PlayMusicStream(Music music) { (void)music; }
void StopMusicStream(Music music) { (void)music; }
void UpdateMusicStream(Music music) { (void)music; }
void SetMusicVolume(Music music, float volume) { (void)music; (void)volume; }
bool IsMusicStreamPlaying(Music music) { (void)music; return false; }
float GetMusicTimeLength(Music music) { (void)music; return 60.0f; }
float GetMusicTimePlayed(Music music) { (void)music; return 1.0f; }
void AttachAudioMixedProcessor(AudioCallback processor) { (void)processor; }
void DetachAudioMixedProcessor(AudioCallback processor) { (void)processor; }
void AttachAudioStreamProcessor(AudioStream stream, AudioCallback processor) { (void)stream; (void)processor; }
void DetachAudioStreamProcessor(AudioStream stream, AudioCallback processor) { (void)stream; (void)processor; }

/* ---------------- Textures, FBOs, Shaders & Primitives -------------------- */

Texture2D LoadTexture(const char *fileName) {
#if HAVE_RAYLIB_STB
    char p[768];
    resolve_asset_path(fileName, p, sizeof(p));
    int w = 0, h = 0, ch = 0;
    unsigned char *px = stbi_load(p, &w, &h, &ch, 4);
    if (px) {
        int tw = w, th = h;
        if (tw > 1280) { tw = w / 4; th = h / 4; if (tw < 1) tw = 1; if (th < 1) th = 1; }
        int id = alloc_sw_tex(tw, th, 0);
        if (tw == w && th == h) {
            memcpy(g_tex[id].rgba, px, (size_t)w * (size_t)h * 4u);
        } else {
            for (int y = 0; y < th; y++) {
                int sy = (int)((long long)y * h / th);
                for (int x = 0; x < tw; x++) {
                    int sx = (int)((long long)x * w / tw);
                    memcpy(&g_tex[id].rgba[((size_t)y * (size_t)tw + (size_t)x) * 4u],
                           &px[((size_t)sy * (size_t)w + (size_t)sx) * 4u], 4u);
                }
            }
        }
        stbi_image_free(px);
        return (Texture2D){ .id = (unsigned int)id, .width = w, .height = h, .mipmaps = 1, .format = 7 };
    }
#else
    (void)fileName;
#endif
    /* Fallback deterministic 1280x720 texture so draw_bg_if_loaded({0,0,1280,720}) maps 1:1 without stb */
    int id = alloc_sw_tex(320, 180, 0);
    for (int y = 0; y < 180; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t *d = &g_tex[id].rgba[((size_t)y * 320u + (size_t)x) * 4u];
            d[0] = (uint8_t)(24u + ((x * 7u + y * 3u) & 63u));
            d[1] = (uint8_t)(30u + ((x * 5u ^ y * 11u) & 63u));
            d[2] = (uint8_t)(34u + ((x * 13u + y * 7u) & 63u));
            d[3] = 255;
        }
    }
    return (Texture2D){ .id = (unsigned int)id, .width = 1280, .height = 720, .mipmaps = 1, .format = 7 };
}
void UnloadTexture(Texture2D texture) {
    if (texture.id > 0 && texture.id < SW_MAX_TEX && g_tex[texture.id].used) {
        free(g_tex[texture.id].rgba);
        memset(&g_tex[texture.id], 0, sizeof(SwTex));
    }
}
void SetTextureFilter(Texture2D texture, int filter) { (void)texture; (void)filter; }
void SetTextureWrap(Texture2D texture, int wrap) { (void)texture; (void)wrap; }
void GenTextureMipmaps(Texture2D *texture) { (void)texture; }
bool IsFontReady(Font font) { return font.texture.id > 0; }
bool IsWindowReady(void) { return true; }
bool IsRenderTextureReady(RenderTexture2D target) { return target.id > 0; }
bool IsShaderReady(Shader shader) { return shader.id > 0; }
bool IsTextureReady(Texture2D texture) { return texture.id > 0; }
Image LoadImage(const char *fileName) { (void)fileName; Image im = { 0 }; return im; }
void ImageCrop(Image *image, Rectangle crop) { (void)image; (void)crop; }
void ImageResize(Image *image, int newWidth, int newHeight) { (void)image; (void)newWidth; (void)newHeight; }
Texture2D LoadTextureFromImage(Image image) { (void)image; Texture2D t = { 0 }; return t; }
void UnloadImage(Image image) { (void)image; }

RenderTexture2D LoadRenderTexture(int width, int height) {
    int id = alloc_sw_tex(width, height, 1); /* real OpenGL FBO bottom-up UV behavior! */
    RenderTexture2D rt = { 0 };
    rt.id = (unsigned int)id;
    rt.texture = (Texture2D){ .id = (unsigned int)id, .width = width, .height = height, .mipmaps = 1, .format = 7 };
    return rt;
}
void UnloadRenderTexture(RenderTexture2D target) { UnloadTexture(target.texture); }

void BeginTextureMode(RenderTexture2D target) {
    g_active_rt = (int)target.id;
    if ((int)target.id == g_overlay_rt_id) g_rec_overlay_draws++;
}
void EndTextureMode(void) { g_active_rt = 0; }
void BeginDrawing(void) { g_active_rt = 0; }
void EndDrawing(void) { g_active_rt = 0; }
void BeginScissorMode(int x, int y, int width, int height) {
    g_scissor_on = 1;
    g_sc_x = x; g_sc_y = y; g_sc_w = width; g_sc_h = height;
    g_rec_scissor_calls++;
    g_rec_last_split_y = y;
}
void EndScissorMode(void) { g_scissor_on = 0; }
void BeginBlendMode(int mode) { (void)mode; }
void EndBlendMode(void) {}

Shader LoadShader(const char *vsFileName, const char *fsFileName) {
    (void)vsFileName; (void)fsFileName;
    Shader s = { .id = 1, .locs = NULL };
    return s;
}
void UnloadShader(Shader shader) { (void)shader; }
int GetShaderLocation(Shader shader, const char *uniformName) {
    (void)shader; (void)uniformName;
    return 0;
}
void SetShaderValue(Shader shader, int locIndex, const void *value, int uniformType) {
    (void)shader; (void)locIndex; (void)value; (void)uniformType;
}
void SetShaderValueTexture(Shader shader, int locIndex, Texture2D texture) {
    (void)shader; (void)locIndex; (void)texture;
}
void BeginShaderMode(Shader shader) { (void)shader; }
void EndShaderMode(void) {}

void ClearBackground(Color color) {
    SwTex *t = cur_target();
    for (int y = 0; y < t->h; y++)
        for (int x = 0; x < t->w; x++)
            blend_px(t, x, y, color);
}

void DrawRectangle(int posX, int posY, int width, int height, Color color) {
    SwTex *t = cur_target();
    int x0 = posX < 0 ? 0 : posX, y0 = posY < 0 ? 0 : posY;
    int x1 = posX + width > t->w ? t->w : posX + width;
    int y1 = posY + height > t->h ? t->h : posY + height;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            blend_px(t, x, y, color);
}
void DrawRectangleRec(Rectangle rec, Color color) {
    DrawRectangle((int)roundf(rec.x), (int)roundf(rec.y),
                  (int)roundf(rec.width), (int)roundf(rec.height), color);
}
void DrawRectangleV(Vector2 position, Vector2 size, Color color) {
    DrawRectangle((int)roundf(position.x), (int)roundf(position.y),
                  (int)roundf(size.x), (int)roundf(size.y), color);
}
void DrawRectangleLines(int posX, int posY, int width, int height, Color color) {
    DrawRectangle(posX, posY, width, 1, color);
    DrawRectangle(posX, posY + height - 1, width, 1, color);
    DrawRectangle(posX, posY, 1, height, color);
    DrawRectangle(posX + width - 1, posY, 1, height, color);
}
void DrawRectangleLinesEx(Rectangle rec, float lineThick, Color color) {
    int th = (int)ceilf(lineThick);
    if (th < 1) th = 1;
    int x = (int)roundf(rec.x), y = (int)roundf(rec.y);
    int w = (int)roundf(rec.width), h = (int)roundf(rec.height);
    DrawRectangle(x, y, w, th, color);
    DrawRectangle(x, y + h - th, w, th, color);
    DrawRectangle(x, y + th, th, h - 2 * th, color);
    DrawRectangle(x + w - th, y + th, th, h - 2 * th, color);
}
void DrawRectangleGradientV(int posX, int posY, int width, int height, Color top, Color bottom) {
    if (height <= 0 || width <= 0) return;
    for (int i = 0; i < height; i++) {
        float u = (height > 1) ? ((float)i / (float)(height - 1)) : 0.0f;
        Color c = {
            (uint8_t)roundf(top.r + (bottom.r - top.r) * u),
            (uint8_t)roundf(top.g + (bottom.g - top.g) * u),
            (uint8_t)roundf(top.b + (bottom.b - top.b) * u),
            (uint8_t)roundf(top.a + (bottom.a - top.a) * u),
        };
        DrawRectangle(posX, posY + i, width, 1, c);
    }
}
void DrawRectangleGradientH(int posX, int posY, int width, int height, Color left, Color right) {
    if (height <= 0 || width <= 0) return;
    for (int i = 0; i < width; i++) {
        float u = (width > 1) ? ((float)i / (float)(width - 1)) : 0.0f;
        Color c = {
            (uint8_t)roundf(left.r + (right.r - left.r) * u),
            (uint8_t)roundf(left.g + (right.g - left.g) * u),
            (uint8_t)roundf(left.b + (right.b - left.b) * u),
            (uint8_t)roundf(left.a + (right.a - left.a) * u),
        };
        DrawRectangle(posX + i, posY, 1, height, c);
    }
}
void DrawLine(int x1, int y1, int x2, int y2, Color color) {
    DrawLineEx((Vector2){ (float)x1, (float)y1 }, (Vector2){ (float)x2, (float)y2 }, 1.0f, color);
}
void DrawLineEx(Vector2 s, Vector2 e, float thick, Color color) {
    SwTex *t = cur_target();
    float dx = e.x - s.x, dy = e.y - s.y;
    int steps = (int)ceilf(fmaxf(fabsf(dx), fabsf(dy)));
    if (steps < 1) steps = 1;
    int r = (int)floorf(thick * 0.5f);
    for (int i = 0; i <= steps; i++) {
        int px = (int)roundf(s.x + dx * ((float)i / (float)steps));
        int py = (int)roundf(s.y + dy * ((float)i / (float)steps));
        for (int oy = -r; oy <= r; oy++)
            for (int ox = -r; ox <= r; ox++)
                blend_px(t, px + ox, py + oy, color);
    }
}
void DrawCircle(int cx, int cy, float radius, Color color) {
    DrawCircleV((Vector2){ (float)cx, (float)cy }, radius, color);
}
void DrawCircleV(Vector2 c, float radius, Color color) {
    SwTex *t = cur_target();
    int r = (int)ceilf(radius);
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= radius * radius)
                blend_px(t, (int)c.x + x, (int)c.y + y, color);
}
void DrawCircleGradient(int centerX, int centerY, float radius, Color inner, Color outer) {
    SwTex *t = cur_target();
    int r = (int)ceilf(radius);
    if (r < 1) return;
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            float d2 = (float)(x * x + y * y);
            if (d2 <= radius * radius) {
                float u = sqrtf(d2) / radius;
                Color c = {
                    (uint8_t)roundf(inner.r + (outer.r - inner.r) * u),
                    (uint8_t)roundf(inner.g + (outer.g - inner.g) * u),
                    (uint8_t)roundf(inner.b + (outer.b - inner.b) * u),
                    (uint8_t)roundf(inner.a + (outer.a - inner.a) * u),
                };
                blend_px(t, centerX + x, centerY + y, c);
            }
        }
    }
}
void DrawCircleLines(int cx, int cy, float radius, Color color) {
    SwTex *t = cur_target();
    for (int a = 0; a < 120; a++) {
        float rad = (float)a * (6.2831853f / 120.0f);
        blend_px(t, cx + (int)roundf(cosf(rad) * radius), cy + (int)roundf(sinf(rad) * radius), color);
    }
}
void DrawTriangle(Vector2 v1, Vector2 v2, Vector2 v3, Color color) {
    DrawLineEx(v1, v2, 1.5f, color);
    DrawLineEx(v2, v3, 1.5f, color);
    DrawLineEx(v3, v1, 1.5f, color);
}
void DrawRing(Vector2 c, float inR, float outR, float a0, float a1, int segs, Color color) {
    (void)inR; (void)segs;
    DrawCircleLines((int)c.x, (int)c.y, outR, color);
    (void)a0; (void)a1;
}

/* Exact Raylib 5.5 rtextures.c DrawTexturePro UV math (including FBO bottom-up flip!) */
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint) {
    (void)rotation;
    if (texture.id == 0 || texture.id >= SW_MAX_TEX || !g_tex[texture.id].used) return;
    SwTex *src = &g_tex[texture.id];
    SwTex *dst = cur_target();

    float width = (float)texture.width, height = (float)texture.height;
    bool flipX = false;
    if (source.width < 0) { flipX = true; source.width *= -1.0f; }
    if (source.height < 0) source.y -= source.height; /* exact rtextures.c:4501! */

    float v_top = source.y / height;
    float v_bot = (source.y + source.height) / height;
    float u_left = flipX ? ((source.x + source.width) / width) : (source.x / width);
    float u_right = flipX ? (source.x / width) : ((source.x + source.width) / width);

    int dx0 = (int)roundf(dest.x - origin.x);
    int dy0 = (int)roundf(dest.y - origin.y);
    int dw = (int)roundf(dest.width), dh = (int)roundf(dest.height);
    if (dw <= 0 || dh <= 0) return;

    int x_start = dx0 < 0 ? 0 : dx0, x_end = (dx0 + dw > dst->w) ? dst->w : (dx0 + dw);
    int y_start = dy0 < 0 ? 0 : dy0, y_end = (dy0 + dh > dst->h) ? dst->h : (dy0 + dh);
    if (g_scissor_on) {
        if (x_start < g_sc_x) x_start = g_sc_x;
        if (y_start < g_sc_y) y_start = g_sc_y;
        if (x_end > g_sc_x + g_sc_w) x_end = g_sc_x + g_sc_w;
        if (y_end > g_sc_y + g_sc_h) y_end = g_sc_y + g_sc_h;
    }

    for (int py = y_start; py < y_end; py++) {
        float tv = ((float)(py - dy0) + 0.5f) / (float)dh;
        float gl_v = v_top + (v_bot - v_top) * tv;
        /* In OpenGL FBO textures, v=0 is bottom row (h-1) and v=1 is top row (0)! */
        float mem_v = src->fbo_flip ? (1.0f - gl_v) : gl_v;
        int sy = (int)floorf(mem_v * (float)src->h);
        if (sy < 0 || sy >= src->h) continue; /* CLAMP out-of-range check */
        for (int px = x_start; px < x_end; px++) {
            float tu = ((float)(px - dx0) + 0.5f) / (float)dw;
            float gl_u = u_left + (u_right - u_left) * tu;
            int sx = (int)floorf(gl_u * (float)src->w);
            if (sx < 0) sx = 0;
            if (sx >= src->w) sx = src->w - 1;
            const uint8_t *sp = &src->rgba[((size_t)sy * (size_t)src->w + (size_t)sx) * 4u];
            Color out = {
                (uint8_t)((sp[0] * tint.r + 127u) / 255u),
                (uint8_t)((sp[1] * tint.g + 127u) / 255u),
                (uint8_t)((sp[2] * tint.b + 127u) / 255u),
                (uint8_t)((sp[3] * tint.a + 127u) / 255u),
            };
            blend_px(dst, px, py, out);
        }
    }
}
void DrawTextureRec(Texture2D texture, Rectangle source, Vector2 position, Color tint) {
    Rectangle dest = { position.x, position.y, fabsf(source.width), fabsf(source.height) };
    DrawTexturePro(texture, source, dest, (Vector2){ 0.0f, 0.0f }, 0.0f, tint);
}
void DrawTextureEx(Texture2D texture, Vector2 position, float rotation, float scale, Color tint) {
    Rectangle src = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
    Rectangle dst = { position.x, position.y, (float)texture.width * scale, (float)texture.height * scale };
    DrawTexturePro(texture, src, dst, (Vector2){ 0.0f, 0.0f }, rotation, tint);
}

/* ---------------- Real TrueType Font Loading & Rendering ------------------ */

Font GetFontDefault(void) { Font f = { .baseSize = 16 }; return f; }
Font LoadFontEx(const char *fileName, int fontSize, int *codepoints, int codepointCount) {
    (void)codepoints; (void)codepointCount;
    Font f = { .baseSize = fontSize };
#if HAVE_RAYLIB_STB
    char p[768];
    resolve_asset_path(fileName, p, sizeof(p));
    FILE *fp = fopen(p, "rb");
    if (!fp) return f;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    for (int i = 1; i < 8; i++) {
        if (!g_fonts[i].loaded) {
            g_fonts[i].ttf_buf = (unsigned char *)malloc((size_t)sz);
            if (fread(g_fonts[i].ttf_buf, 1, (size_t)sz, fp) == (size_t)sz &&
                stbtt_InitFont(&g_fonts[i].info, g_fonts[i].ttf_buf, 0)) {
                g_fonts[i].loaded = 1;
                f.texture.id = (unsigned int)i;
            }
            break;
        }
    }
    fclose(fp);
#else
    (void)fileName;
#endif
    return f;
}
void UnloadFont(Font font) { (void)font; }

Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    if (!text || !text[0]) return (Vector2){ 0.0f, fontSize };
#if HAVE_RAYLIB_STB
    int fid = (int)font.texture.id;
    if (fid > 0 && fid < 8 && g_fonts[fid].loaded) {
        float scale = stbtt_ScaleForPixelHeight(&g_fonts[fid].info, fontSize);
        float w = 0.0f;
        for (const unsigned char *s = (const unsigned char *)text; *s; s++) {
            int adv = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(&g_fonts[fid].info, (int)(*s), &adv, &lsb);
            w += (float)adv * scale + (*(s + 1) ? spacing : 0.0f);
        }
        return (Vector2){ w, fontSize };
    }
#else
    (void)font;
#endif
    return (Vector2){ (float)strlen(text) * (fontSize * 0.56f + spacing), fontSize };
}
int MeasureText(const char *text, int fontSize) {
    return (int)MeasureTextEx(GetFontDefault(), text, (float)fontSize, 1.0f).x;
}
void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize, float spacing, Color tint) {
    if (!text || !text[0] || tint.a == 0) return;
#if HAVE_RAYLIB_STB
    int fid = (int)font.texture.id;
    if (fid <= 0 || fid >= 8 || !g_fonts[fid].loaded) return;
    SwTex *dst = cur_target();
    stbtt_fontinfo *fi = &g_fonts[fid].info;
    float scale = stbtt_ScaleForPixelHeight(fi, fontSize);
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(fi, &ascent, &descent, &lineGap);
    float pen_x = position.x;
    float baseline = position.y + (float)ascent * scale;

    for (const unsigned char *s = (const unsigned char *)text; *s; s++) {
        int gw = 0, gh = 0, xoff = 0, yoff = 0;
        unsigned char *bmp = stbtt_GetCodepointBitmap(fi, 0, scale, (int)(*s), &gw, &gh, &xoff, &yoff);
        if (bmp) {
            int bx = (int)roundf(pen_x) + xoff;
            int by = (int)roundf(baseline) + yoff;
            for (int gy = 0; gy < gh; gy++) {
                for (int gx = 0; gx < gw; gx++) {
                    uint8_t cov = bmp[gy * gw + gx];
                    if (cov > 4) {
                        Color c = tint;
                        c.a = (uint8_t)((cov * tint.a + 127u) / 255u);
                        blend_px(dst, bx + gx, by + gy, c);
                    }
                }
            }
            stbtt_FreeBitmap(bmp, NULL);
        }
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(fi, (int)(*s), &adv, &lsb);
        pen_x += (float)adv * scale + (*(s + 1) ? spacing : 0.0f);
    }
#else
    (void)font; (void)position; (void)fontSize; (void)spacing;
#endif
}
void DrawText(const char *text, int posX, int posY, int fontSize, Color color) {
    DrawTextEx((Font){ .texture.id = 1, .baseSize = fontSize }, text,
               (Vector2){ (float)posX, (float)posY }, (float)fontSize, 1.0f, color);
}

/* ---------------- Include Real src/ui_main.c Directly! -------------------- */

#define main xenokoi_real_main
#include "../src/ui_main.c"
#undef main

static void step_frame(Vector2 mouse_xy, int click_this_frame, float dt_sec) {
    g_mouse = mouse_xy;
    g_mouse_pressed = click_this_frame;
    g_mouse_down = click_this_frame;
    g_dt = dt_sec;
    g_rec_scissor_calls = 0;
    g_rec_last_split_y = -1;
    g_rec_overlay_draws = 0;
    update_draw_frame();
    g_mouse_pressed = 0;
    g_mouse_down = 0;
}

/* Assert full 22-frame (360 ms @ 60 FPS) Stage 1 strobe + Stage 2 scan wipe */
static void assert_full_hub_transition(Vector2 click_pt, ScreenState expected_dest,
                                       int expected_camp, int expect_opt_page_during) {
    const float dt = 1.0f / 60.0f;
    const int expected_flash[12] = { 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0 };

    /* Snapshot outgoing bottom-corner pixel before click (proves zero FBO flip/shift!) */
    step_frame(click_pt, 0, dt);
    uint8_t src_bot_ref[4];
    memcpy(src_bot_ref, &g_tex[canvas.id].rgba[(680u * 1280u + 40u) * 4u], 4u);

    /* Frame 0: click triggers immediate Stage 1 Flash ON #1! */
    step_frame(click_pt, 1, dt);
    assert(hub_trans_active == 1);
    assert(transition_dir == 0);
    assert(screen == expected_dest);
    assert(campaign_overlay_open == expected_camp);
    assert(g_rec_overlay_draws == 0);

    int prev_split = -1;
    uint8_t mid_wipe_top[4] = { 0 };
    int captured_mid_top = 0;
    for (int f = 0; f < 22; f++) {
        if (f > 0) {
            /* Hammer click on QUIT coordinate mid-transition to prove input lock! */
            step_frame((Vector2){ 640.0f, 590.0f }, 1, dt);
            assert(quit_requested == 0);
        }
        if (expect_opt_page_during >= 0) {
            assert(screens_get_options_page() == expect_opt_page_during);
        }
        /* Outgoing bottom-left Giger pixel (via overlay_canvas FBO scissor blit!) must
         * match pre-click outgoing pixel BIT-FOR-BIT before the scan beam reaches y=650! */
        if (g_rec_last_split_y < 0 || g_rec_last_split_y < 650) {
            const uint8_t *p = &g_tex[canvas.id].rgba[(680u * 1280u + 40u) * 4u];
            assert(memcmp(src_bot_ref, p, 3u) == 0);
        }
        /* Record incoming top-left Giger pixel mid-wipe once scan beam passes y=80 */
        if (g_rec_last_split_y > 80 && !captured_mid_top) {
            memcpy(mid_wipe_top, &g_tex[canvas.id].rgba[(40u * 1280u + 40u) * 4u], 4u);
            captured_mid_top = 1;
        }

        if (f < 12) {
            int flash_on = ((((int)(hub_trans_timer * 60.0f + 0.5f)) / 2) & 1) == 0;
            assert(flash_on == expected_flash[f]);
            assert(g_rec_scissor_calls == 0);
            /* Sample center-top of clicked button: bright phosphor ON vs dark recessed OFF */
            int bx = (int)(hub_btn_rect.x + hub_btn_rect.width * 0.5f);
            int by = (int)(hub_btn_rect.y + 4.0f);
            const uint8_t *bp = &g_tex[canvas.id].rgba[((size_t)by * 1280u + (size_t)bx) * 4u];
            if (flash_on) assert(bp[1] > 180); /* intensely lit phosphor channel */
            else          assert(bp[1] < 40);  /* recessed black cutout */
        } else {
            assert(g_rec_overlay_draws == 1);
            assert(g_rec_last_split_y >= prev_split);
            prev_split = g_rec_last_split_y;
        }
    }

    /* Frame 22 (t = 22/60 = 0.3667 s >= 0.360 s): transition complete, live UI active! */
    step_frame((Vector2){ 640.0f, 360.0f }, 0, dt);
    assert(hub_trans_active == 0);
    assert(screen == expected_dest);
    assert(campaign_overlay_open == expected_camp);
    assert(screens_get_options_page() == 0);
    assert(captured_mid_top == 1);
    assert(memcmp(mid_wipe_top, &g_tex[canvas.id].rgba[(40u * 1280u + 40u) * 4u], 3u) == 0);
}

static void save_transition_strip(const char *out_png) {
#if !HAVE_RAYLIB_STB
    (void)out_png;
    return;
#else
    /* Render a 2x3 contact sheet (each tile 640x360 -> 1280x1080 PNG) capturing
     * MAIN MENU -> OPTION across Stage 1 Strobe + Stage 2 CRT Scan-Beam Wipe! */
    const int cap_frames[6] = { 0, 2, 4, 14, 17, 22 };
    const int tile_w = 640, tile_h = 360, cols = 2, rows = 3;
    const int out_w = tile_w * cols, out_h = tile_h * rows;
    uint8_t *sheet = (uint8_t *)calloc((size_t)out_w * (size_t)out_h * 3u, 1);

    screen = SCREEN_START;
    campaign_overlay_open = 0;
    hub_trans_active = 0;
    step_frame((Vector2){ 640.0f, 390.0f }, 0, 1.0f / 60.0f);

    int slot = 0;
    for (int f = 0; f <= 22; f++) {
        step_frame((Vector2){ 640.0f, 390.0f }, (f == 0) ? 1 : 0, 1.0f / 60.0f);
        if (slot < 6 && f == cap_frames[slot]) {
            int ox = (slot % cols) * tile_w;
            int oy = (slot / cols) * tile_h;
            const SwTex *cv = &g_tex[canvas.id];
            for (int y = 0; y < tile_h; y++) {
                for (int x = 0; x < tile_w; x++) {
                    const uint8_t *p00 = &cv->rgba[((size_t)(y * 2) * 1280u + (size_t)(x * 2)) * 4u];
                    const uint8_t *p10 = &cv->rgba[((size_t)(y * 2) * 1280u + (size_t)(x * 2 + 1)) * 4u];
                    const uint8_t *p01 = &cv->rgba[((size_t)(y * 2 + 1) * 1280u + (size_t)(x * 2)) * 4u];
                    const uint8_t *p11 = &cv->rgba[((size_t)(y * 2 + 1) * 1280u + (size_t)(x * 2 + 1)) * 4u];
                    uint8_t *dst = &sheet[((size_t)(oy + y) * (size_t)out_w + (size_t)(ox + x)) * 3u];
                    dst[0] = (uint8_t)((p00[0] + p10[0] + p01[0] + p11[0] + 2u) >> 2);
                    dst[1] = (uint8_t)((p00[1] + p10[1] + p01[1] + p11[1] + 2u) >> 2);
                    dst[2] = (uint8_t)((p00[2] + p10[2] + p01[2] + p11[2] + 2u) >> 2);
                }
            }
            slot++;
        }
    }
    assert(slot == 6);
    assert(stbi_write_png(out_png, out_w, out_h, 3, sheet, out_w * 3) != 0);
    free(sheet);
#endif
}

int main(int argc, char **argv) {
    if (!getcwd(g_repo_root, sizeof(g_repo_root))) snprintf(g_repo_root, sizeof(g_repo_root), ".");

    /* Isolate all runtime save writes (settings.txt, stats.txt, history.txt) in /tmp! */
    char tmpdir[] = "/tmp/xenokoi-hub-trans.XXXXXX";
    assert(mkdtemp(tmpdir) != NULL);
    assert(chdir(tmpdir) == 0);

    InitWindow(CANVAS_W, CANVAS_H, "XENO-KOI HUB TRANSITION TEST");
    storage_load_settings(&settings);
    storage_load_stats(&stats);
    storage_load_history(&history);
    theme_set(settings.background);
    board_load_assets(&assets);
    assets.current_bg = settings.background;
    font_title = LoadFontEx("assets/audiowide.ttf", 96, NULL, 0);
    font_body = LoadFontEx("assets/play.ttf", 36, NULL, 0);
    small_font = font_body;
    canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);
    overlay_canvas = LoadRenderTexture(CANVAS_W, CANVAS_H);
    g_overlay_rt_id = (int)overlay_canvas.id;
    engine_init(&g, settings.ruleset, settings.rounds, 42);
    motion_init();
    taunt_init();
    anim_init(&anim);

    /* 1. Verify jitter-free 2f ON / 2f OFF strobe cadence across 59.94 Hz & 60.05 Hz */
    const int expected_flash[12] = { 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0 };
    const float dts[3] = { 1.0f / 59.94f, 1.0f / 60.0f, 1.0f / 60.05f };
    for (int d = 0; d < 3; d++) {
        for (int k = 0; k < 12; k++) {
            float t = (float)k * dts[d];
            int on = ((((int)(t * 60.0f + 0.5f)) / 2) & 1) == 0;
            assert(on == expected_flash[k]);
        }
    }

    /* 2. Exercise ALL 9 hub transitions end-to-end through UpdateDrawFrame()! */
    screen = SCREEN_START; campaign_overlay_open = 0;
    assert_full_hub_transition((Vector2){ 640, 340 }, SCREEN_START, 1, -1);   /* START MATCH -> OPPONENT */
    assert_full_hub_transition((Vector2){ 1160, 62 }, SCREEN_START, 0, -1);   /* OPPONENT X -> MENU      */
    assert_full_hub_transition((Vector2){ 640, 340 }, SCREEN_START, 1, -1);   /* START MATCH -> OPPONENT */
    assert_full_hub_transition((Vector2){ 810, 594 }, SCREEN_START, 0, -1);   /* OPPONENT BACK -> MENU   */
    assert_full_hub_transition((Vector2){ 640, 390 }, SCREEN_OPTIONS, 0, 0);  /* MENU -> OPTION (Page 1) */

    /* Switch to Options Page 2, then click BACK (SAVE): Page 2 MUST hold steady during wipe! */
    screens_set_options_page(1);
    assert_full_hub_transition((Vector2){ 430, 675 }, SCREEN_START, 0, 1);    /* OPTION P2 -> MENU       */

    assert_full_hub_transition((Vector2){ 640, 440 }, SCREEN_STATS, 0, -1);   /* MENU -> STATS           */
    assert_full_hub_transition((Vector2){ 960, 632 }, SCREEN_START, 0, -1);   /* STATS -> MENU           */
    assert_full_hub_transition((Vector2){ 640, 540 }, SCREEN_CREDITS, 0, -1); /* MENU -> CREDITS         */
    assert_full_hub_transition((Vector2){ 640, 553 }, SCREEN_START, 0, -1);   /* CREDITS -> MENU         */

    /* Match Results -> MAIN MENU uses 360 ms CRT hub transition with 0 loading black screen! */
    engine_init(&g, settings.ruleset, settings.rounds, 42);
    screen = SCREEN_MATCH_RESULT;
    assert_full_hub_transition((Vector2){ 640, 549 }, SCREEN_START, 0, -1);   /* RESULTS -> MAIN MENU    */

    /* 3. Verify dark card-back loading transition (SCREEN_GAME) stays 100% untouched! */
    screen = SCREEN_MATCH_RESULT;
    step_frame((Vector2){ 640, 493 }, 1, 1.0f / 60.0f); /* PLAY AGAIN */
    assert(transition_dir == 1 && transition_target_screen == SCREEN_GAME && hub_trans_active == 0);
    transition_dir = 0; transition_alpha = 0.0f;

    if (argc >= 3 && strcmp(argv[1], "--render-strip") == 0) {
        save_transition_strip(argv[2]);
    }

    assert(chdir(g_repo_root) == 0);
    char rmcmd[600];
    snprintf(rmcmd, sizeof(rmcmd), "rm -rf '%s'", tmpdir);
    if (system(rmcmd) != 0) {}

    printf("All 9 CRT hub transition routes, strobe cadences, Options Page 2 hold, "
           "stationary Giger background, and SCREEN_GAME loading guard checks passed.\n");
    return 0;
}
