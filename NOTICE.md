# Third-party code and licensing

This port is released under **GPL-3.0** (see `LICENSE`). It carries code from
several upstream projects; this file records where each part came from, because
the licences differ and the originals must keep their attribution.

The Modern Combat 3 PortMaster port, its project direction and the `eapx`
extraction tool were created by **EapRules**.

## Where the code comes from

| Part | Origin | Licence |
|---|---|---|
| `portbase/loader/` — bionic ELF loader, relocations, symbol hooking | [gmloader-next](https://github.com/JohnnyonFlame/gmloader-next) by JohnnyonFlame, itself derived from the Vita so-loader by **Andy Nguyen** | GPL (see note) |
| `portbase/thunks/libc/` — bionic→glibc thunks | gmloader-next | GPL |
| `portbase/thunks/libc/time64.cpp` | `y2038` by **Michael G Schwern** | MIT / Artistic |
| `portbase/thunks/libc/fortify.cpp` | The Android Open Source Project (parts © Regents of the University of California) | Apache-2.0 / BSD |
| `portbase/jni/jni.h` | The Android Open Source Project | Apache-2.0 |
| `portbase/loader/leb128.h` | Free Software Foundation (binutils) | GPL |
| `portbase/thunks/khronos/` | glad generator, Khronos headers | MIT / Apache-2.0 |
| `portbase/third_party/powervr/PVRTDecompress.*` | [PowerVR SDK](https://github.com/powervr-graphics/Native_SDK) by Imagination Technologies | MIT |
| `portbase/src/vfp_vector_patch.cpp` register decoding/scalar emission | adapted from VFPVector by **Bythos14** | MIT |
| DRM patch offsets and control-scheme reference | [mc3-vita](https://github.com/v-atamanenko/mc3-vita) by **v-atamanenko** | MIT |
| SWP/SWPB emulation in the SIGILL handler | written for this port; the technique follows the Vita so-loader lineage | GPL-3.0 |
| `tools/eapx.py` — transactional first-boot donor extractor | written by **EapRules** | GPL-3.0 |
| `game/`, `harness/`, `ports/` | written for this port | GPL-3.0 |

The game itself — `libModernCombat3.so`, both OBB containers and every asset —
is © Gameloft and is **never** distributed with this port: the user supplies
their own legitimately obtained copy (BYOG).
