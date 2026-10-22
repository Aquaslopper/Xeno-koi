#version 300 es
// The game's background shader with three cheaper uses of the height map.
//
// A · fog settles low  - the existing fog, weighted by height instead of by luminance alone.
// B · beam occlusion   - the drifting light is blocked by whatever stands between it and the
//                        pixel, so it lands on ridge tops and not in the trenches.
// C · baked AO         - the occlusion term the offline tool bakes into the art, shown live so
//                        it can be judged (and its strength chosen) before anything is re-exported.
//
// None of the three moves a texel of the painting. That is the point: the POM prototype
// (shaders/pom.frag) moved the sampling point and smeared skylines; this one only decides how
// much fog and light land where.
precision highp float;

in vec2 vUV;                 // 0..1, y down: the game's fragTexCoord
out vec4 outColor;

uniform sampler2D uColor;
uniform sampler2D uNormal;
uniform sampler2D uHeight;

uniform vec2  uRes;
uniform float uTime;
uniform float uLight;        // 0/1  the moving normal-map beam
uniform float uFog;          // 0/1  depth fog + vignette
uniform vec2  uVantage;      // pointer position, used as the light's aim in mode B
uniform int   uMode;         // 0 fog, 1 beam, 2 bake
uniform float uStrength;     // 0..2, the single knob the maintainer turns
uniform float uMix;          // 0 = as shipped, 1 = full effect (A/B wipe)
uniform float uPhase;        // 0..1 manual beam angle (mode B), -1 = follow the clock
uniform float uClamp;        // 1 = clamp height sampling at the frame edge

#define TAPS 12
#define AO_TAPS 8

vec3 colorAt(vec2 uv)  { return texture(uColor,  uClamp > 0.5 ? clamp(uv, 0.0, 1.0) : uv).rgb; }
vec3 normalAt(vec2 uv) { return texture(uNormal, uClamp > 0.5 ? clamp(uv, 0.0, 1.0) : uv).rgb; }
float heightAt(vec2 uv){ return texture(uHeight, uClamp > 0.5 ? clamp(uv, 0.0, 1.0) : uv).r; }

// ------------------------------------------------------- A: fog that settles low
float heightFogWeight(vec2 uv)
{
    // Fog does not sit on ridges; it runs down into the valleys and stays there.
    float h = heightAt(uv);
    return pow(1.0 - clamp(h, 0.0, 1.0), 1.6);
}

// ------------------------------------------------------- B: the beam is blocked
vec2 beamLightUV()
{
    if (uPhase >= 0.0) {
        // swept by hand: a half turn, so every ridge can be seen to cast
        float a = uPhase * 6.2831853;
        return vec2(0.5 + 0.42 * cos(a), 0.5 + 0.22 * sin(a));
    }
    if (uMode == 1) {
        // in beam mode the pointer *is* the light, so a ridge can be walked past
        return clamp(uVantage, vec2(0.04), vec2(0.96));
    }
    return vec2(0.08 + 0.84 * (0.5 - 0.5 * cos(uTime * 0.1954768)), 0.50);
}

float beamReach(vec2 uv)
{
    // March toward the light; if the height field stands above the ray, the beam is blocked.
    vec2 lightUV = beamLightUV();
    vec2 d = lightUV - uv;
    d /= max(length(d), 1e-4);
    float h0 = heightAt(uv);
    float occ = 0.0;
    for (int i = 1; i <= TAPS; i++) {
        float f = float(i) / float(TAPS);
        vec2 p = uv + d * (0.22 * f);
        float ray = h0 + 0.28 * 0.22 * f;
        float stand = heightAt(p) * (1.0 - 0.35 * f);   // a blocker has to be a mass, not noise
        occ = max(occ, clamp((stand - ray) * 5.0, 0.0, 1.0) * (1.0 - 0.75 * f));
    }
    return 1.0 - 0.55 * occ * uStrength;
}

// ------------------------------------------------------- C: the offline AO term
float bakedAO(vec2 uv, out float occlusion)
{
    // Same three radii as tools/bake_ao.py: 2 px, 6 px, 16 px at 1280x720.
    float h = heightAt(uv);
    vec3 radii = vec3(2.0, 6.0, 16.0);
    vec3 gains = vec3(0.55, 0.30, 0.15);
    float ao = 1.0;
    for (int r = 0; r < 3; r++) {
        float radius = radii[r];
        float blur = 0.0;
        float wsum = 0.0;
        for (int s = 0; s < AO_TAPS; s++) {
            // ring sample, 8 directions, radius scaled per scale
            float a = float(s) * 0.7853982;
            vec2 o = vec2(cos(a), sin(a)) * radius / uRes;
            float w = 1.0;
            blur += heightAt(uv + o) * w;
            wsum += w;
        }
        blur /= wsum;
        float cavity = clamp(blur - h, 0.0, 1.0);
        ao *= 1.0 - clamp(cavity * gains[r] * 6.0, 0.0, 1.0);
    }
    occlusion = 1.0 - ao;
    return ao;
}

// ------------------------------------------------------- the game's light, unchanged
float lightLayer(float d, float width)
{
    float halfWidth = 0.5 * width;
    float feather = max(0.24, halfWidth * 0.65);
    return 1.0 - smoothstep(halfWidth - feather, halfWidth + 0.08, d);
}

float sampleRidgeEmit(vec2 sampleUV, vec2 lightDirXY)
{
    vec2 cUV = clamp(sampleUV, vec2(0.001), vec2(0.999));
    vec3 sn = normalize(normalAt(cUV) * 2.0 - 1.0);
    vec3 sb = colorAt(cUV);
    float sLum = dot(sb, vec3(0.299, 0.587, 0.114));
    float sMask = smoothstep(0.014, 0.065, sLum);
    float sRelief = smoothstep(0.015, 0.30, length(sn.xy));
    float sDir = 0.55 + 0.45 * smoothstep(-0.30, 0.35, dot(sn.xy, lightDirXY));
    return sRelief * sDir * sMask;
}

vec3 bgMovingLights(vec3 base, vec3 mapRGB, vec2 uv, vec2 lightUV, float flash)
{
    vec3 n = normalize(mapRGB * 2.0 - 1.0);

    float spinPhase = uTime * 0.2150244;
    vec2 beamAxis = vec2(cos(spinPhase), sin(spinPhase));
    float xDistance = abs(dot(uv - lightUV, beamAxis));
    float layer80 = 0.10     * lightLayer(xDistance, 1.568);
    float layer60 = 0.133333 * lightLayer(xDistance, 1.176);
    float layer40 = 0.166667 * lightLayer(xDistance, 0.784);
    float layer20 = 0.20     * lightLayer(xDistance, 0.392);
    float layers = max(max(layer80, layer60), max(layer40, layer20));
    layers = 0.65 * layers + 0.35 * (0.20 * exp(-(xDistance * xDistance) / (0.52 * 0.52)));

    vec2 toLight = (lightUV - uv) * vec2(1.7777778, 1.0);
    float reach = exp(-dot(toLight, toLight) / (0.90 * 0.90));
    vec3 lightDir = normalize(vec3(toLight, 0.42));
    vec3 halfVec = normalize(lightDir + vec3(0.0, 0.0, 1.0));

    float relief = smoothstep(0.015, 0.30, length(n.xy));
    float dirBias = 0.55 + 0.45 * smoothstep(-0.30, 0.35, dot(n.xy, lightDir.xy));
    float slope = relief * dirBias;
    float spec = pow(max(dot(n, halfVec), 0.0), 14.0);

    float baseLum = dot(base, vec3(0.299, 0.587, 0.114));
    float surfaceMask = smoothstep(0.014, 0.065, baseLum);

    vec2 r1 = vec2(7.0 / 1280.0, 7.0 / 720.0);
    vec2 r2 = vec2(13.0 / 1280.0, 13.0 / 720.0);
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

    float beam = layers * reach;
    vec3 warm = vec3(1.0, 0.93, 0.80);
    float diffGain = beam * (slope * 11.0 + bloomEnergy * 6.5);
    float lightBloom = beam * (surfaceMask * slope * (0.32 + 1.05 * spec) * 0.95 + bloomEnergy * 1.15);
    vec3 lightAdd = base * warm * diffGain + warm * lightBloom;
    return clamp(base + (vec3(1.0) - base) * (vec3(1.0) - exp(-lightAdd)), 0.0, 1.0);
}

// ------------------------------------------------------- shipped pipeline
vec3 shippedAt(vec2 uv, float beamGain, float useBake)
{
    vec3 base = colorAt(uv);
    if (useBake > 0.5) {
        float occlusion;
        bakedAO(uv, occlusion);
        float a = clamp(occlusion * uStrength, 0.0, 1.0);
        vec3 warm = vec3(0.72, 0.62, 0.52);
        base = base * (1.0 - a) + warm * base * a;
        base = clamp((base - 0.5) * 1.04 + 0.5, 0.0, 1.0);
    }
    vec3 blended = base;
    if (uLight > 0.5) {
        vec2 lightUV = beamLightUV();
        blended = bgMovingLights(base, normalAt(uv), uv, lightUV, 0.0);
        if (uMode == 1 && beamGain < 0.999) {
            // Knock the occlusion out of the beam's *addition* only. The art itself is
            // untouched, which is the rule this whole page is meant to demonstrate.
            blended = base + (blended - base) * beamGain;
        }
    }

    if (uFog > 0.5 && uv.y > 0.38) {
        float yMask = smoothstep(0.38, 0.88, uv.y);
        float lum = dot(blended, vec3(0.299, 0.587, 0.114));
        float depthMask = smoothstep(0.78, 0.18, lum);
        float w1 = sin(uv.x * 6.283 + uTime * 0.32) * 0.5 + 0.5;
        float w2 = sin(uv.x * 14.137 + uv.y * 3.1415 - uTime * 0.22) * 0.5 + 0.5;
        float w3 = sin(uv.x * 4.712 + uv.y * 7.854 + uTime * 0.15) * 0.5 + 0.5;
        float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
        float fogPatch = smoothstep(0.15, 0.75, drift);        // 'patch' is reserved in ES 3.00
        float fogAlpha = clamp(depthMask * yMask * fogPatch * 0.80, 0.0, 0.55);
        if (uMode == 0) {
            // Same fog budget, redistributed: ridges lose their fog, valleys gain up to 1.9x.
            float low = heightFogWeight(uv);
            fogAlpha = fogAlpha * (1.0 - uStrength) + fogAlpha * 1.9 * uStrength * low;
        }
        const vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
        blended = mix(blended, fogColor, clamp(fogAlpha, 0.0, 0.55));
    }

    float vignette = smoothstep(0.75, 0.25, length(uv - vec2(0.5)));
    return blended * vignette;
}

void main()
{
    vec2 uv = vUV;

    float beamGain = 1.0;
    if (uMode == 1) beamGain = beamReach(uv);

    // Only the term the mode is about differs between the two sides of the wipe.
    vec3 before = shippedAt(uv, 1.0, 0.0);
    vec3 after  = shippedAt(uv, beamGain, uMode == 2 ? 1.0 : 0.0);

    outColor = vec4(mix(before, after, uMix), 1.0);
}
