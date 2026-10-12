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

void main()
{
    vec4 texel = texture(texture0, fragTexCoord);

    // 1. The Depth Mask (Luminance):
    // Raised luminance threshold so fog applies to mid-tones as well, not just pitch-black pixels.
    float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    float depthMask = smoothstep(0.78, 0.18, lum);

    // 2. The Y-Axis Mask:
    // Fog exists on bottom half of screen, smoothly fading towards middle.
    float yMask = smoothstep(0.38, 0.88, fragTexCoord.y);

    // 3. The Patchy Drift:
    // Overlapping sine waves creating active rolling patchy mist.
    float w1 = sin(fragTexCoord.x * 6.283 + time * 0.32) * 0.5 + 0.5;
    float w2 = sin(fragTexCoord.x * 14.137 + fragTexCoord.y * 3.1415 - time * 0.22) * 0.5 + 0.5;
    float w3 = sin(fragTexCoord.x * 4.712 + fragTexCoord.y * 7.854 + time * 0.15) * 0.5 + 0.5;
    float drift = w1 * 0.50 + w2 * 0.30 + w3 * 0.20;
    float patch = smoothstep(0.15, 0.75, drift);

    // 4. Fog Blend:
    // Pure black/shadow fog (#050505) with zero green/teal tint
    vec3 fogColor = vec3(0.0196, 0.0196, 0.0196);
    float fogAlpha = clamp(depthMask * yMask * patch * 1.35, 0.0, 0.88);
    vec3 blended = mix(texel.rgb, fogColor, fogAlpha);

    // 5. Ambient Vignette:
    // Distance from center of UV coordinates (0.5, 0.5); smoothly darkens edges.
    vec2 uvCentered = fragTexCoord - vec2(0.5, 0.5);
    float dist = length(uvCentered);
    float vignette = smoothstep(0.75, 0.25, dist);

    // 6. Ambient Tint:
    // Pure neutral luminance, no green/teal tint
    vec3 ambientTint = vec3(1.0, 1.0, 1.0);

    vec3 finalRgb = blended * vignette * ambientTint;
    finalColor = vec4(finalRgb, texel.a) * colDiffuse * fragColor;
}
