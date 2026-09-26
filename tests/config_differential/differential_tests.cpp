// The differential test for the conversion from HeadTracking.ini to the
// canonical config format.
//
//   Oracle  v1.1.0's reader and startup code, the newest published build
//           (oracle_adapter.h; the repo publishes v* releases only)
//   Import  the frozen reader in src/legacy_config/, through the same startup
//           code
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
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/config/testing/ini_mutations.h"

namespace {

namespace fs = std::filesystem;
namespace testing = cameraunlock::config::testing;
namespace legacy = acr_ht::legacy;

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
// acr.exe. Every folder lives under one root for the run, removed at the end.

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

    fs::path game() const { return root_ / "game"; }
    fs::path legacy() const { return game() / "HeadTracking.ini"; }

    void WriteLegacy(const std::string& bytes) const { WriteFileBytes(legacy(), bytes); }

    std::set<std::string> Names() const {
        std::set<std::string> names;
        for (const auto& entry : fs::directory_iterator(game())) names.insert(entry.path().filename().string());
        return names;
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
// v1.1.0's startup code in src/headtracking_mod.cpp: ApplyConfigToPipeline
// hands the sensitivities and inversions to the processors and builds the
// position settings with PositionSettings::Symmetric, which puts LimitY on both
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

// ---- Comparison ------------------------------------------------------------------

void Compare(const std::vector<Input>& inputs) {
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

        const std::vector<std::string> diff = Differences(oracle, ObservePublished(read));
        for (const std::string& d : diff) std::printf("  comparison 1, %s: %s\n", name.c_str(), d.c_str());
        Check(diff.empty(), name + ": comparison 1, the oracle and the import agree");
        ++compared;
    }
    std::printf("comparison 1: %d inputs\n", compared);
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
