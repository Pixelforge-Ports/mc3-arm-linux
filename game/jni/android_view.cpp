/*
 * The Android view classes - the on-screen keyboard, and nothing else.
 *
 * Modern Combat 3 draws its whole interface itself, in GL. The only place it
 * reaches into Android's widget system is text entry: multiplayer names, the
 * Gameloft Live login. It builds an EditText, asks the InputMethodManager to
 * raise the soft keyboard over the GL surface, and reads the text back out.
 *
 * A handheld has no soft keyboard and no touchscreen to type on, so all of
 * that is answered rather than implemented: the keyboard never appears and the
 * text is always empty. What matters is that every call still resolves and
 * returns something the engine can hold. The pitfall here is specific - it
 * asks for the view's window token and passes it straight back into
 * hideSoftInputFromWindow, so a NULL token is a fault two calls later, inside
 * a method that has nothing to do with the mistake.
 *
 * Every descriptor below is a literal in the donor at .rodata 0x7c8134-0x7c8360.
 */

#include "platform.h"
#include "jni.h"
#include "jni_internals.h"
#include "trace.h"
#include "mc3_classes.h"

AndroidEditText g_edit_box;

/* The IBinder a view hands out as its window token. Android treats it as an
 * opaque handle and so does the game - it is only ever passed back in. */
static JavaObject g_window_token;

/* ---------------------------------------------------------------- View */

static void View_setVisibility(JNIEnv *env, jobject self, jint visibility)
{
    (void)env; (void)self;
    static bool announced = false;
    if (!announced) {
        announced = true;
        trace("View.setVisibility(%d) - no Android views are drawn in this port",
              visibility);
    }
}

static jint View_getVisibility(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return 8; /* View.GONE - nothing of Android's is on screen. */
}

static jobject View_getWindowToken(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return (jobject)&g_window_token;
}

static jboolean View_requestFocus(JNIEnv *env, jobject self)
{
    (void)env; (void)self;
    return JNI_FALSE;
}

static void View_setBackgroundColor(JNIEnv *env, jobject self, jint colour)
{
    (void)env; (void)self; (void)colour;
}

const ManagedMethod androidViewMethods[] = {
    ManagedMethod::Register<&View_setVisibility>(
        AndroidView::clazz, "setVisibility", "(I)V"),
    ManagedMethod::Register<&View_getVisibility>(
        AndroidView::clazz, "getVisibility", "()I"),
    ManagedMethod::Register<&View_getWindowToken>(
        AndroidView::clazz, "getWindowToken", "()Landroid/os/IBinder;"),
    ManagedMethod::Register<&View_requestFocus>(
        AndroidView::clazz, "requestFocus", "()Z"),
    ManagedMethod::Register<&View_setBackgroundColor>(
        AndroidView::clazz, "setBackgroundColor", "(I)V"),
    {NULL},
};

Class AndroidView::clazz = {
    .classpath       = "android/view/View",
    .classname       = "View",
    .managed_methods = androidViewMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidView),
};

/* ------------------------------------------------------------ TextView */

/*
 * The text the player never typed.
 *
 * getText returns a CharSequence, which the engine then passes to something
 * that reads characters out of it. An empty String is a valid CharSequence and
 * reads as zero characters; NULL is a fault inside whatever reads it.
 */
static jobject TextView_getText(JNIEnv *env, jobject self)
{
    (void)self;
    return (jobject)env->NewStringUTF("");
}

static void TextView_setText(JNIEnv *env, jobject self, jobject text)
{
    (void)env; (void)self;
    trace("TextView.setText('%s')", text ? ((String *)text)->str : "");
}

const ManagedMethod androidTextViewMethods[] = {
    ManagedMethod::Register<&TextView_getText>(
        AndroidTextView::clazz, "getText", "()Ljava/lang/CharSequence;"),
    ManagedMethod::Register<&TextView_setText>(
        AndroidTextView::clazz, "setText", "(Ljava/lang/CharSequence;)V"),
    {NULL},
};

Class AndroidTextView::clazz = {
    .classpath       = "android/widget/TextView",
    .classname       = "TextView",
    .managed_methods = androidTextViewMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidTextView),
};

/* ------------------------------------------------------------ EditText */

static jobject EditText_init(JNIEnv *env, jobject self, jclass clazz, jobject context)
{
    (void)env; (void)clazz; (void)context;
    return (jobject)self;
}

static void EditText_setInputType(JNIEnv *env, jobject self, jint type)
{
    (void)env; (void)self; (void)type;
}

/* EditText is a TextView on Android and the game calls both halves on the same
 * object; the lookup is per class, so the two methods are registered here as
 * well rather than relying on an inheritance the fake JVM does not model. */
const ManagedMethod androidEditTextMethods[] = {
    REGISTER_INIT_METHOD(AndroidEditText, EditText_init,
                         "(Landroid/content/Context;)V"),
    ManagedMethod::Register<&EditText_setInputType>(
        AndroidEditText::clazz, "setInputType", "(I)V"),
    ManagedMethod::Register<&TextView_getText>(
        AndroidEditText::clazz, "getText", "()Ljava/lang/CharSequence;"),
    ManagedMethod::Register<&TextView_setText>(
        AndroidEditText::clazz, "setText", "(Ljava/lang/CharSequence;)V"),
    ManagedMethod::Register<&View_setVisibility>(
        AndroidEditText::clazz, "setVisibility", "(I)V"),
    ManagedMethod::Register<&View_getVisibility>(
        AndroidEditText::clazz, "getVisibility", "()I"),
    ManagedMethod::Register<&View_getWindowToken>(
        AndroidEditText::clazz, "getWindowToken", "()Landroid/os/IBinder;"),
    ManagedMethod::Register<&View_requestFocus>(
        AndroidEditText::clazz, "requestFocus", "()Z"),
    ManagedMethod::Register<&View_setBackgroundColor>(
        AndroidEditText::clazz, "setBackgroundColor", "(I)V"),
    {NULL},
};

Class AndroidEditText::clazz = {
    .classpath       = "android/widget/EditText",
    .classname       = "EditText",
    .managed_methods = androidEditTextMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidEditText),
};

/* -------------------------------------------------- InputMethodManager */

static jboolean InputMethodManager_hideSoftInputFromWindow(JNIEnv *env, jobject self,
                                                           jobject token, jint flags)
{
    (void)env; (void)self; (void)token; (void)flags;
    /* Nothing was shown, so nothing was hidden. */
    return JNI_FALSE;
}

static jboolean InputMethodManager_showSoftInput(JNIEnv *env, jobject self,
                                                 jobject view, jint flags)
{
    (void)env; (void)self; (void)view; (void)flags;
    trace("showSoftInput() refused - there is no on-screen keyboard here");
    return JNI_FALSE;
}

const ManagedMethod androidInputMethodManagerMethods[] = {
    ManagedMethod::Register<&InputMethodManager_hideSoftInputFromWindow>(
        AndroidInputMethodManager::clazz, "hideSoftInputFromWindow",
        "(Landroid/os/IBinder;I)Z"),
    ManagedMethod::Register<&InputMethodManager_showSoftInput>(
        AndroidInputMethodManager::clazz, "showSoftInput",
        "(Landroid/view/View;I)Z"),
    {NULL},
};

Class AndroidInputMethodManager::clazz = {
    .classpath       = "android/view/inputmethod/InputMethodManager",
    .classname       = "InputMethodManager",
    .managed_methods = androidInputMethodManagerMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidInputMethodManager),
};

/* ------------------------------------------- the pure data placeholders */

static jobject LayoutParams_init(JNIEnv *env, jobject self, jclass clazz,
                                 jint width, jint height)
{
    (void)env; (void)clazz; (void)width; (void)height;
    return (jobject)self;
}

const ManagedMethod androidLayoutParamsMethods[] = {
    REGISTER_INIT_METHOD(AndroidLayoutParams, LayoutParams_init, "(II)V"),
    {NULL},
};

Class AndroidLayoutParams::clazz = {
    .classpath       = "android/view/ViewGroup$LayoutParams",
    .classname       = "ViewGroup$LayoutParams",
    .managed_methods = androidLayoutParamsMethods,
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidLayoutParams),
};

/*
 * ViewRoot, Window and WindowManager exist only so that the objects handed
 * back by getWindow()/getWindowManager() belong to a class. The game stores
 * them; the donor carries no method name that is looked up on any of the
 * three, so registering methods here would be inventing an API. If one turns
 * up, it announces itself as "Class Window does not have method ..." with the
 * descriptor printed, which is enough to write it.
 */
Class AndroidViewRoot::clazz = {
    .classpath       = "android/view/ViewRoot",
    .classname       = "ViewRoot",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidViewRoot),
};

Class AndroidWindow::clazz = {
    .classpath       = "android/view/Window",
    .classname       = "Window",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidWindow),
};

Class AndroidWindowManager::clazz = {
    .classpath       = "android/view/WindowManager",
    .classname       = "WindowManager",
    .managed_methods = {NULL},
    .native_methods  = {NULL},
    .fields          = {NULL},
    .instance_size   = sizeof(AndroidWindowManager),
};

static const int registered[] = {
    ClassRegistry::register_class(AndroidView::clazz),
    ClassRegistry::register_class(AndroidTextView::clazz),
    ClassRegistry::register_class(AndroidEditText::clazz),
    ClassRegistry::register_class(AndroidInputMethodManager::clazz),
    ClassRegistry::register_class(AndroidLayoutParams::clazz),
    ClassRegistry::register_class(AndroidViewRoot::clazz),
    ClassRegistry::register_class(AndroidWindow::clazz),
    ClassRegistry::register_class(AndroidWindowManager::clazz),
};
