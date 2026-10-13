#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float bloom_intensity;
uniform float crt_intensity;
uniform float gamma;

// Output fragment color
out vec4 finalColor;

void main()
{
    vec4 baseColor = texture(texture0, fragTexCoord);
    vec3 color = baseColor.rgb;

    if (bloom_intensity > 0.001) {
        vec2 texelSize = vec2(1.0 / 1280.0, 1.0 / 720.0);
        vec3 bloomAcc = vec3(0.0);
        float totalWeight = 0.0;

        // 9-tap blur with wider radius to create visible phosphor bloom
        for (int x = -1; x <= 1; x++) {
            for (int y = -1; y <= 1; y++) {
                vec2 offset = vec2(float(x), float(y)) * texelSize * 4.0;
                vec3 sCol = texture(texture0, fragTexCoord + offset).rgb;
                float lum = dot(sCol, vec3(0.299, 0.587, 0.114));
                float bright = clamp((lum - 0.65) * 2.0, 0.0, 1.0);
                float weight = (x == 0 && y == 0) ? 0.25 : ((x == 0 || y == 0) ? 0.125 : 0.0625);
                bloomAcc += sCol * bright * weight;
                totalWeight += weight;
            }
        }
        if (totalWeight > 0.0) bloomAcc /= totalWeight;
        color += bloomAcc * bloom_intensity * 1.5;
    }

    // Authentic analog CRT beam: smooth sine-wave scanline raster
    if (crt_intensity > 0.001) {
        float s = 0.5 + 0.5 * sin(fragTexCoord.y * 720.0 * 3.14159265);
        float beam = pow(s, 0.35);
        color *= mix(1.0, beam, crt_intensity * 0.7);
    }

    // Apply gamma curve: color.rgb = pow(color.rgb, vec3(1.0 / gamma));
    float g = (gamma > 0.1) ? gamma : 1.0;
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / g));

    finalColor = vec4(color, baseColor.a) * colDiffuse * fragColor;
}
