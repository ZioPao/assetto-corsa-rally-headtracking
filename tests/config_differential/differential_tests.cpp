// The differential test for the conversion from HeadTracking.ini to
// CameraUnlock.ini.
//
//   Oracle     v1.1.0's reader and startup code, the newest published build
//              (oracle_adapter.h; the repo publishes v* releases only)
//   Import     the frozen reader in src/legacy_config/, through the same
//              startup code
//   Migration  the config owner's Load in a folder holding only the input as
//              HeadTracking.ini, which imports it through config::Import into
//              a new CameraUnlock.ini, then this build's startup code
//              (src/startup.cpp, which the mod calls) on what the session runs
//              on
//
// v1.1.0's startup code is restated in ObservePublished rather than run: it
// lived in headtracking_mod.cpp beside the game hooks and the bootstrap
// thread, which cannot link outside the game process. The restatement matches
// `git show v1.1.0:src/headtracking_mod.cpp`, ApplyConfigToPipeline and
// RegisterHotkeys.
//
// Comparison 1, oracle against import, is what a player sees change that the
// conversion did not cause: commits since v1.1.0 that change how the file is
// read. There are none. The frozen reader is the reader of 8435729, which
// differs from v1.1.0's only in taking its two smoothing defaults from
// cameraunlock-core's constants, which hold the same 0.0 and 0.15, and every
// core source both compile holds the same bytes at v1.1.0's pin and this one
// (tests/CMakeLists.txt pins them), so the comparison may find no difference
// at all, floats bit for bit.
//
// Comparison 2, import against migration, is the proof for the migration: no
// difference but the approved one, which the import records as dropped. A
// sensitivity or axis inversion the player set away from what v1.1.0 shipped
// (1 and false for every one) is dropped (pose_shaping). The reader clamps
// every float it takes into a finite range the canonical rows hold, and holds
// every hotkey code to a bindable key, so N1 and N2 never apply. The four
// position limits' defaults moved to the fleet's (0.30, 0.20 up and down,
// 0.40 forward, 0.10 back, from 0.15, 0.12, 0.20 and 0.0), so the no-file
// input may differ there and nowhere else; every file with a limit, or without
// the key, imports the value v1.1.0 ran on.
//
// Each input migrates three times: over a Defaults.ini the owner creates with
// the built-in values, from a read-only HeadTracking.ini, and over a
// Defaults.ini that differs from the built-in value on every global row the
// table binds. All three give the settings the import read, since the
// migration writes `default` only where the imported value is what `default`
// gives at that launch. After every load HeadTracking.ini keeps its bytes,
// write time and attributes, the folder holds it and CameraUnlock.ini and
// nothing else, and a second load reads CameraUnlock.ini, imports nothing and
// changes neither file. The distinct migrated files are written beside the
// executable under migrated\, for lint-migrated.mjs to run core's canonical
// config lint over.
//
// Inputs: the first-run HeadTracking.ini of every published build (no build
// shipped one in a ZIP or seeded one; each wrote its own at first launch), no
// file, an empty file, core's mutation corpus over v1.1.0's first-run file,
// and that file with each of its four hotkey codes set to every code from 0x01
// to 0xFE.

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"
#include "startup.h"

#include "cameraunlock/config/canonical_ini.h"
#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

namespace {

namespace fs = std::filesystem;
namespace cfg = cameraunlock::config;
namespace config = acr_ht::config;
namespace testing = cameraunlock::config::testing;
namespace legacy = acr_ht::legacy;
using acr_ht::Config;

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const std::string& what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL: %s\n", what.c_str());
}

std::string ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteFileBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("cannot write " + path.string());
}

// ---- Scratch folders ---------------------------------------------------------
//
// One folder per reading: GetPrivateProfileString, which both readers sit on,
// is free to cache the file it last read. `game` stands for the folder beside
// acr.exe; Defaults.ini sits in `global` beside it. Every folder lives under
// one root for the run and is removed once its input is done, so a run holds
// a handful of folders at a time rather than thousands.

void RemoveTree(const fs::path& root) {
    if (!fs::exists(root)) return;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
    }
    fs::remove_all(root);
}

const fs::path& ScratchRoot() {
    static const fs::path root = [] {
        wchar_t temp[MAX_PATH + 1] = {};
        if (GetTempPathW(MAX_PATH + 1, temp) == 0) throw std::runtime_error("GetTempPathW failed");
        fs::path r = fs::path(temp) / ("acr_ht_diff_" + std::to_string(GetCurrentProcessId()));
        RemoveTree(r);
        return r;
    }();
    return root;
}

class Scratch {
public:
    Scratch() {
        static unsigned s_next = 0;
        root_ = ScratchRoot() / std::to_string(s_next++);
        fs::create_directories(root_ / "game");
    }
    ~Scratch() { RemoveTree(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    fs::path game() const { return root_ / "game"; }
    fs::path legacy() const { return game() / "HeadTracking.ini"; }
    fs::path canonical() const { return game() / "CameraUnlock.ini"; }
    fs::path defaults() const { return root_ / "global" / "Defaults.ini"; }

    void WriteLegacy(const std::string& bytes) const { WriteFileBytes(legacy(), bytes); }

    void WriteDefaults(const std::string& bytes) const {
        fs::create_directories(defaults().parent_path());
        WriteFileBytes(defaults(), bytes);
    }

    std::set<std::string> Names() const {
        std::set<std::string> names;
        for (const auto& entry : fs::directory_iterator(game())) names.insert(entry.path().filename().string());
        return names;
    }

    cfg::ConfigOwnerOptions<Config> Options() const {
        return config::OwnerOptions(game(), cfg::DefaultsFile::At(defaults().wstring()));
    }

private:
    fs::path root_;
};

// ---- What a reading does -------------------------------------------------------
//
// A Record names everything the running mod acts on after reading the file:
// `field.*` the settings, `start.*` the state the session starts in, `hotkey.*`
// the bindings that fire each action, each as `modifiers:code` (Ctrl 1, Shift
// 2, as cameraunlock::input::KeyModifiers numbers them) in ascending order.
// Floats are their bits.

using Record = std::map<std::string, std::string>;

std::string Bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(bits));
    return text;
}

std::string Flag(bool value) { return value ? "1" : "0"; }

struct Binding {
    unsigned modifiers;
    int vk;
};

std::string BindingsText(std::vector<Binding> bindings) {
    std::sort(bindings.begin(), bindings.end(), [](const Binding& a, const Binding& b) {
        return a.modifiers != b.modifiers ? a.modifiers < b.modifiers : a.vk < b.vk;
    });
    std::string text;
    for (const Binding& b : bindings) {
        char item[32];
        std::snprintf(item, sizeof(item), "%s%u:0x%02X", text.empty() ? "" : " ", b.modifiers,
                      static_cast<unsigned>(b.vk));
        text += item;
    }
    return text;
}

// A published Config (v1.1.0's, or the frozen reader's copy of it) through
// v1.1.0's startup code, restated: its ApplyConfigToPipeline hands the
// sensitivities and inversions to the processors and builds the position settings with PositionSettings::Symmetric, which puts LimitY on both
// vertical bounds; the mode starts as rotation and position, or rotation only
// when [Position] Enabled is false; tracking starts as EnableOnStartup says;
// and RegisterHotkeys binds each action's nav key with NavGuarded, which does
// not fire while Ctrl and Shift are both held, and its chord letter with
// ChordGuarded, which fires only while they are.
template <class C>
Record ObservePublished(const C& c) {
    Record r;
    r["field.udp_port"] = std::to_string(c.udp_port);
    r["field.rot.yaw_sensitivity"] = Bits(c.yaw_sensitivity);
    r["field.rot.pitch_sensitivity"] = Bits(c.pitch_sensitivity);
    r["field.rot.roll_sensitivity"] = Bits(c.roll_sensitivity);
    r["field.rot.invert_yaw"] = Flag(c.invert_yaw);
    r["field.rot.invert_pitch"] = Flag(c.invert_pitch);
    r["field.rot.invert_roll"] = Flag(c.invert_roll);
    r["field.local_smoothing"] = Bits(c.local_smoothing);
    r["field.remote_smoothing"] = Bits(c.remote_smoothing);
    r["field.near_clip_cm"] = Bits(c.near_clip_cm);
    r["field.pos.sensitivity_x"] = Bits(c.position_sensitivity_x);
    r["field.pos.sensitivity_y"] = Bits(c.position_sensitivity_y);
    r["field.pos.sensitivity_z"] = Bits(c.position_sensitivity_z);
    r["field.pos.invert_x"] = Flag(c.invert_position_x);
    r["field.pos.invert_y"] = Flag(c.invert_position_y);
    r["field.pos.invert_z"] = Flag(c.invert_position_z);
    r["field.pos.limit_x"] = Bits(c.limit_x);
    r["field.pos.limit_y"] = Bits(c.limit_y);
    r["field.pos.limit_y_down"] = Bits(c.limit_y);
    r["field.pos.limit_z"] = Bits(c.limit_z);
    r["field.pos.limit_z_back"] = Bits(c.limit_z_back);
    r["start.enabled"] = Flag(c.enable_on_startup);
    r["start.mode"] = c.position_enabled ? "RotationAndPosition" : "RotationOnly";
    r["hotkey.Toggle"] = BindingsText({{0, c.toggle_key}, {3, c.chord_toggle_key}});
    r["hotkey.CycleTrackingMode"] = BindingsText({{0, c.cycle_mode_key}, {3, c.chord_cycle_mode_key}});
    return r;
}

const char* ModeName(cameraunlock::TrackingMode mode) {
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition: return "RotationAndPosition";
        case cameraunlock::TrackingMode::RotationOnly:        return "RotationOnly";
        case cameraunlock::TrackingMode::PositionOnly:        return "PositionOnly";
    }
    throw std::logic_error("a tracking mode outside the three");
}

// The state a session starts in from this build's startup code: the session
// the mod builds, set up by the ApplyConfigToPipeline the mod calls, and the
// hotkey lists its RegisterHotkeys registers. A binding without modifiers does
// not fire while Ctrl and Shift are both held, as NavGuarded did, and a
// Ctrl+Shift binding fires only while they are, as ChordGuarded did.
Record ObserveCanonical(const Config& c) {
    cameraunlock::UdpReceiver receiver;
    acr_ht::Session session(receiver);
    acr_ht::ApplyConfigToPipeline(c, session);
    const cameraunlock::SensitivitySettings& rot = session.GetProcessor().GetSensitivity();
    const cameraunlock::PositionSettings& pos = session.GetPositionSettings();

    Record r;
    r["field.udp_port"] = std::to_string(c.udp_port);
    r["field.rot.yaw_sensitivity"] = Bits(rot.yaw);
    r["field.rot.pitch_sensitivity"] = Bits(rot.pitch);
    r["field.rot.roll_sensitivity"] = Bits(rot.roll);
    r["field.rot.invert_yaw"] = Flag(rot.invert_yaw);
    r["field.rot.invert_pitch"] = Flag(rot.invert_pitch);
    r["field.rot.invert_roll"] = Flag(rot.invert_roll);
    r["field.local_smoothing"] = Bits(session.GetLocalSmoothing());
    r["field.remote_smoothing"] = Bits(session.GetRemoteSmoothing());
    r["field.near_clip_cm"] = Bits(c.near_clip_cm);
    r["field.pos.sensitivity_x"] = Bits(pos.sensitivity_x);
    r["field.pos.sensitivity_y"] = Bits(pos.sensitivity_y);
    r["field.pos.sensitivity_z"] = Bits(pos.sensitivity_z);
    r["field.pos.invert_x"] = Flag(pos.invert_x);
    r["field.pos.invert_y"] = Flag(pos.invert_y);
    r["field.pos.invert_z"] = Flag(pos.invert_z);
    r["field.pos.limit_x"] = Bits(pos.limit_x);
    r["field.pos.limit_y"] = Bits(pos.limit_y);
    r["field.pos.limit_y_down"] = Bits(pos.limit_y_down);
    r["field.pos.limit_z"] = Bits(pos.limit_z);
    r["field.pos.limit_z_back"] = Bits(pos.limit_z_back);
    r["start.enabled"] = Flag(c.enable_on_startup);
    r["start.mode"] = ModeName(session.GetMode());
    for (const acr_ht::HotkeyList& list : acr_ht::HotkeyLists(c)) {
        std::vector<Binding> bindings;
        for (const cameraunlock::input::KeyBinding& b : list.bindings) {
            bindings.push_back({static_cast<unsigned>(b.modifiers), b.vk});
        }
        const char* name = list.action == acr_ht::HotkeyAction::ToggleTracking ? "hotkey.Toggle" : "hotkey.CycleTrackingMode";
        Check(r.find(name) == r.end(), std::string(name) + " is registered from one list");
        r[name] = BindingsText(bindings);
    }
    return r;
}

std::vector<std::string> Differences(const Record& a, const Record& b) {
    std::vector<std::string> out;
    for (const auto& [name, value] : a) {
        const auto it = b.find(name);
        if (it == b.end()) {
            out.push_back(name + " only on the left");
        } else if (it->second != value) {
            out.push_back(name + ": " + value + " / " + it->second);
        }
    }
    for (const auto& [name, value] : b) {
        if (a.find(name) == a.end()) out.push_back(name + " only on the right");
    }
    return out;
}

// ---- The approved differences ----------------------------------------------------
//
// What a session runs on once the import has dropped a value: the record the
// import gave, with each dropped value's effect applied. Only pose shaping
// is ever dropped here, so a drop of any other rule fails.

const std::map<std::pair<std::string, std::string>, std::pair<std::string, std::string>>& DropEffects() {
    static const std::map<std::pair<std::string, std::string>, std::pair<std::string, std::string>> effects = {
        {{"Rotation", "YawSensitivity"}, {"field.rot.yaw_sensitivity", Bits(1.0f)}},
        {{"Rotation", "PitchSensitivity"}, {"field.rot.pitch_sensitivity", Bits(1.0f)}},
        {{"Rotation", "RollSensitivity"}, {"field.rot.roll_sensitivity", Bits(1.0f)}},
        {{"Rotation", "InvertYaw"}, {"field.rot.invert_yaw", "0"}},
        {{"Rotation", "InvertPitch"}, {"field.rot.invert_pitch", "0"}},
        {{"Rotation", "InvertRoll"}, {"field.rot.invert_roll", "0"}},
        {{"Position", "SensitivityX"}, {"field.pos.sensitivity_x", Bits(1.0f)}},
        {{"Position", "SensitivityY"}, {"field.pos.sensitivity_y", Bits(1.0f)}},
        {{"Position", "SensitivityZ"}, {"field.pos.sensitivity_z", Bits(1.0f)}},
        {{"Position", "InvertX"}, {"field.pos.invert_x", "0"}},
        {{"Position", "InvertY"}, {"field.pos.invert_y", "0"}},
        {{"Position", "InvertZ"}, {"field.pos.invert_z", "0"}},
    };
    return effects;
}

Record Expected(Record imported, const cfg::ImportResult& result, const std::string& name) {
    for (const cfg::DroppedValue& d : result.dropped) {
        const auto it = DropEffects().find({d.section, d.key});
        Check(it != DropEffects().end() && d.rule == cfg::DropRule::PoseShaping,
              name + ": the import drops [" + d.section + "] " + d.key + ", which only pose shaping may be");
        if (it == DropEffects().end()) continue;
        imported[it->second.first] = it->second.second;
    }
    for (const cfg::PoseShapingValue& v : result.pose_shaping) {
        const bool dropped = std::any_of(result.dropped.begin(), result.dropped.end(), [&](const cfg::DroppedValue& d) {
            return d.rule == cfg::DropRule::PoseShaping && d.section == v.section && d.key == v.key && d.value == v.value;
        });
        Check(v.folded != dropped, name + ": [" + v.section + "] " + v.key + "=" + v.value +
                                       " is dropped exactly when it is not what v1.1.0 shipped");
    }
    Check(result.pose_shaping.size() == DropEffects().size(),
          name + ": the import passes every sensitivity and inversion it read through LegacyPoseShaping");
    return imported;
}

// With no HeadTracking.ini the session runs on the table's defaults, which
// differ from v1.1.0's only in the four position limits the conversion moved
// to the fleet's.
Record MovedDefaults(Record imported) {
    const Config defaults = config::Table().defaults();
    imported["field.pos.limit_x"] = Bits(defaults.position_limit_x);
    imported["field.pos.limit_y"] = Bits(defaults.position_limit_y);
    imported["field.pos.limit_y_down"] = Bits(defaults.position_limit_y_down);
    imported["field.pos.limit_z"] = Bits(defaults.position_limit_z);
    imported["field.pos.limit_z_back"] = Bits(defaults.position_limit_z_back);
    return imported;
}

// ---- Inputs --------------------------------------------------------------------

fs::path DataPath(const char* name) {
    return fs::path(ACR_HT_SOURCE_DIR) / "tests" / "config_differential" / "data" / name;
}

std::string NewestFirstRun() { return ReadFileBytes(DataPath("first-run-v1.1.0.ini")); }

// Every key the frozen reader reads, and how the corpus varies each one. The
// reader refuses a port outside 1024-65535 and a hotkey code that is not a
// bindable key, and clamps a smoothing value into 0-1, a sensitivity into
// -100 to 100, a position limit into 0-10 and a near clip into 0.1-100 or 0.
std::vector<testing::MutationKey> CorpusKeys() {
    const std::vector<std::string> sensitivity = {"150", "-150"};
    const std::vector<std::string> smoothing = {"-0.5", "1.5"};
    const std::vector<std::string> limit = {"-1", "11"};
    const std::vector<std::string> hotkey = {"0x11", "0xFF"};
    return {
        {"Network", "UdpPort", "5000", {"80", "70000"}},
        {"General", "EnableOnStartup", "0", {}},
        {"Hotkeys", "ToggleKey", "0x2D", hotkey, true},
        {"Hotkeys", "CycleModeKey", "0x2E", hotkey, true},
        {"Hotkeys", "ChordToggleKey", "0x4B", hotkey, true},
        {"Hotkeys", "ChordCycleModeKey", "0x4A", hotkey, true},
        {"Rotation", "YawSensitivity", "0.5", sensitivity},
        {"Rotation", "PitchSensitivity", "0.5", sensitivity},
        {"Rotation", "RollSensitivity", "0.5", sensitivity},
        {"Rotation", "InvertYaw", "1", {}},
        {"Rotation", "InvertPitch", "1", {}},
        {"Rotation", "InvertRoll", "1", {}},
        {"Rotation", "LocalSmoothing", "0.3", smoothing},
        {"Rotation", "RemoteSmoothing", "0.6", smoothing},
        {"Camera", "NearClipCm", "2.5", {"-1", "0.05", "150"}},
        {"Position", "Enabled", "0", {}},
        {"Position", "SensitivityX", "0.5", sensitivity},
        {"Position", "SensitivityY", "0.5", sensitivity},
        {"Position", "SensitivityZ", "0.5", sensitivity},
        {"Position", "InvertX", "1", {}},
        {"Position", "InvertY", "1", {}},
        {"Position", "InvertZ", "1", {}},
        {"Position", "LimitX", "0.25", limit},
        {"Position", "LimitY", "0.1", limit},
        {"Position", "LimitZ", "0.3", limit},
        {"Position", "LimitZBack", "0.05", limit},
    };
}

// The generator refuses the call when these and the descriptors name different
// keys, so the corpus covers every key the reader reads.
std::vector<cameraunlock::config::LegacyKey> CorpusReads() {
    std::vector<cameraunlock::config::LegacyKey> reads;
    for (const legacy::Key& key : legacy::ReadKeys()) reads.push_back({key.section, key.key});
    return reads;
}

struct Input {
    std::string name;
    bool present;
    std::string bytes;
};

std::string WithCode(const std::string& base, const std::string& line, int code) {
    const std::size_t at = base.find(line);
    if (at == std::string::npos) throw std::logic_error("no " + line + " in v1.1.0's first-run file");
    const std::string key = line.substr(0, line.find('=') + 1);
    char value[8];
    std::snprintf(value, sizeof(value), "0x%02X", static_cast<unsigned>(code));
    std::string out = base;
    return out.replace(at, line.size(), key + value);
}

std::vector<Input> Inputs() {
    std::vector<Input> inputs = {
        {"v1.0.0 first run", true, ReadFileBytes(DataPath("first-run-v1.0.0.ini"))},
        {"v1.0.1 first run", true, ReadFileBytes(DataPath("first-run-v1.0.1.ini"))},
        {"v1.0.2 first run", true, ReadFileBytes(DataPath("first-run-v1.0.2.ini"))},
        {"v1.0.3 first run", true, ReadFileBytes(DataPath("first-run-v1.0.3.ini"))},
        {"v1.1.0 first run", true, NewestFirstRun()},
        {"no file", false, {}},
        {"empty file", true, {}},
    };
    for (testing::IniMutation& m : testing::GenerateIniMutations(NewestFirstRun(), CorpusReads(), CorpusKeys())) {
        inputs.push_back({"corpus: " + m.name, true, std::move(m.bytes)});
    }
    for (const char* line : {"ToggleKey=0x23", "CycleModeKey=0x21", "ChordToggleKey=0x59", "ChordCycleModeKey=0x47"}) {
        for (int code = 0x01; code <= 0xFE; ++code) {
            char name[64];
            std::snprintf(name, sizeof(name), "%.*s0x%02X", static_cast<int>(std::strchr(line, '=') - line + 1), line,
                          static_cast<unsigned>(code));
            inputs.push_back({name, true, WithCode(NewestFirstRun(), line, code)});
        }
    }
    return inputs;
}

// ---- Checks on a load ------------------------------------------------------------

// A file's bytes, last write time and attributes, which no load may change.
struct FileState {
    std::string bytes;
    unsigned long long written = 0;
    DWORD attributes = 0;
    bool operator==(const FileState& other) const {
        return bytes == other.bytes && written == other.written && attributes == other.attributes;
    }
};

std::optional<FileState> StateOf(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return std::nullopt;
        throw std::runtime_error("cannot read the attributes of " + path.string());
    }
    FileState state;
    state.bytes = ReadFileBytes(path);
    state.written = (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                    data.ftLastWriteTime.dwLowDateTime;
    state.attributes = data.dwFileAttributes;
    return state;
}

bool AsciiCrlf(const std::string& bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c > 0x7E) return false;
        if (c == '\r' && (i + 1 == bytes.size() || bytes[i + 1] != '\n')) return false;
        if (c == '\n' && (i == 0 || bytes[i - 1] != '\r')) return false;
        if (c < 0x20 && c != '\r' && c != '\n') return false;
    }
    return !bytes.empty() && bytes.back() == '\n';
}

bool LogSays(const std::vector<std::string>& log, const std::string& text) {
    return std::any_of(log.begin(), log.end(), [&](const std::string& line) { return line.find(text) != std::string::npos; });
}

// What the reader and the table find in a canonical file, read over the table's
// own defaults, which stand for a Defaults.ini holding the built-in values.
std::vector<std::string> CanonicalDiagnostics(const std::string& bytes, Config& out) {
    std::vector<std::string> found;
    const cfg::CanonicalIni doc = cfg::ParseCanonicalIni(bytes);
    for (const cfg::CanonicalDiagnostic& d : doc.diagnostics) found.push_back("reader: " + cfg::DescribeCanonicalDiagnostic(d));
    const cfg::ConfigTable<Config> table = config::Table();
    out = table.defaults();
    for (const cfg::CanonicalDiagnostic& d : cfg::ApplyCanonical(doc, table, out).diagnostics) {
        found.push_back("table: " + cfg::DescribeCanonicalDiagnostic(d));
    }
    return found;
}

// A Defaults.ini holding a value other than the built-in one on every global row
// the table binds, so a migration that wrote `default` where the imported value
// is not what `default` gives would read back differently over it.
const char* const kSkewedDefaults =
    "[CameraUnlock]\r\nConfigFormat=1\r\n\r\n"
    "[Network]\r\nUdpPort=5252\r\n\r\n"
    "[General]\r\nEnableOnStartup=false\r\nRotationEnabled=true\r\n\r\n"
    "[Smoothing]\r\nLocalSmoothing=0.5\r\nRemoteSmoothing=0.5\r\n\r\n"
    "[Position]\r\nPositionEnabled=false\r\nPositionLimitX=0.5\r\nPositionLimitY=0.45\r\n"
    "PositionLimitYDown=0.35\r\nPositionLimitZ=0.6\r\nPositionLimitZBack=0.25\r\n\r\n"
    "[Hotkeys]\r\nToggleKey=F8\r\nCycleTrackingModeKey=F9\r\n";

// The folder beside this executable the migrated files are written to, for
// lint-migrated.mjs, which pixi run test runs after this test.
fs::path MigratedFolder() {
    std::vector<wchar_t> exe(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size()));
        if (length == 0) throw std::runtime_error("cannot find this executable's path");
        if (length < exe.size()) return fs::path(std::wstring(exe.data(), length)).parent_path() / "migrated";
        exe.resize(exe.size() * 2);
    }
}

struct Tally {
    int created = 0;
    int migrated = 0;
    std::set<std::string> files;
};

// Runs the owner's Load in `s`, whose game folder holds the input as
// HeadTracking.ini or nothing, checks what a load must do beyond comparison 2,
// and returns the settings the session runs on.
Config Migrate(const Input& input, const Scratch& s, const std::string& label, Tally& tally) {
    const std::optional<FileState> legacy_before = StateOf(s.legacy());
    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(StateOf(s.legacy()) == legacy_before, label + ": a load leaves HeadTracking.ini's bytes, write time and attributes");

    const cfg::ConfigLoadStatus want = input.present ? cfg::ConfigLoadStatus::Migrated : cfg::ConfigLoadStatus::Created;
    if (loaded.status != want) std::printf("  %s: %s, %s\n", label.c_str(), cfg::ConfigLoadStatusName(loaded.status), loaded.reason.c_str());
    Check(loaded.status == want, label + ": every legacy input imports, and no file gives a created one");
    if (loaded.status != want) return loaded.config;
    ++(input.present ? tally.migrated : tally.created);
    Check(s.Names() == (input.present ? std::set<std::string>{"CameraUnlock.ini", "HeadTracking.ini"}
                                      : std::set<std::string>{"CameraUnlock.ini"}),
          label + ": the game folder holds HeadTracking.ini and CameraUnlock.ini and nothing else");

    const std::string migrated = ReadFileBytes(s.canonical());
    Check(cfg::HasCanonicalStamp(migrated), label + ": CameraUnlock.ini carries the stamp");
    Check(AsciiCrlf(migrated), label + ": CameraUnlock.ini is ASCII with CRLF line ends");
    Config reread;
    const std::vector<std::string> diagnostics = CanonicalDiagnostics(migrated, reread);
    for (const std::string& d : diagnostics) std::printf("  %s: CameraUnlock.ini, %s\n", label.c_str(), d.c_str());
    Check(diagnostics.empty(), label + ": CameraUnlock.ini reads with no diagnostic");
    if (input.present) tally.files.insert(migrated);

    // The next start reads CameraUnlock.ini, imports nothing and writes nothing.
    const std::optional<FileState> created = StateOf(s.canonical());
    const cfg::ConfigLoadResult<Config> again = cfg::ConfigOwner<Config>(s.Options()).Load();
    Check(again.status == cfg::ConfigLoadStatus::Canonical, label + ": the next start reads CameraUnlock.ini");
    Check(Differences(ObserveCanonical(again.config), ObserveCanonical(loaded.config)).empty(),
          label + ": the next start runs on the same settings");
    Check(StateOf(s.canonical()) == created && StateOf(s.legacy()) == legacy_before,
          label + ": the next start changes neither file");
    Check(!input.present || LogSays(again.log, "is left as it was and is not read"),
          label + ": the next start logs that HeadTracking.ini is not read");
    return loaded.config;
}

// The committed file with each moved default's row holding the value v1.1.0's
// first-run file gives it, which is what that file migrates to over the
// built-in values: every other row is what `default` gives there.
std::string CommittedWithPublishedLimits(const std::string& committed) {
    std::string out = committed;
    const std::pair<const char*, const char*> rows[] = {
        {"\r\nPositionLimitX=default\r\n", "\r\nPositionLimitX=0.15\r\n"},
        {"\r\nPositionLimitY=default\r\n", "\r\nPositionLimitY=0.12\r\n"},
        {"\r\nPositionLimitYDown=default\r\n", "\r\nPositionLimitYDown=0.12\r\n"},
        {"\r\nPositionLimitZ=default\r\n", "\r\nPositionLimitZ=0.2\r\n"},
        {"\r\nPositionLimitZBack=default\r\n", "\r\nPositionLimitZBack=0.0\r\n"},
    };
    for (const auto& [from, to] : rows) {
        const std::size_t at = out.find(from);
        if (at == std::string::npos) throw std::logic_error(std::string("the committed file has no ") + from);
        out.replace(at, std::strlen(from), to);
    }
    return out;
}

// ---- Comparisons -----------------------------------------------------------------

void Compare(const std::vector<Input>& inputs) {
    const std::string committed = ReadFileBytes(fs::path(ACR_HT_SOURCE_DIR) / "config" / "CameraUnlock.ini");
    const cfg::ConfigTable<Config> table = config::Table();
    Tally builtin, readonly, skewed;
    int compared = 0;
    for (const Input& input : inputs) {
        const std::string& name = input.name;

        // The oracle writes its first-run file into a folder with none, as
        // v1.1.0 did, so it reads a copy of its own.
        Record oracle;
        {
            Scratch s;
            if (input.present) s.WriteLegacy(input.bytes);
            oracle = ObservePublished(acr_oracle_view::RunOracle(s.game().string()));
            if (!input.present) {
                Check(ReadFileBytes(s.legacy()) == NewestFirstRun(),
                      name + ": the oracle writes v1.1.0's first-run file, data/first-run-v1.1.0.ini");
            }
        }

        Scratch s;
        if (input.present) s.WriteLegacy(input.bytes);
        const std::set<std::string> before = s.Names();
        legacy::Config read;
        const bool present = legacy::Load(s.legacy().string(), read);
        Check(present == input.present, name + ": the frozen reader finds the file exactly when it is there");
        Check(s.Names() == before, name + ": the frozen reader creates no file");
        Check(!input.present || ReadFileBytes(s.legacy()) == input.bytes, name + ": the frozen reader writes nothing");

        const Record imported = ObservePublished(read);
        {
            const std::vector<std::string> diff = Differences(oracle, imported);
            for (const std::string& d : diff) std::printf("  comparison 1, %s: %s\n", name.c_str(), d.c_str());
            Check(diff.empty(), name + ": comparison 1, the oracle and the import agree");
        }

        // Comparison 2 over a Defaults.ini the owner creates with the built-in
        // values. The import's own result says what it dropped.
        Config mapped = table.defaults();
        const cfg::ImportResult result = config::Import().run({s.legacy().wstring(), s.legacy().string(), false}, mapped);
        Check(result.status == (input.present ? cfg::ImportStatus::Imported : cfg::ImportStatus::Absent),
              name + ": the import reads every input, as the published build did");
        Check(s.Names() == before, name + ": the import run on its own creates no file");
        const Record want = input.present ? Expected(imported, result, name) : MovedDefaults(Expected(imported, result, name));
        {
            const Config migrated = Migrate(input, s, name, builtin);
            const std::vector<std::string> diff = Differences(want, ObserveCanonical(migrated));
            for (const std::string& d : diff) std::printf("  comparison 2, %s: %s\n", name.c_str(), d.c_str());
            Check(diff.empty(), name + ": comparison 2, the session runs as the import read, apart from the approved drops");

            if (fs::exists(s.canonical())) {
                // Over the built-in values the table's own defaults stand for Defaults.ini.
                Config reread;
                CanonicalDiagnostics(ReadFileBytes(s.canonical()), reread);
                Check(Differences(ObserveCanonical(reread), ObserveCanonical(migrated)).empty(),
                      name + ": CameraUnlock.ini reads back as the settings the session runs on");
                // Fresh equals upgrade: no file gives the committed file, and the
                // file v1.1.0 wrote gives it apart from the moved defaults.
                if (name == "no file") {
                    Check(ReadFileBytes(s.canonical()) == committed, name + ": gives the committed file, byte for byte");
                }
                if (name == "v1.1.0 first run") {
                    Check(ReadFileBytes(s.canonical()) == CommittedWithPublishedLimits(committed),
                          name + ": gives the committed file with v1.1.0's position limits, byte for byte");
                }
            }
        }

        if (input.present) {
            // From a read-only HeadTracking.ini, which keeps its attribute. The
            // import run on its own first leaves the folder as it was.
            Scratch r;
            r.WriteLegacy(input.bytes);
            SetFileAttributesW(r.legacy().c_str(), FILE_ATTRIBUTE_READONLY);
            const std::set<std::string> readonly_before = r.Names();
            Config unused = table.defaults();
            config::Import().run({r.legacy().wstring(), r.legacy().string(), false}, unused);
            Check(r.Names() == readonly_before && ReadFileBytes(r.legacy()) == input.bytes,
                  name + ": the import leaves a read-only folder as it was");
            const Config c = Migrate(input, r, name + " (read-only)", readonly);
            Check(Differences(want, ObserveCanonical(c)).empty(),
                  name + ": a read-only HeadTracking.ini imports as a writable one does");
            Check((GetFileAttributesW(r.legacy().c_str()) & FILE_ATTRIBUTE_READONLY) != 0,
                  name + ": HeadTracking.ini keeps its read-only attribute");

            // Over a Defaults.ini that differs everywhere. With no legacy file
            // the settings are Defaults.ini's own, so only an input with a file
            // is held to the import here.
            Scratch k;
            k.WriteLegacy(input.bytes);
            k.WriteDefaults(kSkewedDefaults);
            const Config skew = Migrate(input, k, name + " (skewed Defaults.ini)", skewed);
            const std::vector<std::string> diff = Differences(want, ObserveCanonical(skew));
            for (const std::string& d : diff) std::printf("  comparison 2, %s (skewed Defaults.ini): %s\n", name.c_str(), d.c_str());
            Check(diff.empty(), name + ": the migration gives the import's settings over a Defaults.ini that differs everywhere");
        }
        ++compared;
    }
    std::printf("comparisons 1 and 2: %d inputs\n", compared);
    std::printf("over built-in Defaults.ini: %d created, %d migrated; read-only: %d migrated; skewed Defaults.ini: %d migrated\n",
                builtin.created, builtin.migrated, readonly.migrated, skewed.migrated);

    // Core's canonical config lint runs over these next (lint-migrated.mjs).
    std::set<std::string> files = builtin.files;
    files.insert(readonly.files.begin(), readonly.files.end());
    files.insert(skewed.files.begin(), skewed.files.end());
    const fs::path lint = MigratedFolder();
    fs::remove_all(lint);
    fs::create_directories(lint);
    std::size_t n = 0;
    for (const std::string& file : files) WriteFileBytes(lint / (std::to_string(n++) + ".ini"), file);
    std::printf("%zu distinct migrated files written to %s\n", files.size(), lint.string().c_str());
}

}  // namespace

int main() {
    // Unbuffered, so the lines before an uncaught exception reach the log.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Compare(Inputs());
    RemoveTree(ScratchRoot());
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
