#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform sampler2D normalMap;
uniform float lightEnabled;
uniform float eventFlash;

// Output fragment color
out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);
    float baseLum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    vec3 surfaceLight = vec3(0.0);
    // Additive surface lighting preserves the approved baked TOP/gamma baseline.
    // Decode the original image-space normals (positive Y points down).
    if (lightEnabled > 0.5) {
        vec3 n = normalize(texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0);
        // Distant directional light sweeps left/right; no point-light footprint.
        float sweep = sin(time * 0.085);
        vec3 l = normalize(vec3(sweep * 0.95, -0.35, 0.55));
        vec3 h = normalize(l + vec3(0.0, 0.0, 1.0));
        float relief = max(dot(n, l), 0.0);
        float spec = pow(max(dot(n, h), 0.0), 28.0);
        vec3 accent = vec3(0.78, 0.88, 1.0);
        // Reject the flat-plane specular response so ridges catch the light.
        float flatSpec = pow(max(h.z, 0.0), 28.0);
        float ridgeSpec = max(spec - flatSpec, 0.0);
        // Subtract the flat-plane response: broad light reveals relief rather
        // than adding an even brightness wash to the whole image.
        float ridgeDiffuse = max(relief - max(l.z, 0.0), 0.0);
        vec3 surface = texel.rgb * ridgeDiffuse * 0.65 + accent * ridgeSpec * 1.30;
        vec3 moving = surface;
        vec3 pulse = surface * eventFlash * 4.0;
        surfaceLight = moving + pulse;
    }
    vec3 blended = min(texel.rgb + surfaceLight, vec3(1.0));

    // 1. The Y-Axis Mask:
    // Fog exists on bottom 62% of screen; skip trig waves completely above y = 0.38.
    if (fragTexCoord.y > 0.38) {
        float yMask = smoothstep(0.38, 0.88, fragTexCoord.y);

        // 2. The Depth Mask (Luminance):
        float lum = baseLum;
        float depthMask = smoothstep(0.78, 0.18, lum);

        // 3. The Patchy Drift:
        float w1 = sin(fragTexCoord.x * 6.283 + time * 0.32) * 0.5 + 0.5;
        float w2 = sin(fragTexCoord.x * 14.137 + fragTexCoord.y * 3.1415 - time * 0.22) * 0.5 + 0.5;
        float w3 = sin(fragTexCoord.x * 4.712 + fragTexCoord.y * 7.854 + time * 0.15) * 0.5 + 0.5;
        float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
        float patch = smoothstep(0.15, 0.75, drift);

        // 4. Fog Blend (#050505):
        const vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
        // Preserve the original fog exactly for Deus / missing-normal fallback.
        float fogGain = lightEnabled > 0.5 ? 1.20 : 1.35;
        float fogCap = lightEnabled > 0.5 ? 0.78 : 0.88;
        float fogAlpha = clamp(depthMask * yMask * patch * fogGain, 0.0, fogCap);
        blended = mix(blended, fogColor, fogAlpha);
    }

    // 5. Ambient Vignette:
    vec2 uvCentered = fragTexCoord - vec2(0.5, 0.5);
    float dist = length(uvCentered);
    float vignette = smoothstep(0.75, 0.25, dist);

    finalColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
