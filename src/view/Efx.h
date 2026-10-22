// Efx.h - scene FBO -> feedback trails -> glow -> screen.

#pragma once

#include "cinder/gl/gl.h"

namespace kv4d {

class Efx {
public:
    void setup();
    void resize(ci::ivec2 size);

    void begin(ci::Color background);  // binds the scene FBO
    void end();                        // runs the passes and draws to the screen

    float trailDecay = 0.0f;           // 0 = off, ~0.9 = long trails
    float glowThreshold = 0.6f;
    float glowIntensity = 0.0f;        // 0 = off
    float glowRadius = 6.0f;

private:
    void pass(const ci::gl::GlslProgRef& prog);

    ci::gl::FboRef mScene, mTrail[2];
    int mTrailCurrent = 0;
    ci::gl::GlslProgRef mTrailProg, mGlowProg;
    ci::gl::BatchRef mTrailQuad, mGlowQuad;
};

} // namespace kv4d
