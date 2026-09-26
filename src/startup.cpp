#include "startup.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace acr_ht {

void ApplyConfigToPipeline(const Config& config, Session& session) {
    // The pose is applied as the tracker sends it: no sensitivity and no axis
    // inversion of the mod's own, which the processors' defaults already are.
    cameraunlock::PositionSettings position;
    position.limit_x = config.position_limit_x;
    position.limit_y = config.position_limit_y;
    position.limit_y_down = config.position_limit_y_down;
    position.limit_z = config.position_limit_z;
    position.limit_z_back = config.position_limit_z_back;
    position.local_smoothing = config.local_smoothing;
    position.remote_smoothing = config.remote_smoothing;
    session.GetPositionProcessor().SetSettings(position);

    // One pair of values for rotation and position alike, applied after the
    // position settings so a settings rebuild cannot drop them. The session
    // picks between the two per connection from the receiver's source-address
    // check, so nothing here decides which one is in effect.
    session.SetLocalSmoothing(config.local_smoothing);
    session.SetRemoteSmoothing(config.remote_smoothing);

    session.SetMode(config::StartupTrackingMode(config));
}

std::vector<HotkeyList> HotkeyLists(const Config& config) {
    using namespace cameraunlock::input;

    const std::pair<HotkeyAction, const std::string*> lists[] = {
        {HotkeyAction::ToggleTracking, &config.toggle_key},
        {HotkeyAction::CycleTrackingMode, &config.cycle_tracking_mode_key},
    };
    std::vector<HotkeyList> out;
    for (const auto& [action, list] : lists) {
        KeyBindingsParseResult parsed = ParseKeyBindings(*list);
        if (!parsed.ok()) throw std::logic_error("the config table accepted the hotkey list '" + *list + "': " + parsed.error);
        out.push_back({action, std::move(parsed.bindings)});
    }
    return out;
}

}  // namespace acr_ht
