#version 300 es
// Xeno Koi background shader + Parallax Occlusion Mapping.
//
// Everything inside the "moving-lights" block is assets/depth_fog.fs unchanged apart
// from the two sampling points (colour and normal), which are moved from the screen
// pixel to wherever the parallax ray hit the height field.
precision highp float;

in vec2 vUV;               // 0..1 screen space, y down (same convention as the game's fragTexCoord)
out vec4 outColor;

uniform sampler2D uColor;
uniform sampler2D uNormal;
uniform sampler2D uHeight;

uniform vec2  uRes;
uniform float uTime;
uniform float uFlash;        // thunder / capture flash, as in the game
uniform float uLight;        // 0/1  moving normal-map light
uniform float uFog;          // 0/1  depth fog + vignette

uniform vec2  uVantage;      // where the viewer's eye sits, in uv
uniform float uDepth;        // parallax offset in pixels at the edge of the frame
uniform int   uSteps;        // POM march steps
uniform float uPom;          // 0 = flat (what ships today), 1 = full POM
uniform float uAO;
uniform float uShadow;
uniform float uWipe;         // x position of the A/B divider (0 = off, full POM)

#define MAX_STEPS 64

float heightAt(vec2 uv) { return texture(uHeight, clamp(uv, 0.0, 1.0)).r; }
vec3  colorAt (vec2 uv) { return texture(uColor,  clamp(uv, 0.0, 1.0)).rgb; }
vec3  normalAt(vec2 uv) { return texture(uNormal, clamp(uv, 0.0, 1.0)).rgb; }

// --------------------------------------------------------------- POM
vec2 parallax(vec2 uv, out float travel)
{
    vec2 slope = (uv - uVantage) * (uDepth / (0.5 * uRes.x));
    int steps = max(uSteps, 2);
    float layer = 1.0 / float(steps);

    float tPrev = 0.0;
    float prevH = heightAt(uv);
    float hitT = 1.0;
    bool hit = false;

    for (int i = 1; i <= MAX_STEPS; i++) {
        if (i > steps) break;
        float t = float(i) * layer;
        vec2 c = uv - slope * t;
        float h = heightAt(c);
        if (!hit && h < t) {
            float den = (prevH - tPrev) - (h - t);
            float frac = clamp((prevH - tPrev) / (abs(den) < 1e-6 ? 1e-6 : den), 0.0, 1.0);
            hitT = tPrev + frac * layer;
            hit = true;
        }
        if (!hit) { tPrev = t; prevH = h; }
    }
    travel = hit ? hitT : 1.0;
    return uv - slope * hitT;
}

float contactShadow(vec2 hitUV, vec2 screenUV)
{
    // The same drifting light the beam uses, so shadows point away from it.
    vec2 lightUV = vec2(0.08 + 0.84 * (0.5 - 0.5 * cos(uTime * 0.1954768)), 0.50);
    vec2 toLight = normalize((lightUV - screenUV) * vec2(1.7777778, 1.0) + vec2(1e-4));
    float h0 = heightAt(hitUV);
    float occ = 0.0, wsum = 0.0;
    for (int i = 1; i <= 8; i++) {
        float f = float(i) / 8.0;
        float w = 1.0 - f * f;
        float px = f * 30.0;
        vec2 p = hitUV + toLight * px / uRes;
        float ray = h0 + (px * 0.75) / max(uDepth, 1.0);
        occ += w * clamp((heightAt(p) - ray) * 5.0, 0.0, 1.0);
        wsum += w;
    }
    return occ / wsum;
}

// --------------------------------------------------------------- the game's light
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

vec3 bgMovingLights(vec3 base, vec3 mapRGB, vec2 uv, vec2 sampleUV)
{
    vec3 n = normalize(mapRGB * 2.0 - 1.0);

    float driftPhase = uTime * 0.1954768;
    float spinPhase  = uTime * 0.2150244;
    vec2 lightUV = vec2(0.08 + 0.84 * (0.5 - 0.5 * cos(driftPhase)), 0.50);
    vec2 beamAxis = vec2(cos(spinPhase), sin(spinPhase));
    float xDistance = abs(dot(uv - lightUV, beamAxis));
    float layer80 = 0.10      * lightLayer(xDistance, 1.568);
    float layer60 = 0.133333  * lightLayer(xDistance, 1.176);
    float layer40 = 0.166667  * lightLayer(xDistance, 0.784);
    float layer20 = 0.20      * lightLayer(xDistance, 0.392);
    float layers = max(max(layer80, layer60), max(layer40, layer20));
    layers = 0.65 * layers + 0.35 * (0.20 * exp(-(xDistance * xDistance) / (0.52 * 0.52)));

    vec2 toLight = (lightUV - uv) * vec2(1.7777778, 1.0);
    float dist2 = dot(toLight, toLight);
    float reach = exp(-dist2 / (0.90 * 0.90));
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
        sampleRidgeEmit(sampleUV + vec2(-r1.x, 0.0), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2( r1.x, 0.0), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2(0.0, -r1.y), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2(0.0,  r1.y), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2(-r2.x, -r2.y), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2( r2.x, -r2.y), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2(-r2.x,  r2.y), lightDir.xy) +
        sampleRidgeEmit(sampleUV + vec2( r2.x,  r2.y), lightDir.xy)
    );

    float flashGain = 1.0 + 0.95 * uFlash;
    float beam = layers * reach * flashGain + uFlash * 0.065 * (0.35 + 0.65 * reach);
    vec3 warm = vec3(1.0, 0.93, 0.80);
    float diffGain = beam * (slope * 11.0 + bloomEnergy * 6.5);
    float lightBloom = beam * (surfaceMask * slope * (0.32 + 1.05 * spec) * 0.95 + bloomEnergy * 1.15);
    vec3 lightAdd = base * warm * diffGain + warm * lightBloom;
    return clamp(base + (vec3(1.0) - base) * (vec3(1.0) - exp(-lightAdd)), 0.0, 1.0);
}

void main()
{
    vec2 uv = vUV;

    // ---- the part that is new: where does this pixel's surface actually sit?
    float travel;
    vec2 hitUV = parallax(uv, travel);
    // A/B split: everything left of uWipe is drawn exactly as the game draws it today.
    float pomAmt = uPom * step(uWipe, uv.x);
    vec2 sampleUV = mix(uv, hitUV, pomAmt);

    vec3 base  = colorAt(sampleUV);
    vec3 mapRGB = normalAt(sampleUV);
    vec3 blended = base;

    if (uLight > 0.5) blended = bgMovingLights(base, mapRGB, uv, sampleUV);

    // Occlusion the march already paid for, plus the short march to the light.
    if (pomAmt > 0.001) {
        float ao = 1.0 - uAO * pow(clamp(travel, 0.0, 1.0), 1.2);
        float sh = contactShadow(hitUV, uv);
        blended *= mix(1.0, ao, pomAmt);
        blended *= mix(1.0, 1.0 - 0.6 * sh, pomAmt * uShadow);
    }

    if (uFog > 0.5 && uv.y > 0.38) {
        float yMask = smoothstep(0.38, 0.88, uv.y);
        float lum = dot(blended, vec3(0.299, 0.587, 0.114));
        float depthMask = smoothstep(0.78, 0.18, lum);
        float w1 = sin(uv.x * 6.283 + uTime * 0.32) * 0.5 + 0.5;
        float w2 = sin(uv.x * 14.137 + uv.y * 3.1415 - uTime * 0.22) * 0.5 + 0.5;
        float w3 = sin(uv.x * 4.712 + uv.y * 7.854 + uTime * 0.15) * 0.5 + 0.5;
        float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
        float patch = smoothstep(0.15, 0.75, drift);
        const vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
        float fogAlpha = clamp(depthMask * yMask * patch * 0.80, 0.0, 0.55);
        blended = mix(blended, fogColor, fogAlpha);
    }

    vec2 uvCentered = uv - vec2(0.5);
    float vignette = smoothstep(0.75, 0.25, length(uvCentered));
    outColor = vec4(blended * vignette, 1.0);
}
