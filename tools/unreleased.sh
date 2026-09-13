#!/bin/sh
# unreleased.sh - what is finished here but not yet in anybody's hands?
#
# WHY THIS EXISTS.  "Done" means four different things and the gap between them
# is where the mistakes come from: written, built, submitted, served.  Version.h
# describes the TREE.  A package in dist\ is built.  A Store submission is in
# somebody else's queue.  Only what the Store serves is what a user has.
#
# THIS IS ONE OF SIX AND THEY ARE DELIBERATELY DIFFERENT.  Every port here
# ships on its own channel - a Store, Play, the App Store, GitHub releases - so
# each repository's unreleased.sh is written for its own channel rather than
# copied.  That is on purpose: check-shipped-disks.sh was "one file in five
# repos", it diverged into four files that no two of which agreed, and "the
# check passed" came to mean four different things.  Do not try to unify these.
#
#   sh tools/unreleased.sh
#
# HOW THIS RUNS: BY HAND, AND IT MUST STAY THAT WAY.  Not wired to any workflow,
# and the exit codes are shaped so it cannot usefully become one.
#
# Exit 0 = it measured, INCLUDING when the answer is "twenty commits and two
#          packages are unreleased".  That is the normal state of a working
#          repository.  Four jobs in this family went red daily for the normal
#          state and all four were deleted on 2026-09-13; do not rebuild one.
# Exit 2 = could not measure.  Nothing is asserted when nothing was read.
#
# There is no exit 1.

set -u

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here" && git rev-parse --show-toplevel 2>/dev/null) || {
    echo "CANNOT MEASURE: $here is not inside a git checkout."; exit 2; }

VERSION_H="$root/z80cpmw/Version.h"

echo "z80cpmw - Microsoft Store, and the signed sideload beta"
echo

# --- what the Store serves, measured rather than read out of this tree -------
store_out=$(sh "$here/check-store-version.sh" 2>&1)
store_rc=$?
echo "$store_out" | sed 's/^/  /'
echo
if [ "$store_rc" = 2 ]; then
    echo "  The store could not be measured, so nothing below would mean anything."
    exit 2
fi

live=$(echo "$store_out" | sed -n 's/^  serves  *\([0-9][0-9.]*\).*/\1/p' | head -1)
if [ -z "${live:-}" ]; then
    echo "  CANNOT MEASURE: no 'serves' line in the store output."
    exit 2
fi

# --- the commit that version was cut from ------------------------------------
# No release tag is cut for a Store build - the Store channel leaves no tag and
# no GitHub release behind, which is why check-store-version.sh exists at all.
# So the anchor is the commit that last set VERSION_PATCH to the served value.
patch=${live##*.}
anchor=$(git -C "$root" log --format=%H -S"#define VERSION_PATCH $patch" \
             -- z80cpmw/Version.h 2>/dev/null | head -1)
if [ -z "${anchor:-}" ]; then
    echo "  CANNOT MEASURE: no commit sets VERSION_PATCH to $patch in Version.h."
    echo "  The Store serves $live and this tree has no record of building it."
    exit 2
fi

short=$(git -C "$root" rev-parse --short "$anchor")
echo "  $live was cut at $short - $(git -C "$root" log -1 --format=%s "$anchor")"
echo "  (the commit that set VERSION_PATCH to $patch; the Store channel leaves"
echo "  no tag behind, so this is the best anchor there is.)"
echo

n=$(git -C "$root" rev-list --count "$anchor..HEAD" 2>/dev/null)
if [ "${n:-0}" = "0" ]; then
    echo "  Nothing since.  The tree is what the Store serves."
else
    echo "  $n commit(s) since that build:"
    echo
    git -C "$root" log --format='    %h  %ad  %s' --date=short "$anchor..HEAD"
    echo
    app_n=$(git -C "$root" rev-list --count "$anchor..HEAD" -- z80cpmw/ 2>/dev/null)
    echo "  Of those, $app_n touch z80cpmw/ - the application itself."
    if [ "${app_n:-0}" != "0" ]; then
        git -C "$root" log --format='      %h  %s' "$anchor..HEAD" -- z80cpmw/
        echo
        echo "  Those are features and fixes a Store user does not have.  They"
        echo "  reach one only through a submission Microsoft then approves."
    fi
fi
echo

# --- the sideload channel, which is a GitHub release and does leave a trace ---
echo "  Sideload (GitHub releases):"
if command -v gh >/dev/null 2>&1; then
    latest=$(gh release list --repo avwohl/z80cpmw --limit 1 \
                 --json tagName,isLatest --jq '.[] | select(.isLatest) | .tagName' 2>/dev/null)
    if [ -n "${latest:-}" ]; then
        echo "    newest published    $latest"
        echo "    tree Version.h      $live"
        echo "    Anything built and signed but not published is in dist\\ on the"
        echo "    Windows machine and in todo.txt's [RELEASE] item - neither is"
        echo "    visible from here, so this script cannot count it."
    else
        echo "    could not read the release list"
    fi
else
    echo "    gh is not installed, so the sideload channel cannot be read"
fi
echo

exit 0
