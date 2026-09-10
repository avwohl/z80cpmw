# Known problems

Standing facts that will never be "done": limits of tools this project depends
on, and traps set for whoever edits certain files next. None of them is an open
item — an entry here is something to know, not something to do. That is the
distinction from `CLAUDE.md`, which holds the rules to follow.

Delete an entry only when it stops being true.

---

## 1.0.22 cannot have a crash report resolved

There is no `.pdb` for the shipped 1.0.22 build on **either** channel — Store
**1.0.22** and sideload **1.0.22-beta** came from one build and neither kept its
symbols — and there never will be. A rebuild, against wxWidgets 3.3.3 or against
whatever vcpkg has moved on to, is a different binary with different symbols. A
stack from 1.0.22 will not symbolicate. What `dist\` holds is
`z80cpmw-1.0.21-beta.pdb` and nothing at all for 1.0.22; that is the whole of
the evidence and the whole of what is left.

`build-msix.ps1 -Beta` keeps the `.pdb` now, and that half is no longer
untested: the unsigned rehearsal `-Beta -SkipBuild -SkipSign` was run on
2026-08-28, exits 0, and writes the package and its `.pdb` together, the `.pdb`
hashing equal to `bin\Release\z80cpmw.pdb`. It cannot be undone for 1.0.22.

**And it happened a second time, on the arm that fix did not cover.** Step 6 of
`build-msix.ps1` was `if ($Beta)` until 1.0.25, so a Store package built without
a beta beside it kept no symbols at all. **1.0.24 is such a package.** No `.pdb`
for it exists anywhere — `dist\` holds `z80cpmw-1.0.21-beta.pdb`,
`z80cpmw-1.0.23-beta.pdb` and `z80cpmw-1.0.25-store.pdb`, and nothing carrying
1.0.24 in its name but the `.msix` itself; `bin\Release\z80cpmw.pdb` has been
rebuilt many times since, most recently on 2026-09-07. The choice this paragraph
used to pose — submit `dist\z80cpmw-1.0.24-store.msix` and inherit 1.0.22's
problem, or submit 1.0.25 and not — has since been made, and made the right way:
1.0.25 went to the Store with `z80cpmw-1.0.25-store.pdb` kept beside the package
it came from, so the version users had was symbolicated. The Store has moved on
twice since - it serves **1.0.29** as of 2026-09-10 - and every version since
1.0.25 has kept its symbols on both arms. The 1.0.24 package has no symbols
anywhere and submitting it would be choosing that problem rather than inheriting
it; it is superseded four times over and there is no reason to.

**A THIRD WAY TO LOSE A `.pdb` TURNED UP ON 2026-09-10, AND IT IS NOT THE
SCRIPT'S FAULT.** The rule in CLAUDE.md is about what `build-msix.ps1` does at
package time; it says nothing about RETENTION, and this file was being read as
though it did. `dist\` is gitignored, so nothing in git protects it, and it was
found mid-session to have lost `z80cpmw-1.0.28-beta.msix`, its `.pdb`, and
`z80cpmw-1.0.29-store.pdb` - the symbols for the version the Store was serving at
that moment. Nothing in `build-msix.ps1`, `run_tests.bat` or the `.vcxproj`
deletes anything in `dist\` but its own outputs and the staging folder; all of
them were found in the **Recycle Bin**, deleted from `C:\temp\src\z80cpmw\dist`,
alongside `.pdb` and `.msix` files going back to 1.0.21. What deleted them was
not established.

They were all restored, and the restore was checked rather than assumed:
`z80cpmw-1.0.28-beta.msix` came back hashing
`d205be82ca783c2e51fb7596976c4001ac214b285f92dfd2f8ab66c9c075b2ca`, which is the
sha256 CHANGELOG records for the published artifact. So the standing lesson is a
cheerful one: **the Recycle Bin is the last line of defence, and it works.** Look
there before concluding a `.pdb` is gone - the rule that it cannot be recreated is
about rebuilding, not about recovery. `dist\` now holds symbols for 1.0.21-beta,
1.0.23-beta, 1.0.25-store, 1.0.28-beta, 1.0.29-store, 1.0.30-beta and
1.0.31-store.
1.0.23 escapes by accident rather than by design: its beta was cut from the same
build with `-SkipBuild`, so `z80cpmw-1.0.23-beta.pdb` symbolicates the Store
binary too — which is a property of how that release happened to be cut, not a
rule.

## `emu_host_path_basename()` is a link error waiting to be triggered

`romwbw_emu/src/emu_io.h` declares it and `emu_io_common.cc` defines it — and
`emu_io_common.cc` is the one core file this project deliberately does not
compile (`emu_io_windows.cpp`'s header comment says why, and `WIP.md` has the
long version). Nothing this project compiles calls it today, so there is no
error today. The first call added here produces one.

It will not look like the `emu_host_path_caps()` / `emu_host_file_get_read_name()`
class of break, which are backend functions the core requires *you* to define.
This one is a core function the core already defines, in a file you are not
building. Define it in `emu_io_windows.cpp` alongside the other twelve
hand-synced functions, or the link fails. Found by sweeping all 67 `emu_*`
declarations in `emu_io.h` against every source in `z80cpmw.vcxproj`; it is the
only other undefined name.

## cpmtools with the wrong diskdef exits 0, two different ways

Neither failure mode looks like an error, and they do not look like each other,
so one test does not catch both. Measured 2026-08-27 against
`romwbw_emu/disks`:

- `cpmls -f wbw_hd1k hd1k_combo.img` prints the `0:` header, then 1024 blank
  lines and **not one filename**. Exit 0.
- `cpmls -f wbw_hd1k_0 hd1k_infocom.img` prints 312 **garbage names** —
  `t_u.o_`, `@p`, `4om`, `hu?` — mixed into the blanks. Exit 0. (Read with its
  own diskdef that image has 68 files, all of them legal CP/M names.)

So the first case is indistinguishable from "that utility is not on the image"
unless you count what was listed, and the second is indistinguishable from a
directory full of files unless you check the names are legal CP/M names.
Both are caught by picking the diskdef from each image's own geometry and then
refusing a listing with no names in it *or* with any name that is not printable
ASCII, before going looking for a filename — 233 of those 312 garbage names are
not. `packaging/scripts/verify-disk-assets.sh` used to do that here and was
deleted on 2026-09-05; the check now lives upstream in `romwbw_disks`, which
verifies every image it publishes. Anyone checking an image **by hand** has to do
it by eye. The rule: `hd1k_combo.img` is `wbw_hd1k_0` because of its 1 MB MBR
prefix, a plain 8 MB image is `wbw_hd1k`, and any other size is not a guess to
take — it means the geometry is not one of these.

A `./diskdefs` in the current directory also shadows the system file completely
rather than adding to it, so a partial local copy makes every other format
"unknown". Copy `romwbw_emu/disks/diskdefs` whole — it carries the entire
`wbw_hd1k` family and is the definition of record — rather than writing out the
one definition you think you need.

## An unarmed W8 cannot be told from an armed one by its usage string

Worth keeping because the obvious check is the wrong one, and it was the check
used by hand before any script existed.

`W8` gained `W8 <cpmname> [hostpath]` in `romwbw_emu` 98eb6a1, and then in
a4d3db8 gained an interlock: before handing a host path to the emulator it asks
`HBF_HOST_CAPS` (`0xE9`) whether that emulator promises not to use the path
destructively, and refuses if the answer is no or if the call does not exist.
That interlock is why an old emulator cannot be talked into deleting a user's
disk library by a new `W8`.

The usage string does not discriminate the two. A `w8.com` built between those
two commits prints exactly the same `Usage: W8 <cpmname> [hostpath]` and issues
no probe at all: it takes host paths and asks nobody. Grepping an image for the
usage text passes the very binary the interlock was written to replace.

So it has to be checked as machine code. In `w8.asm`:

    ld    b,H_CAPS      ; 06 E9
    rst   8             ; CF

Three bytes, `06 E9 CF`, at a byte boundary in the extracted `w8.com`. Their
presence is the difference between an armed `W8` and an unarmed one.

This is now asserted upstream, on every image before it is published:
`romwbw_disks` `tools/build_utils.sh` checks it at build time and
`tools/verify_catalog.py` checks it against every published image carrying
`w8.com`. Nothing in this repository needs to re-check it, because this
repository no longer ships an image.

## Nothing transfers from the games disk, and the gate will never say so

The Games disk carries **neither `R8.COM` nor `W8.COM`**, so a user who follows
the in-app help to it and then tries to move a save file has no utility to
run — on either side. No build ships that image and none has for some time, so
it has to be measured where a user actually gets it: `hd1k_games-v0-3.6.0.img`,
downloaded from the catalog on 2026-09-07 and hashing `287601a3…`, the sha256
the 3.6.0 catalog publishes for it. In that file the 8.3 directory pattern
`R8      COM` occurs **zero** times and `W8      COM` zero times, against one
of each in `hd1k_combo-v0-3.6.0.img` (`f4873027…`) — so the search finds them
where they exist, and the answer for the games disk is really nothing rather
than a bad search. It is not a property of one release either:
`hd1k_games-v0-3.5.1.img` in the data folder answers the same and hashes
`7f33738c…`. A third copy used to be measurable at `bin\Release\disks\`, left
there by a build from back when one staged images; it was deleted on 2026-09-07
along with `bin\Release\roms\`, so there is no copy anywhere in this tree to
measure any more, and there never will be again — which is exactly why the
measurement above is taken on the downloaded file.

The half that will not change is the gate. The check that used to run here,
`packaging/scripts/verify-disk-assets.sh` (deleted 2026-09-05), was
severity-split by image: only an image larger than 8 MB carrying a `55 AA` MBR
signature reached `bad()` for a missing utility, while a plain 8 MB image got an
`info` line and never touched the failure count — on the reasoning that a
secondary data disk carrying neither utility is a choice and not a fault.
`hd1k_games.img` is exactly 8,388,608 bytes, so it took the info branch every
time. The v0 catalog states the same thing as data rather than inferring it from
geometry: the `hd1k_games` entry carries `host_transfer: false`, still there in
the published 3.6.0 catalog when it was fetched on 2026-09-07. **A PASS is
compatible with the games disk having no R8 and no W8**, and is
meant to be. Anyone refreshing the images who wants this closed has to check
that image by hand and put the utilities on it deliberately; nothing will go red
if they forget, and the sentence about it in the in-app File Transfer topic is
the only place a user is told.

## The first shipped build to read the romwbw_disks index spends its respin

`romwbw_disks` corrects an already-published RomWBW version by **overwriting the
assets in place and bumping a content-derived `generation`**, never by renaming
them — its `docs/RELEASING.md` section 5 records why: all three clients key
saved state on the filename, so a `-r2` name would strand every user's
downloaded library under a name nothing fetches.

That was exercised once, on 2026-09-06: generation 2, four rebuilt ROMs, both
releases. It was safe only because no shipped client could see those URLs — the
migration to the index (`f91c3a3`) has never been in a released build, and the
build the Store serves still fetches
`avwohl/ioscpm/releases/download/v1.4.12/disks.xml`. That build is **1.0.25**,
not 1.0.23, and its pin is `v1.4.12`, not `v1.4.5`:
`tools/check-store-version.sh` answers `AaronWohl.Z80CPM_1.0.25.0_x64`, on
2026-09-06 in `7ca073c` and again on 2026-09-07, and `git show
211488b:z80cpmw/DiskCatalog.cpp` reads `RELEASE_TAG = L"v1.4.12"`. What survives
that correction is the half that matters here: `git merge-base --is-ancestor
f91c3a3 211488b` answers no, so nothing users have can see the index.

**Do not check that with `git tag --contains`.** This family releases without
tagging: ioscpm 1.5.1 went live on 2026-09-05 and has no `v1.5.1` tag at all, so
an empty `git tag --contains` proves nothing about what users have. The question
is what the *shipped source* fetches — `git show <shipped-commit>:z80cpmw/DiskCatalog.cpp`,
or the `RELEASE_TAG` string in the artifact, where it is UTF-16LE.

Whichever release first carries `f91c3a3` ends this. From then on a correction
upstream is a new RomWBW version entry, not a quiet re-upload, and a user who
already verified a SHA-256 would otherwise get different bytes at the same URL.
Since 2026-09-07 that covers the ROM as well as the images: this application
fetches every ROM from those same URLs and verifies its sha256 each time it
loads one (`loadCatalogRomForStart()` calls `DiskCatalog::verifyRom()` before
`loadROM()`), so an asset regenerated in place stops verifying on a machine
that already holds the old bytes, and that machine needs another download
before it will start.

This is not an action for this repository; it is a cost that this repository's
next release imposes on another, and worth knowing before spending it.

## A first launch with no network cannot start the machine

This is what shipping no ROM costs, and it was spent deliberately on 2026-09-07,
when `roms\emu_avw.rom` and `roms\emu_romwbw.rom` were deleted along with every
line that staged or packaged them: the ROM copy out of both `PostBuildEvent`s in
`z80cpmw.vcxproj` (the Debug one is now a comment and nothing else, the Release
one still copies the app-local CRT DLLs), the `roms\` staging directory and its
`Copy-Item` in `build-msix.ps1`, and the two `File` lines with their
`SetOutPath "$INSTDIR\roms"` in `z80cpmw.nsi`. Every ROM now comes from the
release catalog in `avwohl/romwbw_disks`, reached through the one URL left in
the binary (`catalogv0::INDEX_URL` in `CatalogV0.cpp`), and is checked against
the exact size and the sha256 that only the catalog carries.
A machine that has never reached the network therefore has no ROM it is allowed
to load, and does not start.

The check is not a policy laid over the loader; it is the only way in.
`m_emulator->loadROM()` has exactly one caller in the whole application,
`MainWindow::loadCatalogRomForStart()`, and the statement immediately above it is
`DiskCatalog::verifyRom()`, which refuses a size that is not exactly the
published one or a sha256 that does not match. "Boot it anyway, unverified" is
not a branch somebody forgot to write, and adding it would mean adding a second
caller.

What the user gets instead of a dead machine is a sentence. `loadDefaultROM()`
loads nothing now and stays only to post the notice that the ROM is fetched on
the first start; `romReadyToStart()` fetches the catalog without asking, a
catalog being a few kilobytes where a ROM is 512 KB of somebody's connection;
and `offerRomChoice()` then says which of the two situations this is. With a
catalog in hand that names the ROM, it offers the download — "Download the
RomWBW 3.6.0 ROM now (about 512 KB)?", Yes to fetch it and start, No to not
start. With no catalog at all, which is the offline first launch and the case
where not even the release is known, there is nothing to offer: it says the ROM
is downloaded from the catalog the first time the machine starts, that this app
does not ship one, and to check the network connection and press F5 again. What
it never does is boot empty banks or boot another release's ROM, which is the
whole reason the gate is there.

**A ROM put beside the executable by hand does not rescue this**, and that is
the part that surprises people. `findResourceFile()` still searches `roms\`
beside the executable, the executable's own directory and `..\roms\` before it
reaches the data folder, so a file dropped there does win the lookup — but
winning the lookup only carries it as far as `verifyRom()`, and the size and
hash it has to match exist only in the catalog. The same goes for a ROM an
earlier run already downloaded into `%LOCALAPPDATA%\z80cpmw\data`: the bytes are
on the machine, the claim about them is not. Doing better would mean writing a
ROM's published hash down locally — a second provenance store beside
`DiskLedger`'s — and that has not been built.
