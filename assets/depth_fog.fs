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
// Standard tangent-space normal mapping for the table background.
// normalMap RGB holds the normal (x right, y DOWN, z toward the viewer,
// 0.5 bias); alpha is unused. Lighting is an ambient floor plus two
// directional lights -- a warm key whose direction drifts on a slow
// lissajous, and a cool fixed fill -- with a Blinn-Phong glint. Plain N.L,
// so the relief reads as a surface lit from one side and the highlight
// travels as the key light swings. Deus has no normal map and is untouched.
// Constants are mirrored in tools/make_bg_normal_maps.py for --preview.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float flashAmount;

const float AMBIENT = 0.55;      // floor, so shadowed slopes stay out of pitch black
const float KEY_GAIN = 1.25;
const vec3  KEY_COLOR = vec3(1.00, 0.93, 0.82);
const float FILL_GAIN = 0.30;
const vec3  FILL_COLOR = vec3(0.55, 0.70, 1.00);
const float SPEC_POWER = 48.0;
const float SPEC_GAIN = 0.30;
const float FLASH_GAIN = 1.00;   // how far the thunder spike lifts everything

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec3 n = normalize(texture(normalMap, uv).rgb * 2.0 - 1.0);
    float t = time;

    // Key light: direction drifts, always raking (z stays toward the viewer).
    float az = 0.90 * sin(t * 0.043) + 0.35 * sin(t * 0.017 + 1.3);
    float el = 0.55 + 0.18 * sin(t * 0.029);
    vec3 key = normalize(vec3(sin(az) * cos(el), -sin(el), cos(az) * cos(el)));
    vec3 fill = normalize(vec3(-0.55, 0.35, 0.75));

    float kd = max(dot(n, key), 0.0);
    float fd = max(dot(n, fill), 0.0);
    vec3 hv = normalize(key + vec3(0.0, 0.0, 1.0)); // half-vector, view = +z
    float spec = pow(max(dot(n, hv), 0.0), SPEC_POWER);

    float flash = 1.0 + FLASH_GAIN * flashAmount;
    vec3 lit = vec3(AMBIENT) * flash; // ambient floor
    lit += flash * (KEY_COLOR * (kd * KEY_GAIN + spec * SPEC_GAIN)
                    + FILL_COLOR * fd * FILL_GAIN);
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
