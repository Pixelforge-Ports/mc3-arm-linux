/*
 * The Android application classes this game reaches for.
 *
 * Activity, Context, Intent, Bundle, Handler, Process, Build, Build$VERSION,
 * SystemProperties, Settings$Secure, TelephonyManager, ContentResolver, UUID
 * and java/lang/Object - every one of them a name that appears verbatim in the
 * donor's .rodata.
 *
 * None of these is faked for its own sake. Two jobs run through the group:
 *
 *   the device id   The GDRM check builds one out of whatever the platform
 *                   will give it, trying IMEI, then Build.SERIAL, then
 *                   ro.serialno, then ANDROID_ID, then a random UUID. Every
 *                   step of that chain is one of these classes, and it runs
 *                   before the title screen.
 *
 *   the briefings   A mission briefing is an MP4 played by a second activity:
 *                   the engine fills an Intent and calls startActivity. There
 *                   is no second activity here, so the Intent is read and the
 *                   playback is completed immediately - see mc3_video_skip().
 */

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "trace.h"
#include "app_exit.h"
#include "mc3.h"
#include "mc3_classes.h"

AndroidInputMethodManager g_input_method_manager;
AndroidWindow             g_window;
AndroidWindowManager      g_window_manager;
AndroidTelephonyManager   g_telephony_manager;
AndroidContentResolver    g_content_resolver;
AndroidHandler            g_view_handler;
AndroidViewRoot           g_view_root;

/* ----------------------------------------------------------- Activity */

/*
 * The Intent extras, kept in one place because that is where they are used.
 *
 * The engine builds the briefing Intent across several calls - setClassName,
 * then putExtra("video_name", ...), then putExtra("language", ...) - and only
 * startActivity knows what they were for. Recording them as they arrive is
 * what the reference port does too (source/java.c, putExtra).
 *
 * One set of strings, not one per Intent: the game only ever has one of these
 * in flight, and an Intent object with its own storage would need an instance
 * layout the fake JVM does not allocate.
 */
static char g_video_name[256];
static char g_video_language[64];

static void Activity_startActivity(JNIEnv *env, jobject self, jobject intent)
{
    (void)self; (void)intent;
    trace("startActivity: video_name='%s' language='%s'",
          g_video_name, g_video_language);
    mc3_video_skip(env, g_video_name, g_video_language);
}

static void Activity_startActivityForResult(JNIEnv *env, jobject self,
                                            jobject intent, jint request)
{
    (void)self; (void)intent; (void)request;
    trace("startActivityForResult(%d): video_name='%s'", request, g_video_name);
    mc3_video_skip(env, g_video_name, g_video_language);
}

static void Activity_finish(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    /*
     * The game's own exit. portbase records it and the frame loop consumes it;
     * nothing is torn down here, because finish() arrives from inside the
     * engine's call stack and unwinding the process from there is a crash on
     * the way out rather than an exit. See portbase/android/app_exit.h.
     */
    android_app_request_exit("the game called MainActivity.finish()");
}

static jboolean Activity_hasWindowFocus(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    /*
     * FALSE, following the reference. TRUE reads like the friendlier answer
     * and is not: on Android the engine uses focus to decide whether another
     * app is on top of it, and the paths behind "focused" include showing the
     * soft keyboard, which nothing here can dismiss.
     */
    return JNI_FALSE;
}

static jobject Activity_getWindow(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return (jobject)&g_window;
}

static jobject Activity_getWindowManager(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return (jobject)&g_window_manager;
}

static jobject Activity_getIntent(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    static AndroidIntent launch_intent;
    return (jobject)&launch_intent;
}

static void Activity_setVolumeControlStream(JNIEnv *env, jobject self, jint stream)
{
    (void)env; (void)self; (void)stream;
}

static jboolean Activity_requestWindowFeature(JNIEnv *env, jobject self, jint feature)
{
    (void)env; (void)self; (void)feature;
    return JNI_FALSE;
}

static void Activity_setContentView(JNIEnv *env, jobject self, jobject view)
{
    (void)env; (void)self; (void)view;
}

static void Activity_addContentView(JNIEnv *env, jobject self,
                                    jobject view, jobject params)
{
    (void)env; (void)self; (void)view; (void)params;
}

static jobject Activity_getContentResolver(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return (jobject)&g_content_resolver;
}

/*
 * getSystemService, on both Activity and Context.
 *
 * The game asks for two of them - "window" and "input_method" - and stores
 * whatever comes back. Returning the manager for either keeps the later call
 * on it resolving; the class the method is looked up on is the one the engine
 * named at FindClass time, not this object's, so a single sensible answer per
 * name is enough.
 */
static jobject getSystemService(JNIEnv *env, jobject self, jstring name)
{
    (void)env; (void)self;
    const char *what = name ? ((String *)name)->str : "";

    if (strcmp(what, "input_method") == 0)
        return (jobject)&g_input_method_manager;
    if (strcmp(what, "window") == 0)
        return (jobject)&g_window_manager;

    trace("getSystemService('%s') - answered with the window manager", what);
    return (jobject)&g_window_manager;
}

static jobject Context_getSystemService(JNIEnv *env, jobject self, jstring name)
{
    return getSystemService(env, self, name);
}

const ManagedMethod androidActivityMethods[] = {
    ManagedMethod::Register<&Activity_startActivity>(
        AndroidActivity::clazz, "startActivity", "(Landroid/content/Intent;)V"),
    ManagedMethod::Register<&Activity_startActivityForResult>(
        AndroidActivity::clazz, "startActivityForResult", "(Landroid/content/Intent;I)V"),
    ManagedMethod::Register<&Activity_finish>(
        AndroidActivity::clazz, "finish", "()V"),
    ManagedMethod::Register<&Activity_hasWindowFocus>(
        AndroidActivity::clazz, "hasWindowFocus", "()Z"),
    ManagedMethod::Register<&Activity_getWindow>(
        AndroidActivity::clazz, "getWindow", "()Landroid/view/Window;"),
    ManagedMethod::Register<&Activity_getWindowManager>(
        AndroidActivity::clazz, "getWindowManager", "()Landroid/view/WindowManager;"),
    ManagedMethod::Register<&Activity_getIntent>(
        AndroidActivity::clazz, "getIntent", "()Landroid/content/Intent;"),
    ManagedMethod::Register<&Activity_setVolumeControlStream>(
        AndroidActivity::clazz, "setVolumeControlStream", "(I)V"),
    ManagedMethod::Register<&Activity_requestWindowFeature>(
        AndroidActivity::clazz, "requestWindowFeature", "(I)Z"),
    ManagedMethod::Register<&Activity_setContentView>(
        AndroidActivity::clazz, "setContentView", "(Landroid/view/View;)V"),
    ManagedMethod::Register<&Activity_addContentView>(
        AndroidActivity::clazz, "addContentView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"),
    ManagedMethod::Register<&Activity_getContentResolver>(
        AndroidActivity::clazz, "getContentResolver",
        "()Landroid/content/ContentResolver;"),
    ManagedMethod::Register<&getSystemService>(
        AndroidActivity::clazz, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;"),
    {NULL},
};

Class AndroidActivity::clazz = {
    .classpath       = "android/app/Activity",
    .classname       = "Activity",
    .managed_methods = androidActivityMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidActivity),
};

/* ------------------------------------------------------------ Context */

String AndroidContext::WINDOW_SERVICE("window");

const ManagedMethod androidContextMethods[] = {
    ManagedMethod::Register<&Context_getSystemService>(
        AndroidContext::clazz, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;"),
    {NULL},
};

const FieldId androidContextFields[] = {
    REGISTER_STATIC_FIELD(AndroidContext, WINDOW_SERVICE),
    {NULL},
};

Class AndroidContext::clazz = {
    .classpath       = "android/content/Context",
    .classname       = "Context",
    .managed_methods = androidContextMethods,
    .native_methods  = {NULL},
    .fields          = androidContextFields,
    .instance_size   = sizeof(AndroidContext),
};

/* ------------------------------------------------------------- Intent */

static jobject Intent_init(JNIEnv *env, jobject self, jclass clazz,
                           jobject context, jobject target)
{
    (void)env; (void)clazz; (void)context; (void)target;
    return (jobject)self;
}

static jobject Intent_setClassName(JNIEnv *env, jobject self,
                                   jstring package, jstring name)
{
    (void)env;
    trace("Intent.setClassName(%s, %s)",
          package ? ((String *)package)->str : "(null)",
          name    ? ((String *)name)->str    : "(null)");
    return (jobject)self;
}

static jobject Intent_putExtraString(JNIEnv *env, jobject self,
                                     jstring key, jstring value)
{
    (void)env;
    const char *k = key   ? ((String *)key)->str   : "";
    const char *v = value ? ((String *)value)->str : "";

    /*
     * The two extras the port has to remember. The reference port has a bug
     * here worth not copying: its second comparison tests arg2 instead of
     * arg1, so "language" is only ever recorded when the *value* happens to be
     * that word. Compared against the key in both cases below.
     */
    if (strcmp(k, "video_name") == 0)
        snprintf(g_video_name, sizeof(g_video_name), "%s", v);
    else if (strcmp(k, "language") == 0)
        snprintf(g_video_language, sizeof(g_video_language), "%s", v);

    return (jobject)self;
}

static jobject Intent_putExtraInt(JNIEnv *env, jobject self, jstring key, jint value)
{
    (void)env; (void)key; (void)value;
    return (jobject)self;
}

static jobject Intent_setFlags(JNIEnv *env, jobject self, jint flags)
{
    (void)env; (void)flags;
    return (jobject)self;
}

static jobject Intent_addCategory(JNIEnv *env, jobject self, jstring category)
{
    (void)env; (void)category;
    return (jobject)self;
}

static jint Intent_getIntExtra(JNIEnv *env, jobject self, jstring key, jint fallback)
{
    (void)env; (void)self; (void)key;
    return fallback;
}

static void Intent_removeExtra(JNIEnv *env, jobject self, jstring key)
{
    (void)env; (void)self; (void)key;
}

static jobject Intent_init_empty(JNIEnv *env, jobject self, jclass clazz)
{
    (void)env; (void)clazz;
    return (jobject)self;
}

const ManagedMethod androidIntentMethods[] = {
    REGISTER_INIT_METHOD(AndroidIntent, Intent_init,
                         "(Landroid/content/Context;Ljava/lang/Class;)V"),
    /* The no-argument constructor, named by a run log: the game builds an
     * empty Intent and fills it with setClassName/putExtra afterwards. */
    REGISTER_INIT_METHOD(AndroidIntent, Intent_init_empty, "()V"),
    ManagedMethod::Register<&Intent_setClassName>(
        AndroidIntent::clazz, "setClassName",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"),
    ManagedMethod::Register<&Intent_putExtraString>(
        AndroidIntent::clazz, "putExtra",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;"),
    ManagedMethod::Register<&Intent_putExtraInt>(
        AndroidIntent::clazz, "putExtra",
        "(Ljava/lang/String;I)Landroid/content/Intent;"),
    ManagedMethod::Register<&Intent_setFlags>(
        AndroidIntent::clazz, "setFlags", "(I)Landroid/content/Intent;"),
    ManagedMethod::Register<&Intent_addCategory>(
        AndroidIntent::clazz, "addCategory",
        "(Ljava/lang/String;)Landroid/content/Intent;"),
    ManagedMethod::Register<&Intent_getIntExtra>(
        AndroidIntent::clazz, "getIntExtra", "(Ljava/lang/String;I)I"),
    ManagedMethod::Register<&Intent_removeExtra>(
        AndroidIntent::clazz, "removeExtra", "(Ljava/lang/String;)V"),
    {NULL},
};

Class AndroidIntent::clazz = {
    .classpath       = "android/content/Intent",
    .classname       = "Intent",
    .managed_methods = androidIntentMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidIntent),
};

/* ------------------------------------------------------------- Bundle */

/*
 * A Bundle that forgets everything.
 *
 * The game uses one to persist the GDRM policy across launches - the retry
 * counter, the validity timestamp, the last server response. Every getter here
 * answers "absent", so the policy starts from scratch every run, which is
 * exactly the state it would be in on a first launch and a state it handles.
 * Nothing else in the game stores anything through a Bundle: the save game
 * goes through the filesystem.
 *
 * Storage would be the wrong kind of work here. It would persist the answer to
 * a licence check this port already answers unconditionally in patch.cpp.
 */
static jobject Bundle_init(JNIEnv *env, jobject self, jclass clazz)
{
    (void)env; (void)clazz;
    return (jobject)self;
}

static void    Bundle_putString(JNIEnv *e, jobject s, jstring k, jstring v)  { (void)e; (void)s; (void)k; (void)v; }
static void    Bundle_putInt(JNIEnv *e, jobject s, jstring k, jint v)        { (void)e; (void)s; (void)k; (void)v; }
static void    Bundle_putLong(JNIEnv *e, jobject s, jstring k, jlong v)      { (void)e; (void)s; (void)k; (void)v; }
static void    Bundle_putByteArray(JNIEnv *e, jobject s, jstring k, jbyteArray v) { (void)e; (void)s; (void)k; (void)v; }
static jint    Bundle_getInt(JNIEnv *e, jobject s, jstring k)                { (void)e; (void)s; (void)k; return 0; }
static jlong   Bundle_getLong(JNIEnv *e, jobject s, jstring k)               { (void)e; (void)s; (void)k; return 0; }
static jboolean Bundle_containsKey(JNIEnv *e, jobject s, jstring k)          { (void)e; (void)s; (void)k; return JNI_FALSE; }
static void    Bundle_clear(JNIEnv *e, jobject s)                            { (void)e; (void)s; }

static jobject Bundle_getString(JNIEnv *env, jobject self, jstring key)
{
    (void)self; (void)key;
    return (jobject)env->NewStringUTF("");
}

static jbyteArray Bundle_getByteArray(JNIEnv *env, jobject self, jstring key)
{
    (void)self; (void)key;
    /*
     * A zero-length array, not NULL. The engine passes what it gets straight
     * to GetArrayLength and GetByteArrayElements; NULL there is a fault inside
     * the fake JVM with the game's return address in lr, which reads as a
     * loader bug rather than as an absent preference.
     */
    return env->NewByteArray(0);
}

const ManagedMethod androidBundleMethods[] = {
    REGISTER_INIT_METHOD(AndroidBundle, Bundle_init, "()V"),
    ManagedMethod::Register<&Bundle_putString>(
        AndroidBundle::clazz, "putString", "(Ljava/lang/String;Ljava/lang/String;)V"),
    ManagedMethod::Register<&Bundle_getString>(
        AndroidBundle::clazz, "getString", "(Ljava/lang/String;)Ljava/lang/String;"),
    ManagedMethod::Register<&Bundle_putInt>(
        AndroidBundle::clazz, "putInt", "(Ljava/lang/String;I)V"),
    ManagedMethod::Register<&Bundle_getInt>(
        AndroidBundle::clazz, "getInt", "(Ljava/lang/String;)I"),
    ManagedMethod::Register<&Bundle_putLong>(
        AndroidBundle::clazz, "putLong", "(Ljava/lang/String;J)V"),
    ManagedMethod::Register<&Bundle_getLong>(
        AndroidBundle::clazz, "getLong", "(Ljava/lang/String;)J"),
    ManagedMethod::Register<&Bundle_putByteArray>(
        AndroidBundle::clazz, "putByteArray", "(Ljava/lang/String;[B)V"),
    ManagedMethod::Register<&Bundle_getByteArray>(
        AndroidBundle::clazz, "getByteArray", "(Ljava/lang/String;)[B"),
    ManagedMethod::Register<&Bundle_containsKey>(
        AndroidBundle::clazz, "containsKey", "(Ljava/lang/String;)Z"),
    ManagedMethod::Register<&Bundle_clear>(
        AndroidBundle::clazz, "clear", "()V"),
    {NULL},
};

Class AndroidBundle::clazz = {
    .classpath       = "android/os/Bundle",
    .classname       = "Bundle",
    .managed_methods = androidBundleMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidBundle),
};

/* ------------------------------------------------ Handler and Process */

static jboolean Handler_sendEmptyMessage(JNIEnv *env, jobject self, jint what)
{
    (void)env; (void)self;
    /*
     * The game posts these to make the Java layer do something on the UI
     * thread - show a dialog, hide the loading spinner. Every one of those
     * targets is already a no-op here, so the message is dropped and the
     * "delivered" answer is honest: nothing was queued, but nothing was going
     * to happen either way.
     */
    static bool announced = false;
    if (!announced) {
        announced = true;
        trace("Handler.sendEmptyMessage(%d) - the UI thread has nothing to do "
              "in this port; messages are dropped", what);
    }
    return JNI_TRUE;
}

const ManagedMethod androidHandlerMethods[] = {
    ManagedMethod::Register<&Handler_sendEmptyMessage>(
        AndroidHandler::clazz, "sendEmptyMessage", "(I)Z"),
    {NULL},
};

Class AndroidHandler::clazz = {
    .classpath       = "android/os/Handler",
    .classname       = "Handler",
    .managed_methods = androidHandlerMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidHandler),
};

static void Process_setThreadPriority(JNIEnv *env, jclass clazz, jint priority)
{
    (void)env; (void)clazz;
    /*
     * Ignored on purpose. Android's scale runs -20 (urgent audio) to 19, and
     * the values the engine passes are meaningful only against Android's
     * scheduler policy. Handing them to setpriority() here would need
     * CAP_SYS_NICE for anything negative and would fail silently without it.
     */
    static bool announced = false;
    if (!announced) {
        announced = true;
        trace("Process.setThreadPriority(%d) and later calls ignored", priority);
    }
}

const ManagedMethod androidProcessMethods[] = {
    ManagedMethod::RegisterStatic<&Process_setThreadPriority>(
        AndroidProcess::clazz, "setThreadPriority", "(I)V"),
    {NULL},
};

Class AndroidProcess::clazz = {
    .classpath       = "android/os/Process",
    .classname       = "Process",
    .managed_methods = androidProcessMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidProcess),
};

/* ------------------------------------------- the device-id fallback chain */

/*
 * A single made-up identity, answered by every link of the chain.
 *
 * GDRMPolicy tries the IMEI, then Build.SERIAL, then the ro.serialno system
 * property, then Settings.Secure.ANDROID_ID, then a random UUID, and keeps the
 * first non-empty answer. Which one it lands on does not matter here - what
 * matters is that the answer is the same on every run, because the game salts
 * its save file with it. A random UUID would make each launch look like a
 * different device.
 */
static const char kDeviceId[] = "r36s0000deadbeef";

String AndroidBuild::SERIAL(kDeviceId);
int    AndroidBuildVersion::SDK_INT = 19;

const FieldId androidBuildFields[] = {
    REGISTER_STATIC_FIELD(AndroidBuild, SERIAL),
    {NULL},
};

Class AndroidBuild::clazz = {
    .classpath       = "android/os/Build",
    .classname       = "Build",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = androidBuildFields,
    .instance_size   = sizeof(AndroidBuild),
};

/*
 * Build.VERSION.SDK_INT is deliberately NOT registered, and that needs saying.
 *
 * portbase's static-field accessor returns the field's *address* rather than
 * its contents (jni/jni.cpp, iface_GetStaticField: `return (T)(f->offset)`).
 * For an object field that is exactly right - the address of a String member
 * is the String - and for an int field it is not: the game would receive a
 * pointer value where it expects 19, and it would be a large number, so every
 * "is this at least Android X" test would pass for the wrong reason.
 *
 * Registering it anyway is the failure mode this codebase keeps paying for: a
 * plausible-looking answer that is wrong in a way nothing reports. Left
 * unregistered, GetStaticFieldID says so in the log and the read comes back 0,
 * which reads as "older than any version being tested for" - a defined answer,
 * and one every branch in the device-id chain below has a path for.
 *
 * The real fix belongs in portbase, not here: primitive static fields should
 * read through the recorded address. Noted for the base rather than worked
 * around locally, because every port would want it.
 */
Class AndroidBuildVersion::clazz = {
    .classpath       = "android/os/Build$VERSION",
    .classname       = "Build$VERSION",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidBuildVersion),
};

static jobject SystemProperties_get(JNIEnv *env, jclass clazz, jstring key)
{
    (void)clazz;
    const char *name = key ? ((String *)key)->str : "";
    trace("SystemProperties.get('%s')", name);
    if (strcmp(name, "ro.serialno") == 0)
        return (jobject)env->NewStringUTF(kDeviceId);
    return (jobject)env->NewStringUTF("");
}

const ManagedMethod androidSystemPropertiesMethods[] = {
    ManagedMethod::RegisterStatic<&SystemProperties_get>(
        AndroidSystemProperties::clazz, "get",
        "(Ljava/lang/String;)Ljava/lang/String;"),
    {NULL},
};

Class AndroidSystemProperties::clazz = {
    .classpath       = "android/os/SystemProperties",
    .classname       = "SystemProperties",
    .managed_methods = androidSystemPropertiesMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidSystemProperties),
};

static jobject SettingsSecure_getString(JNIEnv *env, jclass clazz,
                                        jobject resolver, jstring key)
{
    (void)clazz; (void)resolver;
    trace("Settings.Secure.getString('%s')", key ? ((String *)key)->str : "");
    return (jobject)env->NewStringUTF(kDeviceId);
}

const ManagedMethod androidSettingsSecureMethods[] = {
    ManagedMethod::RegisterStatic<&SettingsSecure_getString>(
        AndroidSettingsSecure::clazz, "getString",
        "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;"),
    {NULL},
};

Class AndroidSettingsSecure::clazz = {
    .classpath       = "android/provider/Settings$Secure",
    .classname       = "Settings$Secure",
    .managed_methods = androidSettingsSecureMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidSettingsSecure),
};

static jobject TelephonyManager_getDeviceId(JNIEnv *env, jobject self)
{
    (void)self;
    trace("TelephonyManager.getDeviceId()");
    return (jobject)env->NewStringUTF("351066496380730");
}

const ManagedMethod androidTelephonyManagerMethods[] = {
    ManagedMethod::Register<&TelephonyManager_getDeviceId>(
        AndroidTelephonyManager::clazz, "getDeviceId", "()Ljava/lang/String;"),
    {NULL},
};

Class AndroidTelephonyManager::clazz = {
    .classpath       = "android/telephony/TelephonyManager",
    .classname       = "TelephonyManager",
    .managed_methods = androidTelephonyManagerMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidTelephonyManager),
};

Class AndroidContentResolver::clazz = {
    .classpath       = "android/content/ContentResolver",
    .classname       = "ContentResolver",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidContentResolver),
};

/* --------------------------------------------------- UUID and Object */

static jobject Uuid_randomUUID(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* The last resort of the device-id chain. Fixed rather than random, for
     * the reason in the kDeviceId comment above. */
    static JavaUuid the_uuid;
    return (jobject)&the_uuid;
}

static jobject Uuid_toString(JNIEnv *env, jobject self)
{
    (void)self;
    return (jobject)env->NewStringUTF("00000000-0000-4000-8000-r36s0deadbeef");
}

const ManagedMethod javaUuidMethods[] = {
    ManagedMethod::RegisterStatic<&Uuid_randomUUID>(
        JavaUuid::clazz, "randomUUID", "()Ljava/util/UUID;"),
    ManagedMethod::Register<&Uuid_toString>(
        JavaUuid::clazz, "toString", "()Ljava/lang/String;"),
    {NULL},
};

Class JavaUuid::clazz = {
    .classpath       = "java/util/UUID",
    .classname       = "UUID",
    .managed_methods = javaUuidMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(JavaUuid),
};

static jobject Object_toString(JNIEnv *env, jobject self)
{
    return (jobject)env->NewStringUTF("java.lang.Object");
}

const ManagedMethod javaObjectMethods[] = {
    ManagedMethod::Register<&Object_toString>(
        JavaObject::clazz, "toString", "()Ljava/lang/String;"),
    {NULL},
};

Class JavaObject::clazz = {
    .classpath       = "java/lang/Object",
    .classname       = "Object",
    .managed_methods = javaObjectMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(JavaObject),
};

/* Registration order does not matter - lookup is by name - but keeping it in
 * one place makes it obvious when a class was written and never registered,
 * which is a failure with no error message. */
static const int registered[] = {
    ClassRegistry::register_class(AndroidActivity::clazz),
    ClassRegistry::register_class(AndroidContext::clazz),
    ClassRegistry::register_class(AndroidIntent::clazz),
    ClassRegistry::register_class(AndroidBundle::clazz),
    ClassRegistry::register_class(AndroidHandler::clazz),
    ClassRegistry::register_class(AndroidProcess::clazz),
    ClassRegistry::register_class(AndroidBuild::clazz),
    ClassRegistry::register_class(AndroidBuildVersion::clazz),
    ClassRegistry::register_class(AndroidSystemProperties::clazz),
    ClassRegistry::register_class(AndroidSettingsSecure::clazz),
    ClassRegistry::register_class(AndroidTelephonyManager::clazz),
    ClassRegistry::register_class(AndroidContentResolver::clazz),
    ClassRegistry::register_class(JavaUuid::clazz),
    ClassRegistry::register_class(JavaObject::clazz),
};
