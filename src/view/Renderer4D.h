// Renderer4D.h - draws the 4D grid as instanced cubes or points, with 4D
// rotation and four view modes that can crossfade into each other.

#pragma once

#include "cinder/gl/gl.h"
#include "sim/World4D.h"

#include <array>
#include <string>

namespace kv4d {

enum class ViewMode : int { PROJECT = 0, SLICE = 1, STACK = 2, GRID = 3, COUNT = 4 };
enum class Plane : int { XY = 0, XZ, XW, YZ, YW, ZW, COUNT };

const char* viewModeName(ViewMode m);
const char* planeName(Plane p);
bool planeFromName(const std::string& name, Plane& out);  // "xw", "ZW", ...

struct ViewParams {
    float projDist = 2.0f;      // 4D eye distance in extent units (> 0.5)
    float sliceW = 0.0f;        // hyperplane position, -0.5..0.5
    float sliceWidth = 0.035f;
    float sliceSweep = 0.0f;    // slice oscillation speed (cycles/s); 0 = static
    float gridSpacing = 1.3f;
    float extent = 200.0f;
    float cellScale = 0.75f;
    float pointSize = 1.0f;
    bool points = false;
    bool additive = true;
    bool tesseract = true;
    float threshold = 0.0f;
    float alpha = 1.0f;
    ci::vec3 colorA = ci::vec3(0.1f, 0.6f, 1.0f);
    ci::vec3 colorB = ci::vec3(1.0f, 0.25f, 0.6f);
};

class Renderer4D {
public:
    void setup();

    ViewParams& params() { return mParams; }

    // Rotation angles (radians) and constant angular speeds (radians/second).
    void setAngle(Plane p, float a) { mAngles[(int)p] = a; }
    void setSpeed(Plane p, float s) { mSpeeds[(int)p] = s; }
    float speed(Plane p) const { return mSpeeds[(int)p]; }
    void resetRotation();

    void setMode(ViewMode m, float fadeSeconds);
    ViewMode mode() const { return mModeMix < 0.5f ? mModeA : mModeB; }

    // speedScale multiplies the rotation speeds of the planes that involve w
    // (used for audio); sliceOffset is added to the slice position.
    void update(float dt, float wSpeedScale = 1.0f, float sliceOffset = 0.0f);
    void draw(const World4D& world, float phase, float sizeScale = 1.0f);

    ci::mat4 rotation4() const;

private:
    ci::vec3 place(ViewMode m, ci::vec4 p, ci::vec4 q, float& size) const;
    void drawTesseract();

    ViewParams mParams;
    std::array<float, 6> mAngles{};
    std::array<float, 6> mSpeeds{};
    ViewMode mModeA = ViewMode::PROJECT, mModeB = ViewMode::PROJECT;
    float mModeMix = 0.0f, mFadeRate = 0.0f;
    float mTime = 0.0f, mSliceNow = 0.0f;

    ci::gl::GlslProgRef mProg;
    ci::gl::BatchRef mCubes, mPoints;
};

} // namespace kv4d
