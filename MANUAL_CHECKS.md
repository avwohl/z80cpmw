# Manual checks

Checks that need a person: an installed package, keys pressed, a screen watched.
Nothing here can be settled by reading the source or by any test in this
repository, which is why none of it lives in `todo.txt` — that file keeps a
one-line pointer at this one.

**Delete a check once someone has run it.** The result belongs in `CHANGELOG.md`
under **Verified**, not here. A check that has been run and left in place turns
this file into the same accumulating record `todo.txt` was.

---

## 1. File transfer under an installed MSIX

The behaviour is confirmed in source and documented. What has never happened is
a run against an **installed** package, which is the only thing that reproduces
MSIX file-system redirection — a local unpackaged build cannot, whatever it
prints.

**You do not need the Store build.** Sideload
`dist\z80cpmw-1.0.22-beta.msix` (attached to the `v1.0.22-beta` release); it is
the same binary and also MSIX.

- [ ] ~~`W8 C:\Users\<you>\Desktop\test.txt` → the file appears on the
      Desktop.~~ **This checkbox tests the wrong thing today.** The `W8.COM` on
      the bundled images reads only the parsed FCB and never the command tail,
      so it cannot take a host path whatever the Windows backend does — the
      failure would be guest-side, not MSIX redirection. Re-instate this when
      the images are refreshed (`todo.txt`, disk-image section).
- [ ] `W8 getkey2.com` (a bare name) → the file lands under
      `…\Packages\AaronWohl.Z80CPM_*\LocalCache\Local\z80cpmw\data\`.
- [ ] The path `W8` reports agrees with what About, Settings → Open Folder and
      the boot banner display. All four are supposed to name the same folder.
- [ ] `R8 C:\Users\<you>\Desktop\getkey2.com` → reading an arbitrary user path
      works. `R8` already takes a host path, so this one is testable now.
- [ ] `R8` a file, then check what it printed. `emu_host_file_get_read_name()`
      is new and has never been compiled; the guest should be told the
      **resolved** path — the `LocalCache` one for a bare name — not the name it
      typed. If it prints nothing, the empty-string path is being taken and the
      resolution is not reaching the guest.

## 2. Keystroke delivery, mouse copy/paste, and the first-run Help window

Never watched by a person, and nothing here can automate them. The terminal
parser is covered — `tests\run_tests.bat`, 252 checks in the terminal suite and
354 across all three — but that stops at the byte stream. It says nothing about
whether a keypress becomes a byte or a cell becomes a pixel.

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
   pane.
