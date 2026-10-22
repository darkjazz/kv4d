#include "Audio.h"

#include "cinder/audio/audio.h"
#include "cinder/app/App.h"

#include <algorithm>

using namespace ci;

namespace kv4d {

Audio::~Audio() { disable(); }

bool Audio::enable(const std::string& deviceName) {
    disable();
    auto ctx = audio::Context::master();

    audio::DeviceRef device = audio::Device::getDefaultInput();
    if (!deviceName.empty()) {
        device = nullptr;
        for (auto& d : audio::Device::getInputDevices()) {
            if (d->getName() == deviceName) { device = d; break; }
        }
        if (!device) {
            app::console() << "Audio: no input device named '" << deviceName << "'" << std::endl;
            return false;
        }
    }
    if (!device) {
        app::console() << "Audio: no default input device" << std::endl;
        return false;
    }

    mInput = ctx->createInputDeviceNode(device, audio::Node::Format().autoEnable());
    auto fmt = audio::MonitorSpectralNode::Format().fftSize(2048).windowSize(1024);
    mMonitor = ctx->makeNode(new audio::MonitorSpectralNode(fmt));
    mInput >> mMonitor;
    mInput->enable();
    ctx->enable();
    app::console() << "Audio: listening on " << device->getName() << std::endl;
    return true;
}

void Audio::disable() {
    if (mMonitor) { mMonitor->disable(); mMonitor->disconnectAll(); mMonitor.reset(); }
    if (mInput) { mInput->disable(); mInput->disconnectAll(); mInput.reset(); }
    mAmp = mLow = mMid = mHigh = 0.0f;
}

void Audio::update() {
    if (!mMonitor) return;
    const std::vector<float>& mag = mMonitor->getMagSpectrum();
    if (mag.empty()) return;

    // Bands as in kv: ~21.5 Hz per bin at 44.1 kHz / 2048
    auto bandMean = [&](size_t lo, size_t hi) {
        hi = std::min(hi, mag.size());
        if (lo >= hi) return 0.0f;
        float s = 0.0f;
        for (size_t i = lo; i < hi; i++) s += mag[i];
        return s / (float)(hi - lo);
    };
    float total = bandMean(0, mag.size());
    float low = bandMean(0, 10);      // < 200 Hz
    float mid = bandMean(10, 94);     // 200 Hz - 2 kHz
    float high = bandMean(94, mag.size());

    // FFT magnitudes are small; the scale of 100 matches kv.
    const float scale = 100.0f * gain;
    auto smooth = [&](float& v, float target) {
        target = glm::clamp(target * scale, 0.0f, 1.0f);
        v = v * smoothing + target * (1.0f - smoothing);
    };
    smooth(mAmp, total);
    smooth(mLow, low);
    smooth(mMid, mid);
    smooth(mHigh, high);
}

} // namespace kv4d
