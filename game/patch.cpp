/*
 * Engine patches for Modern Combat 3: Fallen Nation (1.1.7g).
 *
 * There are no instruction patches in this port. The one thing the engine has
 * to be told is a data fact - that the copy protection is satisfied - and that
 * is done by writing three globals, not by rewriting code.
 *
 * Every offset here belongs to the 1.1.7g build whose libModernCombat3.so has
 * sha1 be0d5e8779899e081a538b3930ec711d2df8aeb4. They come from the Vita port's
 * main.c (v-atamanenko, MIT), which drives this exact build.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "platform.h"
#include "so_util.h"
#include "trace.h"
#include "patch.h"
#include "mc3.h"

/*
 * The GDRM unlock.
 *
 * Gameloft's licence check reads three pointers out of .data and dereferences
 * each one; a non-zero word behind every pointer is what it accepts as "this
 * copy is licensed". On Android those are filled in by the Java installer
 * (com/gameloft/android/ANMP/GloftM3HM/installer/GDRMPolicy, which talks to a
 * server that was retired years ago), so on any port they stay null and the
 * first dereference is a segfault long before the title screen.
 *
 * The pointers themselves are what the engine holds, so each one needs storage
 * of its own - writing 1 *into the pointer slot* would make the check
 * dereference the address 0x1. Four bytes each, never freed: they outlive
 * everything in this process by design.
 */
static const struct {
    uint32_t    offset;
    const char *what;
} kDrmSlots[] = {
    {0x0098a278, "GDRM lock word 1"},
    {0x0098a26c, "GDRM lock word 2"},
    {0x0098a27c, "GDRM lock word 3"},
};

/*
 * Why this is not called from so_after_relocate().
 *
 * That hook is the right place for instruction patches - the module is
 * writable and no guest code has run. These three are different: they are data
 * the engine's own start-up writes to. JNI_OnLoad and the 1068 bytes of
 * INIT_ARRAY that run before it are guest code touching guest data, and
 * anything this hook wrote into .data beforehand is theirs to overwrite.
 *
 * The reference port sets them immediately *after* JNI_OnLoad returns
 * (vita-ref main.c, lines 44-55) and that ordering is not incidental - it is
 * the first moment the engine's own initialisation is finished with those
 * words. main.cpp calls this there.
 */
void mc3_unlock_drm(so_module *mod)
{
    for (unsigned i = 0; i < sizeof(kDrmSlots) / sizeof(kDrmSlots[0]); i++) {
        int **slot = (int **)(mod->text_base + kDrmSlots[i].offset);

        int *word = (int *)malloc(sizeof(int));
        if (!word) {
            warning("GDRM unlock: out of memory for %s\n", kDrmSlots[i].what);
            return;
        }

        *word = 1;
        *slot = word;

        trace("GDRM: +0x%08x %s -> %p = 1", kDrmSlots[i].offset,
              kDrmSlots[i].what, (void *)word);
    }
}

/* ------------------------------------------------------------ sysconf */

/*
 * bionic and glibc do not agree on what the _SC_* numbers mean.
 *
 * They are not a standardised ABI - only the names are - and the two libcs
 * assigned them in different orders. The game asks for bionic's numbers
 * because it was compiled against bionic's headers; portbase binds sysconf
 * straight through to the host's; so the host answers a different question and
 * the answer looks valid.
 *
 * This game's allocator makes it audible. It calls sysconf(_SC_PAGE_SIZE) -
 * 40 under bionic - and asserts the result is a power of two:
 *
 *     70cefc  bl   sysconf@plt          ; r0 = 40
 *     70cf04  sub  r0, r0, #1
 *     70cf08  ands r0, r0, r3
 *     70cf0c  bne  70cf58               ; -> abort()
 *
 * Under glibc 40 is _SC_COLL_WEIGHTS_MAX, which answers 255. 255 is not a
 * power of two, so the allocator aborts before the first frame - and the abort
 * is the engine's own, with nothing in the log naming sysconf.
 *
 * Only the constants this game asks for are translated. An unmapped one
 * answers -1 and says so, which is the honest reply: bionic's own sysconf
 * returns -1 for values it does not know, the engine already has to handle it,
 * and it is a great deal better than passing a number through to a host that
 * will confidently answer something unrelated.
 */
static long bionic_sysconf(int name)
{
    switch (name) {
    case 0:   return sysconf(_SC_ARG_MAX);           /* _SC_ARG_MAX          */
    case 5:   return sysconf(_SC_CHILD_MAX);         /* _SC_CHILD_MAX        */
    case 6:   return sysconf(_SC_CLK_TCK);           /* _SC_CLK_TCK          */
    case 10:  return sysconf(_SC_NGROUPS_MAX);       /* _SC_NGROUPS_MAX      */
    case 11:  return sysconf(_SC_OPEN_MAX);          /* _SC_OPEN_MAX         */
    case 25:  return sysconf(_SC_VERSION);           /* _SC_VERSION          */
    case 27:  return sysconf(_SC_STREAM_MAX);        /* _SC_STREAM_MAX       */
    case 28:  return sysconf(_SC_TZNAME_MAX);        /* _SC_TZNAME_MAX       */
    case 37:  return sysconf(_SC_ATEXIT_MAX);        /* _SC_ATEXIT_MAX       */
    case 38:  return sysconf(_SC_IOV_MAX);           /* _SC_IOV_MAX          */
    case 39:                                         /* _SC_PAGESIZE         */
    case 40:  return sysconf(_SC_PAGESIZE);          /* _SC_PAGE_SIZE        */
    case 96:  return sysconf(_SC_NPROCESSORS_CONF);  /* _SC_NPROCESSORS_CONF */
    case 97:  return sysconf(_SC_NPROCESSORS_ONLN);  /* _SC_NPROCESSORS_ONLN */
    case 98:  return sysconf(_SC_PHYS_PAGES);        /* _SC_PHYS_PAGES       */
    case 99:  return sysconf(_SC_AVPHYS_PAGES);      /* _SC_AVPHYS_PAGES     */
    default:
        trace("sysconf(%d): no bionic->glibc mapping, answering -1", name);
        return -1;
    }
}

/*
 * Rebind an import by name, without knowing where its PLT entry is.
 *
 * The alternative was hook_address() on the sysconf@plt stub at +0x0007bedc,
 * which works and is one line - and pins this port to one build of one game
 * for a fact that has nothing to do with either. Walking the relocations finds
 * the same slot from the module's own tables, so the patch survives a rebuild
 * of the donor and reads as what it is.
 *
 * so_after_relocate() is the only correct moment: the imports are bound, and
 * the 267 static initialisers that could call sysconf have not run yet.
 */
static int rebind_import(so_module *mod, const char *symbol, uintptr_t fn)
{
    int patched = 0;

    for (int i = 0; i < mod->num_relplt; i++) {
        Elf_Rel  *rel = &mod->relplt[i];
        Elf_Sym  *sym = &mod->dynsym[ELF_R_SYM(rel->r_info)];
        int       type = ELF_R_TYPE(rel->r_info);

        if (type != R_ARM_JUMP_SLOT && type != R_ARM_GLOB_DAT)
            continue;
        if (sym->st_shndx != SHN_UNDEF)
            continue;
        if (strcmp(mod->dynstr + sym->st_name, symbol) != 0)
            continue;

        *(uintptr_t *)(mod->base + rel->r_offset) = fn;
        patched++;
    }

    return patched;
}

/*
 * The one thing rewritten before the engine starts.
 *
 * Nothing else in this build needs an instruction patch: the copy protection
 * is data, and mc3_unlock_drm() writes it after JNI_OnLoad for the reason
 * above.
 */
void so_patch_binary(so_module *mod)
{
    int n = rebind_import(mod, "sysconf", (uintptr_t)&bionic_sysconf);
    if (n > 0)
        trace("sysconf: %d import slot(s) rebound to the bionic constant table", n);
    else
        warning("sysconf import not found - if the engine aborts inside its "
                "allocator, this is why\n");
}
