#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The pre-canonical HeadTracking.ini reader, frozen. It reads a file the way
// the last build before the canonical config format did, so a player's old
// file is carried over as that build read it. Never edit anything in this
// folder: tests/CMakeLists.txt pins every file here by hash.
//
// Frozen from src/config.cpp and src/config_sanitize.h at 8435729, whose
// reader reads a file exactly as v1.1.0's does, with these changes: it fills
// this frozen copy of that commit's Config and its defaults instead of the
// mod's own, it takes the file's path rather than the folder it sits in and
// says whether the file opened, it writes nothing (the first-run file stays
// with the runtime caller), and it lives in namespace acr_ht::legacy. The
// defaults are the literals the code held then, and NormalizeUdpPort is copied
// in from cameraunlock-core's protocol/port_utils.h, which is not frozen, so a
// later change to core cannot move what an old file means.
namespace acr_ht::legacy {

struct Config {
    std::uint16_t udp_port = 4242;
    bool enable_on_startup = true;

    // Windows virtual-key codes: End, Page Up, and the Y and G of the
    // Ctrl+Shift chords.
    int toggle_key = 0x23;
    int cycle_mode_key = 0x21;
    int chord_toggle_key = 0x59;
    int chord_cycle_mode_key = 0x47;

    float yaw_sensitivity = 1.0f;
    float pitch_sensitivity = 1.0f;
    float roll_sensitivity = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    float local_smoothing = 0.0f;
    float remote_smoothing = 0.15f;

    float near_clip_cm = 1.0f;

    bool position_enabled = true;
    float position_sensitivity_x = 1.0f;
    float position_sensitivity_y = 1.0f;
    float position_sensitivity_z = 1.0f;
    bool invert_position_x = false;
    bool invert_position_y = false;
    bool invert_position_z = false;
    // LimitY bounds leaning down as well as up.
    float limit_x = 0.15f;
    float limit_y = 0.12f;
    float limit_z = 0.20f;
    float limit_z_back = 0.0f;
};

// Reads the file at `ini_path` over `out`. Keys that are absent, or whose value
// the boundary checks in config_sanitize.h refuse, leave the member of `out`
// at whatever it already held. Returns false, having logged it and changed
// nothing, when the file cannot be opened.
bool Load(const std::string& ini_path, Config& out);

struct Key {
    const char* section;
    const char* key;
};

// Every key Load takes a value from. [Rotation] Smoothing and [Position]
// Smoothing are read only to warn that they are ignored, so they are not
// among them.
std::vector<Key> ReadKeys();

}  // namespace acr_ht::legacy
