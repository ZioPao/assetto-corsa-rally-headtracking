#pragma once

// The oracle: v1.1.0's HeadTracking.ini reader, the newest published build,
// compiled from byte copies in oracle/ with the cameraunlock-core sources it
// included at v1.1.0's core pin (3465659), as a library whose `acr_ht` and
// `cameraunlock` namespaces are renamed at compile time so it links beside
// this build's. This header names no core or mod type, so the test includes it
// without the renaming.

#include <cstdint>
#include <string>

namespace acr_oracle_view {

// v1.1.0's Config, field for field under the same names, so the test reads the
// oracle and the frozen reader through one template.
struct OracleConfig {
    std::uint16_t udp_port;
    bool enable_on_startup;
    int toggle_key;
    int cycle_mode_key;
    int chord_toggle_key;
    int chord_cycle_mode_key;
    float yaw_sensitivity;
    float pitch_sensitivity;
    float roll_sensitivity;
    bool invert_yaw;
    bool invert_pitch;
    bool invert_roll;
    float local_smoothing;
    float remote_smoothing;
    float near_clip_cm;
    bool position_enabled;
    float position_sensitivity_x;
    float position_sensitivity_y;
    float position_sensitivity_z;
    bool invert_position_x;
    bool invert_position_y;
    bool invert_position_z;
    float limit_x;
    float limit_y;
    float limit_z;
    float limit_z_back;
};

// What v1.1.0's LoadAndApplyConfig ran on a default Config in the game folder
// `exe_dir`: WriteDefaultConfigIfMissing, then LoadConfig. Writes the default
// HeadTracking.ini when there is none, as that build did.
OracleConfig RunOracle(const std::string& exe_dir);

}  // namespace acr_oracle_view
