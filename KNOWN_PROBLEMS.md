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
for it exists anywhere — `dist\` holds `z80cpmw-1.0.21-beta.pdb` and
`z80cpmw-1.0.23-beta.pdb` and nothing else, and `bin\Release\z80cpmw.pdb` has
been rebuilt at 1.0.25 since. If `dist\z80cpmw-1.0.24-store.msix` is submitted,
its crash dumps are unreadable exactly as 1.0.22's are; submitting 1.0.25
instead is the only version of that choice with symbols behind it. 1.0.23
escapes by accident rather than by design: its beta was cut from the same build
with `-SkipBuild`, so `z80cpmw-1.0.23-beta.pdb` symbolicates the Store binary
too — which is a property of how that release happened to be cut, not a rule.

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
`packaging/scripts/verify-disk-assets.sh` handles both: it picks the diskdef
from each image's own geometry, and then refuses a listing with no names in it
*or* with any name that is not printable ASCII, before it goes looking for a
filename — 233 of those 312 garbage names are not. Anyone checking an image **by
hand** has to do the same by eye. The rule:
`hd1k_combo.img` is `wbw_hd1k_0` because of its 1 MB MBR prefix, a plain 8 MB
image is `wbw_hd1k`.

A `./diskdefs` in the current directory also shadows the system file completely
rather than adding to it, so a partial local copy makes every other format
"unknown". The script sidesteps that by copying `romwbw_emu/disks/diskdefs`
whole into its work directory.

## Nothing transfers from the games disk, and the gate will never say so

`hd1k_games.img` carries **neither `R8.COM` nor `W8.COM`**, so a user who
follows the in-app help to the Games disk and then tries to move a save file has
no utility to run — on either side. Measured on the image this build ships,
`bin\Release\disks\hd1k_games.img`: the 8.3 directory pattern `R8      COM`
occurs **zero** times in it and `W8      COM` zero times, against one of each in
`hd1k_combo.img` — so the search finds them where they exist, and the answer for
the games disk is really nothing rather than a bad search.

The half that will not change is the gate.
`packaging/scripts/verify-disk-assets.sh` is severity-split by image: only an
image larger than 8 MB carrying a `55 AA` MBR signature reaches `bad()` for a
missing utility, while a plain 8 MB image gets an `info` line and never touches
the failure count — on that script's own stated reasoning that a secondary data
disk carrying neither utility is a choice and not a fault. `hd1k_games.img` is
exactly 8,388,608 bytes, so it takes the info branch every time. **A PASS from
that script is compatible with the games disk having no R8 and no W8**, and is
meant to be. Anyone refreshing the images who wants this closed has to check
that image by hand and put the utilities on it deliberately; nothing will go red
if they forget, and the sentence about it in the in-app File Transfer topic is
the only place a user is told.
