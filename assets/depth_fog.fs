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
// One drifting, spinning 4-band light beam over the tangent-space normal map
// with a multi-tap light-only bloom halo and soft-knee highlight rolloff so
// 3D biomechanical ridges glow smoothly without sharp cutoffs or grey wash.
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float flashAmount;

float lightLayer(float distanceFromCenter, float width)
{
    float halfWidth = 0.5 * width;
    float feather = max(0.24, halfWidth * 0.65);
    return 1.0 - smoothstep(halfWidth - feather, halfWidth + 0.08, distanceFromCenter);
}

float sampleRidgeEmit(vec2 sampleUV, vec2 lightDirXY)
{
    vec2 cUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
    vec3 sn = normalize(texture(normalMap, cUV).rgb * 2.0 - 1.0);
    vec3 sb = texture(texture0, cUV).rgb;
    float sLum = dot(sb, vec3(0.299, 0.587, 0.114));
    float sMask = smoothstep(0.014, 0.065, sLum);
    float sRelief = smoothstep(0.015, 0.30, length(sn.xy));
    float sDir = 0.55 + 0.45 * smoothstep(-0.30, 0.35, dot(sn.xy, lightDirXY));
    return sRelief * sDir * sMask;
}

vec3 bgMovingLights(vec3 base, vec2 uv)
{
    vec4 map = texture(normalMap, uv);
    vec3 n = normalize(map.rgb * 2.0 - 1.0);

    // Drift left-right on a 32.1-second round trip; spin 10% faster than drift.
    float driftPhase = time * 0.1954768;
    float spinPhase = time * 0.2150244;
    vec2 lightUV = vec2(0.08 + 0.84 * (0.5 - 0.5 * cos(driftPhase)), 0.50);
    vec2 beamAxis = vec2(cos(spinPhase), sin(spinPhase));
    float xDistance = abs(dot(uv - lightUV, beamAxis));
    float layer80 = 0.10 * lightLayer(xDistance, 1.568);
    float layer60 = 0.133333 * lightLayer(xDistance, 1.176);
    float layer40 = 0.166667 * lightLayer(xDistance, 0.784);
    float layer20 = 0.20 * lightLayer(xDistance, 0.392);
    // Nested bands combined with a soft Gaussian aura so band edges never hard-clip.
    float layers = max(max(layer80, layer60), max(layer40, layer20));
    layers = 0.65 * layers + 0.35 * (0.20 * exp(-(xDistance * xDistance) / (0.52 * 0.52)));

    vec2 toLight = (lightUV - uv) * vec2(1.7777778, 1.0);
    float dist2 = dot(toLight, toLight);
    float reach = exp(-dist2 / (0.90 * 0.90));
    vec3 lightDir = normalize(vec3(toLight, 0.42));
    vec3 halfVec = normalize(lightDir + vec3(0.0, 0.0, 1.0));

    // Smooth 3D relief with wrapped directional bias (no center hole or half-ridge cutoff).
    float relief = smoothstep(0.015, 0.30, length(n.xy));
    float dirBias = 0.55 + 0.45 * smoothstep(-0.30, 0.35, dot(n.xy, lightDir.xy));
    float slope = relief * dirBias;
    float spec = pow(max(dot(n, halfVec), 0.0), 14.0);

    float baseLum = dot(base, vec3(0.299, 0.587, 0.114));
    float surfaceMask = smoothstep(0.014, 0.065, baseLum);

    // 8-tap two-ring bloom kernel around uv so the light itself glows softly over ridges.
    const vec2 r1 = vec2(7.0 / 1280.0, 7.0 / 720.0);
    const vec2 r2 = vec2(13.0 / 1280.0, 13.0 / 720.0);
    float bloomEnergy = 0.125 * (
        sampleRidgeEmit(uv + vec2(-r1.x, 0.0), lightDir.xy) +
        sampleRidgeEmit(uv + vec2( r1.x, 0.0), lightDir.xy) +
        sampleRidgeEmit(uv + vec2(0.0, -r1.y), lightDir.xy) +
        sampleRidgeEmit(uv + vec2(0.0,  r1.y), lightDir.xy) +
        sampleRidgeEmit(uv + vec2(-r2.x, -r2.y), lightDir.xy) +
        sampleRidgeEmit(uv + vec2( r2.x, -r2.y), lightDir.xy) +
        sampleRidgeEmit(uv + vec2(-r2.x,  r2.y), lightDir.xy) +
        sampleRidgeEmit(uv + vec2( r2.x,  r2.y), lightDir.xy)
    );

    float flashGain = 1.0 + 0.95 * flashAmount;
    float beam = layers * reach * flashGain + flashAmount * 0.065 * (0.35 + 0.65 * reach);
    vec3 warm = vec3(1.0, 0.93, 0.80);
    float diffGain = beam * (slope * 11.0 + bloomEnergy * 6.5);
    float lightBloom = beam * (surfaceMask * slope * (0.32 + 1.05 * spec) * 0.95 + bloomEnergy * 1.15);
    vec3 lightAdd = base * warm * diffGain + warm * lightBloom;
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
