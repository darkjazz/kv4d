// Session.h - everything a performance controls: the simulation, the view,
// effects and audio mappings. Keyboard handlers and OSC both act on a Session.

#pragma once

#include "sim/World4D.h"
#include "view/Renderer4D.h"
#include "view/Efx.h"
#include "io/Audio.h"

#include "cinder/Color.h"

#include <string>

namespace kv4d {

enum class SeedType : int { BOX = 0, SYMMETRIC = 1, TESSERACT = 2, CLEAR = 3 };

struct AudioMap {
    float rotate = 0.0f;     // low band -> extra speed on the w planes
    float slice = 0.0f;      // mid band -> slice position offset
    float pulse = 0.0f;      // amplitude -> cell size
    float injectThreshold = 0.0f;  // low band above this injects cells (0 = off)
    float injectCooldown = 0.25f;  // seconds
};

class Session {
public:
    void setup(Dims4 dims);
    void update(float dt);
    void draw();

    // World
    void init(Dims4 dims);
    bool setRule(const std::string& spec);
    void setContinuous(float add);
    void applyPreset(int index);
    bool applyPreset(const std::string& name);
    void seed(SeedType type, float extent, float density);
    void reseed() { seed(mSeedType, mSeedExtent, mSeedDensity); }
    void inject(ci::vec4 center01, float radius01, float density);
    void stepOnce() { mStepRequests++; }

    float genRate = 8.0f;        // generations per second
    bool paused = false;
    bool interpolate = true;
    bool queryAlive = false;

    // Set after a generation when queryAlive is on; cleared by the reader.
    bool aliveReady = false;
    int aliveCount = 0;

    World4D world;
    Renderer4D renderer;
    Efx efx;
    Audio audio;
    AudioMap audioMap;
    ci::Color background = ci::Color(0.0f, 0.0f, 0.0f);

    int presetIndex() const { return mPresetIndex; }
    std::string ruleName() const;

private:
    Grid4D mGrid;
    SeedType mSeedType = SeedType::BOX;
    float mSeedExtent = 0.5f, mSeedDensity = 0.2f;
    uint32_t mSeedCounter = 1;
    int mPresetIndex = 0;
    float mGenAccum = 0.0f;
    int mStepRequests = 0;
    float mInjectTimer = 0.0f;
};

} // namespace kv4d
