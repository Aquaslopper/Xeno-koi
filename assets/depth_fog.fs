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
// One infinitely long beam, smooth at every derivative (a single Gaussian, so
// there is no edge to band or cut), walking the plate on a slow random traverse
// with only a hint of spin. It lights ONLY what the normal map's alpha allows,
// with standard tangent-space lighting: wrapped diffuse plus Blinn-Phong.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float flashAmount;

const float ASPECT       = 1.7777778;
const float BEAM_WIDTH   = 0.28;   // beam half-width, screen-height units
const float WALK_STEP    = 96.0;   // seconds between random anchors
const float SPIN_RATE    = 0.012;  // rad/s, one turn per ~8.7 min
const float LIGHT_HEIGHT = 0.55;
const float MASK_LO      = 0.00;   // normal-map alpha gate: which areas may light
const float MASK_HI      = 0.12;
const float BLACK_LO     = 0.044;  // art-luminance gate: keeps true black black
const float BLACK_HI     = 0.065;
const float REST_LEVEL   = 1.0;    // resting beam brightness, independent of thunder
const float DIFF_GAIN    = 1.0;    // wrapped diffuse strength
const float SPEC_GAIN    = 1.5;
const float SPEC_POWER   = 12.0;

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 map = texture(normalMap, uv);
    vec3 n = normalize(map.rgb * 2.0 - 1.0);

    // Slow random traverse: a new random anchor anywhere on the plate every
    // WALK_STEP seconds, C1-smooth between them.
    float moveTime = time / WALK_STEP;
    float step = floor(moveTime);
    float blend = smoothstep(0.0, 1.0, fract(moveTime));
    vec2 a0 = fract(sin(vec2(step,       step + 17.0) * vec2(12.9898, 78.233)) * 43758.5453);
    vec2 a1 = fract(sin(vec2(step + 1.0, step + 18.0) * vec2(12.9898, 78.233)) * 43758.5453);
    vec2 lightUV = vec2(0.03) + 0.94 * mix(a0, a1, blend);

    // The beam: signed distance to an infinite line. No ends, no edges.
    vec2 toLight = (lightUV - uv) * vec2(ASPECT, 1.0);
    vec2 axis = normalize(vec2(cos(time * SPIN_RATE) / ASPECT, sin(time * SPIN_RATE)));
    float d = abs(dot(toLight, axis));
    float beam = exp(-(d * d) / (BEAM_WIDTH * BEAM_WIDTH));

    vec3 L = normalize(vec3(toLight, LIGHT_HEIGHT));
    vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
    float diff = 0.5 + 0.5 * dot(n, L);  // wrapped: no terminator line
    diff *= diff;
    float spec = pow(max(dot(n, H), 0.0), SPEC_POWER);

    float mask = smoothstep(MASK_LO, MASK_HI, map.a);
    float blackGate = smoothstep(BLACK_LO, BLACK_HI, dot(base, vec3(0.299, 0.587, 0.114)));

    float energy = beam * (REST_LEVEL + 0.95 * flashAmount) + flashAmount * 0.065;

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
