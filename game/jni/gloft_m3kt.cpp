/*
 * com/gameloft/android/GAND/GloftM3KT/GloftM3KT - Gameloft Live push.
 *
 * The second and last Gameloft class in the binary. It is the client for a
 * notification service that was retired with the rest of Gameloft Live, so
 * every method here answers the same thing in a different shape: there is no
 * message, no token, no user.
 *
 * Answering rather than omitting matters. The engine polls HasPushNotification
 * and LaunchGamebyNotification on its way through the main menu; a class that
 * does not answer produces a NULL method id, which the engine calls through
 * without checking - a fault at pc = 0 that looks like a loader bug.
 *
 * Unlike GloftM3HM, none of these descriptors appear as literals in the
 * donor's .rodata: the game builds this class's name at runtime and looks the
 * methods up from strings it assembles. The shapes below are the reference
 * port's return types (source/java.c) expressed as the only descriptor that
 * fits. If one of them turns out to be wrong the symptom is specific and
 * visible - "Class GloftM3KT does not have static method X" in the log, with
 * the descriptor the game actually asked for printed next to it.
 */

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "trace.h"
#include "mc3_classes.h"

static jint GloftM3KT_HasPushNotification(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static jint GloftM3KT_LaunchGamebyNotification(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static void GloftM3KT_ResetNotificationStatus(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
}

static void GloftM3KT_ResetLaunchStatus(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
}

static void GloftM3KT_EnableDisablePushNotifications(JNIEnv *env, jclass clazz, jint on)
{
    (void)env; (void)clazz; (void)on;
}

static jobject GloftM3KT_GetTokenID(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("");
}

static jobject GloftM3KT_GetUserID(JNIEnv *env, jclass clazz)
{
    (void)clazz;
    return (jobject)env->NewStringUTF("");
}

static jint GloftM3KT_SetOfflineDeviceCredential(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    return 0;
}

static void GloftM3KT_SetOfflineUserCredential(JNIEnv *env, jclass clazz,
                                               jstring user, jstring pass)
{
    (void)env; (void)clazz; (void)user; (void)pass;
}

static void GloftM3KT_SetOnlineUserCredential(JNIEnv *env, jclass clazz,
                                              jstring user, jstring pass)
{
    (void)env; (void)clazz; (void)user; (void)pass;
}

static jint GloftM3KT_DeletePush(JNIEnv *env, jclass clazz, jstring id)
{
    (void)env; (void)clazz; (void)id;
    return 0;
}

static jobject GloftM3KT_GetBundleData(JNIEnv *env, jclass clazz)
{
    (void)env; (void)clazz;
    /* A real Bundle, empty. Every getter on it answers zero/absent, which is
     * what "the game was not launched from a notification" means. */
    static AndroidBundle empty;
    return (jobject)&empty;
}

static jobject GloftM3KT_GetJanusToken(JNIEnv *env, jclass clazz,
                                       jstring a, jstring b, jstring c)
{
    (void)clazz; (void)a; (void)b; (void)c;
    return (jobject)env->NewStringUTF("");
}

const ManagedMethod gloftM3KTMethods[] = {
    ManagedMethod::RegisterStatic<&GloftM3KT_HasPushNotification>(
        GloftM3KT::clazz, "HasPushNotification", "()I"),
    ManagedMethod::RegisterStatic<&GloftM3KT_LaunchGamebyNotification>(
        GloftM3KT::clazz, "LaunchGamebyNotification", "()I"),
    ManagedMethod::RegisterStatic<&GloftM3KT_ResetNotificationStatus>(
        GloftM3KT::clazz, "ResetNotificationStatus", "()V"),
    ManagedMethod::RegisterStatic<&GloftM3KT_ResetLaunchStatus>(
        GloftM3KT::clazz, "ResetLaunchStatus", "()V"),
    ManagedMethod::RegisterStatic<&GloftM3KT_EnableDisablePushNotifications>(
        GloftM3KT::clazz, "EnableDisablePushNotifications", "(I)V"),
    ManagedMethod::RegisterStatic<&GloftM3KT_GetTokenID>(
        GloftM3KT::clazz, "GetTokenID", "()Ljava/lang/String;"),
    ManagedMethod::RegisterStatic<&GloftM3KT_GetUserID>(
        GloftM3KT::clazz, "GetUserID", "()Ljava/lang/String;"),
    ManagedMethod::RegisterStatic<&GloftM3KT_SetOfflineDeviceCredential>(
        GloftM3KT::clazz, "SetOfflineDeviceCredential", "()I"),
    ManagedMethod::RegisterStatic<&GloftM3KT_SetOfflineUserCredential>(
        GloftM3KT::clazz, "SetOfflineUserCredential",
        "(Ljava/lang/String;Ljava/lang/String;)V"),
    ManagedMethod::RegisterStatic<&GloftM3KT_SetOnlineUserCredential>(
        GloftM3KT::clazz, "SetOnlineUserCredential",
        "(Ljava/lang/String;Ljava/lang/String;)V"),
    ManagedMethod::RegisterStatic<&GloftM3KT_DeletePush>(
        GloftM3KT::clazz, "DeletePush", "(Ljava/lang/String;)I"),
    ManagedMethod::RegisterStatic<&GloftM3KT_GetBundleData>(
        GloftM3KT::clazz, "GetBundleData", "()Landroid/os/Bundle;"),
    ManagedMethod::RegisterStatic<&GloftM3KT_GetJanusToken>(
        GloftM3KT::clazz, "GetJanusToken",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"),
    {NULL},
};

Class GloftM3KT::clazz = {
    .classpath       = "com/gameloft/android/GAND/GloftM3KT/GloftM3KT",
    .classname       = "GloftM3KT",
    .managed_methods = gloftM3KTMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(GloftM3KT),
};

static const int registered = ClassRegistry::register_class(GloftM3KT::clazz);
