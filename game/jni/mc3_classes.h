#pragma once

#include "jni.h"
#include "jni_internals.h"

/*
 * The Java classes this game asks for.
 *
 * The list is not guesswork and it is not the Vita port's - it is every
 * classpath-shaped string in the donor:
 *
 *   strings -a libModernCombat3.so | grep -E '^(com|java|android|video)/'
 *
 * which returns exactly 23 names. portbase already ships three of them
 * (java/lang/String, java/nio/ByteBuffer, java/lang/ClassLoader); the rest are
 * here, grouped by the file that implements them:
 *
 *   gloft_m3hm.cpp   com/gameloft/android/ANMP/GloftM3HM/GloftM3HM
 *   gloft_m3kt.cpp   com/gameloft/android/GAND/GloftM3KT/GloftM3KT
 *   android_app.cpp  Activity, Context, Intent, Bundle, Handler, Process,
 *                    Build, Build$VERSION, SystemProperties, Settings$Secure,
 *                    TelephonyManager, ContentResolver, UUID, Object
 *   android_view.cpp View, TextView, EditText, ViewGroup$LayoutParams,
 *                    ViewRoot, InputMethodManager, Window, WindowManager
 *   media_audiotrack.cpp  android/media/AudioTrack
 *   video_player.cpp      video/MyVideoView
 *
 * Two rules run through all of them, and both are pitfalls this project has
 * already paid for:
 *
 *   - Never return NULL to the engine. It stores what it gets and dereferences
 *     it later, so a null comes back as a fault somewhere unrelated. Every
 *     object-returning method answers with a real instance of a registered
 *     class, which is also what makes a later method call on it resolve.
 *
 *   - Method lookup is an exact strcmp on name AND descriptor. A signature
 *     that merely looks plausible never matches, and the engine then gets the
 *     same NULL as if the class were missing, with nothing in the log to tell
 *     the two apart. Every descriptor below that takes arguments was read out
 *     of the donor's .rodata; the no-argument ones are the only shape their
 *     return type allows.
 */

#define MC3_CLASS(name)                     \
    class name : public Object {            \
    public:                                 \
        static Class clazz;                 \
        Class *_getClass() { return &clazz; } \
    }

/* com/gameloft/android/ANMP/GloftM3HM/GloftM3HM - the activity the whole game
 * talks to. Everything device-shaped goes through it. */
MC3_CLASS(GloftM3HM);

/* com/gameloft/android/GAND/GloftM3KT/GloftM3KT - Gameloft Live push
 * notifications. A dead service; every method answers "nothing pending". */
MC3_CLASS(GloftM3KT);

MC3_CLASS(AndroidActivity);
MC3_CLASS(AndroidIntent);
MC3_CLASS(AndroidBundle);
MC3_CLASS(AndroidHandler);
MC3_CLASS(AndroidProcess);
MC3_CLASS(AndroidSystemProperties);
MC3_CLASS(AndroidSettingsSecure);
MC3_CLASS(AndroidTelephonyManager);
MC3_CLASS(AndroidContentResolver);
MC3_CLASS(JavaUuid);
MC3_CLASS(JavaObject);

MC3_CLASS(AndroidView);
MC3_CLASS(AndroidTextView);
MC3_CLASS(AndroidEditText);
MC3_CLASS(AndroidLayoutParams);
MC3_CLASS(AndroidViewRoot);
MC3_CLASS(AndroidInputMethodManager);
MC3_CLASS(AndroidWindow);
MC3_CLASS(AndroidWindowManager);

MC3_CLASS(VideoMyVideoView);

/*
 * The four classes the game does not name in one piece.
 *
 * These are why the classpath grep above is a floor and not a ceiling: the
 * binary carries "Lcom/gameloft/android/ANMP/GloftM3HM" (7c8df8), "%s/%s;"
 * (7c8df0) and the tails "GLUtils/SUtils" (7c92e8), "GLUtils/Device" (7c938c),
 * "installer/GameInstaller" (7c93ac) and "GDRMPolicy" (7c9020) separately, and
 * builds each full name with sprintf at the point of use. A grep for whole
 * classpaths cannot see them, and a class the engine asks for by a name
 * nothing registered comes back NULL - which is the silent failure portbase's
 * FindClass warning exists to make audible.
 *
 * They are grouped in gloft_helpers.cpp.
 */
MC3_CLASS(GloftGL2JNILib);
MC3_CLASS(GloftSUtils);
MC3_CLASS(GloftDevice);
MC3_CLASS(GloftGDRMPolicy);

#undef MC3_CLASS

/*
 * android/media/AudioTrack, which is the only class here that owns state.
 *
 * The engine constructs one and then writes PCM into it for the rest of the
 * run, so the SDL output device has to live somewhere that survives between
 * calls and belongs to *this* track rather than to the file. The sink is
 * behind a pointer so that SDL's headers stay out of every other JNI class
 * that includes this one; media_audiotrack.cpp defines it.
 */
struct Mc3AudioSink;

class AndroidAudioTrack : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    Mc3AudioSink *sink;
};

/*
 * The three classes the game reads a *field* off, rather than calling.
 *
 * These cannot use the macro above because a field has to exist as a real
 * member to have an address: REGISTER_STATIC_FIELD records &Class::member, and
 * the fake JVM hands the engine that address directly.
 */
class AndroidContext : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    /* android.content.Context.WINDOW_SERVICE, the string the game passes to
     * getSystemService. Its value is part of the platform API, not a choice. */
    static String WINDOW_SERVICE;
};

class AndroidBuild : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    /* Read by the DRM's device-id fallback chain (see gloft_helpers.cpp). */
    static String SERIAL;
};

/*
 * com/gameloft/android/ANMP/GloftM3HM/installer/GameInstaller.
 *
 * The DRM's entry point, and the only class in this port that needs both kinds
 * of field: the game reads the singleton off the class (m_sInstance) and then
 * the telephony manager off the instance (mDeviceInfo).
 *
 * m_sInstance is a static member of its own type on purpose. portbase answers
 * a static object field with the field's address, so the field has to *be* the
 * object - a pointer member would hand the engine a pointer to a pointer.
 */
class GloftGameInstaller : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    static GloftGameInstaller m_sInstance;

    /* Instance fields are dereferenced properly, so this one is a pointer. */
    AndroidTelephonyManager *mDeviceInfo;
};

class AndroidBuildVersion : public Object {
public:
    static Class clazz;
    Class *_getClass() { return &clazz; }

    /*
     * The Android release this port claims to be. The game branches on it in
     * at least two places - the serial-number fallback is gated on >= 9 - and
     * 19 (KitKat) is what the reference port reports.
     */
    static int SDK_INT;
};

/*
 * The singletons the engine is handed.
 *
 * On Android these are distinct objects with state; here they exist so that a
 * method looked up on what getContext() or getWindow() returned resolves to
 * something. They are deliberately global rather than allocated per call: the
 * engine keeps the pointers it is given for the life of the process and
 * compares some of them, and a fresh object per call would break that
 * silently.
 */
extern GloftM3HM                 g_activity;
extern AndroidInputMethodManager g_input_method_manager;
extern AndroidWindow             g_window;
extern AndroidWindowManager      g_window_manager;
extern AndroidTelephonyManager   g_telephony_manager;
extern AndroidContentResolver    g_content_resolver;
extern AndroidEditText           g_edit_box;
extern AndroidHandler            g_view_handler;
extern AndroidViewRoot           g_view_root;
