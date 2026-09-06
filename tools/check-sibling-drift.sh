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
# sibling, "<repo> <sha> <date> shipped:<build>" - and for each one:
#
#   * finds the checkout at ../<repo> beside this repository,
#   * checks the recorded sha is a real object in it.  A cite naming a commit
#     nobody has is the c26aeb7 failure this document was rewritten to undo,
#     and it is worth catching mechanically rather than by argument,
#   * lists the commits that have landed since,
#   * compares the build number in the tree at the recorded sha against the
#     build the port actually SHIPS, and
#   * checks that every symbol the document cites about that port resolves in
#     that port's tree.
#
# The last two exist because of what 2026-09-02 turned up, and neither was
# visible to a check that only counted commits.
#
# Shipped-build.  A tick means "this is in the tree", which is not what a user
# has.  ioscpm's row 2 was a tick from 2026-07-25 over scrollback that had never
# captured a line, and even after that was fixed the App Store served build 37
# while the tree was at 57 - twenty builds and six months of ticks describing
# software nobody could install.  The shipped: field is hand-maintained, because
# no tree knows what a store is serving; what the script does is compare it to
# the build in the tree and say so.  shipped:unknown is a failure, not a pass -
# an unmeasured claim about what ships is the thing this is here to stop.
#   The comparison runs BOTH WAYS, which it did not until 2026-09-06.  A reading
# AHEAD of what ships is the fault above: ticks over software nobody can install.
# A reading BEHIND what ships is a different fault with the same cause - the
# column was read at an older build than users are running, so its ticks are all
# true and its GAPS are stale, understating the port.  Until 2026-09-06 the test
# was a bare string inequality that printed the ahead message for both, so
# correcting ioscpm's shipped: from 37 to 61 - a build the column was BEHIND -
# produced a true failure with a false explanation.
#
# Citations.  On 2026-09-02 the block describing Android cited NINE symbols that
# existed nowhere in cpmdroid, in its tree or anywhere in its history, and four
# of the claims resting on them asserted the opposite of what that code does.
# They were written from a paraphrase of a commit message.  So: prose about a
# sibling is delimited with
#
#     <!-- cites: <repo> -->  ...  <!-- /cites -->
#
# and every `backticked` identifier inside that region must resolve, with
# git grep, in that sibling at the recorded sha.  A deliberate cross-port
# reference - naming z80cpmw's own function while describing how it differs -
# is declared with
#
#     <!-- cites-elsewhere: symA symB -->
#
# inside the region, so the exception is visible in the document rather than
# silent in the checker.  This rule alone would have caught all nine.
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
# Exit status: 0 when every column is current against origin, its shipped build
# is recorded and level with the tree it was read from, and every symbol it cites
# resolves.  1 when any has drifted, any recorded sha is missing or unreadable,
# any shipped build is unknown or behind, any cited symbol resolves nowhere, or
# any column could not be checked at all.  So it can gate a sweep.  "Could not
# check" counts as a failure on purpose: a gate that cannot verify must not say
# yes.
#
# --no-cites skips the citation pass, which is the slow half; the drift and
# shipped-build checks are cheap and always run.
#
# Run it from anywhere; it locates the repository from its own path.
# It reads only, unless --fetch is given.

set -u

# Citations are claims about code, so only code is searched.  Excluding
# documentation is not tidiness: cpmdroid's own todo.txt now quotes the nine
# fabricated symbols in the course of recording that they were fabricated, and a
# plain grep finds them there and calls them real.  A document cannot be evidence
# for itself.  Paths are exempt - see cite_resolves.
SrcOnly=". :(exclude)*.md :(exclude)*.txt :(exclude)docs/*"

Fetch=no
Cites=yes
for arg in "$@"; do
	case "$arg" in
		--fetch) Fetch=yes ;;
		--no-cites) Cites=no ;;
		-h|--help)
			sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
			echo "usage: $0 [--fetch] [--no-cites]"
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

# Does a cited token resolve in $2 at rev $3?  A file-shaped name is looked up
# as a path first - MANUAL_CHECKS.md is a real thing to cite and grepping file
# CONTENT for its name finds nothing - and then, failing that, as content, since
# `jni.h` is the NDK's header and appears only as an #include.  Everything else
# is a content search over source, documentation excluded: a document quoting a
# symbol is not evidence the symbol exists.
cite_resolves() {
	_tree=$1 _sym=$2 _rev=$3
	case "$_sym" in
		*.h|*.c|*.cc|*.cpp|*.hpp|*.kt|*.kts|*.swift|*.java|*.xml|*.gradle|*.py|*.sh|*.json|*.yml|*.yaml|*.md|*.txt|*.asm)
			if git -C "$_tree" ls-tree -r --name-only "$_rev" 2>/dev/null |
			   grep -qx ".*/$_sym\|$_sym"; then
				return 0
			fi
			;;
	esac
	# shellcheck disable=SC2086 - SrcOnly is a deliberate word list
	if git -C "$_tree" grep -q -F -- "$_sym" "$_rev" -- $SrcOnly 2>/dev/null; then
		return 0
	fi
	_bare=${_sym##*.}
	# shellcheck disable=SC2086
	if [ "$_bare" != "$_sym" ] && [ "${#_bare}" -ge 4 ] &&
	   git -C "$_tree" grep -q -F -- "$_bare" "$_rev" -- $SrcOnly 2>/dev/null; then
		return 0
	fi
	return 1
}

# The commit that last set a port's build number to its current value, so the
# tree can be asked whether anything has landed since.  A matching build number
# is a weaker statement than it looks: a number is only bumped when somebody
# cuts a release, so work that lands afterwards sits in the tree under the
# number of the build before it.  cpmdroid was exactly here on 2026-09-03 -
# versionCode 25 in the tree, versionCode 25 on Play, and four commits between
# them including two scrollback fixes no user had.
# Does every commit in a range touch documentation only?  A column reading is a
# reading of code, so prose landing after it does not invalidate the reading -
# and reporting it as drift at the same weight as a real change is how a checker
# earns a reputation for noise and stops being read.  Conservative on purpose:
# any path outside the documentation set, and any commit that changed nothing,
# makes the whole range count.
docs_only_range() {
	_tree=$1 _range=$2
	_files=$(git -C "$_tree" log --format= --name-only "$_range" 2>/dev/null | sort -u | sed '/^$/d')
	[ -n "$_files" ] || return 1
	for _f in $_files; do
		case "$_f" in
			*.md|*.txt|docs/*|*/docs/*) ;;
			*) return 1 ;;
		esac
	done
	return 0
}

# The human-facing release name in a tree, which is not always the build number:
# cpmdroid ships versionCode 25 as "1.24" and tags the name, not the code.
tree_release_name() {
	_repo=$1 _tree=$2 _sha=$3
	case "$_repo" in
		ioscpm)
			git -C "$_tree" show "$_sha:iOSCPM.xcodeproj/project.pbxproj" 2>/dev/null |
				sed -n 's/.*MARKETING_VERSION = \([0-9.]*\);.*/\1/p' | head -1
			;;
		cpmdroid)
			git -C "$_tree" show "$_sha:app/build.gradle.kts" 2>/dev/null |
				sed -n 's/.*versionName *= *"\([^"]*\)".*/\1/p' | head -1
			;;
		romwbw_emu)
			git -C "$_tree" show "$_sha:VERSION" 2>/dev/null | head -1 | tr -d ' \t\r'
			;;
	esac
}

# Where the shipped artefact was cut from, as "<sha> <label>".
#
# A release tag is the better answer where there is one: it names the commit an
# artefact was built from, where the version-bump commit only names when the
# number changed - romwbw_emu bumps VERSION and tags a commit later, so measuring
# from the bump overstates the gap.  Tags are looked up by BOTH the build number
# and the release name, because ports differ about which they tag.  Without a tag
# the bump commit is all there is, and the caller says so rather than passing an
# upper bound off as the artefact.
version_bump_commit() {
	_repo=$1 _tree=$2 _sha=$3 _built=$4
	_name=$(tree_release_name "$_repo" "$_tree" "$_sha")
	for _t in "v$_built" "$_built" "v$_name" "$_name"; do
		[ -n "${_t#v}" ] || continue
		if git -C "$_tree" rev-parse --verify --quiet "$_t^{commit}" >/dev/null 2>&1; then
			echo "$(git -C "$_tree" rev-parse "$_t^{commit}") $_t"
			return 0
		fi
	done
	# Ports suffix their tags differently - z80cpmw ships 1.0.22 as the signed
	# sideload v1.0.22-beta - so fall back to the newest tag that NAMES this
	# version before giving up on tags entirely.
	_t=$(git -C "$_tree" tag --list "*$_built*" --sort=-v:refname 2>/dev/null | head -1)
	if [ -n "${_t:-}" ] &&
	   git -C "$_tree" rev-parse --verify --quiet "$_t^{commit}" >/dev/null 2>&1; then
		echo "$(git -C "$_tree" rev-parse "$_t^{commit}") $_t"
		return 0
	fi
	case "$_repo" in
		ioscpm)     _f=iOSCPM.xcodeproj/project.pbxproj ;;
		cpmdroid)   _f=app/build.gradle.kts ;;
		romwbw_emu) _f=VERSION ;;
		z80cpmw)    _f=z80cpmw/Version.h ;;
		*)          return 1 ;;
	esac
	_c=$(git -C "$_tree" log --format=%H -S"$_built" "$_sha" -- "$_f" 2>/dev/null | tail -1)
	[ -n "$_c" ] || return 1
	echo "$_c the-commit-that-set-the-number"
}

# The build number in a sibling's tree at a given commit.  Every port keeps it
# somewhere different and none of them is guessable, so the knowledge lives here
# rather than in the document; a port whose file moves must be corrected here,
# and prints "unknown" until it is, which fails.  Read at the recorded sha, not
# at HEAD: the question is what build the reading described.
tree_build() {
	_repo=$1 _tree=$2 _sha=$3
	case "$_repo" in
		ioscpm)
			git -C "$_tree" show "$_sha:iOSCPM.xcodeproj/project.pbxproj" 2>/dev/null |
				sed -n 's/.*CURRENT_PROJECT_VERSION = \([0-9][0-9]*\);.*/\1/p' | head -1
			;;
		cpmdroid)
			git -C "$_tree" show "$_sha:app/build.gradle.kts" 2>/dev/null |
				sed -n 's/.*versionCode *= *\([0-9][0-9]*\).*/\1/p' | head -1
			;;
		romwbw_emu)
			git -C "$_tree" show "$_sha:VERSION" 2>/dev/null | head -1 | tr -d ' \t\r'
			;;
		z80cpmw)
			# This repo's own version, composed from four #defines rather than
			# written out, so it is assembled the way Version.h's own comment
			# says the packaging scripts do it.
			git -C "$_tree" show "$_sha:z80cpmw/Version.h" 2>/dev/null |
				awk '/^#define VERSION_MAJOR/ {a=$3}
				     /^#define VERSION_MINOR/ {b=$3}
				     /^#define VERSION_PATCH/ {c=$3}
				     END { if (a != "") printf "%s.%s.%s", a, b, c }'
			;;
		*)
			;;
	esac
}

# Compare two build identifiers.  Prints -1, 0 or 1 for a<b, a==b, a>b.
# These are not all the same shape - ioscpm's CURRENT_PROJECT_VERSION and
# cpmdroid's versionCode are plain integers, romwbw_emu's VERSION is "1.38" and
# z80cpmw's is "1.0.25" - so compare component by component.  A plain numeric
# test would fail on the dotted ones and a string test would put 1.0.9 above
# 1.0.22.
ver_cmp() {
	awk -v a="$1" -v b="$2" 'BEGIN {
		n = split(a, x, "."); m = split(b, y, ".")
		k = (n > m ? n : m)
		for (i = 1; i <= k; i++) {
			p = (i <= n ? x[i] + 0 : 0)
			q = (i <= m ? y[i] + 0 : 0)
			if (p < q) { print -1; exit }
			if (p > q) { print  1; exit }
		}
		print 0
	}'
}

status=0

# The citation loop runs in a pipeline, hence a subshell, so it cannot set
# $status directly.  It appends here instead and the caller reads it back.
CiteFail=$(mktemp) || exit 1
trap 'rm -f "$CiteFail"' EXIT INT TERM

echo "FEATURE_PARITY.md vs the checkouts in $SiblingDir"
echo

# The read loop runs in this shell, not a subshell, so $status survives it.
while read -r repo sha date shipped rest; do
	[ -n "${repo:-}" ] || continue
	case "$repo" in \#*) continue ;; esac

	tree="$SiblingDir/$repo"
	# This repository describes itself in the same table it describes the others
	# in, and its column is maintained in place rather than read at a commit - so
	# there is no drift to measure, only a shipped state.  It was left out of this
	# block entirely until 2026-09-03, which meant the one column every other
	# column is scored against was the only one whose shipped state nothing
	# checked.  The asymmetry the prose had, reproduced in the mechanism.
	home=no
	[ "$tree" = "$RootDir" ] && home=yes

	if [ ! -d "$tree/.git" ]; then
		echo "$repo	NOT CHECKED OUT at $tree - the column cannot be re-read here"
		status=1
		continue
	fi

	if [ "$Fetch" = yes ]; then
		git -C "$tree" fetch --quiet origin 2>/dev/null ||
			echo "$repo	could not fetch - the comparison below is as old as the last one that worked"
	fi

	[ "$sha" = HEAD ] && sha=$(git -C "$tree" rev-parse --short HEAD 2>/dev/null)

	head=$(git -C "$tree" rev-parse --short HEAD 2>/dev/null)
	if [ -z "$head" ]; then
		echo "$repo	cannot read HEAD of $tree"
		status=1
		continue
	fi

	if [ "$home" = yes ]; then
		# No reading to drift: this column is edited in the same commit as the
		# code it describes.  Only the shipped question is open, and it falls
		# through to the check below.
		echo "$repo	this repository - column maintained in place, HEAD $head"
	else

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
	elif docs_only_range "$tree" "$sha..$ref"; then
		# The reading still stands: nothing it describes has changed.
		echo "$repo	current: read $sha ($date); $behind commit(s) since, documentation only"
	else
		echo "$repo	DRIFTED: read $sha ($date), $ref is $tip, $behind commit(s) since (last fetch: $fetched)"
		git -C "$tree" log --format='		%h %ad %s' --date=short "$sha..$ref"
		status=1
	fi
	fi

	# What the reading describes, against what a user can install.  A tick over
	# a build nobody has is still a lie about the product even when it is a true
	# statement about the tree.
	case "${shipped:-}" in
		shipped:*) ship=${shipped#shipped:} ;;
		*)         ship= ;;
	esac
	built=$(tree_build "$repo" "$tree" "$sha")
	if [ -z "$ship" ]; then
		echo "$repo	SHIPPED BUILD NOT RECORDED - add 'shipped:<build>' to its sibling-readings line"
		echo "		The tree at $sha is build ${built:-unknown}.  What users have is"
		echo "		not in this repository and has to be measured, not inferred."
		status=1
	elif [ "$ship" = unknown ]; then
		echo "$repo	SHIPPED BUILD UNKNOWN - tree at $sha is build ${built:-unknown}"
		echo "		Measure it and record it; an unmeasured claim about what ships"
		echo "		is what this field exists to stop."
		status=1
	elif [ -z "$built" ]; then
		echo "$repo	CANNOT READ THE BUILD NUMBER at $sha - tree_build() has no rule for this port, or its file moved"
		status=1
	elif [ "$(ver_cmp "$built" "$ship")" = 1 ]; then
		echo "$repo	READ AT BUILD $built, SHIPS $ship - every tick in this column describes software no user has"
		status=1
	elif [ "$built" != "$ship" ]; then
		# The other direction, and it is not the same fault.  The reading is
		# BEHIND what ships, so every tick describes software users do have
		# and nothing here overstates the product.  What is wrong is the
		# absences: the column has never been read against the builds that
		# shipped since, so whatever those gained is recorded as a gap that
		# is no longer there.  Same remedy - re-read the column - but do not
		# report it as ticks over software nobody has, because it is not.
		echo "$repo	READ AT BUILD $built, SHIPS $ship - the reading is BEHIND what ships, so this column's GAPS are stale"
		status=1
	else
		# Same number is not the same software.  Say how much has landed since
		# the number was set, because that is the part a version match hides.
		ref=$(version_bump_commit "$repo" "$tree" "$sha" "$built") || ref=
		bump=${ref%% *}
		label=${ref#* }
		since=
		[ -n "${bump:-}" ] &&
			since=$(git -C "$tree" rev-list --count "$bump..$sha" 2>/dev/null)
		if [ -n "${since:-}" ] && [ "$since" != "0" ] &&
		   docs_only_range "$tree" "$bump..$sha"; then
			echo "$repo		build $built ships, and the $since commit(s) since $label are documentation only"
		elif [ -n "${since:-}" ] && [ "$since" != "0" ]; then
			case "$label" in
				the-commit-that-set-the-number)
					echo "$repo	build $built matches what ships, BUT $since commit(s) landed after the number was set"
					echo "		No release tag to measure from, so this counts from the"
					echo "		bump and is an upper bound, not the artefact.  Tag on"
					echo "		release and it becomes exact."
					;;
				*)
					echo "$repo	build $built matches what ships, BUT $since commit(s) landed after $label"
					echo "		The column is read against a tree that is not the tagged"
					echo "		release, even though the numbers agree."
					;;
			esac
			status=1
		else
			echo "$repo		build $built, and that is what ships"
		fi
	fi
done <<SIBLINGS
$Readings
SIBLINGS

# ---------------------------------------------------------------------------
# Citations.  Every `backticked` identifier inside a <!-- cites: repo --> region
# must resolve in that repo at its recorded sha.
#
# The extraction is deliberately conservative: only bare identifiers and dotted
# names are tested, so prose in backticks, expressions, flags and literals are
# skipped rather than guessed at.  A dotted name is tested by its last component
# too, because `SettingsRepository.DEFAULT_SCROLLBACK_LINES` is a real constant
# written with its class.  Missing a citation is a smaller failure here than
# inventing one: this catches fabricated symbols, which is what happened, and
# does not pretend to check English.
if [ "$Cites" = yes ]; then
	echo
	echo "citations"
	echo

	cites=$(awk '
		/<!-- cites: [A-Za-z0-9_.-]+ -->/ {
			match($0, /cites: [A-Za-z0-9_.-]+/)
			repo = substr($0, RSTART + 7, RLENGTH - 7)
			inblock = 1
			next
		}
		/<!-- \/cites -->/ { inblock = 0; repo = ""; next }
		inblock && /<!-- cites-withdrawn:/ {
			line = $0
			sub(/.*cites-withdrawn:[ \t]*/, "", line)
			sub(/-->.*/, "", line)
			n = split(line, wd, /[ \t]+/)
			for (i = 1; i <= n; i++)
				if (wd[i] != "") withdrawn[repo SUBSEP wd[i]] = 1
			next
		}
		inblock && /<!-- cites-elsewhere:/ {
			line = $0
			sub(/.*cites-elsewhere:[ \t]*/, "", line)
			sub(/-->.*/, "", line)
			n = split(line, ex, /[ \t]+/)
			for (i = 1; i <= n; i++)
				if (ex[i] != "") allowed[repo SUBSEP ex[i]] = 1
			next
		}
		inblock {
			n = split($0, part, "`")
			# Fields 2, 4, 6 ... sit between backticks.  An odd count means an
			# unclosed backtick on this line; the last field is prose, not code.
			for (i = 2; i <= n; i += 2) {
				tok = part[i]
				# This document names functions as `foo()` far more often than
				# as `foo`, and skipping that form left a eighth of the
				# citations in the marked regions unchecked - including, when
				# this was noticed, the one sentence that had just been written.
				sub(/\(\)$/, "", tok)
				if (tok !~ /^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*$/) continue
				# Two characters is a letter in prose, not a citation: `R8`,
				# `W8`, `Up`, `_`.  They resolve trivially and prove nothing.
				if (length(tok) < 3) continue
				if (tok in seen_tok && seen[repo SUBSEP tok]) continue
				seen[repo SUBSEP tok] = 1
				print repo "\t" tok
			}
		}
		END {
			for (k in allowed) {
				split(k, p, SUBSEP)
				print p[1] "\tALLOW\t" p[2]
			}
			for (k in withdrawn) {
				split(k, p, SUBSEP)
				print p[1] "\tWITHDRAWN\t" p[2]
			}
		}
	' "$Parity")

	# Collect the declared exceptions first, then test everything else.
	Allowed=$(printf '%s\n' "$cites" | awk -F'\t' '$2 == "ALLOW" { print $1 "\t" $3 }')
	# Withdrawn symbols are the inverse assertion: the document says this port
	# does NOT have them - either because the name was fabricated, or because the
	# prose is "there is no key map on Android, no DEFAULT_KEY_BINDINGS, no
	# decodeKeySequence".  Both are claims that can rot the moment the port gains
	# the thing, and neither is checkable by looking for it.  So the test is
	# inverted: it fails if the symbol ever starts resolving.
	Withdrawn=$(printf '%s\n' "$cites" | awk -F'\t' '$2 == "WITHDRAWN" { print $1 "\t" $3 }')

	printf '%s\n' "$cites" | awk -F'\t' '$2 != "ALLOW" && NF == 2' | sort -u |
	while IFS="$(printf '\t')" read -r repo sym; do
		[ -n "${repo:-}" ] && [ -n "${sym:-}" ] || continue

		if printf '%s\n' "$Allowed" | grep -qx "$repo	$sym"; then
			echo "$repo	$sym	declared as a reference to another port"
			continue
		fi

		if printf '%s\n' "$Withdrawn" | grep -qx "$repo	$sym"; then
			tree="$SiblingDir/$repo"
			sha=$(printf '%s\n' "$Readings" |
				awk -v r="$repo" '$1 == r { print $2; exit }')
			tipref=$(origin_ref "$tree" 2>/dev/null) || tipref=
			if [ -n "$tipref" ] && cite_resolves "$tree" "$sym" "$tipref"; then
				echo "$repo	$sym	DECLARED ABSENT BUT IT NOW RESOLVES - the document says this port does not have it, and it does"
				echo "STATUS_FAIL" >> "$CiteFail"
			else
				echo "$repo	$sym	declared absent, and still absent - as the document says"
			fi
			continue
		fi

		tree="$SiblingDir/$repo"
		[ -d "$tree/.git" ] || continue
		sha=$(printf '%s\n' "$Readings" |
			awk -v r="$repo" '$1 == r { print $2; exit }')
		[ -n "${sha:-}" ] && [ "$sha" != unknown ] || continue
		git -C "$tree" cat-file -e "$sha^{commit}" 2>/dev/null || continue

		if cite_resolves "$tree" "$sym" "$sha"; then
			continue
		fi

		# Absent at the recorded sha is two very different things, and calling
		# them by one name is how this check would earn a reputation for noise.
		# Present at origin means the READING is stale and the symbol arrived
		# after it.  Absent there too, and absent from history, means somebody
		# wrote down a name that never existed.
		tipref=$(origin_ref "$tree") || tipref=
		if [ -n "$tipref" ] && cite_resolves "$tree" "$sym" "$tipref"; then
			echo "$repo	$sym	arrived after the recorded reading - it is in $tipref but not at $sha"
			echo "STATUS_FAIL" >> "$CiteFail"
			continue
		fi

		# History probe, source only.  Documentation is excluded on purpose: a
		# note quoting a fabricated symbol - including the ones written on
		# 2026-09-02 recording that they were fabricated - would otherwise count
		# as evidence the symbol once existed, which is exactly backwards.
		# shellcheck disable=SC2086
		ever=$(git -C "$tree" log --oneline -S"$sym" --all -- $SrcOnly 2>/dev/null |
			wc -l | tr -d ' ')
		if [ "${ever:-0}" = "0" ]; then
			echo "$repo	$sym	RESOLVES NOWHERE, and no commit in that repository ever contained it in source"
		else
			echo "$repo	$sym	NOT IN THE TREE at $sha nor at origin ($ever source commit(s) touched it once)"
		fi
		echo "STATUS_FAIL" >> "$CiteFail"
	done

	if [ -s "$CiteFail" ]; then
		status=1
	else
		echo "every cited symbol resolves in the port it describes."
	fi
fi

echo
if [ "$status" -eq 0 ]; then
	echo "every column is as current as its recorded reading, and every"
	echo "checkout is level with its origin."
else
	echo "re-read what moved, correct the column, then update the sibling-readings block."
fi
exit "$status"
