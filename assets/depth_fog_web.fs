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
    float baseLum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    vec3 blended = texel.rgb;
    if (lightEnabled > 0.5) {
        // Filter normal vectors, not the image: soften small embossed edges.
        // Backgrounds and normals are both 1280x720, including the web build.
        vec2 stepUV = vec2(2.5 / 1280.0, 2.5 / 720.0);
        vec3 encoded = texture2D(normalMap, fragTexCoord).rgb * 4.0;
        encoded += texture2D(normalMap, fragTexCoord + vec2(stepUV.x, 0.0)).rgb;
        encoded += texture2D(normalMap, fragTexCoord - vec2(stepUV.x, 0.0)).rgb;
        encoded += texture2D(normalMap, fragTexCoord + vec2(0.0, stepUV.y)).rgb;
        encoded += texture2D(normalMap, fragTexCoord - vec2(0.0, stepUV.y)).rgb;
        vec3 n = normalize((encoded / 8.0) * 2.0 - 1.0);

        // Distant sun behind the viewer. Constant Z keeps the illumination of
        // a flat surface constant; only the relief responds to the sweep.
        float sunX = 0.78 * sin(time * 0.085);
        vec3 l = vec3(sunX, -sqrt(max(1.0 - sunX * sunX - 0.55 * 0.55, 0.0)), 0.55);
        float diffuse = max(dot(n, l), 0.0);
        // Lambert diffuse + ambient, no ridge thresholds or specular outlines.
        // Event pulses increase the same surface light, not a screen overlay.
        vec3 illumination = vec3(0.56 + (0.80 + eventFlash) * diffuse);
        blended = clamp(texel.rgb * illumination, vec3(0.0), vec3(1.0));
    }

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

    gl_FragColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
