#!/bin/sh
# Report how far the sibling ports have moved since FEATURE_PARITY.md was read
# against them.
#
# FEATURE_PARITY.md's port columns are a reading of four repositories taken on
# one afternoon, and three of them moved the same day.  Nothing reported that,
# so the document was only ever as current as the last person to read all four
# trees and nobody could tell from the file which parts had rotted.  This is
# that report.
#
# It reads the sibling-readings block in FEATURE_PARITY.md - one line per
# sibling, "<repo> <sha> <date>" - and for each one:
#
#   * finds the checkout at ../<repo> beside this repository,
#   * checks the recorded sha is a real object in it.  A cite naming a commit
#     nobody has is the c26aeb7 failure this document was rewritten to undo,
#     and it is worth catching mechanically rather than by argument,
#   * lists the commits that have landed since.
#
# Exit status: 0 when every column is current, 1 when any has drifted or any
# recorded sha is missing or unreadable.  So it can gate a sweep.
#
# Run it from anywhere; it locates the repository from its own path.
# It reads only - it never writes to a sibling.

set -u

ScriptDir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
RootDir=$(CDPATH= cd -- "$ScriptDir/.." && pwd)
SiblingDir=$(CDPATH= cd -- "$RootDir/.." && pwd)
Parity="$RootDir/FEATURE_PARITY.md"

if [ ! -f "$Parity" ]; then
	echo "not found: $Parity" >&2
	exit 1
fi

Readings=$(sed -n '/^```sibling-readings$/,/^```$/p' "$Parity" | sed '1d;$d')
if [ -z "$Readings" ]; then
	echo "no sibling-readings block in FEATURE_PARITY.md" >&2
	exit 1
fi

status=0

echo "FEATURE_PARITY.md vs the checkouts in $SiblingDir"
echo

# The read loop runs in this shell, not a subshell, so $status survives it.
while read -r repo sha date; do
	[ -n "${repo:-}" ] || continue
	case "$repo" in \#*) continue ;; esac

	tree="$SiblingDir/$repo"
	if [ ! -d "$tree/.git" ]; then
		echo "$repo	NOT CHECKED OUT at $tree - the column cannot be re-read here"
		status=1
		continue
	fi

	head=$(git -C "$tree" rev-parse --short HEAD 2>/dev/null)
	if [ -z "$head" ]; then
		echo "$repo	cannot read HEAD of $tree"
		status=1
		continue
	fi

	if [ "$sha" = "unknown" ]; then
		echo "$repo	read $date, COMMIT NOT RECORDED - drift cannot be measured; HEAD is $head"
		status=1
		continue
	fi

	if ! git -C "$tree" cat-file -e "$sha^{commit}" 2>/dev/null; then
		echo "$repo	recorded $sha IS NOT A COMMIT IN $tree - the reading cites something nobody has"
		status=1
		continue
	fi

	behind=$(git -C "$tree" rev-list --count "$sha..HEAD" 2>/dev/null)
	if [ "$behind" = "0" ]; then
		echo "$repo	current: read $sha ($date), HEAD is $head"
	else
		echo "$repo	DRIFTED: read $sha ($date), HEAD is $head, $behind commit(s) since"
		git -C "$tree" log --format='		%h %ad %s' --date=short "$sha..HEAD"
		status=1
	fi
done <<SIBLINGS
$Readings
SIBLINGS

echo
if [ "$status" -eq 0 ]; then
	echo "every column is as current as its recorded reading."
else
	echo "re-read what moved, correct the column, then update the sibling-readings block."
fi
exit "$status"
