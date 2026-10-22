#version 150

uniform int uPoints;

in vec4 vColor;
in vec3 vNormal;

out vec4 oColor;

void main() {
    if (uPoints == 1) {
        vec2 c = gl_PointCoord * 2.0 - 1.0;
        float r = dot(c, c);
        if (r > 1.0) discard;
        oColor = vec4(vColor.rgb, vColor.a * (1.0 - r * r));
        return;
    }
    // Same three-light setup as kv's instance.frag
    vec3 n = normalize(vNormal);
    float key = max(dot(n, normalize(vec3(0.5, 0.5, -1.0))), 0.0);
    float fill = max(dot(n, normalize(vec3(-0.5, 0.2, -0.5))), 0.0) * 0.4;
    float rim = max(dot(n, normalize(vec3(0.0, 1.0, 1.0))), 0.0) * 0.3;
    float lighting = min(0.5 + key * 0.7 + fill + rim, 1.5);
    oColor = vec4(vColor.rgb * lighting, vColor.a);
}
