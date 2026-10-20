#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;

// Output fragment color
out vec4 finalColor;

// moving-lights begin
// Bump lighting for the table background. normalMap RGB holds a
// tangent-space normal (x right, y DOWN, z toward the viewer, 0.5 bias),
// alpha holds the height field (unused here, kept for inspection).
//
// Only the tilted part of the normal answers the lights: the flat,
// viewer-facing component is projected out and a relief mask keeps
// near-flat art quiet, so the pools land on the raised ridges instead of
// washing the whole frame. The mask ramps over a wide slope band so the
// lit/unlit boundary eases in instead of cutting a hard edge. Three wide
// soft lights drift on independent slow paths; the ambient lift keeps a
// lit background slightly brighter than its art, so there is no night
// side and no hard spotlight cone.
//
// flashAmount is 1 at a capture (match taken) or a Koi-Koi call and decays
// through a fast linear fall into a long tail; the envelope lives in
// src/ui_board.c. It lifts the whole lighting factor, so the event reads as
// a thunder flash over the ridges. Deus has no normal map and is untouched.
// Constants are mirrored in tools/make_bg_normal_maps.py for --preview.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float flashAmount;

const float AMBIENT_LIFT = 1.10; // what flat art is multiplied by
const float RELIEF_MIN = 0.07;   // |n.xy| below this counts as flat: stays dark
const float RELIEF_MAX = 0.58;   // |n.xy| above this is lit at full strength
const float LIGHT_HEIGHT = 0.35; // low = grazing light, so ridges read stronger
const float FLASH_GAIN = 1.00;   // how far the thunder spike lifts everything

vec3 movingLight(vec3 n, vec2 uv, vec2 lp, float radius, float gain, vec3 tint)
{
    vec2 d = (lp - uv) * vec2(1.7777778, 1.0); // aspect-corrected offset, uv y is down
    float dist2 = dot(d, d);
    vec3 l = normalize(vec3(d, LIGHT_HEIGHT));
    // Relief response: how far the tilted normal turns toward the light.
    // A flat normal contributes exactly zero here, which is what stops the
    // light from brightening art that has no bumps in it.
    float diff = max(n.x * l.x + n.y * l.y, 0.0);
    vec3 h = normalize(l + vec3(0.0, 0.0, 1.0)); // half-vector, view = +z
    float spec = pow(max(dot(n, h), 0.0), 28.0);
    float atten = 1.0 / (1.0 + dist2 / (radius * radius));
    return tint * (diff + 0.40 * spec) * atten * gain;
}

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 normalTexel = texture(normalMap, uv);
    vec3 n = normalize(normalTexel.rgb * 2.0 - 1.0);
    float relief = smoothstep(RELIEF_MIN, RELIEF_MAX, length(n.xy));
    float flash = 1.0 + FLASH_GAIN * flashAmount;
    float t = time;
    vec3 lit = vec3(AMBIENT_LIFT * flash); // ambient lift
    lit += relief * flash * movingLight(n, uv,
        vec2(0.50 + 0.42 * sin(t * 0.100), 0.30 + 0.18 * sin(t * 0.067 + 1.7)),
        0.62, 3.20, vec3(1.00, 0.97, 0.90));
    lit += relief * flash * movingLight(n, uv,
        vec2(0.22 + 0.26 * cos(t * 0.073 + 0.6), 0.52 + 0.24 * sin(t * 0.073)),
        0.55, 2.80, vec3(0.90, 0.96, 1.00));
    lit += relief * flash * movingLight(n, uv,
        vec2(0.80 + 0.22 * sin(t * 0.055 + 2.9), 0.42 + 0.28 * cos(t * 0.049 + 1.1)),
        0.58, 2.60, vec3(1.00, 0.93, 0.88));
    return clamp(base * lit, 0.0, 1.0);
}
// moving-lights end

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    vec3 blended = texel.rgb;

    // Moving lights over the bump-mapped background (Harvest and the other
    // non-Deus backgrounds; Deus has no normal map and keeps its own look).
    if (lightEnabled > 0.5) {
        blended = bgMovingLights(texel.rgb, fragTexCoord);
    }

    // 1. The Y-Axis Mask:
    // Fog exists on bottom 62% of screen; skip trig waves completely above y = 0.38.
    if (fragTexCoord.y > 0.38) {
        float yMask = smoothstep(0.38, 0.88, fragTexCoord.y);

        // 2. The Depth Mask (Luminance):
        float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
        float depthMask = smoothstep(0.78, 0.18, lum);

        // 3. The Patchy Drift:
        float w1 = sin(fragTexCoord.x * 6.283 + time * 0.32) * 0.5 + 0.5;
        float w2 = sin(fragTexCoord.x * 14.137 + fragTexCoord.y * 3.1415 - time * 0.22) * 0.5 + 0.5;
        float w3 = sin(fragTexCoord.x * 4.712 + fragTexCoord.y * 7.854 + time * 0.15) * 0.5 + 0.5;
        float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
        float patch = smoothstep(0.15, 0.75, drift);

        // 4. Fog Blend (#050505):
        const vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
        float fogAlpha = clamp(depthMask * yMask * patch * 1.35, 0.0, 0.88);
        blended = mix(blended, fogColor, fogAlpha);
    }

    // 5. Ambient Vignette:
    vec2 uvCentered = fragTexCoord - vec2(0.5, 0.5);
    float dist = length(uvCentered);
    float vignette = smoothstep(0.75, 0.25, dist);

    finalColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
