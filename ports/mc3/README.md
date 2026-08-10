# Modern Combat 3: Fallen Nation — PortMaster port (WIP)

Native Android→Linux port of Gameloft's 2011 shooter for ARM handhelds
(R36S and family). Bring your own game: install the release ZIP through
PortMaster's autoinstall, drop your own v1.1.7g APK plus both OBB
expansion files in `ports/mc3/`, and the first launch imports and
validates them automatically. No game data is ever distributed.

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

## Status

Playable on hardware: campaign runs, camera/pads fixed (softfp ABI),
briefings skipped cleanly, SWP atomics emulated on RK3326, eapx
first-boot import in place. Known pending work before the first
release: a crash when restarting a mission from the pause menu, and
the harness autopilot milestone.
