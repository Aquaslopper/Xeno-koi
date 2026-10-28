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
// Owner's brief, implemented literally: ONE infinitely long beam of light with
// no ends on screen, perfectly smooth (a single Gaussian - smooth at every
// derivative, so no banding, no hard cuts and no need to layer beams to hide
// edges), walking the plate on a slow random traverse corner to corner with
// only a hint of spin, and lighting ONLY the areas the normal map's alpha
// channel allows. Standard tangent-space lighting: wrapped diffuse (no hard
// terminator line) plus Blinn-Phong specular, gated by the art's luminance so
// true black stays black, folded through the same soft-knee rolloff as before.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float flashAmount;

// --- the knobs, in screen-height units unless the name says otherwise --------
const float ASPECT       = 1.7777778; // 1280/720: this space is screen-proportional
const float BEAM_WIDTH   = 0.18;      // beam half-width (~115 px); one number, no bands
const float WALK_STEP    = 26.0;      // seconds per traverse between random anchors
const float SPIN_RATE    = 0.012;     // rad/s: one turn per ~8.7 min, a hint of spin
const float LIGHT_HEIGHT = 0.55;      // how far "above" the plate the light sits
const float MASK_LO      = 0.01;      // normal-map alpha gate: which areas may light
const float MASK_HI      = 0.06;
const float BLACK_LO     = 0.014;     // art-luminance gate: keeps true black unlit
const float BLACK_HI     = 0.065;
const float REST_LEVEL   = 1.0;       // resting beam brightness - independent of the thunder
const float DIFF_GAIN    = 2.6;       // wrapped diffuse strength
const float SPEC_GAIN    = 1.5;       // specular strength
const float SPEC_POWER   = 24.0;      // specular tightness

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 map = texture(normalMap, uv);
    vec3 n = normalize(map.rgb * 2.0 - 1.0);

    // Slow random traverse: a new random anchor anywhere on the plate every
    // WALK_STEP seconds, C1-smooth between them, so it enters from left, right,
    // top or bottom and never orbits in place.
    float moveTime = time / WALK_STEP;
    float step = floor(moveTime);
    float blend = smoothstep(0.0, 1.0, fract(moveTime));
    vec2 a0 = fract(sin(vec2(step,       step + 17.0) * vec2(12.9898, 78.233)) * 43758.5453);
    vec2 a1 = fract(sin(vec2(step + 1.0, step + 18.0) * vec2(12.9898, 78.233)) * 43758.5453);
    vec2 lightUV = vec2(0.03) + 0.94 * mix(a0, a1, blend);

    // The beam: signed distance to an infinite line, in corrected space.
    vec2 toLight = (lightUV - uv) * vec2(ASPECT, 1.0);
    vec2 axis = normalize(vec2(cos(time * SPIN_RATE) / ASPECT, sin(time * SPIN_RATE)));
    float d = abs(dot(toLight, axis));
    float beam = exp(-(d * d) / (BEAM_WIDTH * BEAM_WIDTH));

    // Standard tangent-space lighting.
    vec3 L = normalize(vec3(toLight, LIGHT_HEIGHT));
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    float diff = 0.5 + 0.5 * dot(n, L);  // wrapped: no terminator line, no hard cut
    diff *= diff;
    float spec = pow(max(dot(n, H), 0.0), SPEC_POWER);

    float mask = smoothstep(MASK_LO, MASK_HI, map.a);
    float blackGate = smoothstep(BLACK_LO, BLACK_HI, dot(base, vec3(0.299, 0.587, 0.114)));

    float flashGain = 0.95 * flashAmount;   // thunder only - exactly as shipped
    float energy = beam * (REST_LEVEL + flashGain) + flashAmount * 0.065;

    vec3 warm = vec3(1.0, 0.93, 0.80);
    vec3 lightAdd = base * warm * (energy * diff * DIFF_GAIN)
                  + warm * (energy * blackGate * (diff * 0.30 + spec * SPEC_GAIN));
    lightAdd *= mask;
    return clamp(base + (vec3(1.0) - base) * (vec3(1.0) - exp(-lightAdd)), 0.0, 1.0);
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
        float lum = dot(blended, vec3(0.299, 0.587, 0.114));
        float depthMask = smoothstep(0.78, 0.18, lum);

        // 3. The Patchy Drift:
        float w1 = sin(fragTexCoord.x * 6.283 + time * 0.32) * 0.5 + 0.5;
        float w2 = sin(fragTexCoord.x * 14.137 + fragTexCoord.y * 3.1415 - time * 0.22) * 0.5 + 0.5;
        float w3 = sin(fragTexCoord.x * 4.712 + fragTexCoord.y * 7.854 + time * 0.15) * 0.5 + 0.5;
        float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
        float patch = smoothstep(0.15, 0.75, drift);

        // 4. Fog Blend (#050505):
        const vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
        float fogAlpha = clamp(depthMask * yMask * patch * 0.80, 0.0, 0.55);
        blended = mix(blended, fogColor, fogAlpha);
    }

    // 5. Ambient Vignette:
    vec2 uvCentered = fragTexCoord - vec2(0.5, 0.5);
    float dist = length(uvCentered);
    float vignette = smoothstep(0.75, 0.25, dist);

    finalColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
