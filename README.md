# Assetto Corsa Rally Head Tracking

![Assetto Corsa Rally running with this mod](https://raw.githubusercontent.com/itsloopyo/assetto-corsa-rally-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Assetto Corsa Rally that moves the camera with your head while your wheel or controller keeps steering, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **6DOF tracking** - yaw, pitch and roll plus positional lean, peek and duck
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android

## Requirements

- [Assetto Corsa Rally](https://store.steampowered.com/app/3917090/) on Steam, a legitimately purchased copy.
- A tracker that sends OpenTrack UDP pose data to port `4242` (`[Network] UdpPort` in `CameraUnlock.ini`): one 48-byte datagram of six little-endian 64-bit floats, `x, y, z, yaw, pitch, roll`. [OpenTrack](https://github.com/opentrack/opentrack) sends that from any of its inputs (webcam, TrackIR, Tobii, SteamVR). A phone app can send it straight to this PC if it has an OpenTrack or UDP output option; [Headcam](https://headcam.app) does, for free. See [Setting Up OpenTrack](#setting-up-opentrack).
- Windows 10 or 11, 64-bit.

## Installation

### Lopari

Download [Lopari](https://lopari.app), choose **Assetto Corsa Rally**, and click
**Play with head tracking**.

### Standalone Installer

1. Download the installer ZIP from the [Releases](https://github.com/itsloopyo/assetto-corsa-rally-headtracking/releases) page.
2. Extract it anywhere.
3. Double-click `install.cmd`. It finds the game and drops the loader and the mod next to `acr.exe`.
4. Point your tracker at UDP port `4242`. OpenTrack on the same PC sends to `127.0.0.1`; a phone app sends to your PC's local network IP instead, because `127.0.0.1` on a phone is the phone.
5. Launch the game. The mod creates `CameraUnlock.ini` and a log next to the EXE on first run.

If the installer cannot find your game, point it at the install folder yourself. Either set the environment variable:

```powershell
$env:ASSETTO_CORSA_RALLY_PATH = "D:\Games\Assetto Corsa Rally"
```

or pass the path as the first argument:

```powershell
install.cmd "D:\Games\Assetto Corsa Rally"
```

### Manual installation

Copy two files into `<game>\acr\Binaries\Win64\` (the folder containing `acr.exe`, **not** the install root):

- `vendor\ultimate-asi-loader\dinput8.dll` -> `dinput8.dll`
- `plugins\AssettoCorsaRallyHeadTracking.asi` -> `AssettoCorsaRallyHeadTracking.asi`

## Setting Up OpenTrack

The mod listens for OpenTrack pose data on UDP port `4242`, on every network
interface. One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimetres, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this, and the steps below
set it up.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

### Webcam

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam. Select it
under **Input**, pick your camera in its settings, and use the output settings
above. How well it tracks depends on your camera and your lighting, so try it
before buying anything.

### Phone

A phone app can reach the mod directly, with no OpenTrack on the PC, if it sends
the datagram described above. Point it at this PC's IP address (run `ipconfig`
to find it) on port `4242`. Not every phone tracker speaks this protocol, so
check yours for an OpenTrack or UDP output option first. [Headcam](https://headcam.app)
sends it, and I wrote it so decent tracking is free for anyone who already owns
a phone.

Sending direct works when the app filters its own signal on the device. The
mod's smoothing is sized to take the edge off a clean signal rather than to
rescue a noisy one, so a raw feed sent direct will jitter. If it does, point the
app at OpenTrack's **UDP over network** *input* on some other port, say 5252,
and let OpenTrack's filters and curves clean it up before its output forwards to
`127.0.0.1:4242`.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

### Headset or other hardware

If your device has an OpenTrack input driver, select it under **Input** and use
the same output settings. OpenTrack's own **Input** list is the authority on
what it can read; the mod only ever sees what OpenTrack sends.

### Centring

Centring belongs to your tracker. The mod subtracts no centre of its own: it
applies the pose it receives exactly as it arrives, so a stream of zeros holds
the view where the game itself puts it. Press the centre control in your tracker
(OpenTrack's **Center** bind, or the CENTER button in Headcam) and the tracker
zeroes its own output, which leaves the view centred with the mod doing nothing.

That is why there is no centre hotkey here and nothing to re-centre in game. Two
centres in series would drift apart, because each side re-centres at moments the
other cannot see, and you would end up pressing twice to centre once. If the
view sits off to one side, centre it in the tracker.

## Controls

Two equivalent binding sets by default - use whichever your keyboard has:

| Action              | Nav-cluster | Chord          |
|---------------------|-------------|----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y` |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+G` |

`Page Up` / `Ctrl+Shift+G` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

The mod never picks a centre on its own. It uses whatever your tracker sends, so centre it in your tracker app while sitting how you drive.

The tracking mode you pick is saved to `CameraUnlock.ini` as you change it, so the game starts in it next time. `End` changes the current session only: whether tracking is on when the game starts is `EnableOnStartup`.

Each action's keys are a list in `CameraUnlock.ini` (`ToggleKey`, `CycleTrackingModeKey`), written as key names, so either binding can be changed or removed there, which is worth doing if your button box or a wheel plugin already sits on one of them.

## Configuration

`CameraUnlock.ini` is read once at startup, so a restart applies your edits. A value the mod cannot read keeps its default, and `HeadTracking.log` names the line.

<!-- cameraunlock:config -->
The mod reads its settings from `acr\Binaries\Win64\CameraUnlock.ini` in the game folder, and creates the file when it starts and finds none. Edit it with any text editor.

A setting set to `default` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.

`Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.

When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that. Edit it with any text editor.

The built-in value of each setting set to `default` below:

- `UdpPort=4242`
- `EnableOnStartup=true`
- `RotationEnabled=true`
- `LocalSmoothing=0.0`
- `RemoteSmoothing=0.15`
- `PositionEnabled=true`
- `PositionLimitX=0.3`
- `PositionLimitY=0.2`
- `PositionLimitYDown=0.2`
- `PositionLimitZ=0.4`
- `PositionLimitZBack=0.1`
- `ToggleKey=End, Ctrl+Shift+Y`
- `CycleTrackingModeKey=PageUp, Ctrl+Shift+G`

With every setting at its default, the file reads:

```ini
; Assetto Corsa Rally head tracking settings.
; Comments start with ; and go on their own line. Text after a value is part of the value.
; Hotkeys are key names such as End, PageUp or Ctrl+Shift+Y. Separate several with commas; leave empty for none.
; A setting set to default takes its value from Defaults.ini, which every head tracking mod
; that keeps its settings in CameraUnlock.ini reads: %AppData%\CameraUnlock\Defaults.ini on
; Windows, $XDG_CONFIG_HOME/CameraUnlock/Defaults.ini (normally ~/.config/CameraUnlock) on
; Linux, under Wine and Proton too, and ~/Library/Application Support/CameraUnlock/Defaults.ini
; on macOS. The log names the file it read. Write a value instead of default to change that
; setting for this game only.

[CameraUnlock]
; Written by the mod. Leave this section in place.
ConfigFormat=1

[Network]
; UDP port the mod receives tracker data on (OpenTrack protocol).
UdpPort=default

[General]
; true: head tracking is on when the game starts. ToggleKey turns it on and off.
EnableOnStartup=default
; true: turning your head turns the view.
; Tracking mode at startup, with PositionEnabled. The mode hotkey changes both.
RotationEnabled=default

[Smoothing]
; Smoothing when the tracker runs on this PC. 0 is the least, 1 the most.
LocalSmoothing=default
; Smoothing when the tracker is another device on the network, such as a phone.
; 0 is the least, 1 the most.
RemoteSmoothing=default

[Position]
; true: moving your head moves the view.
; Tracking mode at startup, with RotationEnabled. The mode hotkey changes both.
PositionEnabled=default
; How far, in metres, leaning left or right can move the view.
PositionLimitX=default
; How far, in metres, raising your head can move the view.
PositionLimitY=default
; How far, in metres, lowering your head can move the view.
PositionLimitYDown=default
; How far, in metres, leaning forward can move the view.
PositionLimitZ=default
; How far, in metres, leaning back can move the view.
PositionLimitZBack=default

[Hotkeys]
; Turns head tracking on and off.
ToggleKey=default
; Changes the tracking mode: rotation and position, rotation only, position only.
CycleTrackingModeKey=default

[Camera]
; Near clip plane in centimetres, applied while head tracking is driving the view.
; The game's own 5.0 sits further from your eye than the seat back behind you, so
; looking over a shoulder clips the seat away and you see straight through it.
; Pulling the plane in renders it instead. 0 leaves the game's value alone; any
; other value is from 0.1 to 100.0.
NearClipCm=1.0
```
<!-- /cameraunlock:config -->

The default position limits are 0.3 m to either side, 0.2 m up and down, 0.4 m forward and 0.1 m back. In a rally cockpit 0.4 m forward puts your eye out over the bonnet and 0.1 m back puts it inside the seat. For limits sized to the cabin, write `PositionLimitX=0.15`, `PositionLimitY=0.12`, `PositionLimitYDown=0.12`, `PositionLimitZ=0.2` and `PositionLimitZBack=0.0` in it.

`[Camera] NearClipCm` pulls the near clip plane in from the game's 5 cm while tracking is driving the view, so the seat back and headrest beside your head render when you look over a shoulder instead of being clipped away.

## Troubleshooting

**Nothing happens in game.** Read `HeadTracking.log` next to `acr.exe`. It records every step: whether the loader engaged, whether Unreal's object table was found, whether the camera was hooked, and what the first few frames looked like. It is rewritten from scratch on every launch, so it always covers the run you just had; the run before it is kept alongside as `HeadTracking.prev.log`, which is the one to send if the game crashed and you relaunched to check.

**"the object table never appeared" or "no camera manager appeared".** The mod waits for the engine to build a world before it hooks anything, and stays dormant if that never happens. Load into a session and check the log again.

**The view moves the wrong way on one axis.** Invert that axis in your tracker. The mod applies the pose as the tracker sends it and has no inversion settings of its own.

**Looking around does not tilt with the car when it is banked or over a crest.** That is deliberate. Head turns are about the world's up axis, so the turn stays level with the horizon however far the car is leaning. Turning about the car's own up axis instead is what a head strapped into a seat physically does, but it tilts the horizon every time you look into an apex, which is worse to drive to.

**The view does not move even though the tracker is running.** Two tracker apps sending to port 4242 at once make the mod ignore the second one - the log says so explicitly. Close whichever you are not using.

**I launched this with another game still running, and that one had the tracker port.** Close the other game and carry on driving; there is no need to restart Assetto Corsa Rally. The mod retries the port every half second for as long as it is running, so tracking comes up about a second after the port frees. The log shows both halves: `Failed to bind UDP port 4242 ... retrying every 500ms`, then `Bound UDP port 4242 after Ns of waiting - tracking is live`.

**The view drifts away from centre.** Centre in your tracker app while sitting how you drive: opentrack's Center bind, the CENTER button in Headcam, SteamVR's reset. The mod keeps no centre of its own and applies the pose the tracker sends.

**My head goes into the seat, or out through the windscreen.** The positional limits in `CameraUnlock.ini` decide how far the camera may travel from where the game put it. `PositionLimitZBack=0.0` stops you reversing into the headrest, and `PositionLimitZ=0.2` stops a lean in short of the glass; see [Configuration](#configuration) for the cabin-sized set.

**Some of the cockpit vanishes when I look at it up close.** That is the near clip plane cutting away geometry nearer to your eye than it allows. `[Camera] NearClipCm` in `CameraUnlock.ini` pulls it in to 1 cm; lower it further (`0.5`) if anything still disappears.

**Head tracking does nothing in the menus.** That is deliberate. It follows your head only while the camera is on your car, so the menus, the car showcase, the loading screens and the service park are left as the game renders them.

**The view stops following my head when I pause.** Also deliberate, and handled separately: pausing leaves the camera sitting on your car, so the mod asks the engine whether the game is paused rather than relying on where the camera is pointed. The view holds where it is and resumes when you do.

## Updating

Re-run `install.cmd` to update - it overwrites the mod and leaves `CameraUnlock.ini` alone.

## Uninstalling

Run `uninstall.cmd` to remove the mod and the loader; add `/force` to remove the loader even if something else installed it. `CameraUnlock.ini` and `Defaults.ini` are kept.

## Building from source

Requires Visual Studio 2022 or newer (C++ desktop workload), CMake 3.20+, and [pixi](https://pixi.sh).

```powershell
git clone --recurse-submodules https://github.com/itsloopyo/assetto-corsa-rally-headtracking
cd assetto-corsa-rally-headtracking
pixi run build      # -> build/Release/AssettoCorsaRallyHeadTracking.asi
pixi run install    # deploy into the detected game folder
pixi run package    # -> release/AssettoCorsaRallyHeadTracking-v<version>-installer.zip
```

The build needs no game installed: it compiles against no game headers and resolves everything at runtime.

## How it works

The mod loads as an Ultimate ASI Loader plugin next to `acr.exe`. On startup it finds Unreal's FName pool and object table by scanning the module's data for their structure, confirming each by decoding it, and from there uses the engine's own reflection to locate the player camera manager and the exact offsets of its camera cache. It detours `APlayerCameraManager::UpdateCamera`, whose vtable slot it derives at runtime from the engine's own call site.

Each frame it hands the engine back the camera it computed last frame, lets the engine compute the next one untouched, then composes the head pose onto the result. The car's physics, your inputs and the game's own camera logic all run upstream of that and never observe the tracked view.

Because none of this is pinned to addresses in a particular build, a game patch does not require a mod update.

## Community & Support

- Discord: [Loop's Head Tracking Hangout](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch for the released head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your iPhone or Android phone into the head tracker

## License

MIT, copyright itsloopyo / CameraUnlock. See [LICENSE](LICENSE). Both release
ZIPs carry that file, and [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)
lists every third-party component the mod bundles or compiles in, with its own
licence.

## Credits

- **Supernova Games Studios** and **Kunos Simulazioni** for Assetto Corsa Rally.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG.
- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu.
- [OpenTrack](https://github.com/opentrack/opentrack) for the tracking protocol.

## Disclaimer

This mod is not affiliated with or endorsed by Supernova Games Studios or Kunos Simulazioni.
