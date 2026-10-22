#include "Efx.h"

#include "cinder/app/App.h"
#include "cinder/Camera.h"

using namespace ci;

namespace kv4d {

void Efx::setup() {
    auto vert = app::loadAsset("shaders/fullscreen.vert");
    mTrailProg = gl::GlslProg::create(vert, app::loadAsset("shaders/trails.frag"));
    mGlowProg = gl::GlslProg::create(vert, app::loadAsset("shaders/glow.frag"));
    auto quad = geom::Rect(Rectf(-1, -1, 1, 1)).texCoords(vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 1));
    mTrailQuad = gl::Batch::create(quad, mTrailProg);
    mGlowQuad = gl::Batch::create(quad, mGlowProg);
    resize(app::toPixels(app::getWindowSize()));
}

void Efx::resize(ivec2 size) {
    size = glm::max(size, ivec2(1));
    gl::Fbo::Format scene;
    scene.samples(4).colorTexture(gl::Texture2d::Format().internalFormat(GL_RGBA16F));
    mScene = gl::Fbo::create(size.x, size.y, scene);

    gl::Fbo::Format trail;
    trail.disableDepth().colorTexture(gl::Texture2d::Format().internalFormat(GL_RGBA16F));
    for (int i = 0; i < 2; i++) {
        mTrail[i] = gl::Fbo::create(size.x, size.y, trail);
        gl::ScopedFramebuffer fb(mTrail[i]);
        gl::clear(Color::black());
    }
}

void Efx::begin(Color background) {
    gl::context()->pushFramebuffer(mScene);
    gl::pushViewport(ivec2(0), mScene->getSize());
    gl::clear(background);
}

void Efx::pass(const gl::GlslProgRef& prog) {
    gl::ScopedMatrices mats;
    gl::setMatrices(CameraOrtho(-1, 1, -1, 1, -1, 1));
    (prog == mTrailProg ? mTrailQuad : mGlowQuad)->draw();
}

void Efx::end() {
    gl::popViewport();
    gl::context()->popFramebuffer();

    gl::ScopedDepth depth(false);
    gl::ScopedBlend blend(false);

    // Trails: max(current, history * decay) into the other trail buffer.
    int next = 1 - mTrailCurrent;
    {
        gl::ScopedFramebuffer fb(mTrail[next]);
        gl::ScopedViewport vp(ivec2(0), mTrail[next]->getSize());
        gl::ScopedTextureBind t0(mScene->getColorTexture(), 0);
        gl::ScopedTextureBind t1(mTrail[mTrailCurrent]->getColorTexture(), 1);
        gl::ScopedGlslProg sp(mTrailProg);
        mTrailProg->uniform("uCurrent", 0);
        mTrailProg->uniform("uHistory", 1);
        mTrailProg->uniform("uDecay", trailDecay);
        pass(mTrailProg);
    }
    mTrailCurrent = next;

    // Glow straight to the window.
    gl::ScopedViewport vp(ivec2(0), app::toPixels(app::getWindowSize()));
    gl::ScopedTextureBind t0(mTrail[mTrailCurrent]->getColorTexture(), 0);
    gl::ScopedGlslProg sp(mGlowProg);
    mGlowProg->uniform("uTexture", 0);
    mGlowProg->uniform("uTexelSize", vec2(1.0f) / vec2(mTrail[mTrailCurrent]->getSize()));
    mGlowProg->uniform("uThreshold", glowThreshold);
    mGlowProg->uniform("uIntensity", glowIntensity);
    mGlowProg->uniform("uRadius", glowIntensity > 0.0f ? glowRadius : 0.0f);
    pass(mGlowProg);
}

} // namespace kv4d
