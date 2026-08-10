/*
 * com/gameloft/android/ANMP/GloftM3HM/GloftM3HM - the game's activity.
 *
 * Everything Modern Combat 3 wants from the phone comes through this one
 * class: which device it is running on, where its data lives, which language
 * to speak, whether there is a gyroscope, and the loading/disclaimer overlays
 * the Java layer used to draw on top of the GL surface.
 *
 * The answers below are the Vita port's (source/java.c, MIT) wherever it has
 * one, because that port runs this exact build and its answers are known to
 * produce a game that plays. Where this port deliberately answers differently -
 * the two path methods - it is because the paths are the one thing that cannot
 * be copied: they name a real filesystem.
 */

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "io_paths.h"
#include "trace.h"
#include "mc3_classes.h"

GloftM3HM g_activity;

/*
 * One line the first time each method is called, and nothing after that.
 *
 * The engine polls several of these - the gyro queries and isDeviceOpened run
 * per frame - and a log that repeats them buries everything else. What matters
 * for bring-up is *which* methods the game reaches and in what order, and that
 * is answered by the first call.
 */
#define TRACE_ONCE(...)                       \
    do {                                      \
        static bool seen = false;             \
        if (!seen) { seen = true; trace(__VA_ARGS__); } \
    } while (0)

/* ------------------------------------------------------------------ paths */

/*
 * The two expansion files, and where this port looks for them.
 *
 * On Android these live in /sdcard/Android/obb/<package>/ and the names encode
 * the version they were built for: main.1120 is the base data, patch.11428 the
 * update that goes with 1.1.7g. The engine takes these two strings and opens
 * them directly, so they have to be real paths into the player's tree - which
 * is exactly what io_game_dir() is.
 *
 * game/io_paths.cpp also translates the /sdcard/Android/obb/... form, for the
 * places the engine builds the path itself instead of asking. Both routes have
 * to land in the same directory or one of them opens nothing.
 */
static const char kMainObb[]  = "main.1120.com.gameloft.android.ANMP.GloftM3HM.obb";
static const char kPatchObb[] = "patch.11428.com.gameloft.android.ANMP.GloftM3HM.obb";

static jobject GloftM3HM_getZipName(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", io_game_dir(), kMainObb);
    TRACE_ONCE("getZipName -> %s", path);
    return (jobject)env->NewStringUTF(path);
}

static jobject GloftM3HM_getPatchFileLocation(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", io_game_dir(), kPatchObb);
    TRACE_ONCE("getPatchFileLocation -> %s", path);
    return (jobject)env->NewStringUTF(path);
}

/* Where the game writes: the save, the checkpoint, its analytics log. The game
 * tree can be read-only on a PortMaster install, so this is never it. */
static jobject GloftM3HM_getSaveFolder(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    TRACE_ONCE("getSaveFolder -> %s", io_writable_dir());
    return (jobject)env->NewStringUTF(io_writable_dir());
}

/* Where the game reads its bulk data from - the obb directory's parent. */
static jobject GloftM3HM_getSDFolder(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    TRACE_ONCE("getSDFolder -> %s", io_game_dir());
    return (jobject)env->NewStringUTF(io_game_dir());
}

static jobject GloftM3HM_getPackage(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    TRACE_ONCE("getPackage()");
    return (jobject)env->NewStringUTF("com.gameloft.android.ANMP.GloftM3HM");
}

static void GloftM3HM_setupPaths(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* Nothing to set up: the paths this would compute are already answered by
     * the four methods above and by game/io_paths.cpp. */
    TRACE_ONCE("setupPaths()");
}

/* ------------------------------------------------------- device identity */

/*
 * isXperiaPlay - answered TRUE, and this is the single most load-bearing
 * answer in the file.
 *
 * The Xperia Play was the one Android phone of 2011 with physical buttons and
 * two analog pads, and Gameloft shipped a build that switches its whole input
 * model on this query: true selects the gamepad path (keyboardEvent + the two
 * setTouchPadDT* stick entry points) and false selects on-screen touch
 * controls. A handheld has buttons and no touchscreen, so the gamepad path is
 * the only one that can be driven here - and it is the path the Vita port takes
 * for the same reason.
 *
 * The visible consequence of getting this wrong is not a crash: the game runs
 * and draws virtual thumbsticks nothing can press.
 */
static jboolean GloftM3HM_isXperiaPlay(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    TRACE_ONCE("isXperiaPlay -> true (physical controls, gamepad input path)");
    return JNI_TRUE;
}

/* Polled every frame while the pad is open. Not traced at all - even once per
 * run it says nothing, and it is the highest-frequency call in the class. */
static jboolean GloftM3HM_isDeviceOpened(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return JNI_TRUE;
}

static jobject GloftM3HM_getDeviceName(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    /* The Vita port answers "Xperia Play" and the game has a device table it
     * consults by name; giving it a name it knows keeps it on the code path
     * isXperiaPlay above already selected. */
    TRACE_ONCE("getDeviceName -> Xperia Play");
    return (jobject)env->NewStringUTF("Xperia Play");
}

static jobject GloftM3HM_getDeviceFW(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("4.1.2");
}

static jobject GloftM3HM_getCountryCode(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("us");
}

static jobject GloftM3HM_getDeviceLanguage(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    /* The binary carries _french/_german/_italian/_spanish/_japanese/_korean/
     * _chinese/_brazilian/_russian/_English suffixes at .rodata 7c86c8 and
     * appends the match to its asset names. English is the one the port can be
     * sure the donor ships. */
    TRACE_ONCE("getDeviceLanguage -> en");
    return (jobject)env->NewStringUTF("en");
}

static jobject GloftM3HM_getUserAgent(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("Mozilla/5.0 (Linux; Android 4.1.2)");
}

/*
 * The device-model questions, all answered no.
 *
 * These are 2011 workaround gates - a Galaxy Nexus driver bug, an Asus tablet's
 * screen geometry, HTC's audio latency. Answering yes to any of them turns on a
 * quirk for hardware this is not.
 */
#define DEVICE_QUERY(name, value)                       \
    static jboolean GloftM3HM_##name(JNIEnv *env, jclass clazz) \
    { (void)env; (void)clazz; return value; }

DEVICE_QUERY(isHTCDevice,        JNI_FALSE)
DEVICE_QUERY(isSamsung_I9250,    JNI_FALSE)
DEVICE_QUERY(isSamsung_I510,     JNI_FALSE)
DEVICE_QUERY(isSamsung_P7100,    JNI_FALSE)
DEVICE_QUERY(isAsus_TF201,       JNI_FALSE)
DEVICE_QUERY(isHTC_Nexus9,       JNI_FALSE)
DEVICE_QUERY(isHoneycombDevice,  JNI_FALSE)
DEVICE_QUERY(isIceCreamSandwich, JNI_FALSE)
DEVICE_QUERY(mustSwapButtons,    JNI_FALSE)
DEVICE_QUERY(isFirstRunOfTheDay, JNI_FALSE)

/*
 * The renderer generation, and why exactly one of these is true.
 *
 * The three are a mutually exclusive switch over the quality tiers this build
 * ships: generation 0 is the oldest and cheapest, 2 the full one. The Vita port
 * picks 2. It is not a free choice - it selects which shader and texture sets
 * the engine loads, and answering yes to more than one or to none is a
 * combination the game was never built with.
 */
DEVICE_QUERY(useGeneration0, JNI_FALSE)
DEVICE_QUERY(useGeneration1, JNI_FALSE)
DEVICE_QUERY(useGeneration2, JNI_TRUE)

/* ------------------------------------------------------------ gyroscope */

/*
 * There is no gyroscope on a handheld, and saying so is what keeps the game off
 * a whole input mode. The remaining queries still have to answer, because the
 * engine reads its tuning constants from them before it checks whether the
 * sensor exists.
 */
DEVICE_QUERY(hasGyroscope,          JNI_FALSE)
DEVICE_QUERY(isGyroSensibilityHigh, JNI_FALSE)
DEVICE_QUERY(isGyroSensibilityLow,  JNI_FALSE)
DEVICE_QUERY(isGyroPendingToFix,    JNI_FALSE)
DEVICE_QUERY(ignoreGyroLowInc,      JNI_FALSE)
DEVICE_QUERY(isGyroInvertedX,       JNI_FALSE)
DEVICE_QUERY(isInvertedPitchYaw,    JNI_FALSE)
DEVICE_QUERY(isSwitchRollToYaw,     JNI_FALSE)

#undef DEVICE_QUERY

static void GloftM3HM_enableGyroscope(JNIEnv *env, jclass clazz, jboolean on, jfloat rate)
{
    (void)env; (void)clazz; (void)rate;
    TRACE_ONCE("enableGyroscope(%d) - no sensor on this device", (int)on);
}

static void GloftM3HM_enableAccelerometer(JNIEnv *env, jclass clazz,
                                          jboolean on, jfloat rate)
{
    (void)env; (void)clazz; (void)rate;
    TRACE_ONCE("enableAccelerometer(%d) - no sensor on this device", (int)on);
}

static void GloftM3HM_setCalibratingGyro(JNIEnv *env, jclass clazz, jboolean on)
{
    (void)env; (void)clazz; (void)on;
}

static void GloftM3HM_resetGyroCalibration(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
}

/* ------------------------------------------------------------- the view */

static void GloftM3HM_createView(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* The GL surface already exists - main() created it through SDL before the
     * module was even linked, because the GL import table cannot be filled in
     * without a current context. */
    TRACE_ONCE("createView() - the SDL window is already up");
}

static void GloftM3HM_setViewSettings(JNIEnv *env, jclass clazz,
                                      jint a, jint b, jint c, jint d, jint e)
{
    (void)env; (void)clazz;
    TRACE_ONCE("setViewSettings(%d, %d, %d, %d, %d)", a, b, c, d, e);
}

static jboolean GloftM3HM_setCurrentContext(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    /*
     * FALSE, following the reference. This asks the Java layer to make one of
     * the game's extra GL contexts current; this port has exactly one context
     * and SDL owns it, so the honest answer is that the switch did not happen.
     * The engine has a single-context path behind it.
     */
    TRACE_ONCE("setCurrentContext(%d) -> false (one SDL context, no sharing)", which);
    return JNI_FALSE;
}

static jint GloftM3HM_getProcessorNum(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* Three, as the reference answers. The engine sizes its worker pool from
     * this; the R36S has four cores and one is the loader's own frame loop. */
    TRACE_ONCE("getProcessorNum -> 3");
    return 3;
}

/* --------------------------------------------------------- the overlays */

static void GloftM3HM_showloading(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    TRACE_ONCE("showloading(%d)", which);
}

static void GloftM3HM_hideloading(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    TRACE_ONCE("hideloading()");
}

static void GloftM3HM_hidelogoscreen(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    TRACE_ONCE("hidelogoscreen()");
}

static void GloftM3HM_showDisclaimer(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    TRACE_ONCE("showDisclaimer(%d)", which);
}

static void GloftM3HM_showCantGoBackPopup(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
}

static void GloftM3HM_showParentButton(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
}

static void GloftM3HM_hideParentButton(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
}

static void GloftM3HM_ShowAlert(JNIEnv *env, jclass clazz,
                                jstring title, jstring body, jstring button)
{
    (void)clazz;
    /* Worth a line every time: the game only raises an alert when something it
     * cares about failed, and the text is the diagnosis. */
    trace("ShowAlert: %s / %s [%s]",
          title  ? ((String *)title)->str  : "(null)",
          body   ? ((String *)body)->str   : "(null)",
          button ? ((String *)button)->str : "(null)");
}

/* --------------------------------------------------------- the network */

/*
 * Gameloft Live is gone. Every server-shaped method answers "nothing came
 * back", which is a state the game already knows how to be in - it shipped
 * playable offline.
 */
static void GloftM3HM_sendURLRequest(JNIEnv *env, jclass clazz, jstring url)
{
    (void)clazz;
    TRACE_ONCE("sendURLRequest(%s) - no network in this port",
               url ? ((String *)url)->str : "(null)");
}

static jboolean GloftM3HM_hasServerResponse(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return JNI_FALSE;
}

static jobject GloftM3HM_getServerResponse(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("");
}

static jint GloftM3HM_IsWifiEnabled(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static jint GloftM3HM_IsWifiOr3GEnabled(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static void GloftM3HM_openBrowser(JNIEnv *env, jclass clazz, jstring url)
{
    (void)clazz;
    TRACE_ONCE("openBrowser(%s) - ignored, there is no browser here",
               url ? ((String *)url)->str : "(null)");
}

static void GloftM3HM_openGLLive(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    TRACE_ONCE("openGLLive(%d) - ignored", which);
}

static void GloftM3HM_openIGP(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    TRACE_ONCE("openIGP(%d) - ignored (in-game promotion)", which);
}

static jboolean GloftM3HM_CheckOffensive(JNIEnv *env, jclass clazz, jstring text)
{
    (void)env; (void)clazz; (void)text;
    /* The profanity filter for multiplayer names, which needs a server. No
     * multiplayer here, so nothing is rejected. */
    return JNI_FALSE;
}

/* ------------------------------------------------------------ the rest */

static jboolean GloftM3HM_deleteOldData(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* No. This asks permission to wipe a previous installation's save. */
    TRACE_ONCE("deleteOldData -> false (never touch the player's save)");
    return JNI_FALSE;
}

static void GloftM3HM_notifyTrophy(JNIEnv *env, jclass clazz, jint id)
{
    (void)env; (void)clazz;
    trace("achievement %d unlocked", id);
}

static jint GloftM3HM_GetState(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static jint GloftM3HM_getTotalItems(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static jint GloftM3HM_IsScreenLock(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static jint GloftM3HM_IsAndroidOrientationAvailable(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* The panel does not rotate. Saying the platform cannot change orientation
     * is what stops the engine from waiting on a rotation that never lands. */
    return 0;
}

static void GloftM3HM_SetAndroidOrientation(JNIEnv *env, jclass clazz, jint which)
{
    (void)env; (void)clazz;
    TRACE_ONCE("SetAndroidOrientation(%d) - fixed landscape panel", which);
}

static void GloftM3HM_moveToBackground(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    TRACE_ONCE("moveToBackground() - ignored, there is no task switcher");
}

static jobject GloftM3HM_getIMEI(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    /* A stable made-up identity. It feeds the DRM device id and the analytics
     * upload; a fixed value keeps a save made on one run readable on the next. */
    return (jobject)env->NewStringUTF("351066496380730");
}

static jobject GloftM3HM_getHDIDFVd(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("351066496380730");
}

static jobject GloftM3HM_getContext(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* The activity is the context - it is on Android too, Activity extends
     * Context. Returning the same object keeps getSystemService resolving. */
    return (jobject)&g_activity;
}

static jobject GloftM3HM_getPreferenceString(JNIEnv *env, jclass clazz,
                                             jstring key, jstring fallback)
{
    (void)clazz;
    /*
     * The preference store this port does not have. Answering with the
     * caller's own default is better than an empty string: the engine passes
     * the value it wants when the key is unset, and inventing something else
     * would override a default it chose deliberately.
     */
    TRACE_ONCE("getPreferenceString(%s) -> the caller's default",
               key ? ((String *)key)->str : "(null)");
    if (fallback)
        return (jobject)env->NewStringUTF(((String *)fallback)->str);
    return (jobject)env->NewStringUTF("");
}

/*
 * getResource - an APK resource, by name.
 *
 * On Android this reads a file out of res/ and hands back the bytes. Nothing
 * in the player's tree corresponds to it: the resources live inside the .apk,
 * which a PortMaster install does not carry, and the game has a path for the
 * absence because a resource can legitimately be missing on some skus.
 *
 * A zero-length array, never NULL: the engine passes the result straight to
 * GetArrayLength, and NULL there faults inside the fake JVM with the game's
 * return address in lr - which reads as a loader bug rather than as a resource
 * that was not found.
 */
static jbyteArray GloftM3HM_getResource(JNIEnv *env, jclass clazz, jstring name)
{
    (void)clazz;
    TRACE_ONCE("getResource(%s) -> empty (the .apk is not part of a port)",
               name ? ((String *)name)->str : "(null)");
    return env->NewByteArray(0);
}

/*
 * getData - the launch Bundle, echoed back.
 *
 * The descriptor came from a run log, which is the only place it appears:
 * (Landroid/os/Bundle;)Landroid/os/Bundle;. On Android this reads the extras
 * the launcher passed in. Nothing launches this port with extras, so the
 * argument is handed straight back - an empty Bundle whose getters all answer
 * "absent", which is what a normal launch looks like.
 *
 * Returning the caller's own object rather than a fresh one keeps the identity
 * the engine may compare, and answers with a real object if it passed one.
 */
static jobject GloftM3HM_getData(JNIEnv *env, jclass clazz, jobject bundle)
{
    (void)env; (void)clazz;
    TRACE_ONCE("getData() -> the launch bundle, empty");
    if (bundle)
        return bundle;

    static AndroidBundle empty;
    return (jobject)&empty;
}

/* ------------------------------------------------------------- registry */

/*
 * The method table, registered on TWO classes.
 *
 * The engine looks these up on com/gameloft/glf/GL2JNILib as often as on its
 * own activity class - GL2JNILib.init's first act is to resolve createView,
 * enableGyroscope, getDeviceLanguage and a dozen others against the jclass it
 * was handed. Lookup here is per class and knows nothing about inheritance, so
 * a method registered on one is invisible from the other, and the first run
 * that passed a real class object printed exactly that:
 *
 *     Class GL2JNILib does not have static method createView()V.
 *     Class GL2JNILib does not have static method enableGyroscope(ZF)V.
 *     ...
 *
 * Registering the same functions twice is the honest shape of that: on Android
 * these really are two classes that forward to the same activity, and which
 * one the engine asks is not something this port gets to choose. A macro
 * rather than a copy, because a table that drifts between the two would fail
 * only on whichever class the engine happened to use for that one method.
 */
#define MC3_ACTIVITY_METHODS(CLZ) \
    /* paths */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_getZipName>( \
        CLZ::clazz, "getZipName", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getPatchFileLocation>( \
        CLZ::clazz, "getPatchFileLocation", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getSaveFolder>( \
        CLZ::clazz, "getSaveFolder", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getSDFolder>( \
        CLZ::clazz, "getSDFolder", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getPackage>( \
        CLZ::clazz, "getPackage", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_setupPaths>( \
        CLZ::clazz, "setupPaths", "()V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getResource>( \
        CLZ::clazz, "getResource", "(Ljava/lang/String;)[B"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getPreferenceString>( \
        CLZ::clazz, "getPreferenceString", \
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"), \
\
    /* identity */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_isXperiaPlay>( \
        CLZ::clazz, "isXperiaPlay", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isDeviceOpened>( \
        CLZ::clazz, "isDeviceOpened", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isHTCDevice>( \
        CLZ::clazz, "isHTCDevice", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isSamsung_I9250>( \
        CLZ::clazz, "isSamsung_I9250", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isSamsung_I510>( \
        CLZ::clazz, "isSamsung_I510", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isSamsung_P7100>( \
        CLZ::clazz, "isSamsung_P7100", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isAsus_TF201>( \
        CLZ::clazz, "isAsus_TF201", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isHTC_Nexus9>( \
        CLZ::clazz, "isHTC_Nexus9", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isHoneycombDevice>( \
        CLZ::clazz, "isHoneycombDevice", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isIceCreamSandwich>( \
        CLZ::clazz, "isIceCreamSandwich", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_mustSwapButtons>( \
        CLZ::clazz, "mustSwapButtons", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isFirstRunOfTheDay>( \
        CLZ::clazz, "isFirstRunOfTheDay", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_useGeneration0>( \
        CLZ::clazz, "useGeneration0", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_useGeneration1>( \
        CLZ::clazz, "useGeneration1", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_useGeneration2>( \
        CLZ::clazz, "useGeneration2", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getProcessorNum>( \
        CLZ::clazz, "getProcessorNum", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getDeviceName>( \
        CLZ::clazz, "getDeviceName", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getDeviceFW>( \
        CLZ::clazz, "getDeviceFW", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getCountryCode>( \
        CLZ::clazz, "getCountryCode", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getDeviceLanguage>( \
        CLZ::clazz, "getDeviceLanguage", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getUserAgent>( \
        CLZ::clazz, "getUserAgent", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getIMEI>( \
        CLZ::clazz, "getIMEI", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getHDIDFVd>( \
        CLZ::clazz, "getHDIDFVd", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getContext>( \
        CLZ::clazz, "getContext", "()Landroid/content/Context;"), \
\
    /* gyroscope */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_hasGyroscope>( \
        CLZ::clazz, "hasGyroscope", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isGyroSensibilityHigh>( \
        CLZ::clazz, "isGyroSensibilityHigh", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isGyroSensibilityLow>( \
        CLZ::clazz, "isGyroSensibilityLow", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isGyroPendingToFix>( \
        CLZ::clazz, "isGyroPendingToFix", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_ignoreGyroLowInc>( \
        CLZ::clazz, "ignoreGyroLowInc", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isGyroInvertedX>( \
        CLZ::clazz, "isGyroInvertedX", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isInvertedPitchYaw>( \
        CLZ::clazz, "isInvertedPitchYaw", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_isSwitchRollToYaw>( \
        CLZ::clazz, "isSwitchRollToYaw", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_enableGyroscope>( \
        CLZ::clazz, "enableGyroscope", "(ZF)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_enableAccelerometer>( \
        CLZ::clazz, "enableAccelerometer", "(ZF)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_setCalibratingGyro>( \
        CLZ::clazz, "setCalibratingGyro", "(Z)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_resetGyroCalibration>( \
        CLZ::clazz, "resetGyroCalibration", "()V"), \
\
    /* the view */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_createView>( \
        CLZ::clazz, "createView", "()V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_setViewSettings>( \
        CLZ::clazz, "setViewSettings", "(IIIII)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_setCurrentContext>( \
        CLZ::clazz, "setCurrentContext", "(I)Z"), \
\
    /* overlays */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_showloading>( \
        CLZ::clazz, "showloading", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_hideloading>( \
        CLZ::clazz, "hideloading", "()V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_hidelogoscreen>( \
        CLZ::clazz, "hidelogoscreen", "()V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_showDisclaimer>( \
        CLZ::clazz, "showDisclaimer", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_showCantGoBackPopup>( \
        CLZ::clazz, "showCantGoBackPopup", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_showParentButton>( \
        CLZ::clazz, "showParentButton", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_hideParentButton>( \
        CLZ::clazz, "hideParentButton", "()V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_ShowAlert>( \
        CLZ::clazz, "ShowAlert", \
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"), \
\
    /* network */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_sendURLRequest>( \
        CLZ::clazz, "sendURLRequest", "(Ljava/lang/String;)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_hasServerResponse>( \
        CLZ::clazz, "hasServerResponse", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getServerResponse>( \
        CLZ::clazz, "getServerResponse", "()Ljava/lang/String;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_IsWifiEnabled>( \
        CLZ::clazz, "IsWifiEnabled", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_IsWifiOr3GEnabled>( \
        CLZ::clazz, "IsWifiOr3GEnabled", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_openBrowser>( \
        CLZ::clazz, "openBrowser", "(Ljava/lang/String;)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_openGLLive>( \
        CLZ::clazz, "openGLLive", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_openIGP>( \
        CLZ::clazz, "openIGP", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_CheckOffensive>( \
        CLZ::clazz, "CheckOffensive", "(Ljava/lang/String;)Z"), \
\
    /* the rest */ \
    ManagedMethod::RegisterStatic<&GloftM3HM_deleteOldData>( \
        CLZ::clazz, "deleteOldData", "()Z"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_notifyTrophy>( \
        CLZ::clazz, "notifyTrophy", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_GetState>( \
        CLZ::clazz, "GetState", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getData>( \
        CLZ::clazz, "getData", "(Landroid/os/Bundle;)Landroid/os/Bundle;"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_getTotalItems>( \
        CLZ::clazz, "getTotalItems", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_IsScreenLock>( \
        CLZ::clazz, "IsScreenLock", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_IsAndroidOrientationAvailable>( \
        CLZ::clazz, "IsAndroidOrientationAvailable", "()I"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_SetAndroidOrientation>( \
        CLZ::clazz, "SetAndroidOrientation", "(I)V"), \
    ManagedMethod::RegisterStatic<&GloftM3HM_moveToBackground>( \
        CLZ::clazz, "moveToBackground", "()V"), \

const ManagedMethod gloftM3HMMethods[] = {
    MC3_ACTIVITY_METHODS(GloftM3HM)
    {NULL},
};

/* extern, because a namespace-scope const has internal linkage in C++ and
 * gloft_helpers.cpp is where the GL2JNILib class object lives. */
extern const ManagedMethod gl2jniLibMethods[] = {
    MC3_ACTIVITY_METHODS(GloftGL2JNILib)
    {NULL},
};


Class GloftM3HM::clazz = {
    .classpath       = "com/gameloft/android/ANMP/GloftM3HM/GloftM3HM",
    .classname       = "GloftM3HM",
    .managed_methods = gloftM3HMMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftM3HM),
};

static const int registered = ClassRegistry::register_class(GloftM3HM::clazz);
