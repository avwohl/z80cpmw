# Known problems

Standing facts that will never be "done": limits of tools this project depends
on, and traps set for whoever edits certain files next. None of them is an open
item, which is why none of them is in `todo.txt` — an entry here is something to
know, not something to do.

Delete an entry only when it stops being true.

---

## 1.0.22 cannot have a crash report resolved

There is no `.pdb` for the shipped 1.0.22 build on any channel, and there never
will be. The symbols are gone, and a rebuild — against wxWidgets 3.3.3, or
against whatever vcpkg has moved on to — is a different binary with different
symbols. A stack from 1.0.22 will not symbolicate.

`build-msix.ps1 -Beta` now keeps the `.pdb` so that a future beta cannot repeat
it. That is in `todo.txt`, because it has never been run.

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
