// Audio.h - input analysis (amplitude + low/mid/high bands), adapted from kv's
// GraphicsRenderer audio code.

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cinder { namespace audio {
    class InputDeviceNode;
    class MonitorSpectralNode;
}}

namespace kv4d {

class Audio {
public:
    ~Audio();

    // Empty name = default input. Returns false if the device is not found.
    bool enable(const std::string& deviceName = "");
    void disable();
    bool enabled() const { return (bool)mInput; }

    void update();

    float gain = 1.0f;
    float smoothing = 0.8f;

    float amplitude() const { return mAmp; }
    float low() const { return mLow; }
    float mid() const { return mMid; }
    float high() const { return mHigh; }

private:
    std::shared_ptr<cinder::audio::InputDeviceNode> mInput;
    std::shared_ptr<cinder::audio::MonitorSpectralNode> mMonitor;
    float mAmp = 0, mLow = 0, mMid = 0, mHigh = 0;
};

} // namespace kv4d
