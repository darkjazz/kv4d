#version 150
// One 4D CA generation. The 4D grid is packed into a 2D atlas:
//   u = x + X*z, v = y + Y*w
// Output: r = next state, g = current state (kept for render interpolation).
// Must match Grid4D::step on the CPU exactly for Generations rules.

uniform sampler2D uState;
uniform ivec4 uDims;          // X, Y, Z, W
uniform int   uRuleType;      // 0 generations, 1 continuous
uniform int   uNhood;         // 0 Moore (80), 1 von Neumann (8)
uniform int   uNumStates;
uniform int   uBirth[81];
uniform int   uSurvive[81];
uniform float uAdd;

out vec4 oColor;

ivec4 toCell(ivec2 t) {
    return ivec4(t.x % uDims.x, t.y % uDims.y, t.x / uDims.x, t.y / uDims.y);
}

float stateAt(ivec4 c) {
    c = (c + uDims) % uDims;  // periodic; offsets are never below -1
    return texelFetch(uState, ivec2(c.x + uDims.x * c.z, c.y + uDims.y * c.w), 0).r;
}

void main() {
    ivec4 cell = toCell(ivec2(gl_FragCoord.xy));
    float self = stateAt(cell);

    int count = 0;
    float sum = 0.0;
    int n = 0;
    for (int dw = -1; dw <= 1; dw++)
    for (int dz = -1; dz <= 1; dz++)
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
        int manhattan = abs(dx) + abs(dy) + abs(dz) + abs(dw);
        if (manhattan == 0) continue;
        if (uNhood == 1 && manhattan != 1) continue;
        float s = stateAt(cell + ivec4(dx, dy, dz, dw));
        count += (s == 1.0) ? 1 : 0;
        sum += s;
        n++;
    }

    float next;
    if (uRuleType == 1) {
        next = fract(sum / float(n) + uAdd);
    } else {
        int state = int(self);
        if (state == 0) {
            next = (uBirth[count] != 0) ? 1.0 : 0.0;
        } else if (state == 1) {
            next = (uSurvive[count] != 0) ? 1.0 : (uNumStates > 2 ? 2.0 : 0.0);
        } else {
            next = (state + 1 < uNumStates) ? float(state + 1) : 0.0;
        }
    }
    oColor = vec4(next, self, 0.0, 1.0);
}
