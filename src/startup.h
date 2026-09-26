#pragma once

#include <vector>

#include "config.h"

#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"

// What a session starts with, given the settings. Kept out of
// headtracking_mod.cpp, which needs the game process, so the config
// differential test runs the same code the mod does.
namespace acr_ht {

using Session = cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>;

void ApplyConfigToPipeline(const Config& config, Session& session);

enum class HotkeyAction { ToggleTracking, CycleTrackingMode };

struct HotkeyList {
    HotkeyAction action;
    std::vector<cameraunlock::input::KeyBinding> bindings;
};

// Each action with every key its list in CameraUnlock.ini names.
std::vector<HotkeyList> HotkeyLists(const Config& config);

}  // namespace acr_ht
