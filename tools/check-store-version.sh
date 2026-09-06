#!/bin/sh
# check-store-version.sh - what does the Microsoft Store actually serve, and
# does anything in this tree claim otherwise?
#
# WHY THIS EXISTS.  A tick in FEATURE_PARITY.md, a version in CHANGELOG.md and a
# number in Version.h all describe the TREE.  None of them knows what a user can
# install.  ioscpm has had tools/check-store-version.sh since 2026-09-03 and it
# is the reason that port's claims are measured; this repository had no
# equivalent, so `shipped:` in FEATURE_PARITY.md's sibling-readings block was an
# assertion copied out of our own changelog.  On 2026-09-06 the first run of this
# script disagreed with that changelog, which is the whole argument for having
# it.
#
# It is the companion to check-shipped-disks.sh and check-sibling-drift.sh: those ask
# what the tree and the images say, this one asks the store.  Like them it goes
# out to the network, and like them it exits 2 rather than 0 when it cannot.
#
#   sh tools/check-store-version.sh
#
# Exit 0 = measured, and nothing recorded here claims a version the Store does
#          not serve.  The tree being AHEAD of the Store is normal - you always
#          build before you ship - and is reported, not failed.
# Exit 1 = something records a shipped state that contradicts the measurement.
# Exit 2 = could not verify (no network, no curl/wget, no such product, a
#          response shape this cannot read).  A gate that cannot verify must not
#          say yes.

set -u

# The Store product, and the identity it must carry.  The ProductId alone is not
# enough: ids get reused and mistyped, so the answer is refused unless the
# package identity is also ours.  Both were read out of the Store rather than
# derived - the publisher hash in a PackageFamilyName is a hash of the publisher
# string, and computing it wrong yields a plausible name for somebody else's app.
PRODUCT_ID="9NZN870X9P6Z"
IDENTITY_NAME="AaronWohl.Z80CPM"
CATALOG="https://displaycatalog.mp.microsoft.com/v7.0/products/$PRODUCT_ID?market=US&languages=en-us&fieldsTemplate=Details"

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here" && git rev-parse --show-toplevel 2>/dev/null) || root=$(dirname "$here")

VERSION_H="$root/z80cpmw/Version.h"
PARITY="$root/FEATURE_PARITY.md"
CHANGELOG="$root/CHANGELOG.md"

tmp=$(mktemp -d 2>/dev/null || mktemp -d -t store)
trap 'rm -rf "$tmp"' EXIT INT TERM

status=0

get() { # $1 url, $2 dest
    if command -v curl >/dev/null 2>&1; then
        curl -sSfL -m 45 -o "$2" "$1" 2>/dev/null
    elif command -v wget >/dev/null 2>&1; then
        wget -qT 45 -O "$2" "$1" 2>/dev/null
    else
        return 127
    fi
}

vnum() { # 1.0.25 -> sortable integer, four components
    echo "$1" | awk '{
        sub(/^v/, "", $0); n = split($0, a, "."); r = 0
        for (i = 1; i <= 4; i++) r = r * 10000 + (i <= n ? a[i] + 0 : 0)
        print r
    }'
}

# --- what the Store serves -----------------------------------------------------
if ! get "$CATALOG" "$tmp/dc.json"; then
    echo "CANNOT VERIFY: no network, or neither curl nor wget is installed."
    echo "This gate does not pass when it cannot check."
    exit 2
fi

# DisplayCatalog reports the shipped build inside PackageFullName:
#   AaronWohl.Z80CPM_1.0.25.0_x64__pyqcdeggzw67m
# The sibling packageManifests endpoint (what winget's msstore source reads)
# answers PackageVersion "Unknown" for Store packages, so it cannot be used here.
pfn=$(tr ',' '\n' < "$tmp/dc.json" |
      grep -o '"PackageFullName":"[^"]*"' |
      sed 's/.*:"//; s/"$//' | sort -u | head -1)

if [ -z "$pfn" ]; then
    echo "CANNOT VERIFY: no PackageFullName in the catalog response for $PRODUCT_ID."
    echo "Rate-limited, delisted, or the response shape changed."
    exit 2
fi

case "$pfn" in
    "$IDENTITY_NAME"_*) ;;
    *)
        echo "CANNOT VERIFY: $PRODUCT_ID serves '$pfn', which is not $IDENTITY_NAME."
        echo "Either the product id is wrong or it now belongs to another app."
        exit 2 ;;
esac

# AaronWohl.Z80CPM_1.0.25.0_x64__hash -> 1.0.25.0 -> 1.0.25
live_full=${pfn#"$IDENTITY_NAME"_}
live_full=${live_full%%_*}
live=$(echo "$live_full" | awk -F. '{ print $1 "." $2 "." $3 }')

title=$(tr ',' '\n' < "$tmp/dc.json" | grep -o '"ProductTitle":"[^"]*"' |
        sed 's/.*:"//; s/"$//' | head -1)
when=$(tr ',' '\n' < "$tmp/dc.json" | grep -o '"LastModifiedDate":"[^"]*"' |
       sed 's/.*:"//; s/"$//' | sort | tail -1)

echo "Microsoft Store, $PRODUCT_ID (${title:-unknown})"
echo "  serves           $live   ($pfn)"
[ -n "$when" ] && echo "  catalog updated  $(echo "$when" | cut -c1-10)"

# --- what the tree claims to be ------------------------------------------------
mkv=$(awk '/^#define VERSION_MAJOR/ {a=$3} /^#define VERSION_MINOR/ {b=$3}
           /^#define VERSION_PATCH/ {c=$3}
           END { if (a != "") printf "%s.%s.%s", a, b, c }' "$VERSION_H" 2>/dev/null)

if [ -z "$mkv" ]; then
    echo "CANNOT VERIFY: no VERSION_MAJOR/MINOR/PATCH in $VERSION_H."
    exit 2
fi
echo "  this tree        $mkv"

# --- the gap -------------------------------------------------------------------
echo
if [ "$(vnum "$mkv")" -lt "$(vnum "$live")" ] 2>/dev/null; then
    echo "TREE IS BEHIND THE STORE: Version.h $mkv < shipped $live."
    echo "  That is not a normal state.  Somebody edited it downward, or a"
    echo "  release went out from another checkout and this one never caught up."
    status=1
elif [ "$mkv" = "$live" ]; then
    echo "The tree and the Store are on the same version."
    echo "  Same number is not the same software: commits since the build that"
    echo "  version was cut from are in the tree and not in the package."
else
    echo "The tree is ahead of the Store. That is normal - you build before you ship."
    echo "  What is NOT normal is writing $mkv into anything that records what"
    echo "  USERS have.  Packaged is not submitted and submitted is not released."
fi

# --- what this repository records ----------------------------------------------
# FEATURE_PARITY.md's sibling-readings block carries this port's own shipped:
# field, and check-sibling-drift.sh scores the whole z80cpmw column against it.
# That field is hand-maintained because no tree knows what a store is serving -
# this is the measurement it is supposed to be set from.
if [ -f "$PARITY" ]; then
    claim=$(awk '/^z80cpmw[[:space:]]/ { for (i = 1; i <= NF; i++)
                    if ($i ~ /^shipped:/) { print substr($i, 9); exit } }' "$PARITY")
    echo
    if [ -z "$claim" ]; then
        echo "FEATURE_PARITY.md  no shipped: field on the z80cpmw line"
    elif [ "$claim" = "$live" ]; then
        echo "FEATURE_PARITY.md  shipped:$claim agrees with what the Store serves"
    else
        echo "FEATURE_PARITY.md  CLAIMS shipped:$claim, BUT the Store serves $live"
        if [ "$(vnum "$claim")" -gt "$(vnum "$live")" ] 2>/dev/null; then
            echo "  Every tick in the z80cpmw column is scored against software no"
            echo "  user has - and this is the column the other three are measured"
            echo "  against, so the error propagates to all of them."
        else
            echo "  The column is read against a build OLDER than what ships, so"
            echo "  its recorded gaps understate this port and overstate the rest."
        fi
        echo "  Re-read the column at the shipped build, then set this field."
        status=1
    fi
fi

# CHANGELOG.md states the released version in prose.  It is the thing this
# script exists to stop being taken on trust.
if [ -f "$CHANGELOG" ]; then
    said=$(grep -o 'The released Store version is \*\*[0-9][0-9.]*\*\*' "$CHANGELOG" |
           head -1 | sed 's/.*\*\*\([0-9.]*\)\*\*/\1/')
    if [ -n "$said" ] && [ "$said" != "$live" ]; then
        echo
        echo "CHANGELOG.md  says \"The released Store version is $said\", Store serves $live"
        echo "  Prose is not a measurement.  Correct the sentence, not this script."
        status=1
    fi
fi

echo
if [ "$status" != 0 ]; then
    echo "Something records a shipped state the Store does not support."
    exit 1
fi
exit 0
