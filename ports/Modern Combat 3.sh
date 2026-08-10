#!/bin/bash
# PORTMASTER: mc3-native-arm.zip, Modern Combat 3.sh
#
# Modern Combat 3: Fallen Nation (Android build 1.1.7g) — PortMaster launcher.
# Port and project by EapRules: https://github.com/EapRules
#
# The port never ships Gameloft's files: the player drops their own APK plus
# both OBB expansion files in ports/mc3/ and the first launch imports and
# validates them with eapx. An already-extracted tree is used as-is:
#
#   lib/armeabi-v7a/libModernCombat3.so
#   main.1120.com.gameloft.android.ANMP.GloftM3HM.obb
#   patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb
#
# This is a Java-driven JNI game, not NativeActivity: the .so declares no
# libandroid.so and exports no ANativeActivity_onCreate. The loader is the Java
# layer.

# shellcheck disable=SC1090,SC1091,SC2154

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"

export PORT_32BIT="Y"
[ -f "$controlfolder/tasksetter" ]          && source "$controlfolder/tasksetter"
[ -f "$controlfolder/device_info.txt" ]     && source "$controlfolder/device_info.txt"
[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR="/$directory/ports/mc3"
cd "$GAMEDIR" || exit 1

: > "$GAMEDIR/log.txt"
exec > "$GAMEDIR/log.txt" 2>&1

# Which build produced this log. A user reporting a problem is running whatever
# is on their SD card, not necessarily the build they were just handed, and two
# builds produce byte-identical logs otherwise. The string lives in the binary
# (game/port_version.h) and is asked for here, so a launcher and a loader can
# never claim different versions. The chmod is needed this early because the GL
# preflight below also runs the binary.
$ESUDO chmod +x "$GAMEDIR/mc3" 2>/dev/null
# The bundled libraries are not on LD_LIBRARY_PATH yet (that export happens
# further down); without them the binary cannot link and the answer comes back
# empty - on a real device that printed "vunknown". The path rides along just
# for this one call.
PORT_VERSION=$(LD_LIBRARY_PATH="$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$GAMEDIR/mc3" --version 2>/dev/null) || PORT_VERSION=""
echo "Modern Combat 3 port v${PORT_VERSION:-unknown} launcher starting"

# The machine, in every log, whether or not anything goes wrong.
#
# Each line below was asked for by hand in a bug report on a sibling port at
# least once. Asking costs days of round trips with a user who is on a different
# continent and a different firmware, and the answers do not change between
# runs - so they are collected unconditionally. The whole block is a dozen lines
# and prefixed "sys:" so it greps out of the log cleanly.
#
# GL_DIRS is defined here rather than beside the provider search below because
# the survey lists them; the search is what explains them.
# /usr/local/lib first: on the ArkOS builds that carry their working 32-bit
# GL set there (reported by R36S users; credit to Bheathy on Reddit for
# finding the path), the sets under /usr/lib/arm-linux-gnueabihf exist but do
# not load, so the search order is what makes the difference.
GL_DIRS="/usr/local/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf \
/usr/lib/arm-linux-gnueabihf/mali \
/lib/arm-linux-gnueabihf /usr/lib32/mali /usr/lib32"
if [ "$DEVICE_ARCH" = "armhf" ]; then
  GL_DIRS="$GL_DIRS /usr/lib /lib"
fi

echo "sys: uname: $(uname -rm 2>/dev/null)"
_sys_os=$(sed -n 's/^PRETTY_NAME="\{0,1\}\([^"]*\)"\{0,1\}$/\1/p' /etc/os-release 2>/dev/null | head -n 1)
[ -n "$_sys_os" ] || _sys_os=$(cat /etc/*-release 2>/dev/null | head -n 1)
echo "sys: os: ${_sys_os:-unknown}"
echo "sys: cfw: ${CFW_NAME:-unknown} device: ${DEVICE_NAME:-unknown} arch: ${DEVICE_ARCH:-unknown}"
# What GL the firmware actually ships, seen rather than asked about. Filtered to
# the sonames that decide whether this port can run: an unfiltered listing of a
# multiarch library directory is hundreds of names and would bury the block it
# belongs to.
for _sys_gldir in $GL_DIRS; do
  [ -d "$_sys_gldir" ] || continue
  _sys_gl=$(ls "$_sys_gldir" 2>/dev/null \
      | grep -E '^lib(EGL|GLESv1_CM|GLESv2|mali|Mali|GLdispatch|gbm\.|drm\.)' \
      | tr '\n' ' ')
  echo "sys: gl $_sys_gldir: ${_sys_gl:-(no GL libraries)}"
done
# Permissions included on purpose: a render node the user cannot open fails the
# same way a missing driver does.
_sys_dri=$(ls -la /dev/dri 2>/dev/null | sed 1d \
    | awk 'NF>=9 {print $NF" ("$1" "$3":"$4")"}' | tr '\n' ' ')
echo "sys: dri: ${_sys_dri:-none}"
_sys_mem=$(free -m 2>/dev/null | sed -n '2p' | tr -s ' ')
[ -n "$_sys_mem" ] || _sys_mem=$(grep -E '^Mem(Total|Available)' /proc/meminfo 2>/dev/null | tr -s ' \n' ' ')
echo "sys: mem: ${_sys_mem:-unknown}"
_sys_sdl=$(ls "$GAMEDIR"/libs.armhf/libSDL2*.so* 2>/dev/null | xargs -n1 basename 2>/dev/null | tr '\n' ' ')
echo "sys: sdl bundled: ${_sys_sdl:-none}"

# PortMaster's portable metadata points at mc3/cover.png, which is the
# canonical source shipped in the release. ArkOS/dArkOS additionally keeps a
# normalized EmulationStation copy beside the other port artwork. A direct
# update does not rerun PortMaster's metadata importer, so that copy can remain
# stale indefinitely. Refresh only this port's own image when its bytes
# differ; never rewrite gamelist.xml or touch another port's metadata.
ES_PORT_IMAGE="/$directory/ports/images/Modern Combat 3.png"
if [ -f "$GAMEDIR/cover.png" ] && \
   { [ ! -f "$ES_PORT_IMAGE" ] || ! cmp -s "$GAMEDIR/cover.png" "$ES_PORT_IMAGE"; }; then
  mkdir -p "$(dirname "$ES_PORT_IMAGE")"
  if cp -f "$GAMEDIR/cover.png" "$ES_PORT_IMAGE"; then
    echo "Port artwork normalized at $ES_PORT_IMAGE (visible after frontend restart)"
  else
    echo "Warning: could not refresh $ES_PORT_IMAGE; continuing without artwork update"
  fi
fi

export LD_LIBRARY_PATH="$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export LOADER_TRACE=1
# Aspect-correct scaling for panels that are not the R36S's 640x480: fit
# (default) keeps 4:3 and letterboxes, stretch fills the panel, integer scales
# by a whole number. On a real 640x480 panel this is identity.
export MC3_SCALE="${MC3_SCALE:-fit}"
# Camera (right stick) feel, in percent. INVERT_Y=1 flips vertical aim.
export MC3_RPAD_SCALE="${MC3_RPAD_SCALE:-100}"
export MC3_RPAD_INVERT_Y="${MC3_RPAD_INVERT_Y:-0}"
# The game is GLES 2 and imports no fixed-function entry point at all, but
# portbase interposes six calls (glViewport, glScissor, glClear, glTexImage2D,
# glDrawArrays, glDrawElements) that resolve only through the GLES1 table - so
# the table has to be filled, and single dispatch is what makes filling it safe:
# every name in it goes through SDL_GL_GetProcAddress instead of dlsym on a
# separate GLESv1_CM provider, landing in the same dispatch as the GLES2 context
# SDL gave us. Without it the six calls were counted and discarded, which is
# 738 texture uploads and 23484 draws that never reached the driver. The loader
# also sets this as a default; it is set here too so the working configuration
# is visible in the launcher rather than only inside the binary.
export MC3_GL_SINGLE_DISPATCH=1
# Audio routing is decided by what the device actually runs, never by CFW name.
# If a user audio server is present (PipeWire, or a PulseAudio socket), the
# 32-bit game must route through it or it grabs a PCM nobody is listening to and
# plays silence. If none is found, fall back to ALSA dmix, which is what a
# bare-ALSA CFW (the R36S on ArkOS) provides. No device or firmware is named.
_MC3_PW=""
for _pw in /usr/lib32/pipewire-0.3 /usr/lib/arm-linux-gnueabihf/pipewire-0.3; do
  [ -d "$_pw" ] && { _MC3_PW="$_pw"; break; }
done
for _xrd in "${XDG_RUNTIME_DIR:-}" /run/user/0 /var/run/user/0; do
  [ -n "$_xrd" ] && [ -d "$_xrd" ] && { export XDG_RUNTIME_DIR="$_xrd"; break; }
done
_MC3_PULSE=""
for _pulse in "${XDG_RUNTIME_DIR:-}/pulse/native" /run/pulse/native /var/run/pulse/native; do
  [ -n "$_pulse" ] && [ -S "$_pulse" ] && { _MC3_PULSE="$_pulse"; break; }
done
if [ -n "$_MC3_PW" ] || [ -n "$_MC3_PULSE" ]; then
  # A running audio server: route through it, never grab the PCM exclusively.
  unset AUDIODEV ALSA_CONFIG_PATH SDL_AUDIO_DEVICE_NAME ALSA_CARD
  export SDL_AUDIODRIVER=alsa
  export ALSOFT_DRIVERS=alsa
  export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=1
  for _spa in /usr/lib32/spa-0.2 /usr/lib/arm-linux-gnueabihf/spa-0.2; do
    [ -d "$_spa" ] && { export SPA_PLUGIN_DIR="$_spa"; break; }
  done
  [ -n "$_MC3_PW" ] && export PIPEWIRE_MODULE_DIR="$_MC3_PW"
  if [ -n "$_MC3_PULSE" ]; then
    export PULSE_SERVER="unix:$_MC3_PULSE"
  else
    unset PULSE_SERVER
  fi
  echo "Audio: routing through the device's audio server (PipeWire/Pulse), dmix bypassed"
else
  export AUDIODEV="${AUDIODEV:-plug:dmix}"
  export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
  echo "Audio: ALSA dmix (no audio server detected)"
fi

CUR_TTY=/dev/tty0
[ -w "$CUR_TTY" ] || CUR_TTY=/dev/tty1

show_screen() {
  $ESUDO chmod 666 "$CUR_TTY" 2>/dev/null
  printf "\033c" > "$CUR_TTY"
  cat > "$CUR_TTY"
  sleep "${1:-10}"
  printf "\033c" > "$CUR_TTY"
}

# The player's own files, imported by eapx on first boot or laid out by hand.
#
# What a missing file has to produce is a
# screen that says where to put it - not a black panel or a crash inside the
# loader, which is what "the game files are absent" looks like from the engine's
# side.
GAME_SO="$GAMEDIR/lib/armeabi-v7a/libModernCombat3.so"
OBB_MAIN="$GAMEDIR/main.1120.com.gameloft.android.ANMP.GloftM3HM.obb"
OBB_PATCH="$GAMEDIR/patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb"

# A release user should not have to unpack an Android package by hand. eapx
# discovers APK/folder donors by their contents, stages the complete game tree
# away from the live install, validates the native library and both OBB
# containers by size, hash and critical regions, and only then publishes lib/
# and the OBBs. Existing manually-extracted installs skip this path entirely.
if [ ! -f "$GAME_SO" ] || [ ! -f "$OBB_MAIN" ] || [ ! -f "$OBB_PATCH" ]; then
  if ! command -v python3 >/dev/null 2>&1; then
    echo "Game-data import failed: python3 is unavailable"
    show_screen 12 <<EOF

  Modern Combat 3 - Python 3 missing

  Automatic game-data import needs
  Python 3 from the CFW.

  Update PortMaster/your firmware, or
  extract the donor on a computer into:
    ports/mc3/

EOF
    pm_finish
    exit 1
  fi

  if [ ! -f "$GAMEDIR/eapx.py" ] || [ ! -f "$GAMEDIR/mc3.eapx.json" ]; then
    echo "Game-data import failed: eapx runtime or recipe is missing"
    show_screen 12 <<EOF

  Modern Combat 3 - incomplete port

  eapx.py or mc3.eapx.json
  is missing. Reinstall the release ZIP
  through PortMaster/autoinstall.

EOF
    pm_finish
    exit 1
  fi

  echo "Game data is absent; starting content-based first-boot import"
  if ! python3 "$GAMEDIR/eapx.py" install \
       --recipe "$GAMEDIR/mc3.eapx.json" \
       --game-dir "$GAMEDIR" --tty "$CUR_TTY"; then
    echo "Game-data import failed; see $GAMEDIR/eapx.log"
    show_screen 15 <<EOF

  Modern Combat 3 - game data not ready

  Put your own v1.1.7g APK plus both
  OBB expansion files in:
    ports/mc3/

  The filenames do not matter.
  See README.md and eapx.log.

EOF
    pm_finish
    exit 1
  fi
fi

if [ ! -f "$GAME_SO" ] || [ ! -f "$OBB_MAIN" ] || [ ! -f "$OBB_PATCH" ]; then
  echo "Game-data check failed: expected files are missing after import"
  [ -f "$GAME_SO" ]    || echo "  missing: $GAME_SO"
  [ -f "$OBB_MAIN" ]   || echo "  missing: $OBB_MAIN"
  [ -f "$OBB_PATCH" ]  || echo "  missing: $OBB_PATCH"
  show_screen 15 <<EOF

  Modern Combat 3 - missing game data

  Put your own copy of the game in:
    ports/mc3/

  Required build: 1.1.7g
  APK + main.1120 OBB + patch.11428 OBB

EOF
  pm_finish
  exit 1
fi

# The loader patches fixed offsets in the DRM, so a different build would crash
# deep inside startup with nothing in the log about why. This is the exact build
# every offset in this port was taken from.
EXPECTED_SHA1="be0d5e8779899e081a538b3930ec711d2df8aeb4"
GAME_SHA1=$(sha1sum "$GAME_SO" 2>/dev/null | cut -d' ' -f1)
if [ -n "$GAME_SHA1" ] && [ "$GAME_SHA1" != "$EXPECTED_SHA1" ]; then
  echo "Game-data check failed: sha1=$GAME_SHA1 expected=$EXPECTED_SHA1"
  show_screen 14 <<EOF

  Modern Combat 3 - unsupported build

  This port needs the 1.1.7g
  native library. The one found does
  not match.

  Found sha1:
    ${GAME_SHA1:0:20}
    ${GAME_SHA1:20}

EOF
  pm_finish
  exit 1
fi

# SDL must create its context through the device's own 32-bit GL stack. Build
# symlinks in /tmp because the SD card may be exFAT and cannot preserve them.
#
# Which stack that is depends on the device, not on the firmware's name, so it
# is found by capability, in tiers:
#
#   1. A unified Mali blob under one of the exact tested filenames - one .so
#      exporting EGL, GLESv1_CM and GLESv2. Known-good and therefore first.
#   2. A split Mali wrapper set - a directory holding both libEGL.so and
#      libGLESv2.so, the layout a Batocera-derived firmware installs. SDL is
#      pointed straight at those two files rather than left to find a blob.
#      It sits between the blob tiers on purpose: a Knulli device has
#      /usr/lib32/libmali.so.0 next to the wrapper set, the glob tier picks the
#      blob, the preflight can even pass on it, and SDL still dies in
#      SDL_CreateWindow.
#   3. Any other Mali blob, wherever the distribution put it.
#   4. No Mali anything, but a real 32-bit EGL/GLES set - a Mesa/glvnd userland,
#      which is what a Panfrost-only device ships.
#   5. None of those. Say so on screen instead of leaving the user with a black
#      panel.
#
# A candidate that exists is not a driver that works. On a 64-bit userland the
# 32-bit directories can hold an orphaned blob whose own dependencies were never
# installed: /usr/lib32/libmali.so.0 was picked on a muOS device, SDL answered
# "Can't load EGL/GL library on window creation", and every GL import resolved
# to nil. Existence was checked; loadability was not.
#
# So every candidate is dlopen()ed before it is committed to. The probe is the
# port's own binary (--gl-probe): it is 32-bit, it is already here, and it loads
# the library the same way SDL will. ldd would have been simpler and would have
# been wrong on exactly the devices this is for - it execs the host's
# interpreter list, so on a 64-bit rootfs it reports an armhf .so as "not a
# dynamic executable".
#
# A probe that cannot run at all is not a verdict: the candidate is accepted
# unchecked. Acceptance is logged as well as rejection, because silence on the
# happy path makes the preflight invisible in a bug report.
GL_PROBE_REASON=""
GL_REJECTED=""
GL_FIRST_REASON=""
# The symbol the candidate must resolve is a parameter because the tiers ask
# different questions of different kinds of library: does this provide EGL
# (eglGetDisplay), does it provide GLES 2 (glGetString). A library rejected for
# one symbol may be the right answer for another, so the rejection cache is
# keyed by both.
gl_provider_loadable() {
  local _out _rc _sym
  _sym="${2:-eglGetDisplay}"
  GL_PROBE_REASON=""
  case " $GL_REJECTED " in
    *" $1@$_sym "*) GL_PROBE_REASON="already rejected"; return 1 ;;
  esac
  _out=$("$GAMEDIR/mc3" --gl-probe "$1" "$_sym" 2>&1)
  _rc=$?
  if [ "$_rc" = 0 ]; then
    echo "GL: preflight ok - $1 loads and resolves $_sym"
    return 0
  fi
  if [ "$_rc" = 3 ]; then
    GL_PROBE_REASON=$(printf '%s' "$_out" | head -n 1)
    GL_REJECTED="$GL_REJECTED $1@$_sym"
    # The first rejection is the one the on-screen message quotes: it is the
    # candidate the search would have committed to before this check existed.
    [ -n "$GL_FIRST_REASON" ] || GL_FIRST_REASON="$GL_PROBE_REASON"
    echo "GL: rejecting $1 - $GL_PROBE_REASON"
    # dlerror() names one missing dependency and stops, so fixing a firmware by
    # that alone is one library per bug report. The audit reads DT_NEEDED out of
    # the candidate and tries each entry, which turns the whole gap into a list
    # this log already contains.
    "$GAMEDIR/mc3" --gl-probe-deps "$1" 2>&1 | sed 's/^/GL:   /'
    return 1
  fi
  echo "GL: preflight could not run (exit $_rc: $_out); accepting $1 unchecked"
  return 0
}

# Which of the tiers answered. It decides how the shim is built and, past that,
# whether SDL is asked for the "mali" video backend.
GL_TIER=""

MALI_BLOB=""
gl_try_blob() {
  [ -e "$1" ] || return 1
  gl_provider_loadable "$1" || return 1
  MALI_BLOB="$1"
  GL_TIER="blob"
  return 0
}

# Tier 1 - the exact tested blob filenames.
for candidate in \
  /usr/lib/arm-linux-gnueabihf/libmali-bifrost-g31-rxp0-gbm.so \
  /usr/lib/arm-linux-gnueabihf/libMali.so \
  /usr/lib/arm-linux-gnueabihf/libmali.so.1; do
  gl_try_blob "$candidate" && break
done

# Tier 2 - a split wrapper set. Both halves are probed for the symbol SDL will
# actually call through them, because half a working stack renders nothing.
#
# This game is pure GLES 2 - 100 gl* imports, all shader-pipeline, not one
# fixed-function call - so libGLESv2.so is the half that matters and glGetString
# is the right question to ask of it. A third library answering glMatrixMode is
# still looked for beside the pair, but only because portbase resolves its six
# interposed calls through the GLES1 table; with MC3_GL_SINGLE_DISPATCH set
# above those go through SDL instead, so not finding one changes nothing about
# what this game draws.
GL_WRAP_EGL=""
GL_WRAP_GLES=""
GL_WRAP_ES1=""
if [ -z "$GL_TIER" ]; then
  for _gldir in $GL_DIRS; do
    [ -d "$_gldir" ] || continue
    [ -e "$_gldir/libEGL.so" ] && [ -e "$_gldir/libGLESv2.so" ] || continue
    gl_provider_loadable "$_gldir/libEGL.so" || continue
    gl_provider_loadable "$_gldir/libGLESv2.so" glGetString || continue
    GL_WRAP_EGL="$_gldir/libEGL.so"
    GL_WRAP_GLES="$_gldir/libGLESv2.so"
    for _es1 in "$_gldir"/libGLESv1_CM.so "$_gldir"/libGLESv1_CM.so.* \
                "$_gldir"/libmali.so "$_gldir"/libmali.so.* \
                "$_gldir"/libMali.so*; do
      [ -e "$_es1" ] || continue
      gl_provider_loadable "$_es1" glMatrixMode || continue
      GL_WRAP_ES1="$_es1"
      break
    done
    GL_TIER="wrapper"
    break
  done
fi

# Tier 3 - any other Mali blob, wherever the distribution put it.
if [ -z "$GL_TIER" ]; then
  for _gldir in $GL_DIRS; do
    [ -d "$_gldir" ] || continue
    for _cand in "$_gldir"/libmali-*.so "$_gldir"/libmali.so.* \
                 "$_gldir"/libmali.so "$_gldir"/libMali.so*; do
      gl_try_blob "$_cand" && break
    done
    [ -n "$MALI_BLOB" ] && break
  done
fi

GL_SHIM="/tmp/mc3-gl"
rm -rf "$GL_SHIM"
GL_READY=""
GL_PROVIDER=""
if [ -n "$MALI_BLOB" ]; then
  if mkdir -p "$GL_SHIM" \
     && ln -sf "$MALI_BLOB" "$GL_SHIM/libEGL.so.1" \
     && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv1_CM.so.1" \
     && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv2.so.2" \
     && ln -sf "$MALI_BLOB" "$GL_SHIM/libmali.so.1"; then
    GL_READY="y"
    GL_PROVIDER="$MALI_BLOB"
    echo "GL: using Mali blob $MALI_BLOB"
  else
    echo "GL: failed to create /tmp shim, using system libraries"
  fi
elif [ "$GL_TIER" = "wrapper" ]; then
  # SDL is told the two files by path rather than being left to resolve
  # libEGL.so.1 / libGLESv2.so.2 itself: on the firmware this tier is for, the
  # sonames in the library path are the ones that do not work, and the shim
  # cannot outrank a system directory SDL dlopens by absolute name.
  #
  # The shim is still built, under the canonical sonames, because the loader and
  # the game dlopen those directly - SDL_VIDEO_* only reaches SDL.
  if mkdir -p "$GL_SHIM" \
     && ln -sf "$GL_WRAP_EGL" "$GL_SHIM/libEGL.so.1" \
     && ln -sf "$GL_WRAP_GLES" "$GL_SHIM/libGLESv2.so.2"; then
    export SDL_VIDEO_EGL_DRIVER="$GL_WRAP_EGL"
    export SDL_VIDEO_GL_DRIVER="$GL_WRAP_GLES"
    if [ -n "$GL_WRAP_ES1" ]; then
      ln -sf "$GL_WRAP_ES1" "$GL_SHIM/libGLESv1_CM.so.1"
      echo "GL: fixed function (unused by this game) from $GL_WRAP_ES1"
    fi
    # "libmali.so.1" is a name other things resolve too, not just our loader:
    # ROCKNIX's mali-hook dlopens it expecting the real blob and pulls the gbm
    # entry points from it. The shim directory is first on the library path,
    # so aliasing the ES1 wrapper under that soname shadowed the blob and
    # killed the whole stack with "undefined symbol:
    # gbm_surface_create_with_modifiers" (RG DS on ROCKNIX). Alias the
    # firmware's own blob when it has one; the ES1 wrapper only answers the
    # name where nothing else does.
    _mali_real=""
    for _gldir in $GL_DIRS; do
      [ -e "$_gldir/libmali.so.1" ] && { _mali_real="$_gldir/libmali.so.1"; break; }
    done
    if [ -n "$_mali_real" ]; then
      ln -sf "$_mali_real" "$GL_SHIM/libmali.so.1"
      echo "GL: libmali.so.1 aliased to the firmware's own blob $_mali_real"
    elif [ -n "$GL_WRAP_ES1" ]; then
      ln -sf "$GL_WRAP_ES1" "$GL_SHIM/libmali.so.1"
    fi
    if [ -z "$GL_WRAP_ES1" ]; then
      echo "GL: no fixed-function library beside the wrapper set; single dispatch covers the loader's interposed calls"
    fi
    GL_READY="y"
    GL_PROVIDER="$GL_WRAP_EGL"
    echo "GL: using the 32-bit wrapper set in ${GL_WRAP_EGL%/*} (EGL=$GL_WRAP_EGL GLES=$GL_WRAP_GLES)"
  else
    echo "GL: failed to create /tmp shim for the wrapper set, using system libraries"
  fi
else
  # No unified blob: link whatever 32-bit EGL/GLES entry points exist, each
  # under its own name. libEGL is the one SDL cannot start without, so one
  # directory must provide it and the GLES libraries are taken from that same
  # directory - a set assembled from two userlands would not be one stack.
  GL_EGL=""
  mkdir -p "$GL_SHIM" 2>/dev/null
  for _gldir in $GL_DIRS; do
    [ -e "$_gldir/libEGL.so.1" ] || continue
    gl_provider_loadable "$_gldir/libEGL.so.1" || continue
    for _soname in libEGL.so.1 libGLESv1_CM.so.1 libGLESv2.so.2; do
      [ -e "$_gldir/$_soname" ] && ln -sf "$_gldir/$_soname" "$GL_SHIM/$_soname"
    done
    [ -e "$GL_SHIM/libEGL.so.1" ] && { GL_EGL="$_gldir/libEGL.so.1"; break; }
  done
  if [ -n "$GL_EGL" ]; then
    GL_READY="y"
    GL_TIER="mesa"
    GL_PROVIDER="$GL_EGL"
    echo "GL: no Mali blob; using the device's 32-bit EGL/GLES set ($GL_EGL)"
  fi
fi

if [ -n "$GL_READY" ]; then
  export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"

  # Which SDL video backend to ask for.
  #
  # A Batocera-derived firmware carries a vendor "mali" backend that talks to
  # the blob directly; its kmsdrm/x11 defaults are where SDL_CreateWindow dies
  # on those devices. Upstream SDL has no such backend, and naming a backend SDL
  # was not built with makes SDL_Init fail outright - so this is decided by
  # asking SDL what it has, never by firmware name. On a CFW without it the list
  # simply does not contain "mali" and the default is kept.
  #
  # Only on the two Mali tiers: on the Mesa/glvnd tier there is no Mali stack
  # for a "mali" backend to drive.
  if [ "$GL_TIER" = "wrapper" ] || [ "$GL_TIER" = "blob" ]; then
    SDL_INFO=$("$GAMEDIR/mc3" --sdl-info 2>&1)
    printf '%s\n' "$SDL_INFO" | sed 's/^/GL: /'
    if printf '%s\n' "$SDL_INFO" | grep -q '^sdl: video driver: mali$'; then
      export SDL_VIDEODRIVER=mali
      echo "GL: SDL has a 'mali' video driver and the GL stack is the device's Mali one; selecting SDL_VIDEODRIVER=mali"
    else
      echo "GL: SDL has no 'mali' video driver; keeping SDL default (${SDL_VIDEODRIVER:-unset})"
    fi
  fi
else
  rm -rf "$GL_SHIM"
  echo "GL: no 32-bit GL provider found; searched: $GL_DIRS"
  # Two different firmwares end up here and the fix is not the same, so the
  # screen has to say which one this is. "No driver at all" is a missing
  # package; "a driver that will not load" is a 32-bit dependency the firmware
  # never installed next to it, and that is what a 64-bit userland hits.
  GL_FAIL_WHAT="  This firmware ships no 32-bit Mali
  blob and no 32-bit EGL/GLES set, so
  the game cannot open a window."
  if [ -n "$GL_REJECTED" ]; then
    # The panel is 40 columns at its narrowest, so the screen carries the one
    # word that identifies the problem - the library the driver wanted and did
    # not find - and log.txt carries the whole dlerror() text.
    case "$GL_FIRST_REASON" in
      *"cannot open shared object file"*)
        GL_FAIL_REASON="missing: ${GL_FIRST_REASON%%:*}" ;;
      *)
        GL_FAIL_REASON="$GL_FIRST_REASON" ;;
    esac
    GL_FAIL_WHAT="  A 32-bit GPU driver exists but
  cannot be loaded - its own 32-bit
  libraries are not installed:

    ${GL_FAIL_REASON:0:34}"
  fi
  show_screen 14 <<EOF

  Modern Combat 3 - unusable GPU driver

$GL_FAIL_WHAT

  Not starting the game. See log.txt.

EOF
  # And stop here. Starting the loader without a GL provider only replaced this
  # message with a black screen, which reads as a hang and buried the
  # explanation the user had just been shown.
  echo "Not launching the game: there is no GL provider to render with"
  pm_finish
  exit 1
fi

mkdir -p "$GAMEDIR/var"

# Controls are delivered directly through the game's JNI key/pointer exports.
# gptokeyb remains only for PortMaster's standard exit combination.
$GPTOKEYB "mc3" -c "$GAMEDIR/mc3.gptk" &

if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/mc3"
fi

$TASKSET "$GAMEDIR/mc3" "$GAMEDIR"
GAME_RC=$?

$ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null

# The case worth catching: the preflight accepted a provider and SDL still could
# not open a window. That means the failure is past dlopen, somewhere in EGL
# bring-up. Walk the provider the launcher chose - on a Mali blob that is a
# different file from SDL's own default. Done after the run so a healthy boot
# pays nothing.
if [ -n "$GL_PROVIDER" ] && grep -q "SDL_CreateWindow failed" "$GAMEDIR/log.txt"; then
  echo "GL: SDL could not open a window on an accepted provider; auditing $GL_PROVIDER"
  "$GAMEDIR/mc3" --gl-probe-init "$GL_PROVIDER" 2>&1 | sed 's/^/GL:   /'
  "$GAMEDIR/mc3" --gl-probe-deps "$GL_PROVIDER" 2>&1 | sed 's/^/GL:   /'
fi

rm -rf "$GL_SHIM"
unset LD_LIBRARY_PATH SDL_GAMECONTROLLERCONFIG
unset SDL_VIDEODRIVER SDL_VIDEO_EGL_DRIVER SDL_VIDEO_GL_DRIVER

pm_finish
exit "$GAME_RC"
