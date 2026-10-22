// Osc4D.h - OSC control surface for a Session. All addresses live under /kv4d.
// See README.md for the full message list.

#pragma once

#include "cinder/osc/Osc.h"

#include <memory>
#include <functional>

namespace kv4d {

class Session;

class Osc4D {
public:
    Osc4D(Session& session, int receivePort, const std::string& remoteHost, int sendPort);
    ~Osc4D();

    // Sends pending outgoing messages (alive counts).
    void update();

    std::function<void()> onQuit;
    std::function<void(float)> onFrameRate;

private:
    void listen(const std::string& address, std::function<void(const ci::osc::Message&)> fn);
    void setupListeners();

    Session& mSession;
    std::unique_ptr<ci::osc::ReceiverUdp> mReceiver;
    std::unique_ptr<ci::osc::SenderUdp> mSender;
    bool mSenderReady = false;
};

} // namespace kv4d
