#!/bin/sh
#
# verify-disk-assets.sh - check a directory of hd1k_*.img release candidates
# before they are handed to the NSIS or MSIX packager.
#
# Nothing else under packaging/ looks inside the images.  build-nsis.ps1 and
# build-msix.ps1 copy whatever is sitting in bin\Release\disks into the
# installer, and packaging/nsis/z80cpmw.nsi names hd1k_combo.img and
# hd1k_games.img by hand, so a stale image ships silently: the disks are build
# output, they are not tracked in this repository, and no build target puts
# r8.com or w8.com on them.  Editing romwbw_emu/src/w8.asm changes nothing that
# any release candidate holds.
#
# This is the downstream half of romwbw_emu/disks/verify_disk_utils.sh.  That
# script checks the two images that repository tracks, in place, at a tree root.
# This one is pointed at a staging directory of release candidates, wherever the
# release is being cut from, and adds the one check a byte comparison against
# the source cannot make on its own - see "the probe" below.
#
# Usage: packaging/scripts/verify-disk-assets.sh <dir-of-images> [romwbw_emu-root]
#
#   <dir-of-images>    e.g. bin/Release/disks - every hd1k_*.img in it is checked
#   [romwbw_emu-root]  defaults to the sibling checkout beside this repository
#
# Exit: 0  every image checked out completely
#       1  something is wrong with an image - stale binary, missing utility,
#          unarmed W8, or a directory that does not read as CP/M
#       2  the check could not be completed (missing tools, no images, an image
#          of a geometry this script has no diskdef for).  Not a pass: a release
#          gate that cannot run has not said yes.
#
# Needs cpmtools (cpmls, cpmcp) for the extraction and um80 + ul80 (pip install
# um80) for the source comparison.
#
#=============================================================================
# The probe
#=============================================================================
#
# W8 gained "W8 <cpmname> [hostpath]" in romwbw_emu 98eb6a1, and then in a4d3db8
# gained an interlock: before it will hand a host path to the emulator it asks
# HBF_HOST_CAPS (0xE9) whether that emulator promises not to use the path
# destructively, and refuses if the answer is no or if the call does not exist.
# That interlock is why an old emulator cannot be talked into deleting a user's
# disk library by a new W8.
#
# The usage string does NOT discriminate the two.  A w8.com built between those
# two commits prints "Usage: W8 <cpmname> [hostpath]" and issues no probe at
# all: it takes host paths and asks nobody.  Grepping the image for the usage
# text - the obvious check, and the one used by hand before this script - passes
# an image carrying exactly the binary the interlock was written to replace.
#
# So the probe is checked as machine code.  In w8.asm:
#
#     ld    b,H_CAPS      ; 06 E9
#     rst   8             ; CF
#
# Three bytes, 06 E9 CF, at a byte boundary in the extracted w8.com.  Their
# presence is the difference between an armed W8 and an unarmed one.
#
#=============================================================================
# The diskdef
#=============================================================================
#
# Per image, and getting it wrong does not fail: cpmls with the wrong diskdef
# prints an empty or blank-filled directory and exits 0, which reads as "the
# utility is not on this image".  hd1k_infocom.img was once recorded upstream as
# carrying no w8.com exactly that way.  So the diskdef is chosen from the
# image's own geometry and then the listing is checked for being a CP/M
# directory at all before any name is looked up in it:
#
#   hd1k_combo.img and anything larger than 8 MB - a 1 MB MBR prefix followed by
#   8 MB slices - is wbw_hd1k_0, and must carry the 55 AA MBR signature.
#   A plain 8 MB image is wbw_hd1k, and must not.
#
# Any other size stops the run (exit 2) rather than guessing.
#
# cpmtools reads ./diskdefs from the current directory if there is one and the
# system file otherwise, and the two are not interchangeable - a ./diskdefs
# shadows the system file completely.  romwbw_emu/disks/diskdefs carries the
# whole wbw_hd1k family and is the definition of record, so it is copied into
# the work directory and every cpmtools call is made from there.
#

set -u

SELF_DIR=$(cd "$(dirname "$0")" && pwd) || exit 2
REPO_ROOT=$(cd "$SELF_DIR/../.." && pwd) || exit 2

usage() {
    echo "usage: $0 <dir-of-images> [romwbw_emu-root]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
IMGDIR=$(cd "$1" 2>/dev/null && pwd) || { echo "no such directory: $1" >&2; exit 2; }
ROMWBW=${2:-$REPO_ROOT/../romwbw_emu}
ROMWBW=$(cd "$ROMWBW" 2>/dev/null && pwd) || {
    echo "no romwbw_emu checkout at ${2:-$REPO_ROOT/../romwbw_emu}" >&2
    echo "clone it beside this repository, or pass its path as the second argument" >&2
    exit 2
}

fail=0
checked=0
skipped_src=0

ok()   { printf 'ok    %-34s %s\n' "$1" "$2"; }
bad()  { printf 'FAIL  %-34s %s\n' "$1" "$2"; fail=$((fail + 1)); }
note() { printf '        %s\n' "$*"; }

for tool in cpmls cpmcp; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "cannot run: $tool is not on PATH (cpmtools)" >&2
        exit 2
    }
done

# The source comparison needs the MACRO-80 toolchain the .asm files are written
# for.  Without it the disk-resident binaries can still be checked for presence
# and for the probe, but not against the source, and the run is incomplete
# rather than clean - it exits 2 at the end and says which half did not happen.
HAVE_ASM=yes
for tool in um80 ul80; do
    command -v "$tool" >/dev/null 2>&1 || HAVE_ASM=no
done

DEFS=$ROMWBW/disks/diskdefs
[ -f "$DEFS" ] || { echo "cannot run: no $DEFS" >&2; exit 2; }

TMP=$(mktemp -d) || exit 2
trap 'rm -rf "$TMP"' EXIT
cp "$DEFS" "$TMP/diskdefs" || exit 2

# Every cpmtools call runs from $TMP, which carries the diskdefs of record, so
# image paths handed to it are absolute.
cpmtool() { ( cd "$TMP" && "$@" ); }

# Space-separated hex bytes with a leading and trailing space, so a byte string
# can be matched at a byte boundary.  Matching an unaligned hex stream would let
# "...X0 6E 9C FY..." pass as 06 E9 CF.
hexbytes() { od -An -v -tx1 "$1" | tr '\n' ' ' | tr -s ' '; }

# The 256-leading-zero-bytes tripwire from romwbw_emu/disks/verify_disk_utils.sh:
# a ul80 memory image and a bare .COM of the same program differ in every
# address constant by 0x100, so comparing one against the other reports drift
# that is not there.  Neither source has an ORG any more, so a hit here means
# one came back.
is_zero_padded() {
    [ "$(wc -c < "$1" | tr -d ' ')" -gt 256 ] || return 1
    ! head -c 256 "$1" | od -An -v -tx1 | tr -d ' \n' | grep -q '[^0]'
}

# The names cpmls prints for one user area.  Its output is a "N:" header per
# populated area, the names under it, and a blank line between areas; the blank
# lines are part of the format, not garbage.  $2 is the user area wanted.
listnames() {
    cpmtool cpmls -f "$2" "$1" 2>/dev/null | awk -v want="$3" '
        /^[0-9]+:$/ { area = substr($0, 1, length($0) - 1); next }
        /^[[:space:]]*$/ { next }
        area == want { print }
    '
}

#=============================================================================
# Build the reference binaries once
#=============================================================================
if [ "$HAVE_ASM" = yes ]; then
    for util in r8 w8; do
        src=$ROMWBW/src/$util.asm
        if [ ! -f "$src" ]; then
            echo "cannot run: no $src" >&2
            exit 2
        fi
        if ! um80 -o "$TMP/$util.rel" "$src" >"$TMP/$util.log" 2>&1 ||
           ! ul80 -o "$TMP/$util.com" "$TMP/$util.rel" >>"$TMP/$util.log" 2>&1; then
            bad "src/$util.asm" "does not build - see below"
            sed 's/^/        /' "$TMP/$util.log"
            HAVE_ASM=no
            break
        fi
    done
fi
if [ "$HAVE_ASM" != yes ]; then
    skipped_src=1
    echo "NOTE: um80/ul80 unavailable - the disk-resident binaries will be"
    echo "      checked for presence and for the W8 interlock, but NOT against"
    echo "      romwbw_emu/src/{r8,w8}.asm.  pip install um80"
    echo
fi

#=============================================================================
# Per image
#=============================================================================
images=$(find "$IMGDIR" -maxdepth 1 -name 'hd1k_*.img' | sort)
if [ -z "$images" ]; then
    echo "FAIL: no hd1k_*.img in $IMGDIR"
    echo "      Nothing was checked.  Point this at the directory the release"
    echo "      candidates are staged in (bin/Release/disks for a local build)."
    exit 2
fi

for img in $images; do
    base=$(basename "$img")
    size=$(wc -c < "$img" | tr -d ' ')
    mbr=$(dd if="$img" bs=1 skip=510 count=2 2>/dev/null | od -An -tx1 | tr -d ' \n')

    if [ "$size" -gt 8388608 ]; then
        def=wbw_hd1k_0
        if [ "$mbr" != "55aa" ]; then
            bad "$base" "is larger than 8 MB but has no MBR signature (got $mbr)"
            note "A combo image is a 1 MB MBR prefix followed by 8 MB slices."
            note "Without the prefix, slice 0 is not where wbw_hd1k_0 looks."
            continue
        fi
    elif [ "$size" -eq 8388608 ]; then
        def=wbw_hd1k
        if [ "$mbr" = "55aa" ]; then
            bad "$base" "is 8 MB but carries an MBR signature"
            note "Either it is a truncated combo image or it is partitioned;"
            note "wbw_hd1k reads slice data straight off the front and would"
            note "read the partition table as a directory."
            continue
        fi
    else
        bad "$base" "is $size bytes - no diskdef for that geometry"
        note "Expected a plain 8388608-byte hd1k image or a combo image"
        note "larger than that.  Refusing to guess a diskdef: the wrong one"
        note "prints an empty directory and exits 0."
        continue
    fi

    # Is this a CP/M directory at all, or is the diskdef wrong?
    names=$(listnames "$img" "$def" 0)
    count=$(printf '%s\n' "$names" | grep -c . )
    if [ "$count" -eq 0 ]; then
        bad "$base" "user 0 lists no files at all with diskdef $def"
        note "That is what a wrong diskdef looks like - cpmls prints an empty"
        note "or blank directory and exits 0 rather than failing."
        continue
    fi
    odd=$(printf '%s\n' "$names" | LC_ALL=C grep -c -v '^[!-~][!-~]*$')
    if [ "$odd" -ne 0 ]; then
        bad "$base" "$odd of $count names in user 0 are not CP/M filenames"
        note "A directory read with the wrong diskdef decodes as noise."
        printf '%s\n' "$names" | LC_ALL=C grep -v '^[!-~][!-~]*$' |
            head -3 | od -An -c | sed 's/^/        /'
        continue
    fi
    ok "$base" "$count files in user 0, diskdef $def"

    for util in r8 w8; do
        if ! printf '%s\n' "$names" | grep -qi "^$util\.com$"; then
            # The rest of the directory read cleanly, so this is a genuinely
            # absent file rather than a wrong diskdef.  Severity depends on the
            # image: the boot image has to carry the utilities or the feature
            # does not exist, while a secondary data disk carrying neither is a
            # choice, not drift - and an absent w8.com cannot be an unarmed one.
            # A copy that IS present is always checked, on every image.
            if [ "$def" = wbw_hd1k_0 ]; then
                bad "$base $util.com" "is not on the boot image"
                note "The boot image has to carry it: put it back with"
                note "romwbw_emu/disks/rebuild_disk_utils.sh."
            else
                note "info  $base carries no $util.com - nothing to check"
            fi
            continue
        fi

        rm -f "$TMP/from_img.com"
        if ! cpmtool cpmcp -f "$def" "$img" "0:$util.com" "$TMP/from_img.com" 2>/dev/null; then
            bad "$base $util.com" "is listed but could not be extracted"
            continue
        fi

        # The interlock, checked as machine code - see "The probe" above.
        if [ "$util" = w8 ]; then
            if hexbytes "$TMP/from_img.com" | grep -q ' 06 e9 cf '; then
                ok "$base w8.com" "probes HBF_HOST_CAPS before taking a host path"
            else
                bad "$base w8.com" "does not probe HBF_HOST_CAPS (06 e9 cf absent)"
                note "This W8 predates romwbw_emu a4d3db8.  If it also accepts a"
                note "host path it will hand one to any emulator, including the"
                note "ones that used the path destructively.  Do not ship it -"
                note "rebuild the image with romwbw_emu/disks/rebuild_disk_utils.sh."
                continue
            fi
        fi

        [ "$HAVE_ASM" = yes ] || continue

        built_padded=no; held_padded=no
        is_zero_padded "$TMP/$util.com" && built_padded=yes
        is_zero_padded "$TMP/from_img.com" && held_padded=yes
        if [ "$built_padded" != "$held_padded" ]; then
            bad "$base $util.com" "was linked in the other layout"
            note "One side has 256 leading zero bytes and the other does not, so"
            note "every address constant differs by 0x100 and cmp cannot speak."
            note "src/$util.asm must have no ORG.  built=$built_padded held=$held_padded"
            continue
        fi

        checked=$((checked + 1))
        if cmp -s "$TMP/$util.com" "$TMP/from_img.com"; then
            ok "$base $util.com" "matches romwbw_emu/src/$util.asm"
        else
            built=$(wc -c < "$TMP/$util.com" | tr -d ' ')
            held=$(wc -c < "$TMP/from_img.com" | tr -d ' ')
            bad "$base $util.com" \
                "differs from romwbw_emu/src/$util.asm (source builds $built bytes, image holds $held)"
            note "rebuild: romwbw_emu/disks/rebuild_disk_utils.sh, then restage."
        fi
    done
done

echo
if [ "$fail" -ne 0 ]; then
    echo "FAIL: $fail problem(s) in $IMGDIR - these images are not releasable"
    exit 1
fi
# Order matters: with no assembler nothing can have been compared, so the
# "nothing was compared" test below would shadow the reason for it every time.
if [ "$skipped_src" -ne 0 ]; then
    echo "INCOMPLETE: the images read cleanly and W8 is armed on all of them,"
    echo "            but nothing was compared against romwbw_emu/src - install"
    echo "            um80/ul80 and run again before cutting the release."
    exit 2
fi
if [ "$checked" -eq 0 ]; then
    echo "FAIL: nothing was actually compared, though images were found"
    exit 2
fi
echo "PASS: $checked disk-resident binaries match romwbw_emu/src, and every"
echo "      W8 probes HBF_HOST_CAPS before accepting a host path"
exit 0
