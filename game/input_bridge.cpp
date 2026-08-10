/*
 * Input for Modern Combat 3.
 *
 * The engine takes no AInputQueue and no ALooper - it exports Java_* entry
 * points and expects a Java layer to call them. Four of them carry everything:
 *
 *   GL2JNILib.keyboardEvent(key, isDown)          buttons, as Android keycodes
 *   GL2JNILib.nativeSetTouchPadDTLeft(x, y, id)   the left analog pad
 *   GL2JNILib.nativeSetTouchPadDT(x, y, id)       the right analog pad
 *   GL2JNILib.touchEvent(action, x, y, id)        the touchscreen
 *
 * The first three are the Xperia Play input path - the one the game selects
 * because GloftM3HM.isXperiaPlay answers true (see jni/gloft_m3hm.cpp). That
 * is what makes a handheld's physical controls work at all: the touch path
 * would need a screen to press.
 *
 * The button map below is NOT verified on hardware. It is the reference port's
 * Vita mapping transposed to this device's names, and the two decisions worth
 * knowing about are which face button confirms and which one goes back - those
 * are the ones a player notices immediately if they are the wrong way round.
 * Nothing here can settle that; it needs the console.
 */

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "platform.h"
#include "so_util.h"
#include "jni.h"
#include "trace.h"
#include "app_exit.h"
#include "input_bridge.h"
#include "mc3.h"
#include "port_env.h"

/* The engine's entry points, resolved once at init. */
static void (*GL2JNILib_keyboardEvent)(JNIEnv *, jclass, jint, jint);
static void (*GL2JNILib_touchEvent)(JNIEnv *, jclass, jint, jint, jint, jint);
/*
 * ABI_ATTR on these two and nowhere else, and it is the whole camera bug: the
 * game is softfp, this loader is hardfp, and these are the only game exports
 * this port calls that take FLOAT arguments. Integers travel in r0-r3 under
 * both ABIs - which is why every button always worked - but a hardfp caller
 * puts floats in s0/s1 while the softfp callee reads r2/r3, so the engine's
 * pads received whatever garbage sat in those registers: a stuck 1e29 walk on
 * the left pad, a camera pinned to its clamp on the right. The dt spy caught
 * it in memory: R=(0.000, -3.6e+29). ABI_ATTR makes the call site softfp.
 */
static void (ABI_ATTR *GL2JNILib_setTouchPadDT)(JNIEnv *, jclass, jfloat, jfloat, jint);
static void (ABI_ATTR *GL2JNILib_setTouchPadDTLeft)(JNIEnv *, jclass, jfloat, jfloat, jint);
static jint (*GloftM3HM_isGamePlay)(void);

static JNIEnv *g_env;

/* The reference port's placeholder jclass. The engine never dereferences it. */
static jclass kFakeClass = (jclass)0x42424242;

/*
 * Android keycodes, as the engine's own switch reads them.
 *
 * DPAD_CENTER and MOVE_END are not the obvious pair, and they are not a guess:
 * the reference port maps the Vita's cross and circle to exactly these two,
 * which is how this build spells "confirm" and "back" when it believes it is
 * running on an Xperia Play.
 */
enum {
    AKEYCODE_BACK         = 4,
    AKEYCODE_DPAD_UP      = 19,
    AKEYCODE_DPAD_DOWN    = 20,
    AKEYCODE_DPAD_LEFT    = 21,
    AKEYCODE_DPAD_RIGHT   = 22,
    AKEYCODE_DPAD_CENTER  = 23,
    AKEYCODE_BUTTON_X     = 99,
    AKEYCODE_BUTTON_Y     = 100,
    AKEYCODE_BUTTON_L1    = 102,
    AKEYCODE_BUTTON_R1    = 103,
    AKEYCODE_BUTTON_L2    = 104,
    AKEYCODE_BUTTON_R2    = 105,
    AKEYCODE_BUTTON_START = 108,
    AKEYCODE_BUTTON_SELECT = 109,
    AKEYCODE_MOVE_END     = 123,
};

struct ControlMap {
    const char *name;      /* the name the emulator harness uses */
    int         keycode;
    SDL_Scancode key;      /* the keyboard equivalent, for a desktop run */
    SDL_GameControllerButton button;
};

/*
 * The action set, mapped from the Vita port's tested table plus one hardware
 * observation of Enzo's: BUTTON_L2 (104) and BUTTON_R2 (105) are ALIASES of
 * aim and fire in this build - so the lower triggers are free real estate,
 * and they take the two actions the face buttons kept failing to deliver:
 * L2 throws the grenade (MOVE_END, the Vita circle) and R2 reloads (BUTTON_X,
 * the Vita square). B and X keep sending the same two keycodes, so both
 * spellings work regardless of which physical button a given clone reports.
 */
static const ControlMap kControls[] = {
    {"a",      AKEYCODE_DPAD_CENTER,   SDL_SCANCODE_RETURN, SDL_CONTROLLER_BUTTON_A},
    {"b",      AKEYCODE_MOVE_END,      SDL_SCANCODE_ESCAPE, SDL_CONTROLLER_BUTTON_B},
    {"x",      AKEYCODE_BUTTON_X,      SDL_SCANCODE_X,      SDL_CONTROLLER_BUTTON_X},
    {"y",      AKEYCODE_BUTTON_Y,      SDL_SCANCODE_Y,      SDL_CONTROLLER_BUTTON_Y},
    {"l1",     AKEYCODE_BUTTON_L1,     SDL_SCANCODE_Q,      SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    {"r1",     AKEYCODE_BUTTON_R1,     SDL_SCANCODE_E,      SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
    {"l2",     AKEYCODE_MOVE_END,      SDL_SCANCODE_1,      SDL_CONTROLLER_BUTTON_INVALID},
    {"r2",     AKEYCODE_BUTTON_X,      SDL_SCANCODE_3,      SDL_CONTROLLER_BUTTON_INVALID},
    {"start",  AKEYCODE_BUTTON_START,  SDL_SCANCODE_SPACE,  SDL_CONTROLLER_BUTTON_START},
    {"select", AKEYCODE_BUTTON_SELECT, SDL_SCANCODE_TAB,    SDL_CONTROLLER_BUTTON_BACK},
    {"up",     AKEYCODE_DPAD_UP,       SDL_SCANCODE_UP,     SDL_CONTROLLER_BUTTON_DPAD_UP},
    {"down",   AKEYCODE_DPAD_DOWN,     SDL_SCANCODE_DOWN,   SDL_CONTROLLER_BUTTON_DPAD_DOWN},
    {"left",   AKEYCODE_DPAD_LEFT,     SDL_SCANCODE_LEFT,   SDL_CONTROLLER_BUTTON_DPAD_LEFT},
    {"right",  AKEYCODE_DPAD_RIGHT,    SDL_SCANCODE_RIGHT,  SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
};

static void send_key(int keycode, bool down)
{
    if (!GL2JNILib_keyboardEvent || !g_env)
        return;
    /* Sparse and priceless: correlating "the button I pressed" with "the
     * keycode the engine got" is how a physically mislabelled clone pad
     * shows up in a log instead of in a guessing game. */
    trace("key: %d %s", keycode, down ? "down" : "up");
    GL2JNILib_keyboardEvent(g_env, kFakeClass, keycode, down ? 1 : 0);
}

/*
 * The analog pads, spelled the way the reference port spells them: values in
 * -1..1, left Y inverted, pointer ids 1 and 2 on every call, streamed
 * continuously from the pump thread while deflected. Verified on hardware -
 * once the softfp call convention above let the values arrive at all.
 */
static const int kLeftPointerId  = 1;
static const int kRightPointerId = 2;

static int g_screen_w = 640;
static int g_screen_h = 480;

static SDL_GameController *g_pad;

static void *stick_pump(void *);

static void send_stick_left(float x, float y)
{
    if (!GL2JNILib_setTouchPadDTLeft || !g_env)
        return;
    GL2JNILib_setTouchPadDTLeft(g_env, kFakeClass, x, -y, kLeftPointerId);
}

/*
 * Camera feel, adjustable without a rebuild: <PREFIX>_RPAD_SCALE (percent,
 * default 100 - the raw values the reference port sends, verified good on
 * hardware) and <PREFIX>_RPAD_INVERT_Y=1 for flight-stick vertical aim.
 */
static void send_stick_right(float x, float y)
{
    if (!GL2JNILib_setTouchPadDT || !g_env)
        return;

    static float scale  = port_getenv_long("RPAD_SCALE", 100) / 100.0f;
    static bool  invert = port_getenv_long("RPAD_INVERT_Y", 0) != 0;

    GL2JNILib_setTouchPadDT(g_env, kFakeClass, x * scale,
                            (invert ? -y : y) * scale, kRightPointerId);
}

/*
 * Dpad-to-swipe.
 *
 * The campaign asks for swipe gestures - "slide down to cover" in the first
 * mission, and vaults/slides in the same vocabulary later - and a handheld
 * has no screen to swipe. So in gameplay the dpad IS the swipe: press down,
 * a synthetic finger draws a quick downward stroke through the middle of the
 * screen (touchEvent DOWN, six MOVEs, UP - ~180 px in ~100 ms, a deliberate
 * gesture, not a tap). In menus the dpad stays a dpad: the same buttons keep
 * sending DPAD keycodes there, because that is what the Flash menus read.
 */
/*
 * The action codes are the ENGINE's, not Android's, and they are inverted
 * from what MotionEvent would suggest: the working Vita port's pollTouch
 * establishes that this .so reads 1 as finger-DOWN, 2 as move, 0 as
 * finger-UP. The first swipe build used Android's 0=down/1=up and the
 * recognizer saw garbage.
 */
enum { kTouchUp = 0, kTouchDown = 1, kTouchMove = 2 };

/*
 * The pointer, for everything only a finger can press.
 *
 * Held SELECT shows it; the right stick moves it (the camera pauses while it
 * is up - one thumb, one job); A or R1 press and release the synthetic finger
 * at its position, streaming moves while held so drags work too.
 */
static const int kCursorPointerId = 4;

static volatile bool g_cursor_mode = false;
static float g_cursor_x = 320.0f, g_cursor_y = 240.0f;
static bool  g_cursor_down = false;

static bool cursor_mode_active(void)
{
    return g_cursor_mode;
}

static void cursor_mode_set(bool on)
{
    if (g_cursor_mode && !on && g_cursor_down) {
        /* Never leave the finger stuck on the screen. */
        if (GL2JNILib_touchEvent && g_env)
            GL2JNILib_touchEvent(g_env, kFakeClass, kTouchUp,
                                 (jint)g_cursor_x, (jint)g_cursor_y,
                                 kCursorPointerId);
        g_cursor_down = false;
    }
    g_cursor_mode = on;
    if (on) {
        g_cursor_x = g_screen_w * 0.5f;
        g_cursor_y = g_screen_h * 0.5f;
    }
}

static void cursor_tap(bool down)
{
    if (!GL2JNILib_touchEvent || !g_env)
        return;
    g_cursor_down = down;
    trace("cursor: tap %s at (%d, %d)", down ? "down" : "up",
          (int)g_cursor_x, (int)g_cursor_y);
    GL2JNILib_touchEvent(g_env, kFakeClass, down ? kTouchDown : kTouchUp,
                         (jint)g_cursor_x, (jint)g_cursor_y, kCursorPointerId);
}

/* Called from the stick pump with the right stick's value while the pointer
 * is up: moves it, clamped to the screen, dragging if the finger is down. */
static void cursor_move(float x, float y)
{
    if (x == 0.0f && y == 0.0f)
        return;

    const float speed = 4.0f;   /* px per pump tick: ~480 px/s at full tilt */
    g_cursor_x += x * speed;
    g_cursor_y += y * speed;
    if (g_cursor_x < 0) g_cursor_x = 0;
    if (g_cursor_y < 0) g_cursor_y = 0;
    if (g_cursor_x > g_screen_w - 1) g_cursor_x = g_screen_w - 1;
    if (g_cursor_y > g_screen_h - 1) g_cursor_y = g_screen_h - 1;

    if (g_cursor_down && GL2JNILib_touchEvent && g_env)
        GL2JNILib_touchEvent(g_env, kFakeClass, kTouchMove,
                             (jint)g_cursor_x, (jint)g_cursor_y,
                             kCursorPointerId);
}

static const int kSwipePointerId = 3;

/*
 * Swipes are 8-way: the campaign asks for straight strokes AND diagonals.
 * A dpad cannot say "up-right" in one event, so a press opens a short chord
 * window (4 ticks, ~70 ms); when it closes, whatever dpad buttons are held
 * at that instant compose the direction vector. One arrow still means a
 * straight swipe - the window is short enough that a single press feels
 * instant - and two arrows mean the diagonal.
 */
static volatile bool g_swipe_pending = false;
static int   g_swipe_wait;
static bool  g_swipe_active = false;
static int   g_swipe_step;
static float g_swipe_x, g_swipe_y, g_swipe_dx, g_swipe_dy;

void android_input_request_swipe(int dir)
{
    /* Legacy single-direction entry (emulator commands use it): launches
     * through the same chord path with only that arrow held conceptually. */
    (void)dir;
    g_swipe_pending = true;
    g_swipe_wait    = 0;
}

static void swipe_send(int action)
{
    trace("swipe: action=%d at (%d, %d)", action, (int)g_swipe_x, (int)g_swipe_y);
    GL2JNILib_touchEvent(g_env, kFakeClass, action,
                         (jint)g_swipe_x, (jint)g_swipe_y, kSwipePointerId);
}

static void swipe_launch(int vx, int vy)
{
    if (vx == 0 && vy == 0)
        return;

    const float cx = g_screen_w * 0.5f, cy = g_screen_h * 0.5f;
    const float reach_x = g_screen_w * 0.28f, reach_y = g_screen_h * 0.38f;

    g_swipe_x  = cx - vx * reach_x * 0.5f;
    g_swipe_y  = cy - vy * reach_y * 0.5f;
    g_swipe_dx = vx * reach_x / 6.0f;
    g_swipe_dy = vy * reach_y / 6.0f;
    g_swipe_step   = 0;
    g_swipe_active = true;

    swipe_send(kTouchDown);
}

static void swipe_tick(void)
{
    if (!GL2JNILib_touchEvent || !g_env)
        return;

    if (g_swipe_active) {
        g_swipe_step++;
        g_swipe_x += g_swipe_dx;
        g_swipe_y += g_swipe_dy;

        if (g_swipe_step <= 6) {
            swipe_send(kTouchMove);
        } else {
            swipe_send(kTouchUp);
            g_swipe_active = false;
        }
        return;
    }

    if (!g_swipe_pending)
        return;

    if (++g_swipe_wait < 4)   /* the chord window */
        return;
    g_swipe_pending = false;

    int vx = 0, vy = 0;
    if (g_pad) {
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_UP))    vy -= 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  vy += 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  vx -= 1;
        if (SDL_GameControllerGetButton(g_pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) vx += 1;
    }
    swipe_launch(vx, vy);
}

/* ------------------------------------------------------------------ init */

void android_input_init(so_module *mod, JNIEnv *env, int width, int height)
{
    g_env = env;
    g_screen_w = width;
    g_screen_h = height;

    GL2JNILib_keyboardEvent = (void (*)(JNIEnv *, jclass, jint, jint))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_keyboardEvent");
    GL2JNILib_touchEvent = (void (*)(JNIEnv *, jclass, jint, jint, jint, jint))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_touchEvent");
    GL2JNILib_setTouchPadDT = (void (ABI_ATTR *)(JNIEnv *, jclass, jfloat, jfloat, jint))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_nativeSetTouchPadDT");
    GL2JNILib_setTouchPadDTLeft = (void (ABI_ATTR *)(JNIEnv *, jclass, jfloat, jfloat, jint))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_nativeSetTouchPadDTLeft");
    GloftM3HM_isGamePlay = (jint (*)(void))
        so_symbol(mod, "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_isGamePlay");

    /*
     * Say which ones were found. A null entry point here is not a crash - every
     * send_* above checks - it is input that silently does nothing, which from
     * outside is indistinguishable from a game that ignores the pad.
     */
    trace("input: keyboardEvent=%p touchEvent=%p padLeft=%p padRight=%p isGamePlay=%p",
          (void *)GL2JNILib_keyboardEvent, (void *)GL2JNILib_touchEvent,
          (void *)GL2JNILib_setTouchPadDTLeft, (void *)GL2JNILib_setTouchPadDT,
          (void *)GloftM3HM_isGamePlay);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_pad = SDL_GameControllerOpen(i);
            if (g_pad) {
                trace("input: gamepad '%s'", SDL_GameControllerName(g_pad));
                break;
            }
        }
    }

    /* The stick pump thread - see stick_pump() for why it is a thread. */
    pthread_t pump;
    if (pthread_create(&pump, NULL, stick_pump, NULL) == 0)
        pthread_detach(pump);
    else
        warning("input: could not start the stick pump thread\n");

    /*
     * The whole truth about the pad, once. A stick that "moves violently and
     * stays there" can be a phantom axis, a dpad wired as a hat riding the
     * stick axes, or a mapping matched by a name that is not this hardware -
     * and none of those can be told apart from behaviour alone. The mapping
     * string plus the raw axis dump below turn one SD trip into the answer.
     */
    if (g_pad) {
        char *mapping = SDL_GameControllerMapping(g_pad);
        trace("input: mapping: %s", mapping ? mapping : "(none)");
        SDL_free(mapping);

        SDL_Joystick *js = SDL_GameControllerGetJoystick(g_pad);
        if (js)
            trace("input: joystick axes=%d buttons=%d hats=%d",
                  SDL_JoystickNumAxes(js), SDL_JoystickNumButtons(js),
                  SDL_JoystickNumHats(js));
    }
}

/*
 * Raw axis tracing, on change only. MC3_TRACE_AXES=1 (or any diagnosis
 * session) shows what the hardware sends before any mapping opinion; the
 * threshold keeps stick slop from flooding the log.
 */
static void trace_raw_axes(void)
{
    static bool enabled = port_getenv_long("TRACE_AXES", 0) != 0;
    if (!enabled || !g_pad)
        return;

    SDL_Joystick *js = SDL_GameControllerGetJoystick(g_pad);
    if (!js)
        return;

    static Sint16 last[8];
    int n = SDL_JoystickNumAxes(js);
    if (n > 8)
        n = 8;

    for (int i = 0; i < n; i++) {
        Sint16 v = SDL_JoystickGetAxis(js, i);
        if (abs(v - last[i]) > 2000) {
            trace("input: raw axis %d: %d -> %d", i, last[i], v);
            last[i] = v;
        }
    }
}

/* ---------------------------------------------------------------- events */

static const ControlMap *control_by_button(SDL_GameControllerButton button)
{
    for (const ControlMap &c : kControls)
        if (c.button != SDL_CONTROLLER_BUTTON_INVALID && c.button == button)
            return &c;
    return NULL;
}

static const ControlMap *control_by_key(SDL_Scancode key)
{
    for (const ControlMap &c : kControls)
        if (c.key == key)
            return &c;
    return NULL;
}

bool android_input_event(const SDL_Event *event)
{
    switch (event->type) {
    case SDL_QUIT:
        return false;

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        if (event->key.repeat)
            return true;
        const ControlMap *c = control_by_key(event->key.keysym.scancode);
        if (c)
            send_key(c->keycode, event->type == SDL_KEYDOWN);
        return true;
    }

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        const SDL_GameControllerButton b =
            (SDL_GameControllerButton)event->cbutton.button;
        const bool down = (event->type == SDL_CONTROLLERBUTTONDOWN);

        /*
         * SELECT is the pointer modifier, not a key. The engine has HUD
         * elements only a touch can press - the melee prompt, the pause
         * menu - and even the Vita port shipped with "you will have to use
         * the touchscreen" for those. Held SELECT turns the right stick
         * into a pointer (portbase draws it), A or R1 into the tap, and
         * the dpad into an explicit swipe gesture. Released, everything
         * goes back to being buttons.
         */
        if (b == SDL_CONTROLLER_BUTTON_BACK) {
            cursor_mode_set(down);
            return true;
        }

        if (cursor_mode_active()) {
            if (b == SDL_CONTROLLER_BUTTON_A ||
                b == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                cursor_tap(down);
                return true;
            }
            int dir = -1;
            switch (b) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:    dir = 0; break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  dir = 1; break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  dir = 2; break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: dir = 3; break;
            default: break;
            }
            if (dir >= 0) {
                if (down)
                    android_input_request_swipe(dir);
                return true;
            }
        }

        const ControlMap *c = control_by_button(b);
        if (c)
            send_key(c->keycode, down);
        return true;
    }

    default:
        return true;
    }
}

/*
 * The sticks, sampled once per frame rather than driven by events.
 *
 * SDL emits an axis event per unit of movement; the engine wants a position.
 * Sampling is also what lets the "returned to centre" case be sent exactly
 * once - the engine keeps moving the player until it is told the stick stopped,
 * so a dropped centring is a character that walks into a wall forever.
 */
static float axis_value(SDL_GameControllerAxis axis)
{
    if (!g_pad)
        return 0.0f;

    float v = SDL_GameControllerGetAxis(g_pad, axis) / 32767.0f;

    /* A dead zone the pad's own slop cannot cross. 0.2 is the reference
     * port's, and these sticks are not better than the Vita's. */
    static const float kDeadZone = 0.2f;
    if (fabsf(v) < kDeadZone)
        return 0.0f;

    /* Rescale what is left so the usable range still reaches 1.0 - without
     * this the dead zone costs the player a fifth of their top speed. */
    float sign = v < 0 ? -1.0f : 1.0f;
    return sign * (fabsf(v) - kDeadZone) / (1.0f - kDeadZone);
}

/*
 * The stick pump, replicated from the reference port down to its THREADING.
 *
 * Vita polls the pad in a dedicated thread at ~120 Hz, decoupled from the
 * 24 fps render loop, and it is the one arrangement proven to drive this
 * engine's camera. Sending the identical stream from the frame thread - every
 * spelling of it: on-change, continuous, id -1, touch drags, all eight axis
 * mappings - moved nothing on hardware, so the thread is not a detail. The
 * loop below is pollPad() transcribed: same conditionals, same double-zero
 * stop, same 1.10*-1 left transform, same raw right values, ids 1 and 2.
 */
static void *stick_pump(void *arg)
{
    (void)arg;

    float lastLx = 0, lastLy = 0, lastRx = 0, lastRy = 0;
    float lastLastLx = 0, lastLastLy = 0, lastLastRx = 0, lastLastRy = 0;

    for (;;) {
        float lx = axis_value(SDL_CONTROLLER_AXIS_LEFTX);
        float ly = axis_value(SDL_CONTROLLER_AXIS_LEFTY);
        float rx = axis_value(SDL_CONTROLLER_AXIS_RIGHTX);
        float ry = axis_value(SDL_CONTROLLER_AXIS_RIGHTY);

        /* Pointer mode reroutes the right stick to the cursor; the camera
         * pad sees a release so it does not keep the last value latched. */
        if (cursor_mode_active()) {
            cursor_move(rx, ry);
            rx = 0.f;
            ry = 0.f;
        }

        if (lx == 0.f && ly == 0.f && lastLx == 0.f && lastLy == 0.f &&
            (lastLastLx != 0.f || lastLastLy != 0.f))
            send_stick_left(0.f, 0.f);
        if (rx == 0.f && ry == 0.f && lastRx == 0.f && lastRy == 0.f &&
            (lastLastRx != 0.f || lastLastRy != 0.f))
            send_stick_right(0.f, 0.f);

        if ((lx != 0.f || ly != 0.f) ||
            (lx == 0.f && ly == 0.f && (lastLx != 0.f || lastLy != 0.f)))
            send_stick_left(lx * 1.10f, ly * 1.10f);
        if ((rx != 0.f || ry != 0.f) ||
            (rx == 0.f && ry == 0.f && (lastRx != 0.f || lastRy != 0.f)))
            send_stick_right(rx, ry);

        lastLastLx = lastLx; lastLastLy = lastLy;
        lastLastRx = lastRx; lastLastRy = lastRy;
        lastLx = lx; lastLy = ly;
        lastRx = rx; lastRy = ry;

        usleep(8333);   /* the reference port's half-a-60fps-frame cadence */
    }
    return NULL;
}

/*
 * The DT spy: read back what the engine's input singleton actually holds.
 *
 * Disassembly established the layout: the getter at .text+0x6f15dc returns
 * the object stored at .data 0x989204 (an address in the module's own image,
 * so it is readable directly); the right pad lives at [obj+8]/[obj+12], the
 * left at [+16]/[+20], and [obj+0x45] is the byte that switches the camera
 * to the native-touchpad source. Logging them answers, on hardware, the three
 * questions no build so far could: do our writes arrive, does the engine
 * consume (clear) them, and which input source is it configured to read.
 */
static void dt_spy(void)
{
    static bool enabled = port_getenv_long("DT_SPY", 0) != 0;
    if (!enabled)
        return;

    static long tick = 0;
    tick++;

    so_module *mod = mc3_module();
    if (!mod)
        return;

    uintptr_t obj = *(uintptr_t *)(mod->text_base + 0x989204);
    if (!obj)
        return;

    const float   rx  = *(float *)(obj + 8);
    const float   ry  = *(float *)(obj + 12);
    const float   lx  = *(float *)(obj + 16);
    const float   ly  = *(float *)(obj + 20);
    const uint8_t src = *(uint8_t *)(obj + 0x45);

    /* Once a second idle; every quarter second while the right pad reports
     * anything, which is when the answer is being written. */
    const bool busy = (rx != 0.0f || ry != 0.0f);
    if ((busy && tick % 15 == 0) || tick % 60 == 0)
        trace("dtspy: R=(%.3f, %.3f) L=(%.3f, %.3f) src45=%d",
              (double)rx, (double)ry, (double)lx, (double)ly, (int)src);
}

void android_input_tick(void)
{
    trace_raw_axes();
    dt_spy();
    swipe_tick();
}

/* ---------------------------------------------------------------- cursor */

/*
 * The cursor hooks, wired to the SELECT-held pointer above.
 *
 * The game was built for buttons and mostly uses them - but not entirely:
 * the melee prompt, the pause menu and other HUD elements answer only to a
 * touch (the Vita port shipped telling players to use the touchscreen for
 * exactly these). portbase draws the pointer whenever position reports it
 * visible; set/press exist so the emulator can drive the same taps.
 */
extern "C" void android_input_cursor_position(float *x, float *y, int *visible)
{
    if (x) *x = g_cursor_x;
    if (y) *y = g_cursor_y;
    if (visible) *visible = cursor_mode_active() ? 1 : 0;
}

void android_input_cursor_set(float x, float y)
{
    g_cursor_x = x;
    g_cursor_y = y;
}

void android_input_cursor_press(bool down)
{
    cursor_tap(down);
}

/* ------------------------------------------------------- emulator control */

/*
 * How the emulator harness plays the game.
 *
 * These two are wired for real even though nothing else here is finished:
 * pressing a button from the host is what turns the emulator from something
 * that watches the game boot into something that can get past a title screen,
 * and every trip to the SD card it saves is worth more than the ten lines.
 *
 * Both answer false for a name they do not know, which is what stops
 * press_control("triangle") from succeeding silently and reading as an engine
 * that ignored the press.
 */
bool android_input_inject_control(const char *name, bool down)
{
    if (!name)
        return false;

    for (const ControlMap &c : kControls) {
        if (strcmp(name, c.name) == 0) {
            send_key(c.keycode, down);
            return true;
        }
    }
    return false;
}

bool android_input_inject_stick(const char *name, float x, float y)
{
    if (!name)
        return false;

    if (strcmp(name, "left") == 0) {
        send_stick_left(x, y);
        return true;
    }
    if (strcmp(name, "right") == 0) {
        send_stick_right(x, y);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------- autopilot */

/*
 * Not implemented yet, and reporting zero rather than pretending.
 *
 * The autopilot exists so a harness run can answer "does the game *advance*
 * when fed input", which is a different question from "does it draw". Writing
 * one before the port reaches a frame would mean tuning a sequence of presses
 * against a screen nobody has seen. The counters stay at zero, so the harness
 * milestone that reads them cannot pass by accident.
 */
void android_input_autopilot_tick(long frame)   { (void)frame; }
void android_input_autopilot_sample(long frame) { (void)frame; }
long android_input_autopilot_keys(void)   { return 0; }
long android_input_autopilot_scenes(void) { return 0; }

/* Whether the engine says it is in a mission rather than a menu. Used by the
 * harness to tell a game that started from one parked at the title. */
bool mc3_is_gameplay(void)
{
    return GloftM3HM_isGamePlay && GloftM3HM_isGamePlay() > 0;
}
