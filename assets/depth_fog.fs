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
    vec3 blended = texel.rgb;
    if (lightEnabled > 0.5) {
        // Filter normal vectors, not the image: soften small embossed edges.
        // Backgrounds and normals are both 1280x720, including the web build.
        vec2 stepUV = vec2(2.5 / 1280.0, 2.5 / 720.0);
        vec3 encoded = texture(normalMap, fragTexCoord).rgb * 4.0;
        encoded += texture(normalMap, fragTexCoord + vec2(stepUV.x, 0.0)).rgb;
        encoded += texture(normalMap, fragTexCoord - vec2(stepUV.x, 0.0)).rgb;
        encoded += texture(normalMap, fragTexCoord + vec2(0.0, stepUV.y)).rgb;
        encoded += texture(normalMap, fragTexCoord - vec2(0.0, stepUV.y)).rgb;
        vec3 n = normalize((encoded / 8.0) * 2.0 - 1.0);

        // Moving sun: a bright pool sweeps left/right and back. Away from the
        // pool the scene falls to night, so the light has a visible position.
        float sweep = sin(time * 0.085);
        float sunX = 0.5 + 0.45 * sweep;
        vec3 l = normalize(vec3(sweep * 0.85, -0.25, 0.55));
        float diffuse = max(dot(n, l), 0.0);
        // Wide smooth horizontal falloff, full height: no hard pool edges.
        float pool = 1.0 - smoothstep(0.22, 0.95,
                                      abs((fragTexCoord.x - sunX) * 1.77778));
        // Night outside the effect; only the pool is brightened, strongly.
        float illumination = 0.58
            + pool * (0.80 + 0.40 * diffuse + eventFlash * 2.2);
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

    finalColor = vec4(blended * vignette, texel.a) * colDiffuse * fragColor;
}
