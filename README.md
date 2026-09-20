# Modern Combat 3: Fallen Nation — PortMaster port (WIP)

Native Android→Linux port of Gameloft's 2011 shooter for ARM handhelds
(R36S and family). Bring your own game: install the release ZIP through
PortMaster's autoinstall, drop your own v1.1.7g APK plus both OBB
expansion files in `ports/mc3/`, and the first launch imports and
validates them automatically. The OBBs are accepted as loose files, as
an `Android/obb` backup zip, or as that backup's extracted
package-named folder — whichever your copy came as. No game data is
ever distributed.

This README's controls section is the authoritative map — the game was
designed for a touchscreen plus the Xperia Play's controls, and this
port spells all of it with buttons.

## Controls

### Combat

| Control | Action |
|---|---|
| Left stick | Move |
| Right stick | Camera / aim |
| R1 | Fire |
| L1 | Iron sights (aim) |
| R2 (or X) | Reload |
| L2 (or B) | Grenade |
| D-pad Up | Sprint |
| D-pad Down | Crouch / take cover |
| D-pad Left/Right | Change weapon |
| Start | Pause menu |
| A | Confirm (menus) |

Reload and grenade are contextual: the engine ignores them when the
magazine is full or no grenades are carried — that is the game, not a
missing button.

### Pointer mode (hold SELECT)

Some prompts only answer to a touch — the melee/knife icon, the pause
menu buttons, occasional HUD elements. Hold SELECT to raise a pointer:

| Control (SELECT held) | Action |
|---|---|
| Right stick | Move the pointer |
| A or R1 | Tap / press at the pointer |
| One d-pad arrow | Swipe gesture in that direction |
| Two d-pad arrows together | Diagonal swipe (e.g. Up+Right) |

The campaign's tutorial gestures ("slide down", vaults, diagonal
strokes) are these SELECT+arrow swipes. Press the two arrows of a
diagonal within a heartbeat of each other — a short chord window turns
them into one diagonal stroke.

### Tuning (launcher environment variables)

| Variable | Default | Effect |
|---|---|---|
| `MC3_RPAD_SCALE` | 100 | Camera speed, percent |
| `MC3_RPAD_INVERT_Y` | 0 | 1 = invert vertical aim |
| `MC3_NO_AUDIO` | 0 | 1 = disable audio |

## ROCKNIX

Set the device's GPU driver to **libmali** (ROCKNIX's own setting; `gpudriver`
reports the current one). The port has been confirmed running that way on an
RG DS.

In **panfrost** mode it now reaches a live GL context — earlier releases could
not, because the port's bundled libraries shadowed what Mesa's driver needed
and glvnd then loaded no driver at all — but it renders black and crashes
shortly after. That is a fixed-function GLES 1.1 path on Mesa/Panfrost, which
is a different problem from this port's; the crash lands on an address that is
really the value of `GL_NEAREST`, so somewhere a call goes through what the
engine stored as an enum. Recorded here in case anyone wants to pick it up.

## Status

Playable on hardware: campaign runs, camera/pads fixed (softfp ABI),
briefings skipped cleanly, SWP atomics emulated on RK3326, eapx
first-boot import in place. Known pending work before the first
release: a crash when restarting a mission from the pause menu, and
the harness autopilot milestone.

## Pixelforge handheld adaptation

Based on the port work by [EapRules](https://github.com/EapRules/mc3-native-arm). Adaptations by Pixelforge ports (Ronax). Original licences and credits are retained.

The loader automatically detects 640x480, 720x480 (RG34XX-SP), 720x720, 1024x768, 1280x720 and other display sizes. Put `WIDTHxHEIGHT`, for example `720x480`, in `ports/mc3/resolution.txt` to override detection; put `auto` there to restore it. Higher resolutions can reduce performance. Firmware must support ARM32 applications and matching graphics libraries.

First launch uses eapx by EapRules, showing preparation stages and overall percentage in PortMaster, with console progress as a fallback. Supply the exact game version listed above. Keep the device powered on until setup completes. Native controller handling remains active; gptokeyb2 supplies the exit shortcut.

These source changes need handheld verification. Upstream hardware reports describe the original port, not validation of every new resolution.

## Build this adaptation in PowerShell

Install Docker Desktop, select its WSL2 Linux engine, and keep it running. Open PowerShell in this repository and run:

```powershell
docker build -t mc3-build -f portbase/Dockerfile.build portbase
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/src" -w /src mc3-build bash -lc "make -j2 && make libs && bash package_portmaster.sh"
```

Output: `build/mc3-native-arm.zip`. No purchased game data is required to build. A dated Debian snapshot keeps the old glibc build baseline available.

The `package/` directory exposes metadata and images for the website. Run `python tools/sync_package.py` after changing files under `ports/`. Upload the source and package metadata at your release tag, and attach the generated ZIP to the release.

## One-command Windows build

With Docker Desktop running in Linux-container mode, run from this source folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

This script runs the Docker image build, compilation and packaging steps above. Add `-NoCache` to refresh the build environment. No game files are required.
