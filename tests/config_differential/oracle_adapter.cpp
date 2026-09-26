// Compiled into the oracle library only, with `acr_ht` and `cameraunlock`
// renamed, so "config.h" here is v1.1.0's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

namespace acr_oracle_view {

OracleConfig RunOracle(const std::string& exe_dir) {
    acr_ht::Config c;
    acr_ht::WriteDefaultConfigIfMissing(exe_dir);
    acr_ht::LoadConfig(exe_dir, c);

    OracleConfig o{};
    o.udp_port = c.udp_port;
    o.enable_on_startup = c.enable_on_startup;
    o.toggle_key = c.toggle_key;
    o.cycle_mode_key = c.cycle_mode_key;
    o.chord_toggle_key = c.chord_toggle_key;
    o.chord_cycle_mode_key = c.chord_cycle_mode_key;
    o.yaw_sensitivity = c.yaw_sensitivity;
    o.pitch_sensitivity = c.pitch_sensitivity;
    o.roll_sensitivity = c.roll_sensitivity;
    o.invert_yaw = c.invert_yaw;
    o.invert_pitch = c.invert_pitch;
    o.invert_roll = c.invert_roll;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.near_clip_cm = c.near_clip_cm;
    o.position_enabled = c.position_enabled;
    o.position_sensitivity_x = c.position_sensitivity_x;
    o.position_sensitivity_y = c.position_sensitivity_y;
    o.position_sensitivity_z = c.position_sensitivity_z;
    o.invert_position_x = c.invert_position_x;
    o.invert_position_y = c.invert_position_y;
    o.invert_position_z = c.invert_position_z;
    o.limit_x = c.limit_x;
    o.limit_y = c.limit_y;
    o.limit_z = c.limit_z;
    o.limit_z_back = c.limit_z_back;
    return o;
}

}  // namespace acr_oracle_view
