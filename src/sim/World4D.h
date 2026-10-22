// World4D.h - GPU 4D cellular automaton. State lives in two RG32F atlas
// textures that are swapped every generation (r = state, g = previous state).

#pragma once

#include "cinder/gl/gl.h"
#include "Grid4D.h"

namespace kv4d {

class World4D {
public:
    // Throws on FBO/shader failure so the caller can report it.
    void setup(Dims4 dims);
    bool ready() const { return mFbo[0] != nullptr; }

    const Dims4& dims() const { return mDims; }
    Rule4D& rule() { return mRule; }
    const Rule4D& rule() const { return mRule; }

    void step();
    void upload(const Grid4D& grid);     // r = g = grid state
    void download(Grid4D& grid) const;   // r channel only; slow, for tests and queries
    void clear();
    // Hyperball injection, centre in cell coordinates and radius in cells.
    void inject(ci::vec4 center, float radius, float density, float value);

    ci::gl::Texture2dRef stateTexture() const { return mFbo[mCurrent]->getColorTexture(); }
    uint64_t generation() const { return mGeneration; }

private:
    void runPass(const ci::gl::GlslProgRef& prog);

    Dims4 mDims;
    Rule4D mRule;
    ci::gl::FboRef mFbo[2];
    int mCurrent = 0;
    ci::gl::GlslProgRef mStepProg, mInjectProg;
    ci::gl::BatchRef mStepQuad, mInjectQuad;
    uint64_t mGeneration = 0;
    float mInjectSeed = 0.0f;
};

} // namespace kv4d
