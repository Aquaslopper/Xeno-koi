#version 100
precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;

// moving-lights begin
// Bump lighting for the table background. normalMap RGB holds a
// tangent-space normal (x right, y DOWN, z toward the viewer, 0.5 bias),
// alpha holds the height field (unused here, kept for inspection).
// Three wide soft lights drift on independent slow paths; the ambient
// lift keeps every background brighter than its art, so there is no
// night side and no hard spotlight cone.
uniform sampler2D normalMap;
uniform float lightEnabled;

vec3 movingLight(vec3 n, vec2 uv, vec2 lp, float radius, float height, float gain, vec3 tint)
{
    vec2 d = (lp - uv) * vec2(1.7777778, 1.0); // aspect-corrected offset, uv y is down
    float dist2 = dot(d, d);
    vec3 l = normalize(vec3(d, height));
    float diff = max(dot(n, l), 0.0);
    vec3 h = normalize(l + vec3(0.0, 0.0, 1.0)); // half-vector, view = +z
    float spec = pow(max(dot(n, h), 0.0), 28.0);
    float atten = 1.0 / (1.0 + dist2 / (radius * radius));
    return tint * (diff + 0.40 * spec) * atten * gain;
}

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 normalTexel = texture2D(normalMap, uv);
    vec3 n = normalize(normalTexel.rgb * 2.0 - 1.0);
    float t = time;
    vec3 lit = vec3(1.22); // ambient lift
    lit += movingLight(n, uv,
        vec2(0.50 + 0.42 * sin(t * 0.100), 0.30 + 0.18 * sin(t * 0.067 + 1.7)),
        0.62, 0.55, 1.15, vec3(1.00, 0.97, 0.90));
    lit += movingLight(n, uv,
        vec2(0.22 + 0.26 * cos(t * 0.073 + 0.6), 0.52 + 0.24 * sin(t * 0.073)),
        0.55, 0.50, 1.00, vec3(0.90, 0.96, 1.00));
    lit += movingLight(n, uv,
        vec2(0.80 + 0.22 * sin(t * 0.055 + 2.9), 0.42 + 0.28 * cos(t * 0.049 + 1.1)),
        0.58, 0.55, 0.95, vec3(1.00, 0.93, 0.88));
    return clamp(base * lit, 0.0, 1.0);
}
// moving-lights end

void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);
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

    gl_FragColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
