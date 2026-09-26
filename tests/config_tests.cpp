// CameraUnlock.ini on the canonical format: the committed file
// (config/CameraUnlock.ini) is the table's fresh render, a first launch creates
// exactly those bytes, the mode cycle's save changes the lines of its own rows
// and no other byte, End's row is not saved, a row holding default follows
// Defaults.ini, and [Camera] NearClipCm takes 0 or 0.1 to 100.
//
// acr_ht_config_tests --render-config <path> writes the rendered file to <path>
// instead (pixi run render-config).

#include "config.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "cameraunlock/config/canonical_ini.h"
#include "cameraunlock/input/key_bindings.h"

namespace fs = std::filesystem;
namespace cfg = cameraunlock::config;
namespace config = acr_ht::config;

using acr_ht::Config;
using cameraunlock::TrackingMode;

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& message) {
    if (condition) return;
    std::printf("FAIL: %s\n", message.c_str());
    ++g_failures;
}

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    const std::size_t at = text.find(from);
    if (at == std::string::npos) throw std::logic_error("'" + from + "' is not in the text");
    return text.replace(at, from.size(), to);
}

std::string Rendered() { return cfg::RenderCanonicalFresh(config::Table(), config::Header()); }

fs::path TempDir() {
    wchar_t temp[MAX_PATH + 1] = {};
    if (GetTempPathW(MAX_PATH + 1, temp) == 0) throw std::runtime_error("GetTempPathW failed");
    const fs::path dir = fs::path(temp) / ("acr_ht_config_tests_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void RenderMatchesCommittedFile() {
    const std::string committed = ReadBytes(fs::path(ACR_HT_SOURCE_DIR) / "config" / "CameraUnlock.ini");
    Check(committed == Rendered(),
          "config/CameraUnlock.ini is not the table's fresh render; run pixi run render-config");
    const cfg::CanonicalIni doc = cfg::ParseCanonicalIni(committed);
    Check(doc.IsReadable() && doc.diagnostics.empty(), "the committed file reads without a diagnostic");
    Config read = config::Table().defaults();
    Check(cfg::ApplyCanonical(doc, config::Table(), read).diagnostics.empty(),
          "the committed file applies without a diagnostic");
}

void DefaultsAreTheFleetDefaults() {
    using cameraunlock::input::KeyBinding;
    using cameraunlock::input::KeyModifiers;
    constexpr KeyModifiers kChord = KeyModifiers::kCtrl | KeyModifiers::kShift;
    const Config defaults = config::Table().defaults();
    Check(defaults.toggle_key == "End, Ctrl+Shift+Y", "ToggleKey defaults to End, Ctrl+Shift+Y");
    Check(defaults.cycle_tracking_mode_key == "PageUp, Ctrl+Shift+G",
          "CycleTrackingModeKey defaults to PageUp, Ctrl+Shift+G");
    Check(defaults.enable_on_startup, "head tracking is on at startup by default");
    Check(config::StartupTrackingMode(defaults) == TrackingMode::RotationAndPosition,
          "the default tracking mode is rotation and position");
    const auto toggle = cameraunlock::input::ParseKeyBindings(defaults.toggle_key);
    Check(toggle.ok() && toggle.bindings.size() == 2 &&
              toggle.bindings[0] == KeyBinding{KeyModifiers::kNone, VK_END} &&
              toggle.bindings[1] == KeyBinding{kChord, 'Y'},
          "ToggleKey registers End and Ctrl+Shift+Y");
    const auto cycle = cameraunlock::input::ParseKeyBindings(defaults.cycle_tracking_mode_key);
    Check(cycle.ok() && cycle.bindings.size() == 2 &&
              cycle.bindings[0] == KeyBinding{KeyModifiers::kNone, VK_PRIOR} &&
              cycle.bindings[1] == KeyBinding{kChord, 'G'},
          "CycleTrackingModeKey registers Page Up and Ctrl+Shift+G");
    Check(defaults.near_clip_cm == 1.0f, "the near clip plane defaults to 1 cm");
}

// The mode cycle's save changes the two mode lines, from `default` to their
// values, and no other byte; End has no row it may save; the next launch reads
// the saved mode.
void SavesChangeOnlyTheirRows(const fs::path& dir) {
    const fs::path folder = dir / "saves";
    fs::create_directories(folder);
    const fs::path defaults = dir / "saves-global" / "Defaults.ini";
    const fs::path path = folder / "CameraUnlock.ini";
    const auto options = [&] { return config::OwnerOptions(folder, cfg::DefaultsFile::At(defaults.wstring())); };

    cfg::ConfigOwner<Config> owner(options());
    const cfg::ConfigLoadResult<Config> created = owner.Load();
    Check(created.status == cfg::ConfigLoadStatus::Created,
          std::string("a first launch with no legacy file creates the file, not ") +
              cfg::ConfigLoadStatusName(created.status));
    const std::string fresh = ReadBytes(path);
    Check(fresh == Rendered(), "a first launch writes the committed file's bytes");
    Check(!fs::exists(folder / "HeadTracking.ini"), "a first launch writes no legacy file");
    const std::string defaultsBytes = ReadBytes(defaults);

    const cameraunlock::TrackingModeChannels positionOnly = cameraunlock::EncodeTrackingMode(TrackingMode::PositionOnly);
    const cfg::ConfigSaveResult saved = owner.Save([&](Config& c) {
        c.rotation_enabled = positionOnly.rotation_enabled;
        c.position_enabled = positionOnly.position_enabled;
    });
    Check(saved.status == cfg::ConfigSaveStatus::Saved, "the mode cycle saves");
    const std::string afterMode = ReadBytes(path);
    Check(afterMode == Replace(Replace(fresh, "\r\nRotationEnabled=default\r\n", "\r\nRotationEnabled=false\r\n"),
                               "\r\nPositionEnabled=default\r\n", "\r\nPositionEnabled=true\r\n"),
          "the mode cycle changes the two mode lines and no other byte");

    bool refused = false;
    try {
        owner.Save([](Config& c) { c.enable_on_startup = false; });
    } catch (const std::exception&) {
        refused = true;
    }
    Check(refused, "EnableOnStartup is not a row a save may change, so End can never persist");
    Check(ReadBytes(path) == afterMode, "a refused save writes nothing");
    Check(ReadBytes(defaults) == defaultsBytes, "no save writes Defaults.ini");

    const cfg::ConfigLoadResult<Config> next = cfg::ConfigOwner<Config>(options()).Load();
    Check(next.status == cfg::ConfigLoadStatus::Canonical, "the next launch reads CameraUnlock.ini");
    Check(config::StartupTrackingMode(next.config) == TrackingMode::PositionOnly,
          "the next launch starts in position only");
    Check(ReadBytes(path) == afterMode, "the next launch writes nothing");
}

// A row holding default takes Defaults.ini's value; a local row never does.
void DefaultRowsFollowDefaultsIni(const fs::path& dir) {
    const fs::path folder = dir / "follow";
    fs::create_directories(folder);
    const fs::path defaults = dir / "follow-global" / "Defaults.ini";
    fs::create_directories(defaults.parent_path());
    WriteBytes(defaults,
               "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n[Position]\r\nPositionLimitZBack=0.02\r\n\r\n"
               "[Hotkeys]\r\nToggleKey=F8\r\n");
    WriteBytes(folder / "CameraUnlock.ini", Rendered());
    const cfg::ConfigLoadResult<Config> loaded =
        cfg::ConfigOwner<Config>(config::OwnerOptions(folder, cfg::DefaultsFile::At(defaults.wstring()))).Load();
    Check(loaded.status == cfg::ConfigLoadStatus::Canonical, "the committed file loads as canonical");
    Check(loaded.config.position_limit_z_back == 0.02f, "PositionLimitZBack=default follows Defaults.ini");
    Check(loaded.config.toggle_key == "F8", "ToggleKey=default follows Defaults.ini");
    Check(loaded.config.near_clip_cm == 1.0f, "the game's own NearClipCm keeps its value");
}

// NearClipCm reads 0 or a distance from 0.1 to 100, and anything else keeps
// the default with a diagnostic naming the line.
void NearClipTakesZeroOrPointOneToHundred() {
    const auto read = [](const char* value, float& out) {
        const std::string text = Replace(Rendered(), "\r\nNearClipCm=1.0\r\n", std::string("\r\nNearClipCm=") + value + "\r\n");
        Config c = config::Table().defaults();
        const cfg::ApplyReport report = cfg::ApplyCanonical(cfg::ParseCanonicalIni(text), config::Table(), c);
        out = c.near_clip_cm;
        return report.diagnostics.empty();
    };
    float v = 0.0f;
    Check(read("0", v) && v == 0.0f, "NearClipCm=0 leaves the game's plane alone");
    Check(read("0.1", v) && v == 0.1f, "NearClipCm=0.1 is the nearest plane taken");
    Check(read("100", v) && v == 100.0f, "NearClipCm=100 is the furthest plane taken");
    Check(!read("0.05", v) && v == 1.0f, "NearClipCm=0.05 keeps the default with a diagnostic");
    Check(!read("150", v) && v == 1.0f, "NearClipCm=150 keeps the default with a diagnostic");
    Check(!read("-1", v) && v == 1.0f, "NearClipCm=-1 keeps the default with a diagnostic");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc == 3 && std::string(argv[1]) == "--render-config") {
        WriteBytes(argv[2], Rendered());
        std::printf("wrote %s\n", argv[2]);
        return 0;
    }
    if (argc != 1) {
        std::printf("usage: acr_ht_config_tests [--render-config <path>]\n");
        return 2;
    }
    const fs::path dir = TempDir();
    RenderMatchesCommittedFile();
    DefaultsAreTheFleetDefaults();
    SavesChangeOnlyTheirRows(dir);
    DefaultRowsFollowDefaultsIni(dir);
    NearClipTakesZeroOrPointOneToHundred();
    fs::remove_all(dir);
    if (g_failures != 0) {
        std::printf("%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all config checks passed\n");
    return 0;
}
