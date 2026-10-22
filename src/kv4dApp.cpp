// kv4d - 4D cellular automata visualiser.

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"

#include "Session.h"
#include "io/Osc4D.h"

#include <cstdio>

using namespace ci;
using namespace ci::app;
using namespace kv4d;

class kv4dApp : public App {
public:
    void setup() override;
    void resize() override;
    void update() override;
    void draw() override;
    void keyDown(KeyEvent event) override;

private:
    int runSelfTest();
    void drawHud();

    Session mSession;
    std::unique_ptr<Osc4D> mOsc;
    CameraPersp mCam;
    CameraUi mCamUi;
    bool mHud = true;
    double mLastTime = 0.0;
    // -capture <dir>: save frames of each view mode, then quit (for checking renders)
    std::string mCaptureDir;
    int mCaptureFrame = 0;
};

static int argInt(const std::vector<std::string>& args, const std::string& flag, int fallback) {
    for (size_t i = 0; i + 1 < args.size(); i++) {
        if (args[i] == flag) {
            try { return std::stoi(args[i + 1]); } catch (...) {}
        }
    }
    return fallback;
}

static bool hasFlag(const std::vector<std::string>& args, const std::string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

void kv4dApp::setup() {
    const auto& args = getCommandLineArgs();

    if (hasFlag(args, "-selftest")) {
        int failures = runSelfTest();
        std::fflush(stdout);
        std::_Exit(failures == 0 ? 0 : 1);
    }

    int winX = argInt(args, "-screenx", 1280);
    int winY = argInt(args, "-screeny", 800);
    int fps = argInt(args, "-fps", 60);
    int wmode = argInt(args, "-wmode", 0);
    int full = argInt(args, "-full", 0);
    int size = argInt(args, "-size", 20);

    auto displays = Display::getDisplays();
    DisplayRef target = (wmode == 1 && displays.size() > 1) ? displays[1] : Display::getMainDisplay();
    getWindow()->setPos(target->getBounds().getUL() + ivec2(40));
    setWindowSize(winX, winY);
    if (full) setFullScreen(true, FullScreenOptions().display(target));
    setFrameRate((float)fps);

    try {
        mSession.setup(Dims4{ size, size, size, size });
    } catch (const std::exception& e) {
        console() << "kv4d: setup failed: " << e.what() << std::endl;
        quit();
        return;
    }

    mOsc = std::make_unique<Osc4D>(mSession, argInt(args, "-inport", 7000), "127.0.0.1", argInt(args, "-outport", 57121));
    mOsc->onQuit = [this]() { quit(); };
    mOsc->onFrameRate = [this](float f) { setFrameRate(f); };

    mCam.setPerspective(45.0f, getWindowAspectRatio(), 1.0f, 5000.0f);
    mCam.lookAt(vec3(260, 200, 420), vec3(0));
    mCamUi = CameraUi(&mCam, getWindow());
    mLastTime = getElapsedSeconds();

    for (size_t i = 0; i + 1 < args.size(); i++) {
        if (args[i] == "-capture") mCaptureDir = args[i + 1];
    }
}

void kv4dApp::resize() {
    mCam.setAspectRatio(getWindowAspectRatio());
    mSession.efx.resize(toPixels(getWindowSize()));
}

void kv4dApp::update() {
    double now = getElapsedSeconds();
    float dt = (float)std::min(now - mLastTime, 0.1);
    mLastTime = now;
    mSession.update(dt);
    if (mOsc) mOsc->update();
}

void kv4dApp::draw() {
    gl::clear(Color::black());
    mSession.efx.begin(mSession.background);
    {
        gl::ScopedMatrices mats;
        gl::setMatrices(mCam);
        mSession.draw();
    }
    mSession.efx.end();
    if (mHud) drawHud();

    if (!mCaptureDir.empty()) {
        // Every 90 frames: save, then move on to the next view mode.
        int f = ++mCaptureFrame;
        if (f % 90 == 0) {
            int shot = f / 90 - 1;
            writeImage(mCaptureDir + "/kv4d_" + viewModeName(mSession.renderer.mode()) + ".png", copyWindowSurface());
            if (shot + 1 >= (int)ViewMode::COUNT) quit();
            else mSession.renderer.setMode((ViewMode)(shot + 1), 0.0f);
        }
    }
}

void kv4dApp::drawHud() {
    gl::ScopedMatrices mats;
    gl::setMatricesWindow(getWindowSize());
    gl::ScopedBlendAlpha blend;
    const Dims4& d = mSession.world.dims();
    const ViewParams& v = mSession.renderer.params();
    std::vector<std::string> lines = {
        "kv4d  " + std::to_string(d.x) + "x" + std::to_string(d.y) + "x" + std::to_string(d.z) + "x" + std::to_string(d.w) +
            "  gen " + std::to_string(mSession.world.generation()) + "  " + toString((int)getAverageFps()) + " fps" +
            (mSession.paused ? "  [paused]" : ""),
        "rule  " + mSession.ruleName(),
        std::string("view  ") + viewModeName(mSession.renderer.mode()) + (v.points ? "  points" : "  cubes") +
            (v.additive ? "  additive" : "  alpha") + "  slice w " + toString(v.sliceW).substr(0, 5),
        mSession.audio.enabled() ? "audio low " + toString(mSession.audio.low()).substr(0, 4) +
            " mid " + toString(mSession.audio.mid()).substr(0, 4) : "audio off",
        "",
        "1-4 project/slice/stack/grid   space pause  n step  r reseed  [ ] preset  s symmetric seed  t tesseract seed",
        "g cubes/points  b blend  w toggle w-rotation  , . slice  - = gen rate  e trails  l glow  a audio  h hud  f fullscreen",
    };
    float y = 16.0f;
    for (auto& l : lines) {
        gl::drawString(l, vec2(16, y), ColorA(1, 1, 1, 0.8f));
        y += 14.0f;
    }
}

void kv4dApp::keyDown(KeyEvent event) {
    ViewParams& v = mSession.renderer.params();
    switch (event.getChar()) {
        case '1': mSession.renderer.setMode(ViewMode::PROJECT, 1.0f); break;
        case '2': mSession.renderer.setMode(ViewMode::SLICE, 1.0f); break;
        case '3': mSession.renderer.setMode(ViewMode::STACK, 1.0f); break;
        case '4': mSession.renderer.setMode(ViewMode::GRID, 1.0f); break;
        case ' ': mSession.paused = !mSession.paused; break;
        case 'n': mSession.stepOnce(); break;
        case 'r': mSession.reseed(); break;
        case 's': mSession.seed(SeedType::SYMMETRIC, 0.5f, 0.2f); break;
        case 't': mSession.seed(SeedType::TESSERACT, 0.6f, 1.0f); break;
        case '[': mSession.applyPreset(mSession.presetIndex() - 1); break;
        case ']': mSession.applyPreset(mSession.presetIndex() + 1); break;
        case 'g': v.points = !v.points; break;
        case 'b': v.additive = !v.additive; break;
        case 'w': {
            Renderer4D& r = mSession.renderer;
            bool on = r.speed(kv4d::Plane::XW) != 0.0f || r.speed(kv4d::Plane::YW) != 0.0f || r.speed(kv4d::Plane::ZW) != 0.0f;
            r.setSpeed(kv4d::Plane::XW, on ? 0.0f : 0.21f);
            r.setSpeed(kv4d::Plane::YW, 0.0f);
            r.setSpeed(kv4d::Plane::ZW, on ? 0.0f : 0.13f);
            break;
        }
        case ',': v.sliceW = std::max(-0.5f, v.sliceW - 0.02f); break;
        case '.': v.sliceW = std::min(0.5f, v.sliceW + 0.02f); break;
        case '-': mSession.genRate = std::max(0.5f, mSession.genRate * 0.8f); break;
        case '=': mSession.genRate = std::min(60.0f, mSession.genRate * 1.25f); break;
        case 'e': mSession.efx.trailDecay = mSession.efx.trailDecay > 0.0f ? 0.0f : 0.88f; break;
        case 'l': mSession.efx.glowIntensity = mSession.efx.glowIntensity > 0.0f ? 0.0f : 1.2f; break;
        case 'a': if (mSession.audio.enabled()) mSession.audio.disable(); else mSession.audio.enable(); break;
        case 'h': mHud = !mHud; break;
        case 'f': setFullScreen(!isFullScreen()); break;
        default: break;
    }
}

// Runs the GPU step against the CPU reference on several rules and grid shapes.
int kv4dApp::runSelfTest() {
    struct Case { std::string name; Rule4D rule; Dims4 dims; float density; int steps; };
    std::vector<Case> cases;

    int count = 0;
    const RulePreset* presets = rulePresets(&count);
    for (int i = 0; i < count; i++) {
        Rule4D r;
        r.parse(presets[i].spec);
        cases.push_back({ presets[i].name, r, Dims4{ 10, 10, 10, 10 }, presets[i].density, 12 });
    }
    // Non-cubic dims catch atlas layout mistakes; von Neumann covers the other neighbourhood.
    {
        Rule4D r;
        r.parse("B12-17/S11-20/C4/NM");
        cases.push_back({ "non-cubic 6x7x8x9", r, Dims4{ 6, 7, 8, 9 }, 0.3f, 10 });
        Rule4D vn;
        vn.parse("B1-2/S1-3/C3/NV");
        cases.push_back({ "von neumann", vn, Dims4{ 9, 8, 7, 6 }, 0.2f, 10 });
    }

    int failures = 0;
    World4D world;
    for (auto& c : cases) {
        world.setup(c.dims);
        world.rule() = c.rule;
        Grid4D cpu(c.dims), gpu(c.dims);
        cpu.seedRandomBox(0.8f, c.density, c.rule, 42);
        world.upload(cpu);

        int firstBad = -1, mismatches = 0;
        for (int s = 0; s < c.steps && firstBad < 0; s++) {
            cpu.step(c.rule);
            world.step();
            world.download(gpu);
            mismatches = 0;
            for (size_t i = 0; i < cpu.cells().size(); i++) mismatches += cpu.cells()[i] != gpu.cells()[i];
            if (mismatches) firstBad = s + 1;
        }
        bool ok = firstBad < 0;
        failures += !ok;
        std::printf("%s  %-20s %s  alive %d/%d%s\n", ok ? "PASS" : "FAIL", c.name.c_str(), c.rule.toString().c_str(),
                    cpu.alive(), c.dims.count(),
                    ok ? "" : ("  (" + std::to_string(mismatches) + " cells differ at step " + std::to_string(firstBad) + ")").c_str());
    }

    // Continuous: float sums may round differently on the GPU, so compare with tolerance.
    {
        Dims4 d{ 8, 8, 8, 8 };
        Rule4D r;
        r.type = RuleType::CONTINUOUS;
        r.add = 0.01f;
        world.setup(d);
        world.rule() = r;
        Grid4D cpu(d), gpu(d);
        cpu.seedRandomBox(1.0f, 1.0f, r, 7);
        world.upload(cpu);
        float maxErr = 0.0f;
        for (int s = 0; s < 3; s++) {
            cpu.step(r);
            world.step();
        }
        world.download(gpu);
        for (size_t i = 0; i < cpu.cells().size(); i++) {
            float e = std::fabs(cpu.cells()[i] - gpu.cells()[i]);
            maxErr = std::max(maxErr, std::min(e, 1.0f - e));  // values wrap at 1
        }
        bool ok = maxErr < 1e-4f;
        failures += !ok;
        std::printf("%s  %-20s max error %.2e\n", ok ? "PASS" : "FAIL", "continuous", maxErr);
    }

    std::printf("%d failure(s)\n", failures);
    return failures;
}

CINDER_APP(kv4dApp, RendererGl(RendererGl::Options().msaa(0)), [](App::Settings* settings) {
    settings->setHighDensityDisplayEnabled(true);
    settings->setTitle("kv4d");
})
