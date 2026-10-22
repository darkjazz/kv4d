#include "Session.h"

#include "cinder/app/App.h"
#include "cinder/Rand.h"

using namespace ci;

namespace kv4d {

void Session::setup(Dims4 dims) {
    renderer.setup();
    efx.setup();
    init(dims);
    applyPreset(0);
}

void Session::init(Dims4 dims) {
    world.setup(dims);
    mGrid.resize(dims);
    mGenAccum = 0.0f;
    reseed();
}

bool Session::setRule(const std::string& spec) {
    Rule4D r = world.rule();
    if (!r.parse(spec)) {
        app::console() << "kv4d: bad rule spec '" << spec << "'" << std::endl;
        return false;
    }
    world.rule() = r;
    mPresetIndex = -1;
    return true;
}

void Session::setContinuous(float add) {
    world.rule().type = RuleType::CONTINUOUS;
    world.rule().add = add;
    mPresetIndex = -1;
    // Continuous states fill the whole grid; only draw the upper half.
    renderer.params().threshold = 0.5f;
}

void Session::applyPreset(int index) {
    int count = 0;
    const RulePreset* presets = rulePresets(&count);
    index = ((index % count) + count) % count;
    world.rule().parse(presets[index].spec);
    renderer.params().threshold = 0.0f;
    mPresetIndex = index;
    mSeedDensity = presets[index].density;
    mSeedExtent = 0.5f;
    reseed();
}

bool Session::applyPreset(const std::string& name) {
    int count = 0;
    const RulePreset* presets = rulePresets(&count);
    for (int i = 0; i < count; i++) {
        if (name == presets[i].name) { applyPreset(i); return true; }
    }
    return false;
}

std::string Session::ruleName() const {
    int count = 0;
    const RulePreset* presets = rulePresets(&count);
    std::string spec = world.rule().toString();
    if (mPresetIndex >= 0 && mPresetIndex < count) return std::string(presets[mPresetIndex].name) + "  " + spec;
    return spec;
}

void Session::seed(SeedType type, float extent, float density) {
    mSeedType = type;
    mSeedExtent = extent;
    mSeedDensity = density;
    uint32_t s = mSeedCounter++ * 2654435761u;
    mGrid.clear();
    switch (type) {
        case SeedType::BOX: mGrid.seedRandomBox(extent, density, world.rule(), s); break;
        case SeedType::SYMMETRIC: mGrid.seedSymmetric(extent, density, world.rule(), s); break;
        case SeedType::TESSERACT: mGrid.seedWireTesseract(extent); break;
        case SeedType::CLEAR: break;
    }
    world.upload(mGrid);
    mGenAccum = 0.0f;
}

void Session::inject(vec4 c, float radius01, float density) {
    const Dims4& d = world.dims();
    vec4 cell = c * vec4(d.x, d.y, d.z, d.w);
    float radius = radius01 * (float)std::max(std::max(d.x, d.y), std::max(d.z, d.w));
    world.inject(cell, std::max(radius, 0.5f), density, 1.0f);
}

void Session::update(float dt) {
    audio.update();

    // Audio-triggered injection at a random spot.
    mInjectTimer -= dt;
    if (audioMap.injectThreshold > 0.0f && audio.low() > audioMap.injectThreshold && mInjectTimer <= 0.0f) {
        inject(vec4(randFloat(), randFloat(), randFloat(), randFloat()), 0.06f + 0.1f * audio.low(), 0.5f);
        mInjectTimer = audioMap.injectCooldown;
    }

    int steps = 0;
    if (!paused) {
        mGenAccum += dt * genRate;
        while (mGenAccum >= 1.0f && steps < 4) {
            world.step();
            mGenAccum -= 1.0f;
            steps++;
        }
        if (mGenAccum >= 1.0f) mGenAccum = 0.0f;  // fell behind; drop the backlog
    }
    for (; mStepRequests > 0; mStepRequests--) { world.step(); steps++; }

    if (queryAlive && steps > 0) {
        world.download(mGrid);
        aliveCount = mGrid.alive();
        aliveReady = true;
    }

    renderer.update(dt, 1.0f + audioMap.rotate * audio.low() * 4.0f, audioMap.slice * (audio.mid() - 0.25f));
}

void Session::draw() {
    float phase = (interpolate && !paused) ? std::min(mGenAccum, 1.0f) : 1.0f;
    renderer.draw(world, phase, 1.0f + audioMap.pulse * audio.amplitude());
}

} // namespace kv4d
