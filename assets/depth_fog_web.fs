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
uniform float eventFlash;

void main()
{
    vec4 texel = texture2D(texture0, fragTexCoord);
    // Additive surface lighting preserves the approved baked TOP/gamma baseline.
    // Decode the original image-space normals (positive Y points down).
    if (lightEnabled > 0.5) {
        vec3 n = normalize(texture2D(normalMap, fragTexCoord).rgb * 2.0 - 1.0);
        vec2 center = vec2(0.5 + 0.36 * sin(time * 0.17),
                           0.45 + 0.28 * cos(time * 0.13));
        vec2 delta = (center - fragTexCoord) * vec2(1.77778, 1.0);
        vec3 l = normalize(vec3(delta, 0.32));
        vec3 h = normalize(l + vec3(0.0, 0.0, 1.0));
        float falloff = exp(-dot(delta, delta) * 2.2);
        float relief = max(dot(n, l), 0.0);
        float spec = pow(max(dot(n, h), 0.0), 28.0);
        vec3 accent = vec3(0.78, 0.88, 1.0);
        vec3 moving = (texel.rgb * relief * 0.14 + accent * spec * 0.07) * falloff;
        vec3 pulse = (texel.rgb * relief + accent * spec * 0.35) * eventFlash;
        texel.rgb = min(texel.rgb + moving + pulse, vec3(1.0));
    }
    vec3 blended = texel.rgb;

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
        blended = mix(texel.rgb, fogColor, fogAlpha);
    }

    // 5. Ambient Vignette:
    vec2 uvCentered = fragTexCoord - vec2(0.5, 0.5);
    float dist = length(uvCentered);
    float vignette = smoothstep(0.75, 0.25, dist);

    gl_FragColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
