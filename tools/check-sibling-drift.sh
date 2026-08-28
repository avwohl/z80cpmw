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
# Drift is measured against origin, not against the local checkout.  It used to
# be measured against local HEAD, and that let a stale checkout certify a column
# as current: on 2026-08-27 `ioscpm` read "current" here while the checkout was
# two commits behind origin/main, so the column was being blessed against a tree
# nobody else had.  The tip compared against is now origin's default branch -
# refs/remotes/origin/HEAD, falling back to origin/main then origin/master - and
# a local HEAD behind that tip is reported in its own right, because a reading
# taken in that checkout was taken against the wrong source.
#
# What this does NOT do is fetch.  A remote-tracking ref is only as fresh as the
# last `git fetch` in that checkout, so this can still be fooled by a tree that
# has not talked to its remote in a week - it just cannot be fooled by one that
# has fetched and not merged, which is the common case.  Each line prints when
# the sibling last fetched so the age is visible rather than assumed; pass
# --fetch to update the remote-tracking refs first, which is the only thing here
# that writes to a sibling, and is off by default for exactly that reason.
#
# Exit status: 0 when every column is current against origin, 1 when any has
# drifted, any recorded sha is missing or unreadable, or any column could not be
# checked against origin at all.  So it can gate a sweep.  "Could not check"
# counts as a failure on purpose: a gate that cannot verify must not say yes.
#
# Run it from anywhere; it locates the repository from its own path.
# It reads only, unless --fetch is given.

set -u

Fetch=no
for arg in "$@"; do
	case "$arg" in
		--fetch) Fetch=yes ;;
		-h|--help)
			sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
			echo "usage: $0 [--fetch]"
			exit 0
			;;
		*)
			echo "unknown argument: $arg (try --help)" >&2
			exit 1
			;;
	esac
done

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

# The ref that stands for "what everyone else has".  origin/HEAD is the honest
# answer when the checkout has one; the two fallbacks are for a clone made
# without it.  Prints nothing and fails when there is no usable origin ref.
origin_ref() {
	_t=$1
	_r=$(git -C "$_t" symbolic-ref --quiet --short refs/remotes/origin/HEAD 2>/dev/null)
	if [ -n "$_r" ] && git -C "$_t" rev-parse --verify --quiet "$_r" >/dev/null 2>&1; then
		echo "$_r"
		return 0
	fi
	for _r in origin/main origin/master; do
		if git -C "$_t" rev-parse --verify --quiet "$_r" >/dev/null 2>&1; then
			echo "$_r"
			return 0
		fi
	done
	return 1
}

# When this checkout last heard from its remote, so the reader can judge how
# much the origin comparison is worth.  BSD date first, GNU stat second.  A
# fresh clone has no FETCH_HEAD and reports "never", which is true and harmless:
# its refs came from the clone.
fetch_age() {
	_f=$1/.git/FETCH_HEAD
	[ -f "$_f" ] || { echo "never"; return; }
	date -r "$_f" '+%Y-%m-%d %H:%M' 2>/dev/null && return
	stat -c '%y' "$_f" 2>/dev/null | cut -c1-16 && return
	echo "fetch time unknown"
}

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

	if [ "$Fetch" = yes ]; then
		git -C "$tree" fetch --quiet origin 2>/dev/null ||
			echo "$repo	could not fetch - the comparison below is as old as the last one that worked"
	fi

	head=$(git -C "$tree" rev-parse --short HEAD 2>/dev/null)
	if [ -z "$head" ]; then
		echo "$repo	cannot read HEAD of $tree"
		status=1
		continue
	fi

	# The tip to measure the reading against.  Local HEAD is not it: a checkout
	# that has fetched and not merged would report a column as current that is
	# already behind what everyone else can see.
	ref=$(origin_ref "$tree") || ref=
	if [ -z "$ref" ]; then
		echo "$repo	NO ORIGIN REF in $tree - cannot tell whether this checkout is current"
		echo "		A reading taken here can only be checked against the local"
		echo "		HEAD ($head), which is what this check exists to stop."
		echo "		git fetch origin, or add the remote."
		status=1
		continue
	fi
	tip=$(git -C "$tree" rev-parse --short "$ref" 2>/dev/null)
	fetched=$(fetch_age "$tree")

	# Behind origin is worth saying whatever else is true of the reading: it
	# means the tree the column was read against is not the tree that ships.
	stale=$(git -C "$tree" rev-list --count "HEAD..$ref" 2>/dev/null)
	if [ "${stale:-0}" != "0" ]; then
		echo "$repo	CHECKOUT IS $stale COMMIT(S) BEHIND $ref - HEAD $head, $ref $tip"
		echo "		Anything read in this checkout was read against a tree"
		echo "		nobody else has.  git -C $tree pull, then re-read."
		status=1
	fi

	if [ "$sha" = "unknown" ]; then
		echo "$repo	read $date, COMMIT NOT RECORDED - drift cannot be measured; $ref is $tip (last fetch: $fetched)"
		status=1
		continue
	fi

	if ! git -C "$tree" cat-file -e "$sha^{commit}" 2>/dev/null; then
		echo "$repo	recorded $sha IS NOT A COMMIT IN $tree - the reading cites something nobody has"
		status=1
		continue
	fi

	behind=$(git -C "$tree" rev-list --count "$sha..$ref" 2>/dev/null)
	if [ "$behind" = "0" ]; then
		echo "$repo	current: read $sha ($date), $ref is $tip (last fetch: $fetched)"
	else
		echo "$repo	DRIFTED: read $sha ($date), $ref is $tip, $behind commit(s) since (last fetch: $fetched)"
		git -C "$tree" log --format='		%h %ad %s' --date=short "$sha..$ref"
		status=1
	fi
done <<SIBLINGS
$Readings
SIBLINGS

echo
if [ "$status" -eq 0 ]; then
	echo "every column is as current as its recorded reading, and every"
	echo "checkout is level with its origin."
else
	echo "re-read what moved, correct the column, then update the sibling-readings block."
fi
exit "$status"
