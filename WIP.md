# Work In Progress — z80cpmw

Working notes / handoff. Not user-facing docs, and not a record of what shipped:
**what shipped is in [`CHANGELOG.md`](CHANGELOG.md)**. Open items only.

**See also [`todo.txt`](todo.txt)** — the list form, and where the work that
needs a Windows machine is gathered. Anything below that is also an action item
appears there too; this file keeps the longer explanations.

The 1.0.15 round this file used to describe is finished and released, and its
history was moved out — the keymap, mouse copy/paste and About path fix are
`[1.0.15]`, the window position/size, auto-size and scrollable Help are
`[1.0.16-beta]`, the R8/W8 absolute-path handling and data-folder display are
`[1.0.14]`, and the MSIX packaging story from 1.0.20 through the Store release
is `[1.0.20]`, `[1.0.21-beta]` and `[1.0.22]`. The W8-under-MSIX question that
was this file's open question is answered, in source and in
[`docs/FILE_TRANSFER.md`](docs/FILE_TRANSFER.md).

Current version: **1.0.22** (Store, 2026-08-23), with the same build signed for
sideloading as 1.0.22-beta. Since `31d01c6` the version is edited only in
`z80cpmw/Version.h`; the MSIX and NSIS scripts derive theirs from it.

## Building it

`z80cpmw/z80cpmw.vcxproj` uses `PlatformToolset` `v145` (Visual Studio 18) and
expects the wxWidgets headers under `C:\temp\vcpkg\installed\x64-windows\include`.
Both are right for VS 18 / MSVC 14.51, and the four libraries the project links
(`wxbase33u{,d}`, `wxmsw33u{,d}_core`) are exactly what the vcpkg port installs.
From a bare machine that is:

    git clone https://github.com/microsoft/vcpkg C:\temp\vcpkg
    C:\temp\vcpkg\bootstrap-vcpkg.bat
    C:\temp\vcpkg\vcpkg.exe install wxwidgets:x64-windows   # ~40 min
    build.bat                                               # Debug
    build_release.bat                                       # Release

Both configurations build clean against `romwbw_emu` `2dbf6f2` and `cpmemu`
`9fee3c2`. The sibling checkouts have to be present beside this one — the
project compiles the core straight out of them.

**The terminal conformance suite needs none of that.** `TerminalView.cpp`
includes only `pch.h`, which is Win32 and the standard library, so
`tests\run_tests.bat` builds and runs on any machine with a compiler, whether or
not the app itself can be built.

## Not verified on hardware

The behaviour is confirmed in source and documented; what has never happened is
a run against the **installed Store build**, which is the only thing that can
reproduce MSIX file-system redirection — a local unpackaged build cannot.

- [ ] ~~`W8 C:\Users\<you>\Desktop\test.txt` → the file appears on the
      Desktop.~~ **This checkbox tests the wrong thing.** The `W8.COM` in the
      bundled images reads only the parsed FCB and never the command tail, so it
      cannot take a host path whatever the Windows backend does — the failure
      would be guest-side, not MSIX redirection. `W8 <cpmname> [hostpath]` is
      upstream in `romwbw_emu` `98eb6a1` and arrives when the images are
      refreshed; re-instate this then. `R8` with a full path is unaffected and
      is checked below.
- [ ] `W8 getkey2.com` (bare name) → lands under
      `…\Packages\AaronWohl.Z80CPM_*\LocalCache\Local\z80cpmw\data\`.
- [ ] That path agrees with what About, Settings → Open Folder, and the boot
      banner display.
- [ ] `R8 C:\Users\<you>\Desktop\getkey2.com` → reading an arbitrary user path
      works.

Also unverified, and not automatable from here: keystroke delivery to CP/M,
mouse copy/paste rendering, and the first-run Help auto-open visuals. Each
needs a hands-on pass.

## The core is shared by reference; `emu_io_common.cc` is not

`z80cpmw.vcxproj` compiles the core straight out of the sibling repos —
`$(SolutionDir)..\cpmemu\src\qkz80*` and `..\romwbw_emu\src\{emu_init,
hbios_cpu,hbios_dispatch}.cc` — so upstream fixes to those arrive on the next
build with nothing to do. **`emu_io_common.cc` is the exception: the project
references it nowhere**, and `emu_io_windows.cpp` carries its own copies of the
eleven functions it holds — `emu_disk_{open,close,read,write,flush,flush_all,
size}`, `emu_file_{load,load_to_mem,save}`, `emu_get_time`.

That is a deliberate split (the Windows versions use Win32 handles, not
`FILE*`), but it means a fix landing in `emu_io_common.cc` never reaches here
and nothing reports the drift.

Both halves of what this section used to ask for are now done. The file says so
itself: `emu_io_windows.cpp`'s header names `emu_io_common.cc` as the shared
original and lists the eleven, so the next person looks. And the eleven have
been diffed against upstream. The result was mostly reassuring — `ce9268f`'s
hardening was imported *from* this port in the first place (`79ddfc4`, cited in
its own commit message), so nine of the eleven already carried it, and `573c7cc`
does not apply at all because these versions never used `fseeko`/`ftello`. Three
real differences came out of it and are fixed in `[Unreleased]`: the 128 MB load
cap, the missing rewind, and the header comment.

One difference runs the other way and is upstream's to fix: `emu_io_common.cc`'s
`emu_file_save()` uses `rename()` to replace the target, which ISO C leaves
undefined when the target exists and which both the MSVC CRT and mingw's msvcrt
refuse outright. This port uses `MoveFileEx` and is fine; the shared copy has a
dormant bug on Windows. It is in `todo.txt`.
