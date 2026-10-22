#version 150
// Feedback trails: keep the brighter of the new frame and the decayed history.

uniform sampler2D uCurrent;
uniform sampler2D uHistory;
uniform float uDecay;

in vec2 vTexCoord;
out vec4 oColor;

void main() {
    vec4 cur = texture(uCurrent, vTexCoord);
    vec4 hist = texture(uHistory, vTexCoord) * uDecay;
    oColor = vec4(max(cur.rgb, hist.rgb), 1.0);
}
