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
// One soft moving light travels across selected raised regions. RGB stores
// tangent-space normals; alpha gates the light with a soft height mask.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform vec2 heightRange;

float lightLayer(float distanceFromCenter, float width)
{
    float halfWidth = 0.5 * width;
    return 1.0 - smoothstep(halfWidth - 0.06, halfWidth, distanceFromCenter);
}

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 map = texture2D(normalMap, uv);
    vec3 n = normalize(map.rgb * 2.0 - 1.0);

    // Layer three drives the group center through a slow left-right 360 loop.
    float phase = time * 0.0698132; // one loop per 90 seconds
    vec2 lightUV = vec2(0.08 + 0.84 * (0.5 - 0.5 * cos(phase)),
                        0.50 + 0.12 * sin(phase));
    float xDistance = abs(uv.x - lightUV.x);
    float layer80 = 0.10 * lightLayer(xDistance, 0.80);
    float layer60 = 0.20 * lightLayer(xDistance, 0.60);
    float layer40 = 0.40 * lightLayer(xDistance, 0.40);
    float layer20 = 0.80 * lightLayer(xDistance, 0.20);
    // Nested bands select the strongest local layer; they do not add into one blob.
    float layers = max(max(layer80, layer60), max(layer40, layer20));

    vec2 toLight = (lightUV - uv) * vec2(1.7777778, 1.0);
    vec3 lightDir = normalize(vec3(toLight, 0.42));
    float diffuse = max(dot(n, lightDir), 0.0);
    float heightMask = smoothstep(heightRange.x, heightRange.y, map.a);
    float surfaceGate = mix(0.65, 1.0, heightMask); // preserve visible banding on flat areas
    float amount = surfaceGate * layers * diffuse * 8.0 * 0.30; // exaggerated test strength
    vec3 warm = vec3(1.0, 0.93, 0.80);
    vec3 highlight = min(warm * amount, vec3(0.90));
    return base + (vec3(1.0) - base) * highlight;
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
