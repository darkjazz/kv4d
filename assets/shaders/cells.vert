#version 150
// Instanced 4D cell renderer. One instance per cell; cell coordinates come from
// gl_InstanceID and state from the simulation atlas, so nothing is read back.
// The placement maths is mirrored in Renderer4D::place() for the wireframe.

uniform mat4 ciModelViewProjection;
uniform mat4 ciModelView;
uniform mat3 ciNormalMatrix;

uniform sampler2D uState;
uniform ivec4 uDims;
uniform mat4  uRot4;         // 4D rotation
uniform int   uModeA;        // 0 project, 1 slice, 2 stack, 3 grid
uniform int   uModeB;
uniform float uModeMix;      // 0 = A, 1 = B
uniform float uProjDist;     // 4D eye distance, in units of the extent
uniform float uSliceW;       // -0.5..0.5
uniform float uSliceWidth;
uniform float uGridSpacing;
uniform float uExtent;       // world size of the hypercube
uniform float uCellScale;
uniform float uPhase;        // 0..1 interpolation between previous and current state
uniform int   uRuleType;
uniform int   uNumStates;
uniform float uThreshold;
uniform vec3  uColorA;
uniform vec3  uColorB;
uniform float uAlpha;
uniform float uAlphaScale;   // < 1 in additive mode so overlapping w layers don't saturate
uniform int   uPoints;
uniform float uPointSize;

in vec4 ciPosition;
in vec3 ciNormal;

out vec4 vColor;
out vec3 vNormal;

float intensity(float s) {
    if (uRuleType == 1) return s;
    if (s <= 0.0) return 0.0;
    if (s <= 1.0) return 1.0;
    return 1.0 - (s - 1.0) / float(max(uNumStates - 1, 1));
}

// Places a cell for one view mode. p is the unrotated position in [-0.5, 0.5]^4,
// q the rotated one. Returns the 3D position in extent units; size scales the
// cell and t (0..1) picks the colour along the w axis.
vec3 place(int mode, vec4 p, vec4 q, float layer, out float size, out float t) {
    if (mode == 0) {
        float f = uProjDist / max(uProjDist - q.w, 0.05);
        size = f;
        t = clamp(q.w + 0.5, 0.0, 1.0);
        return q.xyz * f;
    }
    if (mode == 1) {
        float d = abs(q.w - uSliceW);
        size = 1.0 - smoothstep(0.0, uSliceWidth, d);
        t = clamp(q.w + 0.5, 0.0, 1.0);
        return q.xyz;
    }
    if (mode == 2) {
        size = 1.0;
        t = clamp(q.w + 0.5, 0.0, 1.0);
        return q.xyz;
    }
    int cols = int(ceil(sqrt(float(uDims.w))));
    int li = int(layer);
    vec2 cell = vec2(li % cols, li / cols) - vec2(float(cols - 1) * 0.5, float((uDims.w + cols - 1) / cols - 1) * 0.5);
    size = 1.0 / float(cols);
    t = float(li) / float(max(uDims.w - 1, 1));
    // Shrink so the whole grid spans about 1.6 extents.
    return (p.xyz + vec3(cell.x, -cell.y, 0.0) * uGridSpacing) * (1.6 / (float(cols) * uGridSpacing));
}

void main() {
    int id = gl_InstanceID;
    int x = id % uDims.x; id /= uDims.x;
    int y = id % uDims.y; id /= uDims.y;
    int z = id % uDims.z;
    int w = id / uDims.z;

    vec2 st = texelFetch(uState, ivec2(x + uDims.x * z, y + uDims.y * w), 0).rg;
    float s = mix(intensity(st.g), intensity(st.r), uPhase);

    vec4 p = (vec4(x, y, z, w) + 0.5) / vec4(uDims) - 0.5;
    vec4 q = uRot4 * p;

    float sizeA, sizeB, tA, tB;
    vec3 posA = place(uModeA, p, q, float(w), sizeA, tA);
    vec3 posB = place(uModeB, p, q, float(w), sizeB, tB);
    vec3 pos = mix(posA, posB, uModeMix);
    float size = mix(sizeA, sizeB, uModeMix) * s;
    float t = mix(tA, tB, uModeMix);

    if (s <= uThreshold || size < 0.01) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);  // outside the clip volume
        vColor = vec4(0.0);
        vNormal = vec3(0.0, 0.0, 1.0);
        return;
    }

    float cell = uExtent / float(max(max(uDims.x, uDims.y), max(uDims.z, uDims.w))) * uCellScale;
    vec3 world = pos * uExtent;
    if (uPoints == 0) world += ciPosition.xyz * cell * size;

    gl_Position = ciModelViewProjection * vec4(world, 1.0);
    if (uPoints == 1) {
        gl_PointSize = max(1.0, uPointSize * size * 300.0 / gl_Position.w);
    }
    vColor = vec4(mix(uColorA, uColorB, t), uAlpha * uAlphaScale * clamp(s, 0.0, 1.0));
    vNormal = ciNormalMatrix * ciNormal;
}
