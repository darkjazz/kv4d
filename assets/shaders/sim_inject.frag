#version 150
// Stamps live cells into a 4D hyperball (periodic distance), leaving the rest
// untouched. Used for audio-driven and OSC injection.

uniform sampler2D uState;
uniform ivec4 uDims;
uniform vec4  uCenter;    // cell coordinates
uniform float uRadius;    // in cells
uniform float uDensity;   // 0..1 probability per cell inside the ball
uniform float uValue;     // state written (1 for generations, any 0..1 for continuous)
uniform float uSeed;

out vec4 oColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233)) + uSeed * 0.1031) * 43758.5453);
}

void main() {
    ivec2 t = ivec2(gl_FragCoord.xy);
    vec4 st = texelFetch(uState, t, 0);
    vec4 c = vec4(t.x % uDims.x, t.y % uDims.y, t.x / uDims.x, t.y / uDims.y);

    vec4 d = abs(c - uCenter);
    d = min(d, vec4(uDims) - d);  // shortest way round the torus
    if (dot(d, d) <= uRadius * uRadius && hash(vec2(t)) < uDensity) {
        st.r = uValue;
    }
    oColor = st;
}
