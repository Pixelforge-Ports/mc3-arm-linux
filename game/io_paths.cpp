/*
 * Path translation for Modern Combat 3: Fallen Nation (1.1.7g).
 *
 * See portbase/src/io_paths.h for the split between what portbase answers and
 * what only this game knows. Everything below was read out of the donor's own
 * .rodata rather than guessed - the offsets are from
 * `strings -a -t x libModernCombat3.so`, so they can be checked:
 *
 *   7c9260  /data/data/com.gameloft.android.ANMP.GloftM3HM
 *   8a5710  /sdcard/Android/data/com.gameloft.android.ANMP.GloftM3HM/files/
 *   7c92b0  /sdcard/Android/obb/com.gameloft.android.ANMP.GloftM3HM
 *   8a5760  /data/data/com.gameloft.android.ANMP.GloftM3HM/tracklog.dat
 *   8a579c  /data/data/com.gameloft.android.ANMP.GloftM3HM/SaveGame.bin
 *   8a57d8  /data/data/com.gameloft.android.ANMP.GloftM3HM/CheckPoint.bin
 *
 * The game splits its filesystem in two and this port has to keep the split,
 * because the two halves have different permissions on a PortMaster install:
 *
 *   /data/data/...  and  /sdcard/Android/data/...  are where it WRITES - the
 *   save, the checkpoint, the analytics log, its preferences. The game tree is
 *   the player's own copy and can sit on a read-only mount, so these go to
 *   io_writable_dir().
 *
 *   /sdcard/Android/obb/...  is where it READS the two expansion files from.
 *   Those are the game data; they go to io_game_dir().
 *
 * Collapsing both onto the game directory is the mistake that is easy to make
 * here and hard to see: it works on a writable card and fails on a read-only
 * one, at the first save, long after the port looks healthy.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "io_paths.h"
#include "trace.h"

#include "mc3.h"

/* The package, in the two spellings the binary uses. */
static const char kDataData[]   = "/data/data/com.gameloft.android.ANMP.GloftM3HM";
static const char kExtFiles[]   = "/sdcard/Android/data/com.gameloft.android.ANMP.GloftM3HM";
static const char kObbDir[]     = "/sdcard/Android/obb/com.gameloft.android.ANMP.GloftM3HM";

/*
 * Re-root `path` at `root`, keeping whatever followed `prefix`.
 *
 * The tail can be empty (the game asks for the bare directory when it builds
 * its own paths by concatenation) and it can start with or without a slash, so
 * the separator is normalised here rather than at each call site.
 */
static const char *reroot(const char *path, const char *prefix, const char *root,
                          char *buf, size_t bufsz)
{
    const char *tail = path + strlen(prefix);
    while (*tail == '/')
        tail++;

    if (*tail)
        snprintf(buf, bufsz, "%s/%s", root, tail);
    else
        snprintf(buf, bufsz, "%s", root);

    return buf;
}

const char *port_fix_path(const char *orig, char *buf, size_t bufsz)
{
    if (!orig || !*orig)
        return NULL;

    /*
     * The engine dots its own absolute paths, so undot them before matching.
     *
     * Several of its file helpers build the name they open as `sprintf(".%s",
     * path)`. Whatever they were given comes back with a "." glued to the
     * front, so the two obb names this port answers with -
     *
     *     getZipName()          -> /<game>/main.1120....obb
     *     getPatchFileLocation() -> /<game>/patch.11428....obb
     *
     * reach the libc thunks as "./<game>/main.1120....obb", which is a
     * *relative* path. portbase's generic fallback then resolves it against the
     * game directory and looks for '/<game>/./<game>/main.1120....obb'. The
     * engine retries the undotted name afterwards for some files, which is why
     * contextData.dump0 eventually opened and the expansion files never did -
     * the retry is not universal, and the difference is invisible in a log that
     * shows both spellings failing.
     *
     * A leading "./" in front of what is otherwise an absolute path can only
     * have come from that concatenation: a genuine relative name does not begin
     * with a slash after the dot. Dropping the dot is therefore safe, and it
     * makes the Android spellings below match their dotted forms too.
     */
    bool undotted_here = false;

    if (orig[0] == '.' && orig[1] == '/') {
        const char *undotted = orig + 1;
        const char *roots[] = {
            io_game_dir(), io_writable_dir(), kObbDir, kExtFiles, kDataData,
        };

        for (unsigned i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
            size_t n = roots[i] ? strlen(roots[i]) : 0;

            /*
             * Only when the tail really is one of the roots this port knows.
             * "./data/Res.array" is a relative name the engine means
             * literally - it lives inside the expansion files - and undotting
             * it would send the open to the host's own /data.
             */
            if (n && strncmp(undotted, roots[i], n) == 0 &&
                (undotted[n] == '/' || undotted[n] == '\0')) {
                orig = undotted;
                undotted_here = true;
                break;
            }
        }
    }

    /*
     * Order matters: the obb directory and the external files directory both
     * begin "/sdcard/Android/", and only the longer match is the right one.
     * Testing the obb path first is what keeps an expansion file from being
     * looked for in the save directory.
     */
    if (strncmp(orig, kObbDir, sizeof(kObbDir) - 1) == 0)
        return reroot(orig, kObbDir, io_game_dir(), buf, bufsz);

    if (strncmp(orig, kExtFiles, sizeof(kExtFiles) - 1) == 0)
        return reroot(orig, kExtFiles, io_writable_dir(), buf, bufsz);

    if (strncmp(orig, kDataData, sizeof(kDataData) - 1) == 0)
        return reroot(orig, kDataData, io_writable_dir(), buf, bufsz);

    /*
     * An undotted host path is already the answer.
     *
     * The Android prefixes above did not match because there is nothing left to
     * translate: the name came out of getZipName()/getPatchFileLocation(),
     * which this port answers with real paths into the player's tree. Returning
     * NULL here would hand portbase the *dotted* original - the buffer is the
     * only way back - and it would resolve it against the game directory again.
     */
    if (undotted_here) {
        snprintf(buf, bufsz, "%s", orig);
        return buf;
    }

    /*
     * No rule. NULL, not `orig`: returning the original claims the name is
     * already correct and skips portbase's generic fallbacks - the bare
     * relative names this engine also uses would then be resolved against the
     * loader's working directory instead of the game tree.
     */
    return NULL;
}

/*
 * The engine writes into a directory it never creates.
 *
 * On Android that directory is the app's own external files dir, and the
 * framework has already made it by the time any native code runs. Here nothing
 * has, so the very first thing the engine does on frame one -
 *
 *     fopen("/sdcard/Android/data/.../files//contextData.dump0", <create mode>)
 *
 * - fails with ENOENT, and the engine does not check: it goes straight on to
 * seek on the null stream. The crash lands inside portbase's FILE* thunk, four
 * frames away from anything that names a directory, and reads as a loader bug.
 *
 * portbase creates the writable root and stops there, which is right - it
 * cannot know which subdirectories an engine expects to find already made.
 * This one is ours: `files/` is the only subdirectory of the writable area
 * this build opens for writing (the saves, the checkpoint and the analytics
 * log all sit flat in the root, see the string offsets at the top of the file).
 */
void mc3_prepare_writable_storage(void)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/files", io_writable_dir());

    if (mkdir(path, 0777) == 0)
        trace("writable storage: created %s", path);
}

/*
 * This build never opens an Android platform font.
 *
 * `grep` over the donor's strings finds no "/system/fonts" and no .ttf name at
 * all - the only ".ttf" in the binary (at 7c356c) is a bare extension used to
 * build names for the game's own fonts, which live in the expansion files.
 * Answering with something anyway would redirect a name the engine never asks
 * for, so the honest answer is that there is nothing to substitute.
 */
const char *port_system_font(void)
{
    return NULL;
}
