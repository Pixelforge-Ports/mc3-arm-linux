#!/usr/bin/env bash
#
# Autonomous verification harness for the Modern Combat 3 port.
#
# It builds the armhf loader, runs it under qemu-arm with software GLES, and
# decides objectively which milestone the port currently reaches. No console,
# no SD card, no eyeballs.
#
# Exit code = highest milestone passed. The loop reads it to know whether the
# last iteration moved forward, stalled, or regressed.
#
# ---------------------------------------------------------------------------
# What makes this MC3's harness and not a copy of a sibling's
#
# Every assertion below names something that is true of THIS game, and each one
# was wrong for at least one of the other four ports:
#
#   - The boot path is JNI_OnLoad plus Java_* entry points, not
#     ANativeActivity_onCreate. This .so declares no libandroid.so and exports
#     no onCreate; asserting on it would pin the port at M2 forever.
#   - The GL vocabulary is GLES2. The game imports 100 gl* entry points and all
#     of them are shader-pipeline calls - glCreateShader, glUniformMatrix4fv,
#     glVertexAttribPointer. A harness checking for a GLES 1.1 context (which
#     Mass Effect Infiltrator needs) would fail a working port here.
#   - The frame call is GL2JNILib.step, driven by us, and the engine's own
#     notion of progress is GloftM3HM.isGamePlay.
#
# A harness copied without that adjustment does not fail loudly. It fails
# quietly, and the loop burns iterations satisfying a check about another game.
# ---------------------------------------------------------------------------
#
# Usage:  harness/verify.sh [--timeout SECS]
set -uo pipefail

PORT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="mc3-build"

# The game is the player's own copy and never ships with the port: the
# extracted tree, with lib/armeabi-v7a/ and the two .obb expansion files.
GAMEDIR="${MC3_GAMEDIR:-$HOME/Archive/handheld/donors/mc3/game}"

# 300s, not 120. Once the six interposed GL calls actually reach the driver
# (see the GLES1-table note in game/main.cpp) every frame is rasterised by
# llvmpipe in software under qemu, and the run settles well under one frame per
# second. At 120s the game rendered 52 frames and M5 failed for wanting 60 - a
# verdict about the emulator's stopwatch, not about the port. The frame budget
# is what the milestone is about, so the clock gives way to it.
TIMEOUT="${TIMEOUT:-300}"
RESULTS="$PORT_DIR/harness/results"
mkdir -p "$RESULTS"

log()  { echo "[verify] $*"; }
fail() { echo "[verify] FAIL: $*"; }

# Milestones, in order. Each one is a hard, observable fact - not an opinion.
#   0 control   qemu-arm in this image runs an armhf binary at all
#   1 build     the loader links into an armhf binary
#   2 load      libModernCombat3.so maps with every import resolved
#   3 jni       JNI_OnLoad, GL2JNILib.init and GloftM3HM.Init all return
#   4 gl        a live GLES 2.0 context, and the engine took the resize
#   5 frames    at least MIN_FRAMES GL2JNILib.step calls, presented
#   6 content   the engine loaded its own data and is drawing it
#   7 autopilot the game advances when fed input, it is not parked in a menu
MIN_FRAMES="${MIN_FRAMES:-60}"

REACHED=0

# ------------------------------------------------------- 0. positive control
#
# Before anything else: prove that this image can run a 32-bit ARM binary.
#
# Without it, every failure below has two explanations - the port is broken, or
# qemu/the cross libraries in the image are - and they are indistinguishable
# from a log. The control is a five-line program built by the same compiler and
# run by the same qemu invocation as the loader, so it fails for exactly the
# reasons that are not the port's fault.
log "M0 positive control (qemu-arm runs an armhf binary)"
if docker run --rm "$IMAGE" sh -c '
      printf "#include <cstdio>\nint main(){printf(\"control ok\\\\n\");}" > /tmp/c.cpp &&
      arm-linux-gnueabihf-g++ -O0 -o /tmp/c /tmp/c.cpp &&
      qemu-arm -L /usr/arm-linux-gnueabihf /tmp/c' 2>&1 | grep -q "control ok"; then
    REACHED=0
    log "M0 ok"
else
    fail "M0: qemu-arm cannot run an armhf binary in $IMAGE - nothing below this"
    fail "    line would be attributable to the port. Rebuild the image."
    echo "0" > "$RESULTS/milestone"
    exit 0
fi

# ---------------------------------------------------------------- 1. build
log "M1 build"
if docker run --rm -v "$PORT_DIR":/src -w /src "$IMAGE" \
     make -j"$(nproc 2>/dev/null || echo 4)" \
     > "$RESULTS/01-build.log" 2>&1; then
    if docker run --rm -v "$PORT_DIR":/src -w /src "$IMAGE" \
         file build/mc3 2>/dev/null | grep -q "ELF 32-bit.*ARM"; then
        REACHED=1
        log "M1 ok"
    else
        fail "build produced no armhf binary"
    fi
else
    fail "compile error, see 01-build.log"
    tail -20 "$RESULTS/01-build.log"
    echo "$REACHED" > "$RESULTS/milestone"
    exit $REACHED
fi

if [ ! -d "$GAMEDIR" ]; then
    fail "game directory not found at $GAMEDIR (set MC3_GAMEDIR)"
    echo "$REACHED" > "$RESULTS/milestone"
    exit $REACHED
fi

# The sha1 of the build every offset in this port was taken from. A different
# build moves the three GDRM lock words, and the resulting crash would say
# nothing about why.
SO_FILE="$GAMEDIR/lib/armeabi-v7a/libModernCombat3.so"
EXPECT_SHA1="be0d5e8779899e081a538b3930ec711d2df8aeb4"
if [ -f "$SO_FILE" ]; then
    GOT_SHA1=$(shasum -a 1 "$SO_FILE" 2>/dev/null | cut -d' ' -f1 | tr 'A-Z' 'a-z')
    if [ "$GOT_SHA1" != "$EXPECT_SHA1" ]; then
        fail "wrong game build: sha1 $GOT_SHA1, expected $EXPECT_SHA1 (MC3 1.1.7g)"
        echo "$REACHED" > "$RESULTS/milestone"
        exit $REACHED
    fi
else
    fail "no $SO_FILE - this does not look like an extracted Modern Combat 3 tree"
    echo "$REACHED" > "$RESULTS/milestone"
    exit $REACHED
fi

# ------------------------------------------------- 2-7. run under emulation
log "M2-M7 running under qemu-arm (timeout ${TIMEOUT}s)"

docker run --rm \
    -v "$PORT_DIR":/src \
    -v "$GAMEDIR":/game:ro \
    -w /src \
    -e SDL_VIDEODRIVER=offscreen \
    -e LIBGL_ALWAYS_SOFTWARE=1 \
    -e GALLIUM_DRIVER=llvmpipe \
    -e EGL_PLATFORM=surfaceless \
    -e LOADER_TRACE=1 \
    -e MC3_FRAME_LIMIT="$((MIN_FRAMES * 10))" \
    "$IMAGE" \
    timeout --signal=INT "$TIMEOUT" \
    qemu-arm -L /usr/arm-linux-gnueabihf \
        ./build/mc3 /game \
    > "$RESULTS/02-run.log" 2>&1
RUN_RC=$?

RUN_LOG="$RESULTS/02-run.log"

# --- M2: every import resolved. This game declares 363 of them, 100 of which
#         are gl*. An unresolved symbol means a missing thunk, which is the
#         most common failure mode for this kind of port.
if grep -q "TRACE: module loaded" "$RUN_LOG"; then
    UNRESOLVED=$(grep -c "unresolved symbol" "$RUN_LOG" || true)
    if [ "$UNRESOLVED" -eq 0 ]; then
        REACHED=2; log "M2 ok (0 unresolved symbols)"
    else
        fail "M2: $UNRESOLVED unresolved symbols"
        grep "unresolved symbol" "$RUN_LOG" | sort -u | head -20
    fi
else
    fail "M2: module never loaded"
    grep -iE "fatal|cannot|no such" "$RUN_LOG" | head -10
fi

# --- M3: the JNI handshake. Each of these has to *return*, not merely be
#         called: JNI_OnLoad and the two inits walk the fake class registry,
#         and a missing class shows up as a fault inside it, never as a
#         diagnostic naming the class.
if [ $REACHED -ge 2 ]; then
    OK=1
    for step in "JNI_OnLoad returned" \
                "GL2JNILib.init returned" \
                "GloftM3HM.Init returned"; do
        grep -q "TRACE: $step" "$RUN_LOG" || { fail "M3: '$step' never printed"; OK=0; }
    done
    if [ $OK -eq 1 ]; then
        REACHED=3; log "M3 ok"
    else
        grep -iE "FindClass|does not have|no class|no method|no field" "$RUN_LOG" \
            | sort -u | head -10
    fi
fi

# --- M4: a live GLES 2.0 context the engine accepted.
#
# Not GLES 1.1. This game imports zero fixed-function entry points; its
# renderer is shaders throughout, and a context without them resolves
# glCreateShader to null and the first step() jumps to address 0.
if [ $REACHED -ge 3 ]; then
    if grep -q "TRACE: GL_VERSION=" "$RUN_LOG" \
       && grep -q "TRACE: GL2JNILib.resize" "$RUN_LOG"; then
        REACHED=4; log "M4 ok ($(grep -oE 'TRACE: GL_VERSION=.*' "$RUN_LOG" | tail -1))"
    else
        fail "M4: no GLES 2.0 context, or the engine never took the resize"
        grep -iE "GL_VERSION|GL_RENDERER|egl|SDL_CreateWindow|context" "$RUN_LOG" | head -10
    fi
fi

# --- M5: it is presenting. The loader owns this loop, so a frame count stuck
#         at zero means our driver never called in, not that the engine
#         stalled - worth telling apart in the log.
FRAMES=$(grep -oE "TRACE: frames=[0-9]+" "$RUN_LOG" | tail -1 | cut -d= -f2)
FRAMES="${FRAMES:-0}"
if [ $REACHED -ge 4 ] && [ "$FRAMES" -ge "$MIN_FRAMES" ]; then
    REACHED=5; log "M5 ok ($FRAMES frames)"
elif [ $REACHED -ge 4 ]; then
    fail "M5: only $FRAMES frames (need $MIN_FRAMES)"
    grep -iE "segfault|abort|fatal|signal|SIGSEGV" "$RUN_LOG" | head -10
fi

# --- M6: the engine is running its own content, not just holding a surface.
#
# A solid-colour clear is indistinguishable from a rendered game by a pixel
# test alone, which is how a sibling port once passed while opening zero
# assets. Assets opened and textures uploaded are what a game that found its
# data looks like; for this game the data is the two .obb files.
# Not an asset count. portbase's counter increments on paths containing
# "/assets/published/", which is a sibling port's layout: this game reads its
# data out of two expansion containers and opens no per-asset file at all, so
# the counter is structurally zero here no matter how well the port works. It
# read `assets=0` on a run that uploaded 738 textures and drew 23484 times.
#
# What is true of THIS game is that both .obb files open. The main one is not a
# zip - it is a Gameloft container the engine reads itself, addressed by hash -
# so there is nothing to unpack and nothing to count inside it; that the engine
# got the file handle is the whole of what the port is responsible for.
MIN_TEXTURES="${MIN_TEXTURES:-10}"
MIN_DRAWS="${MIN_DRAWS:-100}"

if [ $REACHED -ge 5 ]; then
    NONBLACK=$(grep -c "TRACE: framebuffer non-black" "$RUN_LOG" || true)
    SUMMARY=$(grep -oE "TRACE: summary assets=[0-9]+ textures=[0-9]+ draws=[0-9]+" "$RUN_LOG" | tail -1)

    if [ -z "$SUMMARY" ]; then
        fail "M6: loader emitted no summary line"
    else
        A=$(echo "$SUMMARY" | grep -oE "assets=[0-9]+"   | cut -d= -f2)
        T=$(echo "$SUMMARY" | grep -oE "textures=[0-9]+" | cut -d= -f2)
        D=$(echo "$SUMMARY" | grep -oE "draws=[0-9]+"    | cut -d= -f2)
        # Both expansion containers, opened. The path each one travelled is
        # worth asserting on rather than the name alone: the engine builds the
        # name it opens as sprintf(".%s", path), so a working port shows the
        # dotted spelling resolving to a real file, and a broken one shows it
        # resolving to <game>/./<game>/... - which is how this looked before
        # game/io_paths.cpp learned to undot.
        OBB_MAIN=$(grep -c "main\.1120\..*\.obb' (ok)"  "$RUN_LOG" || true)
        OBB_PATCH=$(grep -c "patch\.11428\..*\.obb' (ok)" "$RUN_LOG" || true)

        log "M6 counters: obb main=$OBB_MAIN patch=$OBB_PATCH textures=$T draws=$D nonblack=$NONBLACK (portbase assets counter=$A, inert for this game)"

        # 124 == the timeout killed it, which is success for a game loop.
        ALIVE=0
        { [ $RUN_RC -eq 124 ] || [ $RUN_RC -eq 0 ]; } && ALIVE=1

        if [ $ALIVE -eq 1 ] && [ "$NONBLACK" -gt 0 ] \
           && [ "$OBB_MAIN" -gt 0 ] && [ "$OBB_PATCH" -gt 0 ] \
           && [ "$T" -ge "$MIN_TEXTURES" ] \
           && [ "$D" -ge "$MIN_DRAWS" ]; then
            REACHED=6
            log "M6 ok (both .obb opened, textures=$T draws=$D, survived)"
        else
            [ $ALIVE -eq 0 ]             && { fail "M6: exited early rc=$RUN_RC"; grep -iE "segfault|abort|fatal|signal" "$RUN_LOG" | head -10; }
            [ "$NONBLACK" -eq 0 ]        && fail "M6: renders only black frames"
            [ "$OBB_MAIN" -eq 0 ]        && fail "M6: main.1120....obb never opened - check port_fix_path() and whether it is in the donor tree next to lib/"
            [ "$OBB_PATCH" -eq 0 ]       && fail "M6: patch.11428....obb never opened - same two places"
            [ "$T" -lt "$MIN_TEXTURES" ] && fail "M6: only $T textures uploaded (need $MIN_TEXTURES)"
            [ "$D" -lt "$MIN_DRAWS" ]    && fail "M6: only $D draw calls (need $MIN_DRAWS)"
            grep -iE "missing|no class|not found|ENOENT" "$RUN_LOG" | sort -u | head -8
        fi
    fi
fi

# --- M7: it *advances*, not just draws.
#
# A game parked in its main menu waiting for someone to press A renders at full
# speed, opens its assets and passes M6 whole. This game answers the question
# itself - GloftM3HM.isGamePlay is an export - and the loader prints it, so M7
# has a second witness besides the framebuffer hash.
#
# The autopilot is not written yet and reports zero rather than pretending, so
# this milestone cannot pass by accident.
MIN_SCENES="${MIN_SCENES:-2}"

if [ $REACHED -ge 6 ]; then
    AUTO=$(grep -oE "TRACE: autopilot keys=[0-9]+ scenes=[0-9]+" "$RUN_LOG" | tail -1)
    if [ -z "$AUTO" ]; then
        fail "M7: loader emitted no autopilot line"
    else
        K=$(echo "$AUTO" | grep -oE "keys=[0-9]+"   | cut -d= -f2)
        S=$(echo "$AUTO" | grep -oE "scenes=[0-9]+" | cut -d= -f2)
        GAMEPLAY=$(grep -oE "gameplay=[01]" "$RUN_LOG" | tail -1)
        log "M7 autopilot: $K keys over $FRAMES frames, scene changed $S time(s), ${GAMEPLAY:-gameplay=?}"
        if [ "$S" -ge "$MIN_SCENES" ]; then
            REACHED=7; log "M7 ok"
        else
            fail "M7: the scene changed $S time(s) (need $MIN_SCENES) - the game draws but does not advance"
            fail "     check which keycode the engine accepts and whether it reads keys at all"
        fi
    fi
fi

echo "$REACHED" > "$RESULTS/milestone"
log "=== milestone reached: $REACHED / 7 ==="
exit $REACHED
