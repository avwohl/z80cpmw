# Debugging a guest program with GDB or udb80

z80cpmw has a GDB stub. A debugger can stop the emulated machine, step the
Z80 CPU, set breakpoints, and read and write memory. The debugger talks the
GDB remote serial protocol (RSP) over a TCP socket. GDB and udb80 (from
um80_and_friends) both speak this protocol.

The stub is off by default. A z80cpmw that runs with the stub off runs as
before.

## What you need

- **GDB with the Z80 target**, if you use GDB. Use GDB 18.1 or newer.
  Homebrew's `gdb` formula has the Z80 target.
- **udb80**, if you use udb80: `pip install "um80>=0.4.1"`. udb80 needs no GDB.
  For source-level debugging, assemble with `um80 --debug-info` and link with
  `ul80 --debug-info`. The linker writes the `PROG.dbg` file.

The cpmemu document
[gdb_stub.md](https://github.com/avwohl/cpmemu/blob/main/docs/gdb_stub.md)
has the list of GDB choices and the protocol details. The um80_and_friends
documents
[gdb.md](https://github.com/avwohl/um80_and_friends/blob/main/docs/gdb.md) and
[udb80.md](https://github.com/avwohl/um80_and_friends/blob/main/docs/udb80.md)
have the source-level steps.

## Turn the stub on

1. Open **Emulator > Settings**, and go to the **Debugger** page.
2. Select **Enable the GDB stub**.
3. Set the **Port**. The default is 1234.
4. Press **OK**.

OK restarts the machine. The machine does not run an instruction until a
debugger connects. The status line shows where to connect:

```
Running - waiting for GDB, connect to 127.0.0.1:1234
```

The settings are in `z80cpmw.json`, in the `debugger` object: `gdb`,
`gdbPort` and `gdbAllowNetwork`.

## Connect from the same PC

Start GDB and connect:

```
(gdb) set architecture z80
(gdb) target remote 127.0.0.1:1234
```

When the debugger connects, the CPU is at 0000H in ROM bank 00. No
instruction has run. The status line changes to `Running - GDB connected`.

To use udb80, give the address and the port:

```
udb80 PROG.dbg --connect 127.0.0.1:1234 --wait-program
```

## Find a CP/M program: wait-program

z80cpmw does not load a `.COM` file by itself. CP/M loads the file from a disk
image into bank 8EH at 0100H. The debugger therefore asks the stub to stop
when the program starts.

1. Mount a disk image that holds the `.COM` file. Enable the stub.
2. Press OK. The machine waits for the debugger.
3. Connect udb80 with `--wait-program`. udb80 sends the first bytes of the
   program to the stub, and then it continues the guest.
4. At the CP/M prompt in the z80cpmw window, type the name of the program.
5. The stub stops at 0100H. udb80 then shows the source line.

With GDB, send the same rule by hand, and then continue:

```
(gdb) monitor wait-program sha256 LENGTH HEX64
(gdb) continue
```

`LENGTH` is the length of the program in decimal. `HEX64` is the SHA-256 of
the program. The `PROG.dbg` file has both values in `image.length` and
`image.sha256`. The cpmemu document and the romwbw_emu document
`docs/GDB.md` have the other `monitor` commands, such as `monitor bank` and
`monitor bt`.

## Connect over the network

A debugger on a different PC cannot reach 127.0.0.1. There are two ways.

**Allow connections from the network.**

1. Open **Emulator > Settings > Debugger**.
2. Select **Allow connections from the network**. Press **OK**.
3. Windows Firewall can show a prompt when z80cpmw starts to listen. Choose
   **Private networks**. A public network is not safe for the stub.
4. The status line shows the address of this PC and the port:
   `waiting for GDB, connect to port 1234 on every network address of this PC
   (192.168.1.5)`.
5. On the other machine, connect to that address:

```
(gdb) target remote 192.168.1.5:1234
udb80 PROG.dbg --connect 192.168.1.5:1234 --wait-program
```

When the box is clear, z80cpmw listens on 127.0.0.1 only. When the box has a
check mark, z80cpmw listens on 0.0.0.0, which is every network address of the PC.

**An SSH tunnel**, the safer way. Leave the box clear. Install the OpenSSH
Server on the Windows PC. On the debugger machine, run
`ssh -N -L 1234:127.0.0.1:1234 USER@PC`, and connect the debugger to port 1234
on the debugger machine.

## The risk

**The stub has no password.** Anyone who can reach the port can read and write
the memory of the emulated machine, and can control the machine. The guest can
read and write files on the PC with R8 and W8 (see
[File Transfer](FILE_TRANSFER.md)). A debugger that controls the guest can use
these commands. Select **Allow connections from the network** only on a
network you trust, and only while you debug. Clear the box again when you
finish.

## What ends a debug session

- **Detach** (`detach` in GDB). The machine runs on, with no debugger. The
  stub does not accept a second debugger. To debug again, use Stop and Start,
  or Reset.
- **Kill** (`kill` in GDB, or when udb80 quits). z80cpmw stops the machine.
- **Stop, Reset or Start** in the Emulator menu, and **Emulator > Settings**.
  Each of these ends the session. The debugger gets the end-of-run packet.
- The window closes.

## How the stub runs

Without the stub, z80cpmw runs the CPU on the window thread, in batches of
instructions on a 10 ms timer. The stub waits for the debugger and does not
return until the debugger lets go of the machine. Thus the stub runs on a
thread of its own, the debug thread. While the stub is on, the debug thread
owns the CPU. The window thread does not run the CPU, and the window stays
usable. When the debugger detaches, the CPU goes back to the timer.

## Limits

- No watchpoints. `Z2` to `Z4` get an empty reply.
- One debugger at a time, and one debugger for each Start.
- While the stub is on, the debug thread owns the machine. The program refuses File
  commands that load, close or save a disk image. The automatic save of disks
  waits until you stop the machine.
- While the stub is on, the status line does not show the PC or the
  instruction count. The debug thread writes both, and the window thread does
  not read them.
- The Dazzler window can show a torn picture while the debugger holds the CPU.
- `monitor wait-program` knows bank 8EH and 0100H only. CP/M 3 and ZPM3 load
  programs in other ways.
- The stub does not use the console. The debugger sends its own interrupt (the
  byte 03H) to stop the guest.
- Status: nobody has yet run the Windows build of the stub on a Windows PC.
  `MANUAL_CHECKS.md` in the source tree lists the checks.
