# Manual checks

Checks that need a person: an installed package, keys pressed, a screen watched.
Nothing here can be settled by reading the source or by any test in this
repository, which is why none of it belongs in a to-do list — nothing here can
be closed by writing code.

**Delete a check once someone has run it.** The result belongs in `CHANGELOG.md`
under **Verified**, not here. A check that has been run and left in place turns
this file into an accumulating record instead of a work list.

---

## 1. File transfer under an installed MSIX

The behaviour is confirmed in source and documented. What has never happened is
a run against an **installed** package, which is the only thing that reproduces
MSIX file-system redirection — a local unpackaged build cannot, whatever it
prints.

**You do not need the Store build.** Sideload
`dist\z80cpmw-1.0.22-beta.msix` (attached to the `v1.0.22-beta` release); it is
the same binary and also MSIX.

- [ ] `W8 TEST.TXT C:\Users\<you>\Desktop\test.txt` → the file appears on the
      Desktop, and `W8` prints that path rather than the name you typed.
      Re-instated with the `v1.4.12` catalog repin: the image the app downloads
      now carries a `w8.com` that takes a host path and probes `HBF_HOST_CAPS`
      first — verified in the published bytes, which contain
      `Usage: W8 <cpmname> [hostpath]` and `06 E9 CF` where the old image had
      neither. If your `W8` still prints `Usage: W8 <cpmname>`, you have the old
      image cached in the data folder; delete it and let the app re-download.
- [ ] `W8 getkey2.com` (a bare name) → the file lands under
      `…\Packages\AaronWohl.Z80CPM_*\LocalCache\Local\z80cpmw\data\`.
- [ ] The path `W8` reports agrees with what About, Settings → Open Folder and
      the boot banner display. All four are supposed to name the same folder.
- [ ] `R8 C:\Users\<you>\Desktop\getkey2.com` → reading an arbitrary user path
      works. `R8` already takes a host path, so this one is testable now.
- [ ] `R8` a file, then check what it printed. `emu_host_file_get_read_name()`
      compiles and links now — the build settled that much — but nothing has
      ever read what it returns. The guest should be told the **resolved** path
      — the `LocalCache` one for a bare name — not the name it typed. If it
      prints nothing, the empty-string path is being taken and the resolution is
      not reaching the guest.

## 2. Keystroke delivery, mouse copy/paste, and the first-run Help window

Never watched by a person, and nothing here can automate them.
`tests\run_tests.bat` is **1779 checks in eight suites** now — 516 terminal
conformance, 374 configuration diagnostics, 355 help renderer and assets, 207
interface-v0 catalog, 175 disk provenance, 66 host file transfer, 50 rendering
conformance, 36 HBIOS host file extension.

**A cell becoming a pixel is no longer a person's job.** `tests/test_render.cpp`
creates a real window, drives the parser with real bytes, asks the DWM for the
window with `PrintWindow(PW_RENDERFULLCONTENT)` and samples the bitmap, so the
check that used to want someone looking at a screen and agreeing that `ESC[31m`
is red is gone, and the machine that replaced it found a gap the eye had not:
SGR 90–97 and 100–107 were not handled at all. What it reads is *which colour a
cell is drawn in* (SGR 30–37, 40–47, 90–97, and that no colour is drawn
untranslated) and *which face* (SGR 1 is a heavier face and not merely a
brighter colour, SGR 4 draws a rule under the glyph, the two compose, SGR 5
blinks the character while a selected cell keeps its highlight, and the blink
tick invalidates only the rows carrying a blinking cell). Because the font is
`CLEARTYPE_QUALITY`, "which colour is this cell" is a nearest-neighbour question
asked of the pixel furthest from the background rather than an equality test;
and with no interactive window station the suite prints SKIP and exits 0, so a
green run on a machine with no desktop is not evidence.

**A keypress becoming a byte still is.** Every suite hands the terminal bytes it
composed itself, and the conformance suite injects key records past the layer
these checks are about. Nothing below can be settled by reading the source.

1. **Keystrokes reach CP/M.** At the CP/M prompt, type printable text, then
   backspace over it, then the arrow keys, then Ctrl+C. Right: what you typed
   appears, editing works, Ctrl+C reaches the guest rather than the host.
2. **Modified arrows.** Ctrl+Left / Ctrl+Right / Ctrl+Up / Ctrl+Down should send
   the xterm modified forms `\E[1;5D` `\E[1;5C` `\E[1;5A` `\E[1;5B`. Watch for
   the terminal or the window manager eating them before the app sees them —
   the automated suite injects records past that layer and cannot see it happen.
3. **Mouse copy/paste.** Drag-select a region, copy, paste it back. Right: the
   selection highlights while dragging, the clipboard holds the cell text with
   trailing spaces trimmed, and the paste arrives as keystrokes.
4. **First-run Help.** With no config file present, launch. Right: the Help
   window opens by itself on Getting Started and renders it. The gate is
   `welcomeShown`, which is set and saved the first time it fires, so relaunch
   and confirm it does *not* open again. While the window is up, walk to one of
   the remote topics: it must either fetch or fail visibly, never leave a blank
   pane. Then read the status line — it names the copy you are reading
   ("(downloaded)", "(offline copy, saved …)", "(bundled with the app)",
   "(this session's copy)"). Pull the network out and reopen that topic: it
   should come back from the cache and say so. The rule is covered by the help
   suite; the *offline arm through the window* is not, because it needs WinHTTP
   to fail machine-wide.

## 3. The disk status column, and the verdict behind it

175 headless checks say what `DiskLedger` decides and that `DiskHash` measures
it correctly. None of them can see a word reach the screen:
`SettingsDialogWx.cpp` is in no suite and needs a real window, and the verdict
is computed on the `fetchCatalog` worker against a live catalog and a real data
folder, neither of which a suite has.

**The release picker should already be on RomWBW 3.5.1**, and that is now a
CHECK rather than a setup step. This paragraph used to read "set the release
picker to RomWBW 3.5.1 before any of this", which was right while the migration
left `core.romwbwVersion` empty and the index's `default: true` then chose 3.6.0
for a 3.5.1 library. `Config.cpp`'s `from_json` now reads the release out of the
mounted filenames on every load, so a machine whose slots name `-v0-3.5.1`
images arrives at Disk Images already on 3.5.1.

- [ ] With `-v0-3.5.1` images in the four slots and no `core.romwbwVersion` in
      the file, the picker reads **RomWBW 3.5.1** on opening Settings, without
      anybody setting it. If it reads 3.6.0 the backfill is not running, and
      everything below is looking at the wrong release's catalog: the list only
      ever holds the selected release's entries, so on 3.6.0 the migrated images
      are not in it at all.

Expect the `-v0-3.5.1` names §9 leaves behind rather than the bare ones — the
storage migration renames into `-v0-3.5.1`, `diskv0::PRE_V0_ROMWBW` is `"3.5.1"`
at `DiskMigrationV0.cpp:15`, and `v0NameFor` builds the target from it.

- [ ] Settings → **Disk Images** with `hd1k_combo-v0-3.5.1.img` in the data
      folder and no record for it in `disk_ledger.json`. Right: the status
      column says **"Differs from catalog"**, whichever ioscpm catalog that copy
      came from — `v1.4.12`'s combo hashes `89b8ae1a…` and `v1.4.5`'s does not
      match either, while the v0 3.5.1 catalog names `0ca4ec60…`. This is the
      migration case and it is the one most users are in: there is no ledger
      yet, so the app hashes the file once and says only what it can prove,
      which is that these are not the published bytes and nothing here can tell
      whether that is the superseded image or the user's own writing. The first
      fetch after an upgrade therefore reads ~49MB; watch that the dialog
      **stays responsive** while it does, which is the whole reason that work is
      on the worker.
- [ ] The exception, on a machine that *does* carry a ledger from a build that
      downloaded off ioscpm `v1.4.12` — a development machine, since no released
      build ever wrote one. Right: the same file reads plain **"Downloaded"**,
      because the migration carried a provenance of `89b8ae1a…` across with the
      rename and `diskv0::isEquivalentPriorImage` names that one pair as
      equivalent to `0ca4ec60…` (`DiskMigrationV0.cpp:120-127`). It is keyed on
      provenance, so it is the only route to that verdict: hand-editing the
      hashes into the ledger produces something else, and the bullet below says
      what.
- [ ] Open it a **second** time. Right: the same verdict, and no re-read — the
      measurement is cached in `data\disk_ledger.json` against the file's size
      and write time. If it re-hashes every time, the write time is not
      round-tripping and every launch will pay 211MB.
- [ ] **Delete** that disk and **Download** it again, then reopen Settings.
      Right: **"Downloaded"**, because the download was verified against the
      catalog hash and its provenance recorded. Check `data\disk_ledger.json`
      holds an `installedCatalogSha256` of `0ca4ec60…` for it — the `sha256` the
      v0 3.5.1 catalog gives `hd1k_combo-v0-3.5.1.img`, read out of the copy of
      that published document in `tests\test_catalogv0.cpp` — and **not** the
      `89b8ae1a…` of the ioscpm image it replaced.
- [ ] Boot the guest off that disk, **save a file** in CP/M, quit, and reopen
      Settings. Right: still **"Downloaded"** — the catalog has not moved, so
      writing to the volume must not make it look stale. This is the check that
      the verdict is provenance and not a byte comparison; if it says anything
      about an update, the design has been undone.
- [ ] Corrupt the copy by hand — open `data\hd1k_combo-v0-3.5.1.img` and change
      a byte well past the first megabyte — then reopen Settings. Right: still
      **"Downloaded"**, for the same reason. (`R8`/`W8` may of course break;
      that is not what this checks.)
- [ ] The superseded-and-pristine case, which is the only verdict that would
      ever get an image replaced without asking. **There is no longer a pin to
      move to produce it** — `RELEASE_TAG` is gone and the catalog is fetched,
      not compiled in — so it has to be staged in the ledger instead: with a
      freshly downloaded `hd1k_combo-v0-3.5.1.img`, close the app and edit
      `data\disk_ledger.json` so that both `installedCatalogSha256` **and**
      `measuredSha256` for that file read the same made-up 64 hex digits,
      leaving `measuredSize` and `measuredModified` exactly as they are. Right:
      **"Update available"** — provenance disagrees with the catalog and the
      bytes still hash to the provenance, which is `DiskLedger::freshness`'s
      definition of pristine. Change only `installedCatalogSha256` and leave
      `measuredSha256` real and it must read **"Update available (overwrites
      your changes)"** instead; that pair is what proves the two questions are
      being asked separately. Delete the doctored records afterwards.

## 4. Reset asks first

`onEmulatorReset()` used to reboot on the keystroke. It asks now, and
`MainWindow.cpp` is in no suite — it needs wxWidgets and a real window.

- [ ] With the machine **running**, Emulator → Reset. Right: a Yes/No box.
      **Press Enter**: it must cancel, not reboot — the box is `MB_DEFBUTTON2`
      precisely so the reflex answer is the safe one. Then No: whatever you had
      typed is still on screen. Then Yes: it really reboots.
- [ ] Make Ctrl+R the Reset shortcut — the switch on Settings → Keyboard, or
      `"ctrlRToCpm": false` in the config. Right: the Emulator menu grows a
      `Ctrl+R` hint on *Reset* (`rebuildAccelerators` registers it only when that
      flag is false, and runs on OK, so this needs no relaunch). Press Ctrl+R
      with the machine running: the same box, and CP/M never sees `^R`.
- [ ] Reset on a **stopped** machine. Right: **no dialog at all.**
      `onEmulatorStart()` cold-boots without confirmation, so confirming a
      stopped Reset but not a Start would be arbitrary.

## 5. The configuration report on screen

302 headless checks say what `ConfigManager::load()` collects. None of them can
see the block reach the terminal, or survive what clears it.

- [ ] Break `z80cpmw.json` by hand — delete a closing brace — and launch.
      Right: the boot output carries a **Configuration report** whose line
      begins "could not be read:", **names the backup file** (`z80cpmw.json.bad`,
      or `.bad2` … `.bad20` if that name is taken) and **gives the parser's line
      and column**. That block is the only place in the whole UI those three
      facts appear.
- [ ] With it on screen, press **Start**. Right: it is still there afterwards —
      `startEmulator()` clears the terminal and `printNotices()` reprints it.
      Then open Settings and press OK: it **stays** even so, because the file was
      renamed away and a save cannot overwrite what is no longer there.
- [ ] Separately, add a member nothing reads (`"banana": 1`) and launch. Right: an
      "unrecognised setting:" block saying it will be dropped at the next save —
      and pressing OK in Settings really does retract that one. This is the pair
      the one above is the exception to.
- [ ] Write `"keys"` as an **array** under `"keyboard"` and launch. Right: a
      "wrong kind of value:" block naming `keyboard.keys`. This is the case the
      diagnostics were written for: the file parses, so nothing is renamed and
      no dialog appears; none of your bindings are read (the built-in ones are
      used instead, filled in by `load()`); and the block on screen is the only
      warning the user gets.
- [ ] With that block up, open Settings and press **OK**. Right: it **stays** —
      and `z80cpmw.json` still holds the array exactly as you typed it, because
      `to_json` splices the section the loader could not read back in at the
      pointer it came from. `saveSettings()` retracts the *unrecognised setting*
      block and must not retract this one: a save is what makes that one false,
      and what keeps this one true.
- [ ] Then quit and relaunch. Right: the same block again, off the same file —
      nothing corrected the section and nothing wrote over it. Only correcting
      it by hand takes the block down, and it goes at the launch after that.
- [ ] Save a profile, break it by hand, load it. Right: a message box saying the
      profile could not be read, pointing at the terminal for the reason and
      saying current settings are unchanged — not a silent disappearance from
      the Load Profile list. Any block already on screen about `z80cpmw.json`
      is **still there**, with the profile's behind it: a profile that will not
      read changes no setting, so it may not take down the report about the
      file still in force.

## 6. The bell

- [ ] Settings → Terminal, clear **Sound the bell (BEL, character 7)**, OK. Get
      the guest to *print* `0x07` — `TYPE` a file with one in it, `PRINT CHR$(7)`
      in MBASIC, or any WordStar rejection. Typing Ctrl+G at the CCP prompt is
      not the same thing: the CCP echoes it as `^G` and nothing rings. Right:
      silence. Tick the box again and repeat: it rings.
- [ ] The half that matters at launch: with the box **cleared and saved**, quit,
      relaunch, and try `0x07` again **before opening Settings**. Right: still
      silent. `TerminalView` constructs with the bell on, so if `applyConfig()`
      ever stops calling `setBellEnabled()` this is a setting that only works
      after you change it a second time.

## 7. The Keyboard page

- [ ] Settings → Keyboard, select a key, type a new sequence, OK, then press the
      key at the CP/M prompt. Right: the new bytes arrive, with no restart of the
      app. Bind it to something you can see — a printable letter — rather than
      trusting an escape sequence you cannot read on screen.
- [ ] A **reserved** row — Shift+PageUp, Shift+PageDown, Ctrl+Home, Ctrl+End.
      Right: greyed, Status "Reserved", and the page **says what the app uses it
      for** in words ("scroll back one page"). A row that is merely un-editable
      is a mystery, which is the thing `reservedPurpose()` exists to prevent.
      The reserved rows sit below the ten navigation keys, so scroll for them.

## 8. Settings on a short screen

The dialog is four notebook pages and its height comes from `Fit()`, so adding
the Keyboard page moved it: 819 to 1105px at 200% scaling on the display it was
written on. A 1366x768 laptop at 100% has about 728px of work area, and nothing
in the fit knows that.

- [ ] Open Settings on a **1366x768 laptop at 100%**, or in a **1024x768 RDP
      session**, and walk all four tabs. Right: the status line and the
      OK/Cancel row are on screen on every one of them, and the notebook is what
      gave way. At the floor the page content does start to overlap — if it
      does, say which page and where.
- [ ] While you are there, Settings → Disk Images: the download section (folder
      path, catalog list, Download, Delete, progress) must be fully drawn. It
      was unreachable at 200% scaling until the dialog was paged.

What was measured is the *shrink*: the dialog forced to 1024x768 and to 300x200
with `SetWindowPos` on a 3840x2160 display at 200%. The clamp itself reads the
monitor's work area (`wxDisplay(this).GetClientArea()` into
`placeDialogInWorkArea`), and no real 768-line work area has ever been in front
of it.

## 9. The interface-v0 storage migration, and what Settings does after it

The catalog moved to `avwohl/romwbw_disks` and every published image gained a
`-v0-3.5.1` suffix. The first launch of this build renames the images in the
data folder, moves their ledger records, and rewrites the four disk slots and
every profile. None of that can be checked here: the renames are `MoveFileExA`,
and every consequence of them is in `MainWindow.cpp` and `SettingsDialogWx.cpp`,
which are in no suite. The headless suites cover the decisions
(`tests\test_diskledger.cpp`, `tests\test_config.cpp`) and nothing else.

Start from a machine that already has disks: at least `hd1k_combo.img` and one
more in `…\z80cpmw\data`, a `disk_ledger.json` beside them, a slot configured
for each, and a saved profile that names one.

- [ ] Launch. Right: the data folder now holds `hd1k_combo-v0-3.5.1.img` and
      **no** `hd1k_combo.img`, and the file's **size and modified time are
      unchanged** — this is a rename, and the ledger's measurement cache is
      keyed on exactly those two facts. If the timestamp moved, it was copied,
      and every user pays a 211 MB re-hash.
- [ ] The disks are **mounted** and the machine boots. `z80cpmw.json` names the
      new paths, and `core.interfaceV0Migrated` is `true`.
- [ ] `data\disk_ledger.json` is keyed on the **new** names and each record kept
      its `installedCatalogSha256` and its three `measured*` fields.
- [ ] A disk you imported yourself (`W8` a file into that folder, or copy an
      image in by hand) is **still there under its own name**. Nothing outside
      the twenty published filenames may be touched.
- [ ] Settings → **Disk Images**. Right: the status column says **"Downloaded"**
      for what you had, not "Available", and the read is quick — the migration
      carried the measurements, so nothing is re-hashed. This is the check that
      says the rename and the ledger moved together.
- [ ] Settings → **Machine**: the four dropdowns show the configured disks
      selected, under their new `-v0-3.5.1` names. Press **OK**, then reopen
      Settings. Right: the same four are still selected and the machine is still
      running them. **This is the check that matters most** — the same four
      controls used to reset to "(None)" whenever the list did not carry the
      configured name, and OK then erased all four slots with no confirmation
      and nothing said.
- [ ] Repeat that with the **network unplugged**, so the catalog fetch never
      returns and the dropdowns carry nothing at all. Right: same answer. OK
      must not erase a slot because a fetch failed.
- [ ] Configure a slot with File → **Load Disk** from somewhere outside the data
      folder, then open Settings and press OK. Right: that slot still names your
      file, and it is still mounted.
- [ ] In Settings → Disk Images, **Download** any disk while four slots are
      configured, then press OK. Right: the four slots are unchanged. Then
      **Delete** a disk that is not in a slot and press OK: still unchanged.
- [ ] **Load the profile** you saved. Right: it mounts the same disks it always
      did. Its file under `…\z80cpmw\profiles\` names the `-v0-3.5.1` paths and
      still holds everything else it held.
- [ ] Launch a **second** time. Right: nothing is renamed, nothing is written,
      and everything above is still true. The pass is idempotent and this is the
      cheapest way to find out that it is not.
- [ ] On a machine with **no** data folder at all, launch and press **F5**.
      Right: it downloads the two defaults — under the names of the release the
      index picks, which with no stored preference is **3.6.0**, so
      `hd1k_combo-v0-3.6.0.img` and not the `-v0-3.5.1` the migration above
      produces — and then asks for that release's ROM before anything boots.
      The disks come first and the ROM second, because `onEmulatorStart()` sends
      a machine with nothing mounted through `downloadAndStartWithDefaults()`
      and only its tail reaches `startEmulator()`, where the gate is. Note that
      the URL half landed in this same tree, so the bytes come from
      `romwbw_disks` and not from the ioscpm release area; §10 is the check for
      that half and for the ROM.

## 10. The catalog itself: the index, the release list, and the ROM

The URL half. `RELEASE_TAG` is gone, and the application now fetches
`index-v0.json`, picks a RomWBW release, verifies and fetches that release's
catalog, and builds every asset URL as `base_url + filename`. The parse is
checked headlessly against the real published documents — 152 checks, measured
on 2026-09-07 by building `tests\test_catalogv0.cpp` with `CatalogV0.cpp`,
`DiskLedger.cpp` and `DiskMigrationV0.cpp` under mingw `g++ -std=c++17`, with
the arm that compares the fixtures against a `..\romwbw_disks` checkout skipping
for want of one — and cannot be checked here; what has to be driven is
everything either side of it — the transport, the dialog and the first-run path
— none of which is in any suite.

Two checks that used to stand at the head of this list have gone, because they
were driven on 2026-09-07 and the result is in `CHANGELOG.md`: the app fetched
the live index, the release control held both published releases with **3.6.0**
selected, and the disk list held the 3.6.0 set. Both of them also asked for the
wrong answer — they were written when 3.5.1 was the default and 3.6.0 was marked
`preview`. The index fixture in `tests\test_catalogv0.cpp`, which is a copy of
the published document, gives 3.6.0 `"status": "stable"` and `"default": true`
and 3.5.1 `"default": false`; `catalogv0::displayLabel` appends the status only
when it is not "stable", so nothing on screen says *preview* at all, and the
live run agrees about which one comes up selected. (The arm of that suite which
compares the fixtures against a `..\romwbw_disks` checkout skipped here, so the
fixture is the evidence and not the repository.) What that run did not look at
is what is kept below.

- [ ] Confirm with a packet capture, a proxy or the debug log that the requests
      went to `github.com/avwohl/romwbw_disks` — `catalog-v0/index-v0.json` and
      then `v0-romwbw-3.6.0/catalog-v0-3.6.0.json` for a machine on the index's
      default — and that **nothing** was fetched from `avwohl/ioscpm`. Nobody
      has watched the wire; the dialog filling is not evidence of where from.
- [ ] **Download** one disk you do not have. Right: it lands under its
      `-v0-<release>` name, the status column reads **Downloaded**, and
      `disk_ledger.json` gains a record for it *with* an
      `installedCatalogSha256`. That last part is the check that the hash came
      from the same catalog entry the URL did.
- [ ] Read the note under the release control. It says how many ROMs the
      catalog publishes and that the default one is fetched and checked against
      its published size and checksum before the machine starts — and then, as
      the tree stands, ends `The ROM in the app is kept as the offline
      fallback.` There is no ROM in the app any more. That sentence is a string
      literal in `SettingsDialogWx::updateRomwbwVersionNote()` that outlived the
      files it describes, and its other arm — a release whose catalog publishes
      no ROM `can only be started with the ROM the app ships` — describes
      something that is now not a start at all. Nothing compiles a string, so
      reading it on screen is the only way this gets caught; report it rather
      than ticking the box.
- [ ] Switch the release the other way from wherever you are — to **3.5.1** on a
      machine sitting on the default 3.6.0. Right: the note changes to say that
      starting will offer to fetch that release's ROM, the list refills with
      `-v0-3.5.1.img` names, `hd1k_ws4` **appears** (3.5.1 publishes it and
      3.6.0 does not), and — this is the one that matters — **not one file in
      the data folder was deleted or renamed**. Check the folder before and
      after.
- [ ] Press **OK**, reopen Settings. Right: 3.5.1 is still selected, and
      `core.romwbwVersion` in `z80cpmw.json` is `"3.5.1"`. Now switch back to
      3.6.0, OK, reopen. Right: 3.6.0, and **still** nothing deleted. Switching
      back and forth is the operation that destroyed a library on the iOS port
      and it must cost only two small HTTP GETs.
- [ ] Select **3.5.1** again, download one of its disks and mount it, then press
      **F5** on a machine whose banks hold the 3.6.0 ROM. Right: it does **not**
      start on the ROM already in its banks. A box titled **ROM needed** names
      RomWBW 3.5.1, names `emu_avw-v0-3.5.1.rom`, says why it is not ready, and
      offers **two** answers: download it and start, or do not start. There is
      no third answer any more — the "go back to the release the app ships a ROM
      for" arm went with the ROMs. Answer **Yes**: the status bar counts a
      512 KB download, the ROM
      lands in the data folder under that name, and the guest boots with **no**
      `*** WARNING: HBIOS/CBIOS Version Mismatch ***` line. That banner
      appearing is the whole failure this release exists to remove — if you see
      it, the ROM in the banks is not the one the disks were built for.
- [ ] Select a release, then press **Cancel**. Right: reopening Settings shows
      the release you had before, not the one you cancelled.
- [ ] The failed switch. With 3.5.1 selected, unplug the network, select
      **3.6.0**, and let the refresh fail. Right: the control goes back to
      **3.5.1** and the status line says the fetch failed. Now press **OK**,
      plug the network back in, and reopen Settings. Right: it is still 3.5.1
      and the list is 3.5.1's. A release that could not be fetched must not
      become the one the next fetch uses — the control, `core.romwbwVersion` and
      `DiskCatalog`'s own preference all have to end up saying the same thing.
- [ ] While a refresh is running, try to change the release again. Right: the
      control is **disabled** until the fetch comes back, exactly as the Refresh
      button is.
- [ ] Unplug the network and open Settings. Right: the release control shows
      what the configuration says and is **disabled**, the status line reports
      the fetch failure, and pressing **OK** leaves `core.romwbwVersion`
      unchanged in the file. A dialog that could not show the list must not be
      able to forget the choice.
- [ ] Unplug the network, remove every disk from the four slots, and press
      **F5** with the two default images still in the data folder under their
      `-v0-3.5.1` names. Right: it says nothing about downloading and **mounts
      them** — that half has not changed, and `downloadAndStartWithDefaults()`
      still takes the nothing-to-fetch path without asking the network anything.
      Use the `-v0-3.5.1` spelling: with no catalog in hand `cachedDefaultDisk`
      probes only what `diskv0::v0NameFor` builds — which is
      `diskv0::PRE_V0_ROMWBW`, still `"3.5.1"` — and the pre-v0 bare name, so
      a folder holding only `-v0-3.6.0` images offline reads as empty. Then it
      does **not** boot: the mount is followed by `startEmulator()`, which is
      where the ROM gate is, and the gate cannot verify a ROM without the
      catalog. The terminal says it is looking up the catalog, the lookup fails,
      and a **Cannot start** box says so. The disks being offline-capable and
      the ROM not being so is the whole shape of the cost — see the ROM gate
      below for what the box must say.
- [ ] Now delete `hd1k_games-v0-3.5.1.img`, still offline, and press **F5**.
      Right: it says it is looking up the catalog, reports the failure in one
      line, mounts the combo image it still has — and then stops at the same
      gate, for the same reason. It must not hang and must not sit there with
      nothing said. What this checks is the disk half: one line about the
      catalog, the image it has mounted and named, and no silence.
- [ ] Plug the network back in and repeat with an empty data folder. Right: it
      fetches the catalog, downloads both defaults, and — check
      `disk_ledger.json` — records an `installedCatalogSha256` for each, then
      goes on to offer the ROM. Before this release that path fetched no catalog
      at all, so both images were written with no checksum check and no ledger
      record.

### The ROM gate

`MainWindow.cpp` and `SettingsDialogWx.cpp` are in no suite and cannot be, so
every line of this is a check a person has to make. The rule being checked is
one sentence: **starting a machine on RomWBW X requires X's ROM, fetched from
X's catalog and checked against the size and sha256 that catalog publishes.**
There is nothing left to fall back to. The package ships no ROM — the files were
deleted on 2026-09-07, the vcxproj stages none, and neither installer carries
one — so `loadDefaultROM()` loads nothing and only puts a notice on the screen,
and the rule's other half is now the blunt one: **a machine that cannot reach
the catalog does not start.**

The gate WAS driven on 2026-09-07, twice, and `CHANGELOG.md` records both runs:
once before the ROMs were deleted, and once after, with `bin\Release\roms`
removed and no ROM anywhere on the machine. That second run is what deleted the
success-path check that used to head this list - F5 raised the **ROM needed**
box naming `emu_avw-v0-3.6.0.rom`, Yes downloaded it, the file that landed
hashed `01d1ca6d...` at 524,288 bytes against exactly what the 3.6.0 catalog
publishes, and the guest booted on it.

**What has never been run is the NO-NETWORK path**, and that is the distinction
this section now turns on: every failure below was reached by deleting the ROM,
not by removing the connection. "The catalog is unreachable" and "the ROM is
missing" are different branches and only the second has been seen.

- [ ] **Open the Emulator menu and look at Start, before anything else.** It
      must be **enabled** on a machine with no ROM. This is first because it is
      the check that was missing: 1.0.26-beta shipped with `updateMenuState()`
      still reading `hasROM()`, so on every fresh install Start was greyed and
      F5 did nothing — and since Start is the only thing that fetches a ROM,
      there was no way out of it. It was found by a person installing the
      package, not by any check here.

      **Do not settle for the scripted answer.** Posting `WM_COMMAND
      ID_EMU_START` starts the machine whether or not the item is greyed, which
      is exactly why the driven run passed on the broken build. Either click the
      menu, or read the state: `GetMenuState(GetMenu(hwnd), 2001, MF_BYCOMMAND)`
      and check `& (MF_GRAYED | MF_DISABLED)`. Same for F5, which Windows
      suppresses whenever its menu item is disabled. See `WIP.md`.
- [ ] Launch with an **empty data folder** and read the boot screen before
      touching F5. Right: it carries the notice `loadDefaultROM()` sets — the
      RomWBW ROM is downloaded the first time the machine starts, this app does
      not ship one, press F5 and it will be fetched and checked against the
      catalog, and Emulator > Settings > Disk Images chooses which release and
      which ROM. Then press Start and Reset, both of which clear the terminal:
      the notice must come back both times, because `printNotices()` is the only
      thing that puts it back and a machine waiting for its first ROM with
      nothing said looks like a machine that is simply broken.
- [ ] **Empty data folder, network down, F5.** Right: it reports and does not
      boot — and the report is about **disks**, not about the ROM. The terminal
      says it is looking up the disk catalog, prints the failure on its own
      line, and ends "No disk images are available. Check your network
      connection, or use Settings > Disk Images to download one." The ROM gate
      is never reached: with nothing mounted, `startWithDefaultsAfterCatalog`
      returns before `startEmulator()`, which is where the gate lives. If a ROM
      box appears here instead, the two failures are wired the wrong way round.
- [ ] **Disks but no ROM, network down.** Put the two default images in the data
      folder by hand under their `-v0-3.5.1` names, unplug, F5. Right: it mounts
      them and says "Loaded default disks.", then does not boot: the terminal
      says it is looking up the disk catalog for the ROM, that fails, and a box
      titled **Cannot start** — OK only, no choices — says it does not yet know
      which RomWBW release to run, that the ROM is downloaded from the catalog
      the first time the machine starts and this app does not ship one, and to
      check the network and press F5 again. Watch that terminal line: with no
      release known yet the name is empty and the string is assembled around it,
      so it reads "for the RomWBW  ROM..." with a doubled space. Cosmetic, but
      it is what you will see and no suite can see it.
- [ ] The same thing with the release **already stored and its ROM already in
      the data folder** — set `core.romwbwVersion` to `"3.6.0"` in
      `z80cpmw.json`, leave a good `emu_avw-v0-3.6.0.rom` beside the disks,
      unplug, F5. Right: it **still** does not start, and this time the box
      names RomWBW 3.6.0 and says the ROM for it is not ready. This is the cost
      of the rule written as a check rather than as a caveat: the published size
      and sha256 live only in the catalog, so a ROM that cannot be checked is a
      ROM that will not be loaded, and there is no second place a hash is
      remembered. If this one boots, something is putting unverified bytes in
      the banks.
- [ ] Answer **No** to that box. Right: nothing starts, and nothing is
      downloaded. The Yes arm was driven on 2026-09-07 and is in `CHANGELOG.md`;
      the No arm was not, and "does not start" is the half that must not quietly
      become "starts on something else".
- [ ] Quit and relaunch that machine, network up, and press **F5**. Right: one
      small catalog GET, no box, no download, and it boots. The ROM was already
      there; it is verified again on the way in — size and sha256 on every load,
      not only after a download — and that hash of 512 KB is all it costs.
- [ ] **Change the ROM in Settings.** The dropdown is on Settings → **Machine**
      ("ROM:") and it is the release catalog's own `roms[]` now rather than two
      packaged filenames, which is what makes `emu_rcz80` reachable at all. Pick
      **EMU RCZ80**, press OK, then **F5**. Right: the gate notices — it
      compares the loaded ROM's filename against the one the catalog picks for
      the stored preference, so a change of ROM within one release is not waved
      through by a release comparison both ROMs satisfy — offers
      `emu_rcz80-v0-3.6.0.rom`, fetches it on Yes, and the status bar says
      "Loaded ROM: emu_rcz80-v0-3.6.0.rom". Then quit, relaunch, F5: still
      emu_rcz80, no box, and `core.rom` in `z80cpmw.json` is the id
      `"emu_rcz80"` and not a filename.

      **Expect this one to fail as the tree stands, and report exactly what the
      control showed.** `MainWindow.cpp` seeds the dialog with
      `settings.romFile = m_emulator->getROMName()`, which
      `loadCatalogRomForStart` set to the catalog *filename*, while
      `populateROMList` matches `id`s and appends anything it cannot match as a
      "(not in this release)" row. So the control is likely to open on
      `emu_rcz80-v0-3.6.0.rom (not in this release)` rather than on the EMU
      RCZ80 row, and OK then writes that filename into `core.rom`, where
      `chooseRom` matches no id and the next start falls back to the catalog's
      default. `Config`'s `from_json` converts a stored `emu_avw`-shaped
      filename back to the id at the next parse but has nothing to convert
      `emu_rcz80-v0-3.6.0.rom` to, so it clears the field: the preference is
      gone rather than merely misspelled. This is read out of the source, not
      run — which is why it is a check and not a bug report.
- [ ] With a catalog ROM running, open Settings and press **OK** without
      touching the ROM control, then **F5**. Right: the machine keeps running
      the same ROM and nothing is fetched. Write down what the ROM control said
      when the dialog opened before you press OK — see the bullet above for why
      that is the interesting half.
- [ ] Corrupt the downloaded ROM: with the network up, quit, flip one byte in
      `emu_avw-v0-3.6.0.rom`, relaunch, and press **F5**. From a fresh launch,
      because the gate short-circuits when the banks already hold the right
      release's ROM: verification happens where a ROM is **loaded**, and a
      machine that has already started this session has nothing left to re-read.
      Right: it is refused as not matching the published checksum, offered, and
      one re-download fixes it. Truncate it instead and the size check must
      catch it first — that check is exact rather than the ">=" a cached disk
      gets, and the 1 MB completeness floor guarding a cached disk cannot see a
      512 KB file at all.
- [ ] Break it so the retry fails too — corrupt it and pull the network after
      the catalog is in hand. Right: the failure message names the release, the
      file and the reason, and the machine does not start. Exactly one
      re-download is spent, not a loop: the box after a failed download is
      OK-only and offers nothing.
- [ ] Interrupt the ROM download (pull the network mid-transfer). Right: no
      `.rom` is left half-written in the data folder, and no
      `emu_avw-v0-3.6.0.rom.new` is left beside it either. A previously good
      copy of that ROM must still be there and unchanged — the transfer writes
      beside the real name and moves onto it only after both checks pass.
- [ ] **Delete the ROM** out of the data folder — quit first, delete, relaunch,
      network up, F5. Right: it is offered and fetched again exactly as the
      first time, because a fetched ROM lands in the data folder and nothing
      else on the machine has one.
      `findResourceFile()` still looks in `<app>\roms`, `<app>` and
      `<app>\..\roms` before it looks there, so a ROM dropped beside the
      executable by hand still wins — copy the file to `<app>` under its catalog
      name and confirm it boots from there with no download, and that it is
      still checked against the catalog's size and sha256 on the way in.
- [ ] Install the package — either channel — and confirm there is **no** `roms`
      directory under the install root and no `.rom` file anywhere in it. This
      is the inverse of the check that used to be here, which asked that
      `roms\emu_avw.rom` still be present. The NSIS uninstaller still *deletes*
      `emu_avw.rom`, `emu_romwbw.rom` and `SBC_simh_std.rom` from
      `$INSTDIR\roms`: that is cleanup of installs that had them, not a packing
      list, and it is the one place in the packaging where those names survive
      on purpose.

## 11. The Catalog index field

The setting that points this client at another `romwbw_disks` catalog. Every
part of it lives in `SettingsDialogWx.cpp` and `MainWindow.cpp`, which are in no
suite and cannot be — a real window and an interactive window station — so this
section is the only coverage it has. The resolution underneath it *is* covered:
`tests/test_catalogv0.cpp` has the precedence, the trimming and the normalizing
at 207 checks.

Everything below was driven with `WM_COMMAND` + `PrintWindow` on 2026-09-09, so
these are re-checks rather than first runs. What a person adds is a pair of eyes
on the wrapping, which a text dump cannot see: `WM_GETTEXT` returns the whole
label whether or not the screen shows it, and that is exactly how the clipped
note shipped in the first place.

**Set up a catalog you control.** Any static file server will do:

    python -m http.server 8731 --bind 127.0.0.1

with a copy of the published `index-v0.json` in the directory, edited so it is
recognisable — change a `label` to something like `RomWBW 3.5.1 LOCAL-TEST` and
drop one of the two releases. `http://` is accepted; ioscpm refuses anything but
`https`/`file`, and that difference is deliberate here because it is what makes a
local test server usable on the desktop.

- [ ] Settings → **Disk Images** with the field empty. The note reads *"Leave
      empty for the catalog this build ships with."* and then `In use:` followed
      by the built-in URL **on its own lines and complete**. The URL is 85
      characters and the control is about 62 wide, so it wraps onto a second
      line. If any line is cut off mid-word at the right edge, the wrapping has
      regressed — this is the check that matters most, because the clipped form
      is what shipped.
- [ ] Type the local URL. The note changes **as you type**, without pressing
      anything: it becomes the custom-catalog form, and `In use:` names what you
      typed. Read the whole warning — five lines about the shared data folder —
      and confirm none of it is cut off.
- [ ] Press **Refresh** with the URL typed and **without** pressing OK. The
      release picker fills from *your* index (the `LOCAL-TEST` label is the
      proof) and the disk list changes with it. If it comes back with the
      built-in catalog's releases, the typed value is not reaching the fetch.
- [ ] Press **Cancel**. Re-open Settings: the field is empty again and the
      picker is back on the built-in catalog's releases.
- [ ] Type it again and press **OK**. `z80cpmw.json` gains
      `core.catalogIndexUrl` with that URL, and re-opening Settings shows it.
- [ ] Paste the **built-in** URL into the field, with a space or two around it,
      and press OK. `core.catalogIndexUrl` is stored **empty**, not as that
      string. Storing it would pin the install to today's default, and the note
      invites copying it, so this is the easy mistake to make.
- [ ] Relaunch with `set ROMWBW_INDEX_URL=<a second local index>` and a
      *different* URL still in the field. The field is **disabled**, still shows
      the stored value, and the note says the variable wins and names the
      variable's URL. The picker holds the variable's catalog.
- [ ] With the variable still set, press **OK** and quit. `core.romwbwVersion`
      in `z80cpmw.json` is **unchanged**. The variable is documented as winning
      for one run and storing nothing; a test catalog that publishes only one
      release collapses the picker to it, and OK used to write that over the
      user's release.
- [ ] Point the field at a catalog that does **not** publish the release the
      machine is set to, and press F5. The refusal names the custom catalog and
      says to choose a release it does publish or clear the field. It must not
      say "check the network connection" — the network is fine, and it was
      fetching the index a moment earlier.
- [ ] Mount an image, switch to a catalog that publishes a different image under
      the same name, download it — the warning about replacing a disk you have
      written to appears — then press F5. The boot output carries a line saying
      that slot is not the image the catalog publishes, and the machine starts
      anyway.
