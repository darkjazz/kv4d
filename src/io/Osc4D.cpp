#include "Osc4D.h"
#include "Session.h"

#include "cinder/Log.h"

using namespace ci;
using Message = osc::Message;

namespace kv4d {

// SuperCollider sends whole numbers as int32 and the rest as float; accept both.
static float argF(const Message& m, uint32_t i, float fallback = 0.0f) {
    if (i >= m.getNumArgs()) return fallback;
    switch (m.getArgType(i)) {
        case osc::ArgType::INTEGER_32: return (float)m.getArgInt32(i);
        case osc::ArgType::FLOAT: return m.getArgFloat(i);
        case osc::ArgType::DOUBLE: return (float)m.getArgDouble(i);
        case osc::ArgType::INTEGER_64: return (float)m.getArgInt64(i);
        case osc::ArgType::BOOL_T: return 1.0f;
        case osc::ArgType::BOOL_F: return 0.0f;
        default: return fallback;
    }
}

static int argI(const Message& m, uint32_t i, int fallback = 0) {
    return (int)std::lround(argF(m, i, (float)fallback));
}

static bool isString(const Message& m, uint32_t i) {
    return i < m.getNumArgs() && m.getArgType(i) == osc::ArgType::STRING;
}

static vec3 argV3(const Message& m, uint32_t i) { return vec3(argF(m, i), argF(m, i + 1), argF(m, i + 2)); }

Osc4D::Osc4D(Session& session, int receivePort, const std::string& remoteHost, int sendPort)
    : mSession(session) {
    mReceiver = std::make_unique<osc::ReceiverUdp>((uint16_t)receivePort);
    setupListeners();
    try {
        mReceiver->bind();
        mReceiver->listen([](asio::error_code error, asio::ip::udp::endpoint) -> bool {
            if (error) {
                CI_LOG_E("kv4d OSC receive error: " << error.message());
                return false;
            }
            return true;
        });
    } catch (const osc::Exception& ex) {
        CI_LOG_E("kv4d OSC: cannot listen on " << receivePort << ": " << ex.what());
    }

    mSender = std::make_unique<osc::SenderUdp>(0, remoteHost, (uint16_t)sendPort);
    try {
        mSender->bind();
        mSenderReady = true;
    } catch (const osc::Exception& ex) {
        CI_LOG_E("kv4d OSC: cannot bind sender: " << ex.what());
    }
}

Osc4D::~Osc4D() {
    if (mReceiver) mReceiver->close();
    if (mSender) mSender->close();
}

void Osc4D::listen(const std::string& address, std::function<void(const Message&)> fn) {
    mReceiver->setListener("/kv4d" + address, [fn, address](const Message& m) {
        try {
            fn(m);
        } catch (const std::exception& e) {
            CI_LOG_E("kv4d OSC " << address << ": " << e.what());
        }
    });
}

void Osc4D::update() {
    if (mSenderReady && mSession.aliveReady) {
        Message m("/kv4d/world/alive");
        m.append(mSession.aliveCount);
        m.append((int32_t)mSession.world.generation());
        mSender->send(m);
        mSession.aliveReady = false;
    }
}

void Osc4D::setupListeners() {
    Session& s = mSession;

    // ---- world ----
    listen("/world/init", [&s](const Message& m) {
        int n = argI(m, 0, 16);
        Dims4 d{ n, argI(m, 1, n), argI(m, 2, n), argI(m, 3, n) };
        s.init(d);
    });
    listen("/world/rule", [&s](const Message& m) {
        if (isString(m, 0)) s.setRule(m.getArgString(0));
    });
    listen("/world/preset", [&s](const Message& m) {
        if (isString(m, 0)) s.applyPreset(m.getArgString(0));
        else s.applyPreset(argI(m, 0));
    });
    listen("/world/continuous", [&s](const Message& m) { s.setContinuous(argF(m, 0, 0.005f)); });
    // births / survivals: pairs of lo hi ranges, replacing the current set
    listen("/world/births", [&s](const Message& m) {
        Rule4D& r = s.world.rule();
        r.type = RuleType::GENERATIONS;
        r.birth.fill(0);
        for (uint32_t i = 0; i + 1 < m.getNumArgs(); i += 2) r.setBirthRange(argI(m, i), argI(m, i + 1));
    });
    listen("/world/survivals", [&s](const Message& m) {
        Rule4D& r = s.world.rule();
        r.type = RuleType::GENERATIONS;
        r.survive.fill(0);
        for (uint32_t i = 0; i + 1 < m.getNumArgs(); i += 2) r.setSurviveRange(argI(m, i), argI(m, i + 1));
    });
    listen("/world/states", [&s](const Message& m) { s.world.rule().numStates = std::max(2, argI(m, 0, 2)); });
    listen("/world/nhood", [&s](const Message& m) {
        s.world.rule().nhood = argI(m, 0) == 1 ? Neighborhood::VON_NEUMANN : Neighborhood::MOORE;
    });
    listen("/world/add", [&s](const Message& m) { s.world.rule().add = argF(m, 0, 0.005f); });
    listen("/world/seed", [&s](const Message& m) {
        s.seed((SeedType)glm::clamp(argI(m, 0), 0, 3), argF(m, 1, 0.5f), argF(m, 2, 0.2f));
    });
    listen("/world/reseed", [&s](const Message&) { s.reseed(); });
    listen("/world/inject", [&s](const Message& m) {
        s.inject(vec4(argF(m, 0, 0.5f), argF(m, 1, 0.5f), argF(m, 2, 0.5f), argF(m, 3, 0.5f)),
                 argF(m, 4, 0.1f), argF(m, 5, 0.5f));
    });
    listen("/world/rate", [&s](const Message& m) { s.genRate = std::max(0.0f, argF(m, 0, 8.0f)); });
    listen("/world/pause", [&s](const Message& m) { s.paused = argI(m, 0, 1) != 0; });
    listen("/world/step", [&s](const Message&) { s.stepOnce(); });
    listen("/world/interpolate", [&s](const Message& m) { s.interpolate = argI(m, 0, 1) != 0; });
    listen("/world/query/alive", [&s](const Message& m) { s.queryAlive = argI(m, 0, 1) != 0; });

    // ---- view ----
    Renderer4D& r = s.renderer;
    ViewParams& v = s.renderer.params();
    listen("/view/mode", [&r](const Message& m) {
        r.setMode((ViewMode)glm::clamp(argI(m, 0), 0, 3), argF(m, 1, 1.0f));
    });
    auto plane = [](const Message& m, Plane& p) {
        if (isString(m, 0)) return planeFromName(m.getArgString(0), p);
        int i = argI(m, 0, -1);
        if (i < 0 || i > 5) return false;
        p = (Plane)i;
        return true;
    };
    listen("/view/rotate", [&r, plane](const Message& m) {
        Plane p;
        if (plane(m, p)) r.setSpeed(p, argF(m, 1));
    });
    listen("/view/angle", [&r, plane](const Message& m) {
        Plane p;
        if (plane(m, p)) r.setAngle(p, argF(m, 1));
    });
    listen("/view/reset", [&r](const Message&) { r.resetRotation(); });
    listen("/view/project", [&v](const Message& m) { v.projDist = std::max(0.6f, argF(m, 0, 2.0f)); });
    listen("/view/slice", [&v](const Message& m) {
        v.sliceW = glm::clamp(argF(m, 0), -0.5f, 0.5f);
        if (m.getNumArgs() > 1) v.sliceWidth = std::max(0.001f, argF(m, 1));
        if (m.getNumArgs() > 2) v.sliceSweep = std::max(0.0f, argF(m, 2));
    });
    listen("/view/grid", [&v](const Message& m) { v.gridSpacing = argF(m, 0, 1.3f); });
    listen("/view/extent", [&v](const Message& m) { v.extent = argF(m, 0, 200.0f); });
    listen("/view/geometry", [&v](const Message& m) { v.points = argI(m, 0) == 1; });
    listen("/view/cellscale", [&v](const Message& m) { v.cellScale = argF(m, 0, 0.75f); });
    listen("/view/pointsize", [&v](const Message& m) { v.pointSize = argF(m, 0, 1.0f); });
    listen("/view/color", [&v](const Message& m) {
        v.colorA = argV3(m, 0);
        if (m.getNumArgs() >= 6) v.colorB = argV3(m, 3);
    });
    listen("/view/alpha", [&v](const Message& m) { v.alpha = argF(m, 0, 0.85f); });
    listen("/view/blend", [&v](const Message& m) { v.additive = argI(m, 0, 1) != 0; });
    listen("/view/threshold", [&v](const Message& m) { v.threshold = argF(m, 0); });
    listen("/view/tesseract", [&v](const Message& m) { v.tesseract = argI(m, 0, 1) != 0; });
    listen("/view/background", [&s](const Message& m) { s.background = Color(argF(m, 0), argF(m, 1), argF(m, 2)); });

    // ---- efx ----
    listen("/efx/trails", [&s](const Message& m) { s.efx.trailDecay = glm::clamp(argF(m, 0), 0.0f, 0.99f); });
    listen("/efx/glow", [&s](const Message& m) {
        s.efx.glowThreshold = argF(m, 0, 0.6f);
        s.efx.glowIntensity = argF(m, 1, 1.0f);
        s.efx.glowRadius = argF(m, 2, 6.0f);
    });

    // ---- audio ----
    listen("/audio/enable", [&s](const Message& m) {
        if (argI(m, 0, 1)) s.audio.enable(isString(m, 1) ? m.getArgString(1) : "");
        else s.audio.disable();
    });
    listen("/audio/gain", [&s](const Message& m) { s.audio.gain = argF(m, 0, 1.0f); });
    listen("/audio/map", [&s](const Message& m) {
        if (!isString(m, 0)) return;
        std::string target = m.getArgString(0);
        float amt = argF(m, 1);
        if (target == "rotate") s.audioMap.rotate = amt;
        else if (target == "slice") s.audioMap.slice = amt;
        else if (target == "pulse") s.audioMap.pulse = amt;
        else if (target == "inject") s.audioMap.injectThreshold = amt;
    });

    // ---- app ----
    listen("/framerate", [this](const Message& m) { if (onFrameRate) onFrameRate(argF(m, 0, 60.0f)); });
    listen("/quit", [this](const Message&) { if (onQuit) onQuit(); });
}

} // namespace kv4d
