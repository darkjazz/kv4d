#include "Renderer4D.h"

#include "cinder/app/App.h"
#include "cinder/gl/VboMesh.h"

#include <algorithm>
#include <cctype>

using namespace ci;

namespace kv4d {

static const char* kModeNames[] = { "project", "slice", "stack", "grid" };
static const char* kPlaneNames[] = { "xy", "xz", "xw", "yz", "yw", "zw" };
// Axis pairs for each plane, matching kPlaneNames.
static const int kPlaneAxes[6][2] = { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 1, 2 }, { 1, 3 }, { 2, 3 } };

const char* viewModeName(ViewMode m) { return kModeNames[glm::clamp((int)m, 0, 3)]; }
const char* planeName(Plane p) { return kPlaneNames[glm::clamp((int)p, 0, 5)]; }

bool planeFromName(const std::string& name, Plane& out) {
    std::string n = name;
    for (auto& c : n) c = (char)std::tolower((unsigned char)c);
    for (int i = 0; i < 6; i++) {
        std::string rev = { kPlaneNames[i][1], kPlaneNames[i][0] };
        if (n == kPlaneNames[i] || n == rev) { out = (Plane)i; return true; }
    }
    return false;
}

void Renderer4D::setup() {
    mProg = gl::GlslProg::create(app::loadAsset("shaders/cells.vert"), app::loadAsset("shaders/cells.frag"));
    mCubes = gl::Batch::create(geom::Cube().size(vec3(1.0f)), mProg);

    vec3 origin(0.0f), normal(0.0f, 0.0f, 1.0f);
    auto layout = gl::VboMesh::Layout().attrib(geom::POSITION, 3).attrib(geom::NORMAL, 3);
    auto mesh = gl::VboMesh::create(1, GL_POINTS, { layout });
    mesh->bufferAttrib(geom::POSITION, sizeof(vec3), &origin);
    mesh->bufferAttrib(geom::NORMAL, sizeof(vec3), &normal);
    mPoints = gl::Batch::create(mesh, mProg);

    resetRotation();
}

void Renderer4D::resetRotation() {
    mAngles.fill(0.0f);
    mSpeeds.fill(0.0f);
    mSpeeds[(int)Plane::XW] = 0.21f;
    mSpeeds[(int)Plane::ZW] = 0.13f;
    mSpeeds[(int)Plane::XZ] = 0.05f;
}

void Renderer4D::setMode(ViewMode m, float fadeSeconds) {
    if (m == mode() && mModeMix == 0.0f) return;
    // Projections overlap every w layer, so they read better additive;
    // slices and the grid are best as solid lit cubes.
    mParams.additive = (m == ViewMode::PROJECT || m == ViewMode::STACK);
    mModeA = mode();
    mModeB = m;
    if (fadeSeconds <= 0.0f) {
        mModeA = m;
        mModeMix = 0.0f;
        mFadeRate = 0.0f;
    } else {
        mModeMix = 0.0f;
        mFadeRate = 1.0f / fadeSeconds;
    }
}

mat4 Renderer4D::rotation4() const {
    mat4 r(1.0f);
    for (int i = 0; i < 6; i++) {
        if (mAngles[i] == 0.0f) continue;
        int a = kPlaneAxes[i][0], b = kPlaneAxes[i][1];
        mat4 g(1.0f);
        float c = cosf(mAngles[i]), s = sinf(mAngles[i]);
        // glm is column-major: g[col][row]
        g[a][a] = c;  g[b][a] = -s;
        g[a][b] = s;  g[b][b] = c;
        r = g * r;
    }
    return r;
}

void Renderer4D::update(float dt, float wSpeedScale, float sliceOffset) {
    mTime += dt;
    for (int i = 0; i < 6; i++) {
        bool involvesW = kPlaneAxes[i][1] == 3;
        mAngles[i] = fmodf(mAngles[i] + mSpeeds[i] * (involvesW ? wSpeedScale : 1.0f) * dt, 2.0f * (float)M_PI);
    }
    if (mFadeRate > 0.0f) {
        mModeMix += mFadeRate * dt;
        if (mModeMix >= 1.0f) {
            mModeA = mModeB;
            mModeMix = 0.0f;
            mFadeRate = 0.0f;
        }
    }
    float sweep = mParams.sliceSweep > 0.0f ? 0.45f * sinf(mTime * mParams.sliceSweep * 2.0f * (float)M_PI) : 0.0f;
    mSliceNow = glm::clamp(mParams.sliceW + sweep + sliceOffset, -0.5f, 0.5f);
}

// CPU mirror of place() in cells.vert, for geometry drawn alongside the cells.
vec3 Renderer4D::place(ViewMode m, vec4 p, vec4 q, float& size) const {
    switch (m) {
        case ViewMode::PROJECT: {
            float f = mParams.projDist / std::max(mParams.projDist - q.w, 0.05f);
            size = f;
            return vec3(q) * f;
        }
        case ViewMode::SLICE:
            size = 1.0f - glm::smoothstep(0.0f, mParams.sliceWidth, fabsf(q.w - mSliceNow));
            return vec3(q);
        case ViewMode::STACK:
            size = 1.0f;
            return vec3(q);
        default:
            size = 0.0f;  // no single tesseract in grid mode
            return vec3(p);
    }
}

void Renderer4D::drawTesseract() {
    mat4 r = rotation4();
    vec3 pts[16];
    float vis[16];
    for (int i = 0; i < 16; i++) {
        vec4 p((i & 1) ? 0.5f : -0.5f, (i & 2) ? 0.5f : -0.5f, (i & 4) ? 0.5f : -0.5f, (i & 8) ? 0.5f : -0.5f);
        vec4 q = r * p;
        float sa, sb;
        vec3 a = place(mModeA, p, q, sa);
        vec3 b = place(mModeB, p, q, sb);
        pts[i] = glm::mix(a, b, mModeMix) * mParams.extent;
        // Visible unless the mode is grid; slice mode shows it dimly.
        auto modeVis = [](ViewMode m) { return m == ViewMode::GRID ? 0.0f : (m == ViewMode::SLICE ? 0.35f : 1.0f); };
        vis[i] = glm::mix(modeVis(mModeA), modeVis(mModeB), mModeMix);
    }
    gl::VertBatch vb(GL_LINES);
    vec3 col = glm::mix(mParams.colorA, mParams.colorB, 0.5f);
    for (int i = 0; i < 16; i++) {
        for (int bit = 1; bit < 16; bit <<= 1) {
            int j = i | bit;
            if (j == i) continue;
            vb.color(ColorA(col.x, col.y, col.z, 0.35f * vis[i]));
            vb.vertex(pts[i]);
            vb.color(ColorA(col.x, col.y, col.z, 0.35f * vis[j]));
            vb.vertex(pts[j]);
        }
    }
    gl::ScopedGlslProg sp(gl::getStockShader(gl::ShaderDef().color()));
    vb.draw();
}

void Renderer4D::draw(const World4D& world, float phase, float sizeScale) {
    if (!world.ready()) return;
    const Dims4& d = world.dims();
    const Rule4D& rule = world.rule();

    gl::ScopedBlend blend(GL_SRC_ALPHA, mParams.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    gl::ScopedDepthTest depthTest(!mParams.additive);
    gl::ScopedDepthWrite depthWrite(!mParams.additive);

    if (mParams.tesseract) drawTesseract();

    gl::ScopedTextureBind tex(world.stateTexture(), 0);
    mProg->uniform("uState", 0);
    mProg->uniform("uDims", ivec4(d.x, d.y, d.z, d.w));
    mProg->uniform("uRot4", rotation4());
    mProg->uniform("uModeA", (int)mModeA);
    mProg->uniform("uModeB", (int)mModeB);
    mProg->uniform("uModeMix", mModeMix);
    mProg->uniform("uProjDist", mParams.projDist);
    mProg->uniform("uSliceW", mSliceNow);
    mProg->uniform("uSliceWidth", mParams.sliceWidth);
    mProg->uniform("uGridSpacing", mParams.gridSpacing);
    mProg->uniform("uExtent", mParams.extent);
    mProg->uniform("uCellScale", mParams.cellScale * sizeScale);
    mProg->uniform("uPhase", phase);
    mProg->uniform("uRuleType", (int)rule.type);
    mProg->uniform("uNumStates", rule.numStates);
    mProg->uniform("uThreshold", mParams.threshold);
    mProg->uniform("uColorA", mParams.colorA);
    mProg->uniform("uColorB", mParams.colorB);
    mProg->uniform("uAlpha", mParams.alpha);
    mProg->uniform("uAlphaScale", mParams.additive ? std::min(1.0f, 1.5f / (float)d.w) : 1.0f);
    mProg->uniform("uPoints", mParams.points ? 1 : 0);
    mProg->uniform("uPointSize", mParams.pointSize * sizeScale);

    if (mParams.points) {
        gl::enable(GL_PROGRAM_POINT_SIZE);
        mPoints->drawInstanced(d.count());
        gl::disable(GL_PROGRAM_POINT_SIZE);
    } else {
        mCubes->drawInstanced(d.count());
    }
}

} // namespace kv4d
