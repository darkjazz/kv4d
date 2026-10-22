#include "World4D.h"

#include "cinder/app/App.h"
#include "cinder/Camera.h"

using namespace ci;

namespace kv4d {

void World4D::setup(Dims4 dims) {
    GLint maxTex = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
    if (dims.atlasW() > maxTex || dims.atlasH() > maxTex) {
        throw std::runtime_error("World4D: atlas " + std::to_string(dims.atlasW()) + "x" +
                                 std::to_string(dims.atlasH()) + " exceeds GL_MAX_TEXTURE_SIZE " +
                                 std::to_string(maxTex));
    }
    mDims = dims;

    gl::Texture2d::Format tf;
    tf.setInternalFormat(GL_RG32F);
    tf.setDataType(GL_FLOAT);
    tf.setMinFilter(GL_NEAREST);
    tf.setMagFilter(GL_NEAREST);
    tf.setWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    tf.enableMipmapping(false);

    gl::Fbo::Format ff;
    ff.setColorTextureFormat(tf);
    ff.disableDepth();

    for (int i = 0; i < 2; i++) mFbo[i] = gl::Fbo::create(dims.atlasW(), dims.atlasH(), ff);
    mCurrent = 0;

    if (!mStepProg) {
        auto vert = app::loadAsset("shaders/fullscreen.vert");
        mStepProg = gl::GlslProg::create(vert, app::loadAsset("shaders/sim_step.frag"));
        mInjectProg = gl::GlslProg::create(vert, app::loadAsset("shaders/sim_inject.frag"));
        auto quad = geom::Rect(Rectf(-1, -1, 1, 1));
        mStepQuad = gl::Batch::create(quad, mStepProg);
        mInjectQuad = gl::Batch::create(quad, mInjectProg);
    }
    clear();
}

void World4D::clear() {
    // glClear is unreliable on float attachments; clear explicitly.
    const float zeros[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 2; i++) {
        gl::ScopedFramebuffer fb(mFbo[i]);
        glClearBufferfv(GL_COLOR, 0, zeros);
    }
    mGeneration = 0;
}

void World4D::runPass(const gl::GlslProgRef& prog) {
    int next = 1 - mCurrent;
    gl::ScopedFramebuffer fb(mFbo[next]);
    gl::ScopedViewport vp(ivec2(0), mFbo[next]->getSize());
    gl::ScopedMatrices mats;
    gl::setMatrices(CameraOrtho(-1, 1, -1, 1, -1, 1));
    gl::ScopedDepth depth(false);
    gl::ScopedBlend blend(false);
    gl::ScopedTextureBind tex(mFbo[mCurrent]->getColorTexture(), 0);
    gl::ScopedGlslProg sp(prog);

    prog->uniform("uState", 0);
    prog->uniform("uDims", ivec4(mDims.x, mDims.y, mDims.z, mDims.w));
    (prog == mStepProg ? mStepQuad : mInjectQuad)->draw();
    mCurrent = next;
}

void World4D::step() {
    if (!ready()) return;
    int birth[kMaxNeighbors + 1], survive[kMaxNeighbors + 1];
    for (int i = 0; i <= kMaxNeighbors; i++) {
        birth[i] = mRule.birth[i];
        survive[i] = mRule.survive[i];
    }
    mStepProg->uniform("uRuleType", (int)mRule.type);
    mStepProg->uniform("uNhood", (int)mRule.nhood);
    mStepProg->uniform("uNumStates", mRule.numStates);
    mStepProg->uniform("uBirth", birth, kMaxNeighbors + 1);
    mStepProg->uniform("uSurvive", survive, kMaxNeighbors + 1);
    mStepProg->uniform("uAdd", mRule.add);
    runPass(mStepProg);
    mGeneration++;
}

void World4D::inject(vec4 center, float radius, float density, float value) {
    if (!ready()) return;
    mInjectSeed += 1.0f;
    mInjectProg->uniform("uCenter", center);
    mInjectProg->uniform("uRadius", radius);
    mInjectProg->uniform("uDensity", density);
    mInjectProg->uniform("uValue", value);
    mInjectProg->uniform("uSeed", mInjectSeed);
    runPass(mInjectProg);
}

void World4D::upload(const Grid4D& grid) {
    if (!ready()) return;
    const auto& cells = grid.cells();
    std::vector<float> rg(cells.size() * 2);
    for (size_t i = 0; i < cells.size(); i++) rg[2 * i] = rg[2 * i + 1] = cells[i];
    for (int i = 0; i < 2; i++) {
        mFbo[i]->getColorTexture()->update(rg.data(), GL_RG, GL_FLOAT, 0, mDims.atlasW(), mDims.atlasH());
    }
}

void World4D::download(Grid4D& grid) const {
    if (!ready()) return;
    if (grid.dims().count() != mDims.count()) grid.resize(mDims);
    std::vector<float> rg((size_t)mDims.count() * 2);
    gl::ScopedFramebuffer fb(mFbo[mCurrent]);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, mDims.atlasW(), mDims.atlasH(), GL_RG, GL_FLOAT, rg.data());
    auto& cells = grid.cells();
    for (size_t i = 0; i < cells.size(); i++) cells[i] = rg[2 * i];
}

} // namespace kv4d
