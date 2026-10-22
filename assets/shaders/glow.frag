#version 150

uniform sampler2D uTexture;
uniform vec2 uTexelSize;    // 1.0 / texture size
uniform float uThreshold;   // Brightness threshold for glow (0.0 - 1.0)
uniform float uIntensity;   // Glow intensity multiplier
uniform float uRadius;      // Blur radius for glow

in vec2 vTexCoord;
out vec4 oColor;

// Extract luminance
float luminance(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec4 original = texture(uTexture, vTexCoord);

    // Extract bright areas and blur them
    vec4 glow = vec4(0.0);
    float totalWeight = 0.0;

    // Sample in a circular pattern for the blur
    int samples = 12;
    for (int i = 0; i < samples; i++) {
        float angle = float(i) * 6.28318 / float(samples);
        for (float r = 1.0; r <= uRadius; r += 1.0) {
            vec2 offset = vec2(cos(angle), sin(angle)) * r * uTexelSize;
            vec4 sampleColor = texture(uTexture, vTexCoord + offset);

            // Only include pixels above threshold
            float lum = luminance(sampleColor.rgb);
            if (lum > uThreshold) {
                float weight = 1.0 / (r * 0.5 + 1.0);  // Falloff with distance
                glow += (sampleColor - uThreshold) * weight;
                totalWeight += weight;
            }
        }
    }

    // Also check center pixel
    float centerLum = luminance(original.rgb);
    if (centerLum > uThreshold) {
        glow += (original - uThreshold) * 2.0;
        totalWeight += 2.0;
    }

    // Normalize and apply intensity
    if (totalWeight > 0.0) {
        glow /= totalWeight;
        glow *= uIntensity;
    }

    // Additive blend: original + glow
    oColor = original + glow;
    oColor.a = original.a;
}
