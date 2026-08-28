# z80cpmw Configuration (`z80cpmw.json`)

z80cpmw keeps its settings in a JSON file you can edit by hand:

```
%LOCALAPPDATA%\z80cpmw\z80cpmw.json
```

**Finding it:** *Emulator → Settings → Open Folder* opens the **data** folder
(`...\z80cpmw\data`, where disks and R8/W8 transfers live). The `z80cpmw.json`
file is one level up, in the `z80cpmw` folder. On any MSIX install — the
Microsoft Store build or the signed sideload beta — both are redirected under
`...\Packages\AaronWohl.Z80CPM_<hash>\LocalCache\Local\z80cpmw\`. The `<hash>`
is derived from the package publisher, so the Store build and the beta get
different ones and can be installed side by side. You don't need to know that
path — the app shows the real, resolved data-folder location in *Settings* (a
copyable field) and in *Help → About*. For R8/W8 file transfer specifically,
see [FILE_TRANSFER.md](FILE_TRANSFER.md).

Close z80cpmw before editing the file, then restart for changes to take effect.
(The same settings, including the keyboard map, are also viewable in-app from
*Help → Help Topics → Configuration File*.)

**You do not have to edit the file to change a key.** *Emulator → Settings →
Keyboard* lists every bindable key with what it sends, and edits the same
`keyboard.keys` object described below. Changes there take effect when you press
OK — no restart — and anything in the file the page does not recognise is left
exactly as you wrote it.

## Keyboard map

CP/M is pure ASCII and has no built-in function or navigation keys. Each CP/M
terminal historically defined its **own** escape sequences for them, so there is
no single standard — the correct bytes depend on the terminal your CP/M software
expects (VT100, ADM-3A, Televideo, Kaypro, …).

z80cpmw therefore lets you bind each special key to whatever bytes you choose,
written as **termcap-style escape strings** (the same notation used in
termcap/terminfo entries). The bindings live under `keyboard` in `z80cpmw.json`:

```json
"keyboard": {
  "f1ToCpm": false,
  "f5ToCpm": false,
  "ctrlRToCpm": true,
  "keys": {
    "Insert": "\\E[2~",
    "F2": "\\EOQ"
  }
}
```

> **Backslashes are doubled.** JSON uses the backslash for its own escaping, so
> every backslash is written **twice** in the file. The Escape character (`\E`
> in termcap) becomes `\\E`, so Insert is stored as `"\\E[2~"`.

### Escape syntax

| Notation | Meaning |
| --- | --- |
| `\E`            | Escape, `0x1B` (written `\\E` in JSON) |
| `\n` `\r` `\t`  | Newline, Return, Tab |
| `\b` `\f` `\s`  | Backspace, Form-feed, Space |
| `\NNN`          | One byte in octal, e.g. `\033` = Escape |
| `^X`            | Control-X, e.g. `^C` = `0x03` |
| `^?`            | Delete, `0x7F` |

Any other character stands for itself. An **empty value unbinds** that key —
which is what the **Unbind** button on the Keyboard page writes.

The Keyboard page checks what you type against this table before it stores it,
because nothing else does: an escape the decoder does not recognise is not
reported anywhere, it is simply sent as different bytes. `\x1b` sends `x`, `1`,
`b`; `\400` overflows a byte and sends `NUL`; `^1` sends `0x11`. A file edited by
hand gets no such check, so those are the mistakes to look for if a key sends
something unexpected.

### Bindable key names

`Up`, `Down`, `Left`, `Right`, `Home`, `End`, `Insert`, `Delete`, `PageUp`,
`PageDown`, and `F1` through `F12`. Names are case-insensitive; `Ins`, `Del`,
`PgUp` and `PgDn` also work.

**Modifiers.** A name may be prefixed with `Ctrl+`, `Shift+` and `Alt+`, in any
order and any combination — `Ctrl+Left`, `Ctrl+Shift+F3`. `Control+` is accepted
for `Ctrl+`, and prefixes are case-insensitive like the rest of the name. A
modified key is a binding in its own right, so `Ctrl+Left` and `Left` can send
different bytes.

A modified key you have not bound falls back to the plain one, which is what
every modified key did before prefixes existed — so `Shift+Left` sends whatever
`Left` sends unless you say otherwise.

Four combinations are reserved by the app and never reach CP/M. The app answers
them before the keymap is consulted, so binding one of them in `keys` sends
nothing — and the configuration report (below) names it at startup if you do,
in the same words used here:

- `Shift+PageUp` — scroll back one page
- `Shift+PageDown` — scroll forward one page
- `Ctrl+Home` — jump to the oldest scrollback line
- `Ctrl+End` — return to the live screen

The Keyboard page in *Settings* lists these four as greyed rows marked
**Reserved**, each beside the key it belongs to, and says which of the four
things above it does when you select one. They are shown rather than left out on
purpose: a key that is simply missing from the list looks like an oversight.

The test is whether that modifier is **held**, not that it is the only one held,
so `Ctrl+Shift+PageUp` scrolls back as well — a modifier you add on top of a
reserved combination does not hand the key back to CP/M.

`Alt` is the Windows menu key, so an `Alt+` binding is honoured only when you
have bound that exact combination — the plain-key fallback does not apply to it,
and with nothing bound to `Alt` the menus behave normally.

Only `Ctrl+Up`, `Ctrl+Down`, `Ctrl+Left` and `Ctrl+Right` are bound by default;
see the table below.

### Default bindings (VT220 / xterm)

Shown as written in the file (doubled backslashes):

| Key | Sends | Key | Sends |
| --- | --- | --- | --- |
| Up       | `\\E[A`  | F1  | `\\EOP`   |
| Down     | `\\E[B`  | F2  | `\\EOQ`   |
| Right    | `\\E[C`  | F3  | `\\EOR`   |
| Left     | `\\E[D`  | F4  | `\\EOS`   |
| Home     | `\\E[H`  | F5  | `\\E[15~` |
| End      | `\\E[F`  | F6  | `\\E[17~` |
| Insert   | `\\E[2~` | F7  | `\\E[18~` |
| Delete   | `^?`     | F8  | `\\E[19~` |
| PageUp   | `\\E[5~` | F9  | `\\E[20~` |
| PageDown | `\\E[6~` | F10 | `\\E[21~` |
|          |          | F11 | `\\E[23~` |
|          |          | F12 | `\\E[24~` |

The four Ctrl+arrows use the xterm modified-key form, where the `5` is the
control modifier:

| Key | Sends | Key | Sends |
| --- | --- | --- | --- |
| Ctrl+Up    | `\\E[1;5A` | Ctrl+Right | `\\E[1;5C` |
| Ctrl+Down  | `\\E[1;5B` | Ctrl+Left  | `\\E[1;5D` |

A CP/M editor is more likely to want the WordStar word-left / word-right pair,
which is one line away:

```json
"Ctrl+Left": "^A", "Ctrl+Right": "^F"
```

These match the VT100-style sequences the terminal already used for the arrow
keys. Change any line to suit your CP/M program. For example, to make `F1`–`F4`
easier to parse in a hand-written key reader, give them the same `CSI` form as
the rest:

```json
"F1": "\\E[11~", "F2": "\\E[12~", "F3": "\\E[13~", "F4": "\\E[14~"
```

### Application shortcut keys

A few keys can be claimed by the app instead of the guest. A claimed key is
swallowed **whole** — CP/M never sees it — so these settings decide *who
receives* the keystroke, not what it sends.

| Setting | Default | Effect when set |
| --- | --- | --- |
| `"f1ToCpm"` | `false` | `true` sends `F1` to CP/M (Help stays on the Help menu) |
| `"f5ToCpm"` | `false` | `true` sends `F5` and `Shift+F5` to CP/M |
| `"ctrlRToCpm"` | `true` | `false` makes `Ctrl+R` the Reset shortcut again |

The Keyboard page carries these three as checkboxes, worded the other way round
— ticked means the **app** keeps the key. Changing one there rebuilds the
accelerator table and relabels the menu straight away, so the shortcut starts or
stops working when you press OK rather than at the next launch.

`F1` and `F5` default to the app because CP/M has no function keys, so reserving
them costs nothing. **`Ctrl+R` defaults the other way**, because `^R` (`0x12`) is
ordinary ASCII that CP/M reads: it retypes the current line at the command
prompt, and WordStar-family editors bind it too. Reserving it would take a
working key away from the guest. Reset therefore lives on the **Emulator** menu,
with no default shortcut.

**Reset asks first while the machine is running**, so a `Ctrl+R` you did not
mean — on a config that has claimed the key — can be answered No, and No is the
default button. A Reset on a stopped machine goes through without asking: there
is no CP/M session to lose, and *Start* cold-boots without asking either.

The menu updates its own shortcut hints to match these settings, so an item
never advertises a key that is no longer bound.

`F10` normally opens the Windows menu bar; z80cpmw delivers it to CP/M when it is
bound in the keymap.

## When the file says something the app cannot use

A hand-edited setting that z80cpmw does not recognise used to be absorbed in
silence and then deleted at the next save. It is now reported: at startup — and
again whenever you load a profile — the terminal shows a **configuration
report** listing what could not be used, where in the file it is (`display.fontsize`,
`disks[1].pth`, `keyboard.keys.PgeUp`), and what happens to it next:

- **Unrecognised setting.** Nothing reads it, so it does nothing, and the next
  time settings are saved it is dropped from the file.
- **Wrong kind of value** — `"keys"` written as an array, `"disks"` as an
  object. That whole section is skipped and the built-in defaults are used.
  **Fix this one before you do anything else in the app.** The file itself
  parses, so it is *not* renamed out of the way and startup will not save over
  it — but the next save of any kind writes the built-in defaults over the
  section that was skipped, and several saves happen without you asking for
  one: closing the window (it records where the window was), the first-run
  welcome flag, a setting changed with the ROM's `SYSCONF` while the machine
  runs, and *Start* on a config with no disks, which loads the defaults and
  saves them. Simply quitting the app is enough to lose the section, so edit
  the file — or close the app and edit it — before you carry on.
- **Unknown key name** (`F13`, `PgeUp`) or a **reserved** one (the four
  combinations above). The binding is ignored, but the line is **kept** —
  nothing ever removes an entry from `keyboard.keys`, so it is still in the file
  to be corrected.
- **Could not be read at all** — a syntax error, or a file that will not open.
  The report carries the parser's own message, which for a syntax error names
  the line and column; the file is renamed to `z80cpmw.json.bad` (`.bad2`,
  `.bad3`, … if that name is taken) and the defaults are used. Nothing is
  written over the original, and the report stays on screen — it is the only
  place the backup's name is shown, so it is not taken down when settings are
  saved. (If the rename could not be done at all, the report says so and names
  the reason; that is the one case where a later save does overwrite the file.)

A **profile** that cannot be read is treated exactly the same way: same report,
same rename to `<name>.json.bad`. Because it has been renamed it also
disappears from the *Load Profile* list, and the settings you were running stay
in force.

The report survives *Emulator → Start* and *Emulator → Reset* clearing the
screen, so it is still readable after the machine boots. So do the notices about
the ROM: if the ROM named in the file cannot be loaded, the line saying which
ROM is running instead stays on screen until you choose a ROM.

## Mouse copy and paste

Drag with the mouse to select text in the terminal, then **right-click** for
Copy and Paste. `Ctrl+C` and `Ctrl+V` are deliberately left untouched so they
still reach CP/M as `^C` and `^V`. Paste works only while the emulator is
running.

## Other settings

| Setting | Meaning |
| --- | --- |
| `display.fontSize` | Terminal font size, in points |
| `display.scrollbackLines` | Lines of terminal history kept for scrollback (0 = off) |
| `display.bell` | Whether `BEL` (character 7) makes a sound (default `true`) |
| `core.rom`         | ROM image to load at startup |
| `core.bootString`  | Text typed automatically at the boot menu |
| `disks`            | Disk images assigned to units 0–3 |

Most of these are easier to change from *Emulator → Settings*.
