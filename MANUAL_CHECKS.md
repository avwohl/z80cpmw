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
`tests\run_tests.bat` is **1467 checks in seven suites** now — 516 terminal
conformance, 355 help renderer and assets, 302 configuration diagnostics, 142
disk provenance, 66 host file transfer, 50 rendering conformance, 36 HBIOS host
file extension.

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

142 headless checks say what `DiskLedger` decides and that `DiskHash` measures
it correctly. None of them can see a word reach the screen:
`SettingsDialogWx.cpp` is in no suite and needs a real window, and the verdict
is computed on the `fetchCatalog` worker against a live catalog and a real data
folder, neither of which a suite has.

- [ ] Settings → **Disk Images** with `hd1k_combo.img` already downloaded, on a
      machine that has never run 1.0.25. Right: the status column says
      **"Differs from catalog"** if that copy came from `v1.4.5`, and plain
      **"Downloaded"** if it came from `v1.4.12`. This is the migration case and
      it is the one most users are in — there is no ledger yet, so the app
      hashes the file once and says only what it can prove. The first fetch
      after an upgrade therefore reads ~49MB; watch that the dialog **stays
      responsive** while it does, which is the whole reason that work is on the
      worker.
- [ ] Open it a **second** time. Right: the same verdict, and no re-read — the
      measurement is cached in `data\disk_ledger.json` against the file's size
      and write time. If it re-hashes every time, the write time is not
      round-tripping and every launch will pay 211MB.
- [ ] **Delete** that disk and **Download** it again, then reopen Settings.
      Right: **"Downloaded"**, because the download was verified against the
      catalog hash and its provenance recorded. Check `data\disk_ledger.json`
      holds an `installedCatalogSha256` of `89b8ae1a…` for it.
- [ ] Boot the guest off that disk, **save a file** in CP/M, quit, and reopen
      Settings. Right: still **"Downloaded"** — the catalog has not moved, so
      writing to the volume must not make it look stale. This is the check that
      the verdict is provenance and not a byte comparison; if it says anything
      about an update, the design has been undone.
- [ ] Corrupt the copy by hand — open `data\hd1k_combo.img` and change a byte
      well past the first megabyte — then reopen Settings. Right: still
      **"Downloaded"**, for the same reason. (`R8`/`W8` may of course break;
      that is not what this checks.)
- [ ] Point `RELEASE_TAG` at `v1.4.5`, rebuild, and open Settings with a
      `v1.4.12` copy installed and untouched. Right: **"Update available"** —
      the superseded-and-pristine case, which is the only one that would ever be
      replaced without asking. Put `RELEASE_TAG` back afterwards.

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
      Right: it downloads the two defaults — under their `-v0-3.5.1` names,
      because that is where `DiskCatalog::getDiskPath` files them now — and
      boots. Note that the URL half landed in this same tree, so the bytes come
      from `romwbw_disks` and not from the ioscpm release area; §10 is the
      check for that half.

## 10. The catalog itself: the index, the release list, and the ROM

The URL half. `RELEASE_TAG` is gone, and the application now fetches
`index-v0.json`, picks a RomWBW release, verifies and fetches that release's
catalog, and builds every asset URL as `base_url + filename`. The parse is
checked headlessly against the real published documents
(`tests\test_catalogv0.cpp`, 107 checks) and cannot be checked here; what has to
be driven is everything either side of it — the transport, the dialog and the
first-run path — none of which is in any suite.

- [ ] Open Settings → **Disk Images** on a working network. Right: the list
      fills, the **RomWBW release** control at the top says **RomWBW 3.5.1**,
      and the filenames in the list end `-v0-3.5.1.img`. Confirm with a packet
      capture, a proxy or the debug log that the requests went to
      `github.com/avwohl/romwbw_disks` — `catalog-v0/index-v0.json` and then
      `v0-romwbw-3.5.1/catalog-v0-3.5.1.json` — and that **nothing** was fetched
      from `avwohl/ioscpm`.
- [ ] **Download** one disk you do not have. Right: it lands under its
      `-v0-<release>` name, the status column reads **Downloaded**, and
      `disk_ledger.json` gains a record for it *with* an
      `installedCatalogSha256`. That last part is the check that the hash came
      from the same catalog entry the URL did.
- [ ] Open the release control. Right: it offers **RomWBW 3.5.1** and **RomWBW
      3.6.0 (preview)** — the word *preview* must be on screen — and the note
      under it says this build boots the ROM it ships with and downloads no
      ROMs.
- [ ] Select **3.6.0**. Right: the note changes to say the guest will report an
      HBIOS/CBIOS version mismatch, the list refills with `-v0-3.6.0.img`
      names, `hd1k_ws4` is **gone** (3.6.0 does not publish it), and — this is
      the one that matters — **not one file in the data folder was deleted or
      renamed**. Check the folder before and after.
- [ ] Press **OK**, reopen Settings. Right: 3.6.0 is still selected, and
      `core.romwbwVersion` in `z80cpmw.json` is `"3.6.0"`. Now switch back to
      3.5.1, OK, reopen. Right: 3.5.1, and **still** nothing deleted. Switching
      back and forth is the operation that destroyed a library on the iOS port
      and it must cost only two small HTTP GETs.
- [ ] With 3.6.0 selected, download one 3.6.0 disk and mount it. Right: both the
      3.5.1 and the 3.6.0 copies of that image are in the folder, and the guest
      prints `*** WARNING: HBIOS/CBIOS Version Mismatch ***`, which is what the
      note said would happen.
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
      **F5** with the two default images still in the data folder. Right: it
      says nothing about downloading, mounts them and boots. Offline start must
      not need a catalog.
- [ ] Now delete `hd1k_games-v0-3.5.1.img`, still offline, and press **F5**.
      Right: it says it is looking up the catalog, reports the failure in one
      line, mounts the combo image it still has, and boots. It must not hang and
      must not sit there with nothing said.
- [ ] Plug the network back in and repeat with an empty data folder. Right: it
      fetches the catalog, downloads both defaults, and — check
      `disk_ledger.json` — records an `installedCatalogSha256` for each. Before
      this release that path fetched no catalog at all, so both images were
      written with no checksum check and no ledger record.
