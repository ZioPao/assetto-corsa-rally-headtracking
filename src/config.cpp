#include "config.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "legacy_config/legacy_config.h"
#include "logging.h"

#include "cameraunlock/config/value_codecs.h"
#include "cameraunlock/input/key_bindings.h"

namespace acr_ht::config {

namespace {

namespace cfg = ::cameraunlock::config;
using cfg::schema::Concept;
using ::cameraunlock::input::FormatKeyBindings;
using ::cameraunlock::input::KeyModifiers;

constexpr const wchar_t* kIniName = L"CameraUnlock.ini";
constexpr const wchar_t* kLegacyIniName = L"HeadTracking.ini";

// data/games.json's display_name for assetto-corsa-rally.
constexpr const char* kDisplayName = "Assetto Corsa Rally";

constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;

// [Camera] NearClipCm: 0, which leaves the game's plane alone, or a distance
// from 0.1 to 100 centimetres. A plane nearer than 0.1 cm leaves the depth
// buffer too little precision across the whole scene.
class NearClipCodec {
public:
    using Value = float;

    cfg::CodecParseResult<float> Parse(std::string_view text) const {
        cfg::CodecParseResult<float> read = range_.Parse(text);
        if (read.ok() && read.value != 0.0f && read.value < kMinCm) {
            cfg::CodecParseResult<float> refused;
            refused.error = "0, or a number from 0.1 to 100.0";
            return refused;
        }
        return read;
    }

    std::string Render(float value) const {
        if (value != 0.0f && value < kMinCm) {
            throw std::invalid_argument("NearClipCm " + std::to_string(value) + " is neither 0 nor 0.1 to 100");
        }
        return range_.Render(value);
    }

    bool Equal(float a, float b) const { return range_.Equal(a, b); }

private:
    static constexpr float kMinCm = 0.1f;
    cfg::FloatCodec range_{0.0f, 100.0f};
};

std::unique_ptr<cfg::ConfigOwner<Config>> g_owner;

cfg::ImportResult RunImport(const cfg::LegacyInput& input, Config& out) {
    legacy::Config read;
    const bool present = legacy::Load(input.ansi_path, read);

    std::vector<cfg::DroppedValue> dropped;
    std::vector<cfg::PoseShapingValue> pose_shaping;
    // v1.1.0 shipped every sensitivity at 1 and every inversion false, and the
    // axis signs the engine needs were already in camera_transform.cpp, so the
    // mod applies the pose as the tracker sends it and folds nothing.
    const auto shaping = [&](auto value, auto shipped, const char* section, const char* key) {
        cfg::LegacyPoseShaping(value, shipped, section, key, pose_shaping, dropped);
    };
    shaping(read.yaw_sensitivity, 1.0f, "Rotation", "YawSensitivity");
    shaping(read.pitch_sensitivity, 1.0f, "Rotation", "PitchSensitivity");
    shaping(read.roll_sensitivity, 1.0f, "Rotation", "RollSensitivity");
    shaping(read.invert_yaw, false, "Rotation", "InvertYaw");
    shaping(read.invert_pitch, false, "Rotation", "InvertPitch");
    shaping(read.invert_roll, false, "Rotation", "InvertRoll");
    shaping(read.position_sensitivity_x, 1.0f, "Position", "SensitivityX");
    shaping(read.position_sensitivity_y, 1.0f, "Position", "SensitivityY");
    shaping(read.position_sensitivity_z, 1.0f, "Position", "SensitivityZ");
    shaping(read.invert_position_x, false, "Position", "InvertX");
    shaping(read.invert_position_y, false, "Position", "InvertY");
    shaping(read.invert_position_z, false, "Position", "InvertZ");

    // The reader keeps the port inside 1024-65535, each smoothing value finite
    // and inside 0-1, each limit finite and inside 0-10 and the near clip at 0
    // or inside 0.1-100, so all of them carry over as they are.
    out.udp_port = read.udp_port;
    out.enable_on_startup = read.enable_on_startup;
    out.local_smoothing = read.local_smoothing;
    out.remote_smoothing = read.remote_smoothing;
    out.near_clip_cm = read.near_clip_cm;

    // [Position] Enabled chose the startup mode and nothing else: the cycle
    // reached every mode either way.
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(
        read.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                              : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = channels.rotation_enabled;
    out.position_enabled = channels.position_enabled;

    // LimitY bounded leaning down as well as up.
    out.position_limit_x = read.limit_x;
    out.position_limit_y = read.limit_y;
    out.position_limit_y_down = read.limit_y;
    out.position_limit_z = read.limit_z;
    out.position_limit_z_back = read.limit_z_back;

    // Each action had a nav key and the letter of its Ctrl+Shift chord. The
    // reader holds both to a code from 0x01 to 0xFE that is not a modifier, so
    // neither can be out of range.
    out.toggle_key = FormatKeyBindings({{KeyModifiers::kNone, read.toggle_key}, {kChord, read.chord_toggle_key}});
    out.cycle_tracking_mode_key =
        FormatKeyBindings({{KeyModifiers::kNone, read.cycle_mode_key}, {kChord, read.chord_cycle_mode_key}});

    return present ? cfg::ImportResult::Imported(std::move(dropped), std::move(pose_shaping))
                   : cfg::ImportResult::Absent(std::move(dropped), std::move(pose_shaping));
}

}  // namespace

cfg::ConfigTable<Config> Table() {
    cfg::ConfigTable<Config> table;
    table.Concept<Concept::UdpPort>(&Config::udp_port)
        .Concept<Concept::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<Concept::RotationEnabled>(&Config::rotation_enabled)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::local_smoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remote_smoothing)
        .Concept<Concept::PositionEnabled>(&Config::position_enabled)
        .Writable()
        .Concept<Concept::PositionLimitX>(&Config::position_limit_x)
        .Concept<Concept::PositionLimitY>(&Config::position_limit_y)
        .Concept<Concept::PositionLimitYDown>(&Config::position_limit_y_down)
        .Concept<Concept::PositionLimitZ>(&Config::position_limit_z)
        .Concept<Concept::PositionLimitZBack>(&Config::position_limit_z_back)
        .Concept<Concept::ToggleKey>(&Config::toggle_key)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key)
        .Local("Camera", "NearClipCm", &Config::near_clip_cm, NearClipCodec(),
               "Near clip plane in centimetres, applied while head tracking is driving the view.\n"
               "The game's own 5.0 sits further from your eye than the seat back behind you, so\n"
               "looking over a shoulder clips the seat away and you see straight through it.\n"
               "Pulling the plane in renders it instead. 0 leaves the game's value alone; any\n"
               "other value is from 0.1 to 100.0.");
    return table;
}

cfg::RenderHeader Header() {
    cfg::RenderHeader header;
    header.display_name = kDisplayName;
    return header;
}

cfg::LegacyImport<Config> Import() {
    cfg::LegacyImport<Config> import;
    import.run = &RunImport;
    for (const legacy::Key& key : legacy::ReadKeys()) import.keys.push_back({key.section, key.key});
    return import;
}

cfg::ConfigOwnerOptions<Config> OwnerOptions(const std::filesystem::path& folder, cfg::DefaultsFile defaults) {
    cfg::ConfigOwnerOptions<Config> options;
    options.path = (folder / kIniName).wstring();
    options.table = Table();
    options.import = Import();
    options.legacy_path = (folder / kLegacyIniName).wstring();
    options.header = Header();
    options.defaults = std::move(defaults);
    return options;
}

Config Load(const std::filesystem::path& folder, cfg::DefaultsFile defaults) {
    g_owner = std::make_unique<cfg::ConfigOwner<Config>>(OwnerOptions(folder, std::move(defaults)));
    const cfg::ConfigLoadResult<Config> result = g_owner->Load();
    for (const std::string& line : result.log) Log::Line("[config] %s", line.c_str());
    if (!result.reason.empty()) Log::Line("[config] %s", result.reason.c_str());
    Log::Line("[config] %s", cfg::ConfigLoadStatusName(result.status));
    return result.config;
}

cameraunlock::TrackingMode StartupTrackingMode(const Config& config) {
    const auto mode = cameraunlock::DecodeTrackingMode(config.rotation_enabled, config.position_enabled);
    if (!mode) throw std::logic_error("RotationEnabled and PositionEnabled are both false, which the table never gives");
    return *mode;
}

void SaveTrackingMode(cameraunlock::TrackingMode mode) {
    const cameraunlock::TrackingModeChannels channels = cameraunlock::EncodeTrackingMode(mode);
    const cfg::ConfigSaveResult result = g_owner->Save([channels](Config& c) {
        c.rotation_enabled = channels.rotation_enabled;
        c.position_enabled = channels.position_enabled;
    });
    if (result.status != cfg::ConfigSaveStatus::Saved) {
        Log::Line("[config] [General] RotationEnabled and [Position] PositionEnabled %s: %s",
                  cfg::ConfigSaveStatusName(result.status), result.reason.c_str());
    }
    for (const std::string& line : result.log) Log::Line("[config] %s", line.c_str());
}

}  // namespace acr_ht::config
