#include "config.h"

#include <windows.h>

#include <string>

#include "logging.h"

namespace acr_ht {
namespace {

constexpr char kIniName[] = "HeadTracking.ini";

// The file a fresh install lands with. Values here must stay in step with the
// Config struct's member initialisers - tests/legacy_reader_tests.cpp locks
// that by generating this file and reading it back over a poisoned Config.
constexpr char kDefaultIniText[] =
    "; Assetto Corsa Rally Head Tracking - configuration\n"
    "; Edit values, restart the game to apply.\n"
    ";\n"
    "; Controls (all remappable, see [Hotkeys]):\n"
    ";           End  / Ctrl+Shift+Y   toggle tracking\n"
    ";           PgUp / Ctrl+Shift+G   cycle tracking mode (rotation and position\n"
    ";                                 / rotation only / position only)\n\n"
    "[Network]\n"
    "UdpPort=4242\n\n"
    "[General]\n"
    "EnableOnStartup=1\n\n"
    "[Hotkeys]\n"
    "; Windows virtual-key codes, read as hex - a bare 24 is 0x24, not 36. Each\n"
    "; action has a nav-cluster key and a Ctrl+Shift+<key> chord, and both fire\n"
    "; it - remap either or both.\n"
    "; Common codes: Home 0x24, End 0x23, Insert 0x2D, Delete 0x2E, PgUp 0x21,\n"
    "; PgDn 0x22, F1-F12 0x70-0x7B, A-Z 0x41-0x5A, numpad 0-9 0x60-0x69.\n"
    "ToggleKey=0x23\n"
    "CycleModeKey=0x21\n"
    "ChordToggleKey=0x59\n"
    "ChordCycleModeKey=0x47\n\n"
    "[Rotation]\n"
    "YawSensitivity=1.0\n"
    "PitchSensitivity=1.0\n"
    "RollSensitivity=1.0\n"
    "InvertYaw=0\n"
    "InvertPitch=0\n"
    "InvertRoll=0\n"
    "; Smoothing covers rotation and position alike, and which value is used is\n"
    "; picked per connection from where the tracker sends from. 0.0 none .. 1.0\n"
    "; heavy. LocalSmoothing is for a tracker running on this PC and nothing\n"
    "; floors it, so 0.0 really is zero-latency. RemoteSmoothing is for a device\n"
    "; on the network, e.g. a phone over WiFi.\n"
    "LocalSmoothing=0.0\n"
    "RemoteSmoothing=0.15\n\n"
    "[Camera]\n"
    "; Near clip plane in centimetres, applied while head tracking is driving\n"
    "; the view. The game's own 5.0 sits further from your eye than the seat\n"
    "; back behind you, so looking over a shoulder clips the seat away and you\n"
    "; see straight through it. Pulling the plane in renders it instead.\n"
    "; 0 leaves the game's value alone.\n"
    "NearClipCm=1.0\n\n"
    "[Position]\n"
    "Enabled=1\n"
    "SensitivityX=1.0\n"
    "SensitivityY=1.0\n"
    "SensitivityZ=1.0\n"
    "InvertX=0\n"
    "InvertY=0\n"
    "InvertZ=0\n"
    "; How far the camera may travel from where the game put it, in metres.\n"
    "; These are cabin-sized: the headrest is against the back of your head and\n"
    "; the windscreen is an arm's length away, so a head allowed to roam a\n"
    "; room's worth of space ends up inside the seat or out over the bonnet.\n"
    "LimitX=0.15\n"
    "LimitY=0.12\n"
    "LimitZ=0.20\n"
    "; Backward travel. Zero by default: strapped into a rally seat your head\n"
    "; is already touching the headrest, so there is nowhere to go and any\n"
    "; travel here is spent moving your eye into the seat. Raise it only if you\n"
    "; sit forward of the headrest.\n"
    "LimitZBack=0.0\n";

std::string IniPath(const std::string& exe_dir) {
    return exe_dir + "\\" + kIniName;
}

}  // namespace

void WriteDefaultConfigIfMissing(const std::string& exe_dir) {
    const std::string path = IniPath(exe_dir);

    // CREATE_NEW rather than "does it exist?" followed by a truncating open: the
    // two steps can straddle a file the user (or a second launch) writes in
    // between, and never overwriting a user's config is the whole promise here.
    const HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_EXISTS) return;
        Log::Line("[config] could not create %s (%lu) - the game directory is not writable. "
                  "Built-in defaults are in use and edits there will not be read.",
                  path.c_str(), error);
        return;
    }

    // A short write leaves a file that parses as a config but is missing keys,
    // which then reads as "the mod ignores my setting". Say so instead.
    constexpr DWORD kTextBytes = static_cast<DWORD>(sizeof(kDefaultIniText) - 1);
    DWORD written = 0;
    const BOOL ok = WriteFile(file, kDefaultIniText, kTextBytes, &written, nullptr);
    const DWORD writeError = GetLastError();
    CloseHandle(file);
    if (!ok || written != kTextBytes) {
        Log::Line("[config] %s was created but only %lu of %lu bytes could be written (%lu); "
                  "delete it and restart the game for a complete default config.",
                  path.c_str(), written, kTextBytes, writeError);
    }
}

}  // namespace acr_ht
