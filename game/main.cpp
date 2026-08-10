/*
 * Modern Combat 3: Fallen Nation - entry point.
 *
 * This game is driven, not hosted. It declares no libandroid.so, exports no
 * ANativeActivity_onCreate, and has no frame loop of its own: what it exports
 * is JNI_OnLoad plus 63 Java_* entry points, and on Android a Java layer calls
 * them one by one. Here that Java layer is this file.
 *
 *   $ readelf -d libModernCombat3.so | grep NEEDED
 *   liblog, libGLESv2, libz, libstdc++, libm, libc, libdl
 *
 * No libandroid, and GLESv2 rather than GLESv1_CM - this is a shader-based
 * renderer with no fixed-function path at all.
 *
 * The call order below is transcribed from the Vita port's main.c
 * (v-atamanenko, MIT), which runs this exact build. It is not deducible from
 * the symbol names, and two of its choices are worth naming because the
 * obvious alternative is wrong:
 *
 *   - GL2JNILib.init comes before the game's own Init, and both come before
 *     nativeMC3Init. The names suggest the opposite grouping.
 *   - getViewSettings is called for its side effects and its answer thrown
 *     away, then resize is called with the real panel size. Skipping the first
 *     leaves the renderer without the settings the second one applies.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "so_util.h"
#include "khronos/gles2.h"

#include "jni.h"

#include "app_exit.h"
#include "crash.h"
#include "cursor_draw.h"
#include "emulator_control.h"
#include "fb_probe.h"
#include "fix_path.h"
#include "input_bridge.h"
#include "patch.h"
#include "port_env.h"
#include "trace.h"

#include "gl_probe.h"
#include "mc3.h"
#include "mc3_classes.h"
#include "port_version.h"
#include "sdl_info.h"

/* portbase's activity counters, declared where they are used rather than in a
 * header: three longs whose only consumer is the summary line at the end. */
extern "C" long android_io_assets_opened(void);
extern "C" long android_gl_textures_uploaded(void);
extern "C" long android_gl_draw_calls(void);
/* portbase's viewport scaler (src/symtab_glprobe.cpp); it exports no header. */
extern "C" void viewport_scale_init(int phys_w, int phys_h);

/*
 * The library, and where it sits in the player's tree.
 *
 * "armeabi-v7a", unlike the sibling Mass Effect port: this is a 2013 build and
 * the v7a split was routine by then, so the loader's own search path - which
 * derives the ABI directory from the host architecture - already finds it.
 */
static const char *kNativeLib    = "libModernCombat3.so";
static const char *kNativeLibDir = "lib/armeabi-v7a";

/* The R36S panel. The engine takes its resolution from the resize call below
 * and this port never changes it afterwards. */
static const int kWidth  = 640;
static const int kHeight = 480;

/* ---------------------------------------------------------- the module */

static so_module *g_module;

so_module *mc3_module(void) { return g_module; }

/*
 * port_guest_module - one of the ten. Deliberately C++ linkage; portbase's
 * pthread thunks resolve the game's thread callbacks through it.
 */
so_module *port_guest_module(void) { return g_module; }

/*
 * Every import nothing answers, named up front.
 *
 * The loader points unresolved jump slots at a stub that aborts on first use,
 * which surfaces one missing symbol per run, from inside a crash, with no
 * stack. Walking the table here lists all of them at once - and it is the fact
 * the harness reads to decide M2.
 *
 * Undefined *weak* symbols are not failures: resolving them to zero is what a
 * real dynamic linker does and the game tests them before use.
 */
static int report_unresolved_symbols(so_module *mod)
{
    int missing = 0;

    for (int i = 0; i < mod->num_dynsym; i++) {
        Elf_Sym *sym = &mod->dynsym[i];
        if (sym->st_shndx != SHN_UNDEF)
            continue;

        const char *name = mod->dynstr + sym->st_name;
        if (!name || !*name)
            continue;

        if (so_resolve_link(mod, name))
            continue;

        if (ELF32_ST_BIND(sym->st_info) == STB_WEAK) {
            trace("weak import left null: %s", name);
            continue;
        }

        fprintf(stderr, "unresolved symbol: %s\n", name);
        missing++;
    }

    fflush(stderr);
    return missing;
}

extern "C" int so_after_relocate(so_module *mod)
{
    g_module = mod;
    trace("module loaded");

    /*
     * Arm the fault handler here rather than after so_load_module() returns.
     * The 1068 bytes of INIT_ARRAY this module declares run immediately after
     * this hook; a fault in one of them without a handler installed is
     * reported by qemu as a bare "uncaught target signal 11" with no pc and no
     * stack - the whole first half of the boot as a blind spot, exactly where
     * the engine's own code starts running.
     */
    crash_report_init(mod, kNativeLib);

    int missing = report_unresolved_symbols(mod);
    if (missing == 0) {
        so_patch_binary(mod);
        return 0;
    }

    fatal("%d import(s) of %s have no implementation (listed above).\n"
          "       Running the game now would fault on the first call to any of\n"
          "       them, from inside a static constructor, with nothing but an\n"
          "       address to go on.",
          missing, mod->soname ? mod->soname : "the module");
    return 1;
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    /* Unbuffered from the first line: the log is the only diagnostic that
     * leaves the console, and a crash must not take it with it. */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /*
     * The launcher's GL provider preflight, before anything that expects a
     * game directory: these modes load one library and exit.
     *
     * The launcher searches the device's 32-bit library directories for a GL
     * stack and a file that exists is not a driver that works, so each
     * candidate is dlopen()ed by this binary - the one 32-bit process the port
     * is guaranteed to have - before the launcher commits to it. See
     * game/gl_probe.h.
     */
    if (argc >= 2 && strcmp(argv[1], "--gl-probe") == 0)
        return gl_probe_main(argc - 2, argv + 2);
    if (argc >= 3 && strcmp(argv[1], "--gl-probe-init") == 0)
        return gl_probe_init(argv[2], gl_probe_report_stdout, NULL);
    if (argc >= 3 && strcmp(argv[1], "--gl-probe-deps") == 0)
        return gl_probe_deps(argv[2], gl_probe_report_stdout, NULL);

    /* The same idea one layer up: the launcher has to pick a video backend for
     * SDL, and only SDL knows which ones it was built with. */
    if (argc >= 2 && strcmp(argv[1], "--sdl-info") == 0)
        return sdl_info_main();

    /* The launcher asks the binary for the version rather than carrying its own
     * copy, so the two can never disagree. Plain stdout, not trace(): the caller
     * is a shell substitution, and it runs before LOADER_TRACE means anything. */
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", MC3_PORT_VERSION);
        return 0;
    }

    /* First line of every run: a log that does not name its build cannot be
     * told apart from a log produced by the build before it. */
    trace("Modern Combat 3 port v%s (bring-up)", MC3_PORT_VERSION);

    if (argc < 2) {
        fprintf(stderr,
                "usage: %s <modern-combat-3-directory>\n"
                "\n"
                "The directory is your own copy of the game: the extracted tree\n"
                "with lib/armeabi-v7a/ in it and the two .obb expansion files.\n"
                "It is never bundled with this port.\n",
                argv[0]);
        return 2;
    }

    const char *game_dir = argv[1];

    /* Before anything of the game's runs: the libc path thunks translate
     * against this, and the engine opens its first file from inside a static
     * initialiser. */
    io_set_game_dir(game_dir);
    mc3_prepare_writable_storage();

    char lib_dir[PATH_MAX];
    char lib_path[PATH_MAX];
    snprintf(lib_dir,  sizeof(lib_dir),  "%s/%s", game_dir, kNativeLibDir);
    snprintf(lib_path, sizeof(lib_path), "%s/%s", lib_dir, kNativeLib);

    struct stat st;
    if (stat(lib_path, &st) != 0) {
        fatal("'%s' does not exist.\n"
              "       This does not look like an extracted Modern Combat 3 tree.",
              lib_path);
        return 1;
    }
    trace("native library found: %s (%lld bytes)", lib_path, (long long)st.st_size);

    /*
     * GL first, before the module is linked.
     *
     * The game's 100 gl* imports are bound by asking the driver for each entry
     * point, and there is no driver to ask until a context is current.
     * Relocating first would bind every one of them to null.
     *
     * A failure here is deliberately not fatal: "does every import resolve?"
     * and "is there a usable GLES context?" are different questions, and
     * aborting on the second makes the first unanswerable from the log.
     */
    SDL_Window   *window = NULL;
    SDL_GLContext gl     = NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        trace("SDL_Init(video+gamecontroller) failed: %s", SDL_GetError());
    } else {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        window = SDL_CreateWindow("Modern Combat 3",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  kWidth, kHeight, SDL_WINDOW_OPENGL);
        if (!window)
            trace("SDL_CreateWindow failed: %s", SDL_GetError());
        else if (!(gl = SDL_GL_CreateContext(window)))
            trace("no GLES 2.0 context: %s", SDL_GetError());
    }

    if (gl) {
        load_gles2_funcs();

        /*
         * The GLES1 table has to be filled too, and not because this game
         * imports GLES1. It imports none - not one entry point.
         *
         * portbase interposes six calls to count and diagnose them
         * (glViewport, glScissor, glClear, glTexImage2D, glDrawArrays,
         * glDrawElements) and every one of those wrappers resolves its real
         * entry point through find_gles1_function() alone - no fallback to
         * symtable_gles2, no SDL_GL_GetProcAddress, unlike resolve_gl_entry()
         * thirty lines above them in src/symtab_glprobe.cpp. With the GLES1
         * table empty all six are counted and then discarded:
         *
         *     GLSTATS: *** DROPPED glDrawElements - not forwarded to the
         *     driver (symtable_gles1 has no glDrawElements) ***
         *
         * That was 23484 draw calls and 738 texture uploads that never reached
         * the driver, and a framebuffer black for a reason no GL state could
         * explain. Filling the table is what makes the six resolvable.
         *
         * GL_SINGLE_DISPATCH is what makes filling it safe. It is portbase's
         * own switch, and it forces every GLES1-table name through
         * SDL_GL_GetProcAddress instead of dlsym on the GLESv1_CM provider, so
         * the six land in the same dispatch as the GLES2 context SDL gave us.
         * Without it the emulator can bind them in desktop GL while SDL uses
         * GLES2, two ABIs that share no state; on the device libmali exports
         * EGL, GLESv1_CM and GLESv2 from one blob and both routes are the same
         * pointer. Set as a default rather than an override - an explicit
         * MC3_GL_SINGLE_DISPATCH still wins, so the split stays reproducible.
         *
         * The portbase side of this is fixed: the six wrappers now resolve
         * through resolve_gl_entry(), which falls back to symtable_gles2 and
         * SDL, so a GLES2 port that never fills the GLES1 table still draws.
         * Filling the table and unifying its dispatch is kept here anyway,
         * because it makes the emulator bind the same pointers the device
         * does - the split it prevents is an emulator artifact, and an
         * explicit MC3_GL_SINGLE_DISPATCH still wins for A/B runs.
         */
        setenv("MC3_GL_SINGLE_DISPATCH", "1", 0);
        load_gles1_funcs();

        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(window, &w, &h);
        trace("drawable %dx%d", w, h);

        /*
         * Arm the viewport scaler with the panel it just measured. The
         * launcher has exported MC3_SCALE since 1.0.0 and the machinery in
         * portbase remapped nothing, because this call was missing: on any
         * panel that is not 640x480 the game rendered into the bottom-left
         * corner (reported on an R46H, root-caused with a patched binary on
         * an RG34XXSP by Codebr0ken - the fix is theirs).
         */
        viewport_scale_init(w, h);

        const GLubyte *(*get_string)(GLenum) =
            (const GLubyte *(*)(GLenum))SDL_GL_GetProcAddress("glGetString");
        if (get_string) {
            const char *ver = (const char *)get_string(0x1F02 /* GL_VERSION  */);
            const char *rnd = (const char *)get_string(0x1F01 /* GL_RENDERER */);
            trace("GL_VERSION=%s | GL_RENDERER=%s", ver ? ver : "?", rnd ? rnd : "?");
        }
    }

    /* The fake JavaVM has to exist before the module is linked: the engine's
     * static initialisers run during so_load_module() and reach for it. */
    JavaVM *vm  = NULL;
    JNIEnv *env = NULL;
    if (JNI_CreateJavaVM(&vm, &env, NULL) != JNI_OK || !vm || !env) {
        fatal("could not create the JNI environment.");
        return 1;
    }

    /*
     * Map, relocate and link.
     *
     * The VM argument is NULL on purpose and it is not an oversight:
     * so_load_module() calls JNI_OnLoad itself when a module exports one, and
     * for this game that would run it in the wrong place. The working boot
     * sequence puts JNI_OnLoad first on the game thread, ahead of everything
     * below - passing NULL here leaves that call to us.
     */
    so_set_options(NULL, lib_dir);

    so_module *mod = so_load_module(kNativeLib, NULL, NULL);
    if (!mod) {
        fatal("could not load '%s' from '%s'.", kNativeLib, lib_dir);
        fflush(NULL);
        _exit(1);
    }

    trace("so_load_module returned (text_base=%p size=%zu)",
          (void *)mod->text_base, (size_t)mod->text_size);

    /* ---------------------------------------------------------------- *
     * The boot sequence, in the reference port's order.
     * ---------------------------------------------------------------- */

    auto JNI_OnLoad = (int (*)(JavaVM *, void *))so_symbol(mod, "JNI_OnLoad");
    auto GL2JNILib_init = (void (*)(JNIEnv *, jclass))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_init");
    auto InAppBilling_nativeInit = (void (*)(JNIEnv *, jclass, jobject))
        so_symbol(mod, "Java_com_gameloft_android_ANMP_GloftM3HM_iab_InAppBilling_nativeInit");
    auto MyVideoView_Init = (void (*)(JNIEnv *))
        so_symbol(mod, "Java_video_MyVideoView_Init");
    auto Game_Init = (void (*)(JNIEnv *, jobject))
        so_symbol(mod, "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_Init");
    auto Game_nativeMC3Init = (void (*)(JNIEnv *, jclass))
        so_symbol(mod, "Java_com_gameloft_android_ANMP_GloftM3HM_GloftM3HM_nativeMC3Init");
    auto GL2JNILib_getViewSettings = (int (*)(void))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_getViewSettings");
    auto GL2JNILib_resize = (void (*)(JNIEnv *, jclass, int, int))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_resize");
    auto GL2JNILib_step = (void (*)(void))
        so_symbol(mod, "Java_com_gameloft_glf_GL2JNILib_step");

    if (!JNI_OnLoad || !GL2JNILib_init || !Game_Init || !GL2JNILib_step) {
        fatal("%s is missing one of the entry points the port drives:\n"
              "       JNI_OnLoad=%p GL2JNILib.init=%p GloftM3HM.Init=%p step=%p",
              kNativeLib, (void *)JNI_OnLoad, (void *)GL2JNILib_init,
              (void *)Game_Init, (void *)GL2JNILib_step);
        fflush(NULL);
        _exit(1);
    }

    /*
     * Real class objects, not the reference port's 0x42424242 sentinel.
     *
     * The Vita's FalsoJNI dispatches on the *value* of a jclass, so a sentinel
     * costs it nothing. This port resolves methods through a class registry
     * and the engine's own code reads the object it is handed: with the
     * sentinel, GL2JNILib.init died immediately at
     *
     *     FATAL: [tid 0x00000007] SIGSEGV at 0x4242424a
     *
     * which is 0x42424242 + 8 - the game dereferencing the class pointer it
     * was given. The two classes below are the ones these entry points belong
     * to on Android, so they are also what a real JVM would pass.
     */
    jclass gl_class   = (jclass)&GloftGL2JNILib::clazz;

    jclass game_class = (jclass)&GloftM3HM::clazz;

    /*
     * GloftM3HM.Init takes the activity *instance*, not its class - and the
     * distinction is not a guess, it is the sixteenth instruction group of the
     * function:
     *
     *     234298  mov r1, r6                 ; r6 = the second argument
     *     2342ac  ldr pc, [r3, #124]         ; JNINativeInterface[31]
     *
     * Entry 31 is GetObjectClass, so Init calls GetObjectClass(env, arg2) and
     * caches the result - which is only meaningful if arg2 is an object. On
     * Android the method is declared non-static and arg2 is `thiz`.
     *
     * Passing &GloftM3HM::clazz there faulted inside portbase's
     * iface_GetObjectClass, which does `((Object *)jobj)->_getClass()`: a
     * virtual call whose vtable load reads the first word of a Class, and the
     * first word of a Class is its classpath pointer. The dump says exactly
     * that and nothing else:
     *
     *     SIGSEGV at 0x2f6d6f62   r3 = 0x2f6d6f63 = "com/"
     *     r0 = r1 = 0x400a196c    [r0] = 0x40085e78 (the classpath string)
     *     lr = module+0x002342b0  (the instruction after the call above)
     *
     * The faulting pc is the first four bytes of "com/gameloft/..." with the
     * Thumb bit stripped. Nothing about it is ambiguous.
     *
     * The reference port hands 0x42424242 to every one of these entry points
     * and gets away with it because FalsoJNI's GetObjectClass returns a
     * constant without dereferencing its argument. portbase resolves methods
     * through a real registry, so what an entry point is given has to be the
     * kind of thing it is about to be used as.
     */
    jobject game_thiz = (jobject)&g_activity;

    trace("-> JNI_OnLoad at +0x%08lx",
          (unsigned long)((uintptr_t)JNI_OnLoad - mod->text_base));
    JNI_OnLoad(vm, NULL);
    trace("JNI_OnLoad returned");

    /* The licence. Here and not earlier - see game/patch.cpp. */
    mc3_unlock_drm(mod);

    /* Input before the engine's own init: GL2JNILib.init is where the engine
     * builds the input state the entry points below write into. */
    android_input_init(mod, env, kWidth, kHeight);

    GL2JNILib_init(env, gl_class);
    trace("GL2JNILib.init returned");

    if (InAppBilling_nativeInit) {
        InAppBilling_nativeInit(env, game_class, (jobject)0x24242424);
        trace("InAppBilling.nativeInit returned");
    } else {
        trace("no InAppBilling.nativeInit export");
    }

    if (MyVideoView_Init) {
        MyVideoView_Init(env);
        trace("MyVideoView.Init returned");
    } else {
        trace("no MyVideoView.Init export - briefings may not skip cleanly");
    }

    /*
     * The JNIEnv, printed as three numbers, so a fault dump can be read
     * against facts instead of guesses.
     *
     * GloftM3HM.Init dies inside `ldr ip, [r8]; ldr pc, [ip, #452]` at
     * +0x0023db64 - which is env->GetStaticMethodID, entry 113 of
     * JNINativeInterface. Whether that is our function or garbage cannot be
     * decided from the dump alone: the crash handler reports pc and ip, and
     * nothing in the log says what those values should have been. These three
     * lines are the missing half of that comparison.
     */
    trace("JNIEnv=%p functions=%p GetStaticMethodID=%p (entry 113, +452)",
          (void *)env, (void *)env->functions,
          (void *)env->functions->GetStaticMethodID);

    Game_Init(env, game_thiz);
    trace("GloftM3HM.Init returned");

    if (Game_nativeMC3Init) {
        Game_nativeMC3Init(env, game_class);
        trace("GloftM3HM.nativeMC3Init returned");
    } else {
        trace("no nativeMC3Init export");
    }

    if (GL2JNILib_getViewSettings) {
        GL2JNILib_getViewSettings();
        trace("GL2JNILib.getViewSettings returned");
    }

    if (GL2JNILib_resize) {
        GL2JNILib_resize(env, gl_class, kWidth, kHeight);
        trace("GL2JNILib.resize(%d, %d) returned", kWidth, kHeight);
    }

    emulator_control_init();

    /*
     * A bounded run. <PREFIX>_FRAME_LIMIT stops the process after that many
     * frames so an automated run terminates on a fact rather than on a
     * stopwatch; unset - the normal case for a player - means run forever.
     */
    const long frame_limit = port_getenv_long("FRAME_LIMIT", 0);

    long frames = 0;
    while (frame_limit == 0 || frames < frame_limit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (!android_input_event(&ev))
                goto done;
        }

        /* The game's own exit request, read next to the event drain rather
         * than trusted to it: a full SDL queue drops the SDL_QUIT that
         * android_app_request_exit() pushes, and a dropped exit is a freeze. */
        if (android_app_exit_requested())
            goto done;

        if (!emulator_control_tick(frames))
            goto done;
        android_input_tick();
        android_input_autopilot_tick(frames);
        mc3_video_skip_tick();

        /* Entering and returning, for the first few. "frames=1" as the last
         * line cannot distinguish "the second frame never started" from "the
         * second frame never came back", and those are different bugs. */
        if (frames < 5)
            trace("-> GL2JNILib.step #%ld", frames + 1);

        GL2JNILib_step();
        frames++;

        if (frames <= 5)
            trace("<- GL2JNILib.step #%ld returned", frames);

        if (window) {
            int w = 0, h = 0;
            SDL_GL_GetDrawableSize(window, &w, &h);
            android_fb_probe(frames, w, h);
            android_input_autopilot_sample(frames);
            android_cursor_draw(w, h);
            emulator_control_after_draw(frames, w, h);
            SDL_GL_SwapWindow(window);
        }

        if (frames <= 5 || frames % 10 == 0)
            trace("frames=%ld", frames);

        /* Whether the engine is in a mission or still in a menu. "It froze"
         * and "it is drawing a screen that does not change" look the same from
         * outside, and the frame counter cannot tell them apart. */
        if (frames % 100 == 0)
            trace("activity f=%ld gameplay=%d", frames, (int)mc3_is_gameplay());
    }

done:
    emulator_control_shutdown(frames);
    trace("frames=%ld", frames);
    /* The three counters the harness reads to tell "the engine is rendering
     * its own content" apart from "the loader cleared the screen 600 times".
     * A solid-colour clear passes a pixel test on its own; assets opened and
     * textures uploaded are what a game that found its data looks like. */
    trace("summary assets=%ld textures=%ld draws=%ld",
          android_io_assets_opened(), android_gl_textures_uploaded(),
          android_gl_draw_calls());
    trace("autopilot keys=%ld scenes=%ld",
          android_input_autopilot_keys(), android_input_autopilot_scenes());
    trace("run finished: %ld frames (%s)", frames,
          android_app_exit_requested() ? "the game asked to exit"
                                       : "the loader stopped driving it");

    /*
     * The engine started threads of its own and they are still running.
     * Tearing the window and the GL context down from here would pull them out
     * from under those threads and produce a segfault that has nothing to do
     * with why the loader stopped. Leave it to the kernel.
     */
    fflush(NULL);
    _exit(0);
}
