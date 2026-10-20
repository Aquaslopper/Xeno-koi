#version 100
precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float time;
uniform sampler2D normalMap;
uniform float lightEnabled;

void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);
    vec3 blended = texel.rgb;

    // Harvest Option B: the normal texture's alpha is the smoothed height
    // mask. A directional sun behind the viewer slowly sweeps left/right
    // across the top at constant front elevation; flat/low areas and every
    // other background keep a factor of exactly 1.0. No spotlight is used.
    if (lightEnabled > 0.5) {
        vec4 normalTexel = texture2D(normalMap, fragTexCoord);
        vec3 n = normalize(normalTexel.rgb * 2.0 - 1.0);
        float sunAngle = 1.10 * sin(time * 0.0523599); // 120-second smooth sweep
        vec3 l = normalize(vec3(0.70710678 * sin(sunAngle),
                                -0.70710678 * cos(sunAngle), 0.70710678));
        float diffuse = clamp(dot(n, l), 0.0, 1.0);
        float directional = clamp((diffuse - l.z) / (1.0 - l.z), 0.0, 1.0);
        float response = 0.18 * directional;
        blended = clamp(texel.rgb + vec3(1.0, 0.93, 0.80) * (normalTexel.a * response), vec3(0.0), vec3(1.0));
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
