/*
 * The Gameloft helper classes the game names in pieces.
 *
 * SUtils, Device, GameInstaller and GDRMPolicy all live under the game's own
 * package, and the binary never spells any of them out: it holds the package
 * prefix, the tail, and a "%s/%s" and assembles the classpath at the point of
 * use. The four full names below are that assembly done by hand.
 *
 *   GLUtils/SUtils           preferences and the two data folders
 *   GLUtils/Device           device capabilities; its native half is exported
 *                            by the .so as Java_..._GLUtils_Device_nativeInit
 *   installer/GameInstaller  the DRM's entry point and device-id source
 *   installer/GDRMPolicy     where the licence answer is cached
 *
 * All four are on the boot path - GameInstaller.nativeStart runs before the
 * title screen - so a missing one is not a late surprise, it is the first
 * thing that goes wrong.
 */

#include <stdio.h>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "io_paths.h"
#include "trace.h"
#include "mc3_classes.h"

static const char kPackage[] = "com/gameloft/android/ANMP/GloftM3HM";

/* ------------------------------------------------------------- SUtils */

/*
 * The same three answers GloftM3HM gives, because the game asks either class
 * depending on which subsystem is asking. They are duplicated rather than
 * shared: the two classes are looked up independently and a method registered
 * on one is invisible from the other.
 */
static jobject SUtils_getSaveFolder(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF(io_writable_dir());
}

static jobject SUtils_getSDFolder(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF(io_game_dir());
}

static jobject SUtils_getPreferenceString(JNIEnv *env, jclass clazz,
                                          jstring key, jstring fallback)
{
    (void)clazz;
    (void)key;
    if (fallback)
        return (jobject)env->NewStringUTF(((String *)fallback)->str);
    return (jobject)env->NewStringUTF("");
}

static jobject SUtils_getPackage(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("com.gameloft.android.ANMP.GloftM3HM");
}

static jobject SUtils_getContext(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return (jobject)&g_activity;
}

/*
 * The binary also names two string fields on this class, mPreferencesName
 * (7c9290) and SDFolder (7c92a4). They are left unregistered because the
 * getters above answer the same two questions, and every use of those names in
 * the donor sits next to the getter's name rather than on its own - which
 * reads as the Java class storing what the getter returns, not as something
 * the native side reaches for. If it does reach for one, GetStaticFieldID
 * prints its name and adding it is two lines.
 */
const ManagedMethod gloftSUtilsMethods[] = {
    ManagedMethod::RegisterStatic<&SUtils_getSaveFolder>(
        GloftSUtils::clazz, "getSaveFolder", "()Ljava/lang/String;"),
    ManagedMethod::RegisterStatic<&SUtils_getSDFolder>(
        GloftSUtils::clazz, "getSDFolder", "()Ljava/lang/String;"),
    ManagedMethod::RegisterStatic<&SUtils_getPreferenceString>(
        GloftSUtils::clazz, "getPreferenceString",
        "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"),
    /* Both named by the first run's log: JNI_OnLoad looks them up on SUtils,
     * not on the activity class, and a lookup that misses returns NULL into a
     * method id the engine calls without checking. */
    ManagedMethod::RegisterStatic<&SUtils_getPackage>(
        GloftSUtils::clazz, "getPackage", "()Ljava/lang/String;"),
    ManagedMethod::RegisterStatic<&SUtils_getContext>(
        GloftSUtils::clazz, "getContext", "()Landroid/content/Context;"),
    {NULL},
};

static char g_sutils_path[128];

Class GloftSUtils::clazz = {
    .classpath       = g_sutils_path,
    .classname       = "SUtils",
    .managed_methods = gloftSUtilsMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftSUtils),
};

/* ------------------------------------------------------------- Device */

static jobject Device_getUserAgent(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("Mozilla/5.0 (Linux; Android 4.1.2)");
}

const ManagedMethod gloftDeviceMethods[] = {
    ManagedMethod::RegisterStatic<&Device_getUserAgent>(
        GloftDevice::clazz, "getUserAgent", "()Ljava/lang/String;"),
    {NULL},
};


static char g_device_path[128];

Class GloftDevice::clazz = {
    .classpath       = g_device_path,
    .classname       = "Device",
    .managed_methods = gloftDeviceMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftDevice),
};

/* ------------------------------------------------------ GameInstaller */

GloftGameInstaller GloftGameInstaller::m_sInstance;

const FieldId gloftGameInstallerFields[] = {
    REGISTER_STATIC_FIELD(GloftGameInstaller, m_sInstance),
    REGISTER_FIELD(GloftGameInstaller, mDeviceInfo),
    {NULL},
};

static char g_game_installer_path[128];

Class GloftGameInstaller::clazz = {
    .classpath       = g_game_installer_path,
    .classname       = "GameInstaller",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = gloftGameInstallerFields,
    .instance_size   = sizeof(GloftGameInstaller),
};

/* --------------------------------------------------------- GL2JNILib */

/*
 * com/gameloft/glf/GL2JNILib - the class the frame loop's entry points belong
 * to.
 *
 * Two things hang off it. Its frame-loop entry points (init, step, resize) are
 * native and live in the .so, so main.cpp calls them directly - the class
 * object exists for them because this build dereferences the jclass it is
 * handed. And the engine looks the activity's own methods up here as well,
 * which is why the table comes from gloft_m3hm.cpp; the comment on
 * MC3_ACTIVITY_METHODS there has the evidence.
 */
extern const ManagedMethod gl2jniLibMethods[];   /* jni/gloft_m3hm.cpp */

Class GloftGL2JNILib::clazz = {
    .classpath       = "com/gameloft/glf/GL2JNILib",
    .classname       = "GL2JNILib",
    .managed_methods = gl2jniLibMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftGL2JNILib),
};

/* --------------------------------------------------------- GDRMPolicy */

/*
 * Where the licence answer would be written down.
 *
 * The policy stores its verdict, a validity timestamp and a retry counter
 * through these two methods so a later launch can skip the server round trip.
 * There is no server and patch.cpp already answers the check unconditionally,
 * so writing any of it down would only create state that could later disagree
 * with the unlock.
 */
static void GDRMPolicy_UpdatePreferences(JNIEnv *env, jclass clazz,
                                         jstring key, jstring value, jint mode)
{
    (void)env; (void)clazz; (void)value; (void)mode;
    static bool announced = false;
    if (!announced) {
        announced = true;
        trace("GDRMPolicy.UpdatePreferences('%s', ...) and later calls dropped - "
              "the licence is granted in patch.cpp, not cached",
              key ? ((String *)key)->str : "");
    }
}

static void GDRMPolicy_UpdatePreferences2(JNIEnv *env, jclass clazz,
                                          jstring key, jlong value, jint mode)
{
    (void)env; (void)clazz; (void)key; (void)value; (void)mode;
}

const ManagedMethod gloftGDRMPolicyMethods[] = {
    ManagedMethod::RegisterStatic<&GDRMPolicy_UpdatePreferences>(
        GloftGDRMPolicy::clazz, "UpdatePreferences",
        "(Ljava/lang/String;Ljava/lang/String;I)V"),
    ManagedMethod::RegisterStatic<&GDRMPolicy_UpdatePreferences2>(
        GloftGDRMPolicy::clazz, "UpdatePreferences2",
        "(Ljava/lang/String;JI)V"),
    {NULL},
};

static char g_gdrm_policy_path[128];

Class GloftGDRMPolicy::clazz = {
    .classpath       = g_gdrm_policy_path,
    .classname       = "GDRMPolicy",
    .managed_methods = gloftGDRMPolicyMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftGDRMPolicy),
};

/* ----------------------------------------------------------- registry */

/*
 * The four classpaths are composed here rather than written out as literals.
 *
 * Not style: the package prefix appears in this file once, so a port of the
 * other regional build (GAND/GloftM3KT ships a second package) changes one
 * string instead of four - and, more usefully, the composition mirrors what
 * the game itself does, so the names cannot drift apart by a typo in one of
 * them. A misspelt classpath is invisible: FindClass simply never matches.
 *
 * Composition has to happen before any lookup, and the registry is built from
 * static initialisers, so it runs from one too - and from the same one that
 * registers, so the order between them is not left to chance.
 */
static int register_gloft_helpers(void)
{
    snprintf(g_sutils_path, sizeof(g_sutils_path),
             "%s/GLUtils/SUtils", kPackage);
    snprintf(g_device_path, sizeof(g_device_path),
             "%s/GLUtils/Device", kPackage);
    snprintf(g_game_installer_path, sizeof(g_game_installer_path),
             "%s/installer/GameInstaller", kPackage);
    snprintf(g_gdrm_policy_path, sizeof(g_gdrm_policy_path),
             "%s/installer/GDRMPolicy", kPackage);

    GloftGameInstaller::m_sInstance.mDeviceInfo = &g_telephony_manager;

    ClassRegistry::register_class(GloftGL2JNILib::clazz);
    ClassRegistry::register_class(GloftSUtils::clazz);
    ClassRegistry::register_class(GloftDevice::clazz);
    ClassRegistry::register_class(GloftGameInstaller::clazz);
    ClassRegistry::register_class(GloftGDRMPolicy::clazz);
    return 0;
}

static const int registered = register_gloft_helpers();
