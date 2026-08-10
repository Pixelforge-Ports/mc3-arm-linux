/*
 * video/MyVideoView - the mission briefings, skipped cleanly.
 *
 * A briefing is an MP4 (the binary names "%s/video/LOGO_Gameloft_320x480.mp4"
 * at .rodata 0x7c83d4 and builds the mission ones the same way). On Android
 * the engine hands it to a second activity through an Intent and then *waits*:
 * it will not advance until that activity calls back into the .so through
 * Java_video_MyVideoView_SetFinished. The Vita port replaces the activity with
 * its own AVPlayer; there is no such stack here.
 *
 * So the callback is made - but DEFERRED, not immediate, and the delay is the
 * fix for a hang seen on hardware: startActivity runs inside the engine's own
 * playMovie path, *before* the engine has entered its "waiting for the video"
 * state. Delivering SetFinished right there hands the completion to a state
 * machine that is not listening yet; the engine then starts waiting and stays
 * there forever - black screen, music still playing, frames still pumping.
 * The Vita port never hits this because its AVPlayer finishes seconds later,
 * from the main loop. So: arm a countdown here, deliver from the frame loop
 * (mc3_video_skip_tick) the way a real player would.
 *
 * The mechanism matters more than the feature: leaving startActivity empty is
 * not "briefings do not play", it is the game stopping at the first one and
 * never continuing - a freeze with no error, at a point that looks like the
 * mission failed to load.
 *
 * The three Java_video_MyVideoView_* entry points are exported by the game's
 * own library, so this class forwards to them rather than implementing them:
 * on Android MyVideoView's methods are declared native and their bodies are in
 * the .so.
 */

#include <stddef.h>
#include <stdio.h>

#include "platform.h"
#include "so_util.h"
#include "jni.h"
#include "jni_internals.h"
#include "trace.h"
#include "mc3.h"
#include "mc3_classes.h"

static int     g_finish_delay = 0;
static JNIEnv *g_finish_env   = NULL;

void mc3_video_skip(JNIEnv *env, const char *video_name, const char *language)
{
    so_module *mod = mc3_module();
    if (!mod) {
        warning("video: asked to skip '%s' before the module was loaded\n",
                video_name ? video_name : "(none)");
        return;
    }

    if (!so_symbol(mod, "Java_video_MyVideoView_SetFinished")) {
        /*
         * Loud, because the consequence is a hang and the cause would not be
         * visible anywhere else. This export exists in the 1.1.7g build the
         * port targets; its absence means the donor is a different build.
         */
        warning("video: this library exports no Java_video_MyVideoView_SetFinished.\n"
                "       The game waits for that callback before leaving a briefing,\n"
                "       so it will stop at the first one.\n");
        return;
    }

    trace("briefing '%s' (%s) skipped - SetFinished armed, delivering in a few frames",
          video_name && *video_name ? video_name : "(unnamed)",
          language && *language ? language : "default");

    g_finish_env   = env;
    g_finish_delay = 30;   /* ~half a second: enough for playMovie to unwind
                              and the engine to actually start waiting */
}

void mc3_video_skip_tick(void)
{
    if (g_finish_delay <= 0 || --g_finish_delay > 0)
        return;

    so_module *mod = mc3_module();
    if (!mod)
        return;

    /*
     * Resolved at delivery rather than cached at arm time. It costs a symbol
     * table walk once per briefing - a handful of times in a whole playthrough
     * - and it removes a class of bug this project has hit before: a pointer
     * captured from a module that had not finished linking.
     */
    auto SetFinished = (void (*)(JNIEnv *, jobject))
        so_symbol(mod, "Java_video_MyVideoView_SetFinished");
    if (!SetFinished)
        return;

    trace("briefing skip: SetFinished delivered");

    /* 0x42424242 is the reference port's placeholder object: the engine stores
     * the pointer without dereferencing it, and a recognisable value makes it
     * obvious in a fault address if that ever stops being true. */
    SetFinished(g_finish_env, (jobject)0x42424242);
}

/*
 * The class itself, for the lookups the engine makes on it by name.
 *
 * playMovie is registered as a *managed* method that does the skip, because
 * the engine can reach a video by this route as well as through the Intent -
 * the binary logs "playMovie() - Begin" from its own code at 0x7c8384. Both
 * routes have to end in SetFinished or only one kind of video is skippable.
 */
static void MyVideoView_playMovie(JNIEnv *env, jclass clazz, jstring name)
{
    (void)clazz;
    mc3_video_skip(env, name ? ((String *)name)->str : "", "");
}

const ManagedMethod videoMyVideoViewMethods[] = {
    ManagedMethod::RegisterStatic<&MyVideoView_playMovie>(
        VideoMyVideoView::clazz, "playMovie", "(Ljava/lang/String;)V"),
    {NULL},
};

Class VideoMyVideoView::clazz = {
    .classpath       = "video/MyVideoView",
    .classname       = "MyVideoView",
    .managed_methods = videoMyVideoViewMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(VideoMyVideoView),
};

static const int registered = ClassRegistry::register_class(VideoMyVideoView::clazz);
