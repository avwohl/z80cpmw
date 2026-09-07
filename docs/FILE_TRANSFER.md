# File Transfer with R8 / W8 — where your files go

`R8` and `W8` are the CP/M utilities that move files between the emulated CP/M
system and your real (host) computer:

- **`W8 name`** — **export**: copy a file *out* of CP/M onto the host.
- **`R8 name`** — **import**: copy a host file *into* CP/M.

The one question everyone hits is *"I ran `W8 myfile.com` — where did it actually
go?"* The answer depends on the platform, because every modern OS sandboxes an
app's storage. This page gives the exact location and the quickest way to open it
on each port of the emulator.

> **Windows (`z80cpmw`) is authoritative here** — it is this repository and the
> behaviour is verified against the source. The macOS / iOS / Android sections were
> verified against the **current `main`** of the sibling ports (`ioscpm` build 41 /
> v1.4.11, and `cpmdroid`) on **2026-07-23**. Those ports move independently, so a
> later build may differ — confirm in the app if in doubt.

## Quick reference

| Platform | `W8` writes / `R8` reads | Fastest way to open it |
| --- | --- | --- |
| **Windows** (unpackaged) | `%LOCALAPPDATA%\z80cpmw\data\` | *Emulator → Settings → Open Folder* |
| **Windows** (Microsoft Store / MSIX) | `…\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\data\` | *Emulator → Settings → Open Folder* (don't type the path by hand — the app shows it) |
| **Windows** (any) — `R8` with a full path | reads exactly the file you named | — |
| **Windows** (any) — `W8` with a full path | writes exactly where you named, and prints the resolved path | — |
| **macOS** | `~/Library/Containers/com.awohl.cpm/Data/Documents/Exports\|Imports/` | menu **Open Exports/Imports Folder** → Finder |
| **iOS / iPadOS** | app's **Documents** → `Exports/` and `Imports/` | **Files app → On My iPhone/iPad → Z80CPM → Exports/Imports** |
| **Android** | `/storage/emulated/0/Android/data/com.awohl.cpmdroid/files/Exports\|Imports/` | a third-party file manager, or a PC over USB (see below) |

---

## Windows (`z80cpmw`)

On Windows you control the location directly, and this is the **recommended**
workflow:

**`R8` takes a full path — even on the Store build.** z80cpmw is a full-trust
app, so absolute, UNC, and rooted paths are used verbatim and nothing is
redirected:

```
R8 C:\Users\me\Downloads\prog.com   → reads from Downloads
```

**`W8` takes one too**, in the form `W8 <cpmname> [hostpath]`:

```
W8 OUT.COM C:\Users\me\Desktop\out.com   → writes to Desktop
W8 OUT.COM                               → the data folder, as out.com
```

This document used to say `W8` took a host path when it did not, then said it
did not once that was corrected. Both are now history, and it is worth being
exact about where the answer comes from, because it does not come from this
repository. `W8.COM` and `R8.COM` are not built here and never were: they are
assembled from `src/w8.asm` and `src/r8.asm` in
[`avwohl/romwbw_disks`](https://github.com/avwohl/romwbw_disks) by that
repository's `tools/build_utils.sh`, and written into slice 0 of `hd1k_combo` —
the only image that carries them, and the only thing that repository adds to the
stock upstream disk set. So the pair reaches you inside a disk image you
download, and until you have downloaded the combo there is no `W8` to run.

There is no compiled-in pin deciding which copy you get any more. `RELEASE_TAG`
is gone from `DiskCatalog.cpp`; the only address compiled in for anything the
catalog serves is the index itself, `catalogv0::INDEX_URL` (the in-app help
fetches its own text from elsewhere, and the crash dialog links to the issue
tracker — neither carries a ROM or a disk image). Which release's
combo you download is the *RomWBW release:* picker on *Emulator → Settings →
Disk Images*. That choice does not change the utilities, though, and it is not
supposed to: `tools/build_utils.sh` takes no version argument — one build serves
every RomWBW release — because `W8`/`R8` talk to the emulator's private
host-file function block (`0xE1`–`0xEA`), which belongs to the *interface*
version and which RomWBW itself knows nothing about. Both published releases
therefore carry the same `w8.com`, the one that takes the path and asks the
emulator whether it is safe to before doing so (`HBF_HOST_CAPS`, `0xE9` in
`romwbw_emu/src/hbios_dispatch.h`). That is enforced at build time rather than
assumed: `build_utils.sh` refuses to publish a `w8.com` that does not contain
the probe's three bytes, `06 e9 cf`.

(The catalog also marks which images carry the pair — `"host_transfer": true`,
which is set on `hd1k_combo` and on nothing else in either published release —
and z80cpmw parses it into `DiskItem::hostTransfer` at `CatalogV0.cpp:279`. But
no code in this application reads that flag yet, so it is not why the combo is
the disk you want; it just happens to agree.)

That same `w8.com` also *tells you where the file went*, which is worth more on
this platform than on any other: it asks the emulator for the effective
destination (`HBF_HOST_GETNAME`) instead of echoing what you typed, so a bare
name prints as the data folder, and an installed Store/MSIX build prints the
redirected `LocalCache` path the OS actually wrote to rather than the
`%LOCALAPPDATA%` path the app asked for.

If your `W8` prints `Usage: W8 <cpmname>` with no `[hostpath]`, you are running
a combo image older than the one the catalog now serves. An install that
predates the move keeps the copy it already had: `DiskMigrationV0` renames
`hd1k_combo.img` onto the v0 name `hd1k_combo-v0-3.5.1.img` rather than
replacing it, deliberately, and while the disk ledger can tell that such an
image is *superseded*, nothing in this application calls `getFreshness()` yet —
so no automatic refresh will happen and no control offers one. Replace it by
hand: select it on *Emulator → Settings → Disk Images*, press **Delete**, then
**Download**. Do that knowing what it costs — a downloaded disk is a writable
CP/M volume, so any file you have saved inside that combo goes with it.

**A bare name goes to the app's data folder.** `W8 out.com` (no path) lands in:

- Unpackaged / NSIS install: `%LOCALAPPDATA%\z80cpmw\data\`
- Microsoft Store / MSIX install: the OS *redirects* `%LOCALAPPDATA%` writes into
  the package container, so the real location is
  `%LOCALAPPDATA%\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\data\`.
  The `<hash>` is derived from the package publisher and differs between the Store
  build and the signed beta (they install side-by-side), so **don't try to type
  this path** — let the app tell you.

**How to find the data folder.** The app always shows the *real, resolved* path
(it follows the MSIX redirection for you) in three places:

- **Emulator → Settings** — a read-only, copyable "Data folder" field with an
  **Open Folder** button that opens it in Explorer.
- **Help → About** — the same path under "Data Folder (disks and R8/W8 transfers)".
- **The boot banner** — printed in the terminal when the app starts.

## macOS (`ioscpm`, Mac Catalyst)

The macOS app is sandboxed, so its "Documents" folder lives inside the app
container. `W8`/`R8` always use fixed subfolders — there is **no per-transfer
Save/Open dialog**:

- Export (`W8`): `~/Library/Containers/com.awohl.cpm/Data/Documents/Exports/`
- Import (`R8`): `~/Library/Containers/com.awohl.cpm/Data/Documents/Imports/`

**How to find / use them** (menu items; same on macOS and iOS):

- **Open Exports Folder** — opens `Exports/` in Finder to collect what `W8` wrote.
- **Open Imports Folder** — opens `Imports/`.
- **Import File… (for R8)** — a standard file picker that copies any host file into
  `Imports/`; then run `R8 name` to read it. This is the supported way to pull in a
  file from an arbitrary location, since `R8` itself only reads `Imports/` and `W8`
  only writes `Exports/`.

(The `~/Library/Containers` path is hidden by default in Finder, so use the menu.)

## iOS / iPadOS (`ioscpm`)

Same app as macOS, same fixed subfolders inside the app's sandbox **Documents**
directory:

- Export (`W8`): `Documents/Exports/`
- Import (`R8`): `Documents/Imports/`

**How to find them:** the app publishes its Documents folder to the system **Files
app**, so:

> **Files → Browse → On My iPhone (or iPad) → Z80CPM → Exports / Imports**

(The app's display name is **Z80CPM**.) From there you can move, share, or open
exported files in any other app. In-app menu items do the same: **Open Exports
Folder** / **Open Imports Folder** jump to the Files app there, and **Import File…
(for R8)** opens the system picker to copy any file into `Imports/` for a later
`R8`. File Sharing over a cable (Finder/iTunes) reaches the same folders.

To import: stage the file in `Imports/` — via **Import File…**, Files, AirDrop,
etc. — then run `R8 name` (or plain `R8` to pull the first file in the folder).

## Android (`cpmdroid`)

`W8`/`R8` use fixed folders in the app's *external files* directory:

- Export (`W8`): `/storage/emulated/0/Android/data/com.awohl.cpmdroid/files/Exports/`
- Import (`R8`): `/storage/emulated/0/Android/data/com.awohl.cpmdroid/files/Imports/`

**Finding these is the catch on Android.** Since Android 11, the `Android/data/…`
tree is **hidden from the built-in Files app / document picker**, even though the
app writes there without needing any storage permission. After `W8`, the app just
shows a toast ("W8: Saved …"); it does **not** offer a Share sheet or a "save as"
picker. To get an exported file off the device you currently need one of:

- a **third-party file manager** that can browse `Android/data/…` (many can), or
- a **PC over USB** (MTP), navigating to `Android/data/com.awohl.cpmdroid/files/Exports`, or
- **`adb pull`**:
  `adb pull /storage/emulated/0/Android/data/com.awohl.cpmdroid/files/Exports/out.com`

To import, place the file in the matching `Imports/` folder the same way, then run
`R8 name`.

> **Known limitation / parity gap:** cpmdroid has no Share/SAF export UI, so the
> export folder is awkward to reach on modern Android. Adding a `Share`
> (`ACTION_SEND`) or "save to…" (`ACTION_CREATE_DOCUMENT`) step is the tracked
> parity improvement — see [FEATURE_PARITY.md](../FEATURE_PARITY.md), item 4.

---

## "I can't find my exported file" — checklist

0. **Windows: read what `W8` printed.** It prints the real destination,
   redirection and all — not the name you typed. That is usually the whole
   answer. (If it printed no path at all, see the `W8` note above.)
1. **Did you give a full path?** (Windows only.) If so it's at that exact path,
   not in any data folder.
2. **Windows:** open *Emulator → Settings → Open Folder*, or read the path shown in
   *Help → About* / the boot banner. Don't guess the `Packages\…` path.
3. **macOS:** use the **Open Exports Folder** menu item — the container path is
   hidden in Finder.
4. **iOS/iPadOS:** **Files → On My iPhone → Z80CPM → Exports**.
5. **Android:** it's in `Android/data/com.awohl.cpmdroid/files/Exports/`, which the
   stock Files app hides — use a capable file manager, a PC over USB, or `adb pull`.

## See also

- [CONFIGURATION.md](CONFIGURATION.md) — the config file and its data-folder note.
- [FEATURE_PARITY.md](../FEATURE_PARITY.md) — item 4 tracks R8/W8 across the ports.
- In-app **Help → Getting Started** (each port ships its own File Transfer topic).
