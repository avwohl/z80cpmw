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

Version in the tree: **1.0.30** (`z80cpmw/Version.h`), packaged and signed on
2026-09-10 as `dist\z80cpmw-1.0.30-beta.msix` and not yet published. The version
released on the Store is **1.0.29**, measured with `tools/check-store-version.sh`
on 2026-09-10; this line said **1.0.22** until 2026-09-09 and **1.0.25** until
2026-09-10, and was four releases behind each time. Since `31d01c6` the
version is edited only in `Version.h`; the MSIX and NSIS scripts derive theirs
from it, and todo.txt reserves bumping it for the moment something is packaged —
so a tree with unpackaged work in it sits under `[Unreleased]` at the number of
the last package, which is what 1.0.29 is now.

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

**Four of the seven suites need none of that.** `tests\run_tests.bat` runs the
terminal conformance suite, then the help renderer and asset suite, then the
rendering suite, then the disk provenance suite, all before the two blocks that
`exit /b 1` when a sibling checkout is missing: `TerminalView.cpp` and
`HelpAssets.cpp` reach for Win32 and the standard library and nothing else, and
`DiskLedger.cpp` reaches for neither while `DiskHash.cpp` wants only Win32 and
bcrypt, so those four build and run on any machine with a compiler, whether or
not the app itself can be built. The
rendering suite wants one thing more — an interactive window station, since it
renders a real window with `PrintWindow(PW_RENDERFULLCONTENT)` and samples the
pixels — and prints SKIP and exits 0 where there is no desktop rather than
turning CI red for want of one. The host file transfer suite needs
`..\romwbw_emu`, the HBIOS suite needs `..\cpmemu` as well, and the
configuration diagnostics suite is last because it needs both on the include
path even though it links nothing out of either. All **eight** pass: 516, 355,
50, 175, 207, 66, 36 and 374 checks, **1779** in total.

**One of them reads the environment, so keep it hermetic.** The interface-v0
catalog suite exercises `catalogv0::indexUrl()`, which consults
`$ROMWBW_INDEX_URL` — and until 2026-09-09 it had no test that set the variable
and no code that cleared it, so it failed **eight checks** for anyone who had
that variable exported. That is precisely the person working on the catalog
index, and the failure looked like their own change. `main()` clears it before
the first section now, and `test_index_url_environment` sets and unsets it
deliberately. Any new suite that reads the environment owes the same.

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

**A posted `WM_COMMAND` bypasses the menu's enabled state, so it cannot tell you
whether a user could have done the same thing.** This one shipped a bug. The
driver sends `WM_COMMAND ID_EMU_START` straight to the frame, which the window
procedure handles whether or not `Emulator > Start` is greyed — and on
1.0.26-beta it *was* greyed, because `updateMenuState()` still read
`hasROM()` and the package no longer carries a ROM. So the scripted F5 started
the machine on a machine where a human's F5 did nothing, and every check passed.
The F5 accelerator fails the same way a click would: Windows suppresses an
accelerator whose menu item is disabled.

Read the state as well as sending the command. `GetMenuState(GetMenu(hwnd), id,
MF_BYCOMMAND)` and test `& (MF_GRAYED | MF_DISABLED)`; a return of `0xFFFFFFFF`
means the id is not in the menu at all, which is its own finding. Any check that
asserts "the user can do X" has to look at that, not at what a posted message
achieved.

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

Two more from the 2026-09-10 pass, both of which looked like the application
failing and were the driver failing.

**`FindWindowW(null, "Settings")` does not reliably find the dialog from an
`Add-Type -MemberDefinition` P/Invoke**, and the failure is silent: the handle
comes back zero for twenty seconds and the obvious reading is that the dialog
never opened. It opens in well under a second — measured by enumerating instead.
Enumerate the process's own top-level windows (`EnumWindows` filtered on
`GetWindowThreadProcessId`) and match on class `#32770` plus the title; that also
gives you a list to print when the match fails, which a null handle does not.

**`MF_BYCOMMAND` is `0x0000`; `0x0400` is `MF_BYPOSITION`.** Passing the latter to
`GetMenuState` asks for a menu *position* of 2004, which does not exist, so it
returns `0xFFFFFFFF` — the same value that means "this id is not in the menu at
all". The check written to prove `Emulator > Settings` was clickable reported it
missing instead, on a build where it was present and enabled.

**Declare `SendMessageW` with `CharSet=CharSet.Unicode` when the last parameter is
a `StringBuilder`.** The default marshalling is ANSI, so a `WM_GETTEXT` against a
Unicode window reads the UTF-16 buffer as bytes and stops at the first `NUL`:
the frame's title came back as `z`, one character of `z80cpmw - Z80 CP/M
Emulator`. Every label and every combo selection read that way is truncated to
its first character, which looks like empty controls rather than a broken read.

Four more from the 2026-09-09 pass over the Settings dialog, which cost the same
hour each.

**UI Automation is useless against this dialog.** wx exposes no UIA or MSAA
roles, so every control — the notebook tabs included — comes back as a generic
`Pane` with no `TabItem` and no `SelectionItemPattern`. It looks like exactly the
right tool and returns nothing usable. Do not spend the hour.

**Change the notebook page by posting `WM_KEYDOWN` `VK_RIGHT` (0x27) to the tab
control.** comctl32's tab proc turns that into a real `TCN_SELCHANGE`, which is
what makes wx swap the panel. `TCM_SETCURSEL` moves the tab strip and tells wx
nothing, so the old page stays up; `TCM_GETITEMRECT` carries a pointer and
crashes the app, per the rule above. Loop until the control you want reports
visible rather than counting key presses.

**`GetClassNameW` returns an empty string when it is called from inside a
PowerShell `EnumChildWindows` delegate**, while working perfectly outside one.
Collect the child handles in a native `Add-Type` helper and read the class names
in a second pass. `WM_GETTEXT` and `WM_SETTEXT` are marshalled cross-process by
USER32 and work either way, so those are how to read and set a field.

**A control off the visible page is still enumerable** (`IsWindowVisible` false,
everything else readable), so every page's state can be dumped without switching
to it; the switch is only needed for a screenshot. The trap: **a `wxStaticText`
returns its whole label to `WM_GETTEXT` even when the screen shows a fraction of
it.** A text dump therefore cannot see a clipped label, which is how a note whose
warning was cut off after seventy characters passed every scripted check and
shipped. Anything about what a label *says* has to be read off the `PrintWindow`
bitmap.

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
