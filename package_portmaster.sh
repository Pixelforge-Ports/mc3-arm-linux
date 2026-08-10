#!/usr/bin/env bash
#
# Build a game-data-free PortMaster package for the Modern Combat 3 port.
#
# Every check below exists because its absence already produced a broken zip
# once: a libs.armhf/ copied by hand (no MANIFEST, two libraries short of the
# transitive closure), and a launcher whose PortMaster signature no longer
# matched the release filename.
set -euo pipefail

cd "$(dirname "$0")"

OUT="build/mc3-native-arm.zip"
STAGE="build/pkg-portmaster"

[ -x build/mc3 ] \
    || { echo "build/mc3 missing - run make first" >&2; exit 1; }
# MANIFEST.txt and licenses/ only exist when tools/collect_libs.sh produced the
# directory. A hand-copied libs.armhf/ has neither, and is how the port shipped
# without libcrypto.so.1.1 and libbz2.so.1.0 (both needed by libzip.so.4).
[ -f build/libs.armhf/MANIFEST.txt ] \
    || { echo "build/libs.armhf missing or hand-made - run make libs first" >&2; exit 1; }
[ -d build/libs.armhf/licenses ] \
    || { echo "build/libs.armhf/licenses missing - run make libs again" >&2; exit 1; }

rm -rf "$STAGE"
rm -f "$OUT"
mkdir -p "$STAGE/mc3"

# BYOG: the zip ships ONLY the loader, launcher and metadata (like the sibling
# ports, a few MB). Game data is NOT bundled - a multi-GB zip stalls
# PortMaster's on-device installer, and redistributing it would be piracy. The
# user copies their own tree into ports/mc3/; the loader's
# assets-fallback (fix_path) handles the flat layout.
cp "ports/Modern Combat 3.sh"                       "$STAGE/"
cp build/mc3                              "$STAGE/mc3/"
cp ports/mc3/mc3.gptk             "$STAGE/mc3/"
cp ports/mc3/port.json                    "$STAGE/mc3/"
cp ports/mc3/gameinfo.xml                 "$STAGE/mc3/"
# The artwork PortMaster merges into the frontend's game list when it installs
# the zip. Without them the port installs correctly and then appears as a bare
# filename with no image - which is exactly how this one looked, because
# gameinfo.xml never named an image and neither file existed.
cp ports/mc3/cover.png                    "$STAGE/mc3/"
cp ports/mc3/screenshot.png               "$STAGE/mc3/"
cp ports/mc3/README.md                    "$STAGE/mc3/"
cp ports/mc3/CREDITS.md                   "$STAGE/mc3/"
cp ports/mc3/PUT_MC3_DATA_HERE.txt "$STAGE/mc3/"
cp ports/mc3/mc3.eapx.json        "$STAGE/mc3/"
cp tools/eapx.py                                  "$STAGE/mc3/"
cp -R build/libs.armhf                            "$STAGE/mc3/"

# PortMaster releases carry the copyright terms of everything they
# redistribute. collect_libs.sh already produced one file per bundled .so;
# move them next to the port's own licences so a user finds all of them in one
# place.
mkdir -p "$STAGE/mc3/licenses/libraries"
cp LICENSE   "$STAGE/mc3/licenses/LICENSE-portmaster-port.txt"
cp NOTICE.md "$STAGE/mc3/licenses/NOTICE.md"
cp portbase/third_party/gmloader/LICENSE.md "$STAGE/mc3/licenses/LICENSE-gmloader.md"
cp portbase/third_party/powervr/LICENSE.md  "$STAGE/mc3/licenses/LICENSE-powervr.txt"
# stb_truetype is not a bundled .so, so collect_libs.sh never sees it - but it is
# compiled into the loader and therefore redistributed as object code, and its
# MIT alternative requires the notice to travel with the binary.
cp portbase/third_party/stb/LICENSE.md      "$STAGE/mc3/licenses/LICENSE-stb.md"
mv "$STAGE/mc3/libs.armhf/licenses/"* "$STAGE/mc3/licenses/libraries/"
rmdir "$STAGE/mc3/libs.armhf/licenses"

chmod +x "$STAGE/Modern Combat 3.sh" "$STAGE/mc3/mc3" \
         "$STAGE/mc3/eapx.py"

# The parentheses matter: without them -delete binds to the last -name only and
# AppleDouble files copied from a macOS volume survive into the zip.
find "$STAGE" \( -name '._*' -o -name '.DS_Store' \) -delete
mkdir -p "$(dirname "$OUT")"
(cd "$STAGE" && zip -qr "../../$OUT" .)

# PortMaster rewrites an unsigned root launcher in place to add this line. Its
# implementation opens the file with mode "w" before writing, so an interrupted
# install can leave a zero-byte launcher on exFAT. Ship the canonical signature
# ourselves: with the release filename below, PortMaster recognizes it and does
# not touch the launcher after extraction.
EXPECTED_SIGNATURE="# PORTMASTER: mc3-native-arm.zip, Modern Combat 3.sh"
ACTUAL_SIGNATURE="$(unzip -p "$OUT" "Modern Combat 3.sh" | sed -n '2p')"
[ "$ACTUAL_SIGNATURE" = "$EXPECTED_SIGNATURE" ] || {
    echo "package has wrong PortMaster signature: $ACTUAL_SIGNATURE" >&2
    exit 1
}
[ "$(unzip -p "$OUT" "Modern Combat 3.sh" | wc -c | tr -d ' ')" -gt 0 ] || {
    echo "package has an empty launcher" >&2
    exit 1
}
unzip -tq "$OUT" >/dev/null

# The packaged eapx must be the canonical one. An earlier port shipped 0.2.0
# while the source tree was already at 0.4.1, because nobody compared them - the
# copy in tools/ is easy to forget and impossible to notice from the outside.
canonical="${EAPX_CANONICAL:-$HOME/Projects/Others/handheld/eapx/eapx.py}"
if [ -f "$canonical" ]; then
  if ! cmp -s tools/eapx.py "$canonical"; then
    echo "refusing package: tools/eapx.py differs from the canonical $canonical" >&2
    echo "  packaged:  $(sed -n 's/^VERSION = "\(.*\)"/\1/p' tools/eapx.py)" >&2
    echo "  canonical: $(sed -n 's/^VERSION = "\(.*\)"/\1/p' "$canonical")" >&2
    exit 1
  fi
else
  echo "note: canonical eapx not found at $canonical; packaged copy not verified" >&2
fi

listing="$(unzip -Z1 "$OUT")"
for required in "Modern Combat 3.sh" "mc3/mc3" \
                "mc3/mc3.gptk" "mc3/port.json" \
                "mc3/gameinfo.xml" "mc3/README.md" \
                "mc3/cover.png" "mc3/screenshot.png" \
                "mc3/CREDITS.md" \
                "mc3/PUT_MC3_DATA_HERE.txt" \
                "mc3/eapx.py" \
                "mc3/mc3.eapx.json" \
                "mc3/licenses/LICENSE-portmaster-port.txt" \
                "mc3/licenses/LICENSE-powervr.txt" \
                "mc3/licenses/LICENSE-stb.md" \
                "mc3/licenses/libraries/libstdc++.so.6.copyright" \
                "mc3/libs.armhf/MANIFEST.txt" \
                "mc3/libs.armhf/libcrypto.so.1.1" \
                "mc3/libs.armhf/libbz2.so.1.0"; do
    case "$listing" in
        *"$required"*) ;;
        *) echo "package missing $required" >&2; exit 1 ;;
    esac
done

# Bring your own game: nothing from the donor may ever reach the release.
case "$listing" in
    *libModernCombat3.so*|*.obb*|*com.gameloft*|*GloftM3HM*)
        echo "refusing package: proprietary game data found" >&2
        exit 1
        ;;
esac

# The zip must carry the binary that was just built, not whatever was lying in
# the staging directory. This has bitten the project twice: a package built
# from a stale stage would have shipped a port whose text never rendered, and
# nothing in the checks above would have noticed - they all pass on an old
# binary. Comparing the hashes is the only check that catches it.
built_sha="$(shasum -a 256 build/mc3 | cut -d' ' -f1)"
packed_sha="$(unzip -p "$OUT" mc3/mc3 | shasum -a 256 | cut -d' ' -f1)"
[ "$built_sha" = "$packed_sha" ] || {
    echo "refusing package: the zipped binary is not the one just built" >&2
    echo "  built:  $built_sha" >&2
    echo "  packed: $packed_sha" >&2
    exit 1
}

echo "$OUT"
# Single source: game/port_version.h. The binary answers --version with the same
# string, but it is armhf and the packaging host is not, so read the header.
echo "port version: $(sed -n 's/^#define MC3_PORT_VERSION "\(.*\)"$/\1/p' game/port_version.h)"
echo "binary sha256: $built_sha"
du -h "$OUT"
