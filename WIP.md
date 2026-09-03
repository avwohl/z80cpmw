# Work In Progress — z80cpmw

Working notes / handoff. Not user-facing docs, and not a record of what shipped:
**what shipped is in [`CHANGELOG.md`](CHANGELOG.md)**. Open items only.

**See also [`CLAUDE.md`](CLAUDE.md)** for the build recipe and the standing
rules, and [`MANUAL_CHECKS.md`](MANUAL_CHECKS.md) for what needs a person. This
file keeps the longer explanations that do not fit either.

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

The sibling checkouts have to be present beside this one — the project compiles
the core straight out of them, with no version gate, so a core that grows a
required backend function breaks the link here on the next build and not before.
That stopped being a live question on 2026-08-28: the tree was built and driven
(`978b623`), so `emu_host_file_get_read_name()` in `emu_io_windows.cpp` — added
because `hbios_dispatch.cc` had grown a requirement, and until then never
compiled with MSVC — links. Nobody wrote down which sibling shas that build was
taken against, which is worth doing next time. The last reading this file
recorded is `romwbw_emu` `17cd380` (`v1.36-1`) and `cpmemu` `9fee3c2`, and both
checkouts stand five commits past it as of 2026-08-28.
`tools/check-sibling-drift.sh` reports where the siblings stand.

**Three of the six suites need none of that.** `tests\run_tests.bat` runs the
terminal conformance suite, then the help renderer and asset suite, then the
rendering suite, all before the two blocks that `exit /b 1` when a sibling
checkout is missing: `TerminalView.cpp` and `HelpAssets.cpp` reach for Win32 and
the standard library and nothing else, so those three build and run on any
machine with a compiler, whether or not the app itself can be built. The
rendering suite wants one thing more — an interactive window station, since it
renders a real window with `PrintWindow(PW_RENDERFULLCONTENT)` and samples the
pixels — and prints SKIP and exits 0 where there is no desktop rather than
turning CI red for want of one. The host file transfer suite needs
`..\romwbw_emu`, the HBIOS suite needs `..\cpmemu` as well, and the
configuration diagnostics suite is last because it needs both on the include
path even though it links nothing out of either. All six pass: 516, 244, 50, 66,
36 and 108 checks, 1020 in total.

## Not verified on hardware

Moved to [`MANUAL_CHECKS.md`](MANUAL_CHECKS.md), which is the checklist form:
what to do, in what order, and what right looks like. Two checks are open there
— file transfer under an **installed** MSIX, which is the only thing that
reproduces file-system redirection, and the hands-on pass over keystroke
delivery, mouse copy/paste rendering and the first-run Help window. A check is
deleted from that file once someone runs it, and the result goes to
`CHANGELOG.md` under **Verified**.

## Driving the app from a script

`MainWindow.cpp` is in no suite — it needs wxWidgets and a real window — so the
changes that live there were verified by building the app into a private
directory and driving it with `WM_COMMAND` and `PrintWindow`, with the real
`z80cpmw.json` backed up first and restored byte-identical afterwards. Three
things cost an hour each on 2026-08-28 and are worth not rediscovering.

**Common-control messages that carry a pointer are not marshalled across a
process boundary.** `TCM_GETITEMRECT`, `LVM_GETITEMTEXTW` and `LVM_SETITEMSTATE`
all take an address, and one sent from another process hands the app the
*driver's* address: it dereferences it and dies with an access violation inside
`comctl32`. That crashed z80cpmw twice and wrote two dumps. Either allocate the
structure inside the target with `VirtualAllocEx` / `WriteProcessMemory`, or
stay on pointer-free messages.

**Call `SetProcessDPIAware()` before measuring anything.** The display here is at
200% scaling, and a DPI-unaware driver process gets every `GetWindowRect` result
halved by virtualisation — the 900x819 Settings dialog reads back as 450x410,
which looks exactly like `SetSize` being ignored.

**wx's notebook tab control is class `_wx_SysTabCtl32`, not `SysTabControl32`.**
A `FindWindowEx` on the documented name finds nothing.

## The core is shared by reference; `emu_io_common.cc` is not

`z80cpmw.vcxproj` compiles the core straight out of the sibling repos —
`$(SolutionDir)..\cpmemu\src\qkz80*` and `..\romwbw_emu\src\{emu_init,
hbios_cpu,hbios_dispatch}.cc` — so upstream fixes to those arrive on the next
build with nothing to do. **`emu_io_common.cc` is the exception: the project
references it nowhere**, and `emu_io_windows.cpp` carries its own copies of the
twelve functions it holds — `emu_disk_{open,close,read,write,flush,flush_all,
size}`, `emu_file_{load,load_to_mem,save}`, `emu_get_time` and `emu_rename`.

That is a deliberate split (the Windows versions use Win32 handles, not
`FILE*`), but it means a fix landing in `emu_io_common.cc` never reaches here
and nothing reports the drift.

Both halves of what this section used to ask for are now done. The file says so
itself: `emu_io_windows.cpp`'s header names `emu_io_common.cc` as the shared
original and lists the twelve, so the next person looks. And they have
been diffed against upstream. The result was mostly reassuring — `ce9268f`'s
hardening was imported *from* this port in the first place (`79ddfc4`, cited in
its own commit message), so nine of them already carried it, and `573c7cc`
does not apply at all because these versions never used `fseeko`/`ftello`. Three
real differences came out of it and are fixed in `[Unreleased]`: the 128 MB load
cap, the missing rewind, and the header comment.

One difference ran the other way and upstream has taken it: `emu_io_common.cc`
grew its own `emu_rename()`, which is `MoveFileExA(..., MOVEFILE_REPLACE_EXISTING)`
on Windows and plain `rename()` elsewhere, and `emu_file_save()` now goes
through it. The twelfth hand-synced function here is that same shim.

One trap is left, and it is in [`KNOWN_PROBLEMS.md`](KNOWN_PROBLEMS.md):
`emu_host_path_basename()` is declared in `emu_io.h` but defined only in
`emu_io_common.cc`, so the first call added to this port links against nothing.
