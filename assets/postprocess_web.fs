#version 100
precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float bloom_intensity;
uniform float crt_intensity;
uniform float gamma;

vec3 bloomContrib(vec3 c)
{
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    float bright = clamp((lum - 0.65) * 2.0, 0.0, 1.0);
    return c * bright;
}

void main()
{
    vec4 baseColor = texture2D(texture0, fragTexCoord);
    vec3 color = baseColor.rgb;

    if (bloom_intensity > 0.001) {
        const vec2 stepUV = vec2(4.0 / 1280.0, 4.0 / 720.0);

        // 9-tap normalized blur reusing baseColor for the center tap (weights sum to 1.0)
        vec3 bloomAcc = bloomContrib(color) * 0.25;
        bloomAcc += (
            bloomContrib(texture2D(texture0, fragTexCoord + vec2(-stepUV.x, 0.0)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2( stepUV.x, 0.0)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2(0.0, -stepUV.y)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2(0.0,  stepUV.y)).rgb)
        ) * 0.125;
        bloomAcc += (
            bloomContrib(texture2D(texture0, fragTexCoord + vec2(-stepUV.x, -stepUV.y)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2( stepUV.x, -stepUV.y)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2(-stepUV.x,  stepUV.y)).rgb) +
            bloomContrib(texture2D(texture0, fragTexCoord + vec2( stepUV.x,  stepUV.y)).rgb)
        ) * 0.0625;

        color += bloomAcc * (bloom_intensity * 1.5);
    }

    // Authentic analog CRT beam: smooth sine-wave scanline raster
    if (crt_intensity > 0.001) {
        float s = max(0.0, 0.5 + 0.5 * sin(fragTexCoord.y * 2261.94671));
        float beam = pow(s, 0.35);
        color *= mix(1.0, beam, crt_intensity * 0.7);
    }

    // Apply gamma curve only when non-neutral
    float g = (gamma > 0.1) ? gamma : 1.0;
    if (abs(g - 1.0) > 0.005) {
        color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / g));
    } else {
        color = clamp(color, 0.0, 1.0);
    }

    gl_FragColor = vec4(color, baseColor.a) * colDiffuse * fragColor;
}
