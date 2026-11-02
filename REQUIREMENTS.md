# Tab5 ThinClient Fork — Requirements

Owner: Tom Foth
Date: 2026-11-01
Status: Approved for planning

## 1. Purpose

Extend the TAB5 ThinClient firmware so the M5Stack Tab5 is a standalone pocket computer.
Every program in this document runs on the Tab5 itself.
No program may depend on a network host to run.
Network access is needed only for SSH, SCP, and wget, which talk to the network by nature.

## 2. Starting point

The base is TAB5 ThinClient by airpocket-soundman.
Source: https://github.com/airpocket-soundman/Tab5_thinclient
License: MIT for project code; LGPL-2.1-or-later for LibSSH-ESP32 (see THIRD_PARTY_LICENSES.md in the repo).
Build system: PlatformIO, Arduino framework (pioarduino), environment `tab5`.

ThinClient already provides these, and they must keep working unchanged:

- A local Linux-like CLI at the `tab5:/$` prompt.
- A virtual file system with `/flash`, `/sd`, and `/usb`.
- SSH client (`ssh ...`).
- SCP-style transfer (`scp get`, `scp put`).
- Embedded MicroPython (`python`, `python <file.py>`, `python -c ...`).
- Image viewer, BLE keyboard support, Wi-Fi and SSH profile screens.

The CLI is not a POSIX shell.
It has no pipes, redirection, quoting, or expansion.
Example: `python -c print('hello')` works; `python -c "print('hello')"` passes the quote characters to MicroPython and prints nothing.

## 3. Target hardware

- M5Stack Tab5 with the Tab5 Keyboard (A164) on Ext.Port1.
- Display controller: ST7121 (newest panel revision).
- The ThinClient build currently on the device already works on this panel, keyboard, and Wi-Fi.
- MicroPython reports `(1, 29, 0, 'preview')`, built with GCC 14.2.0, platform `tab5`.

## 4. New programs

Each program is a new CLI command compiled into the firmware.
Each one runs in the foreground, owns the terminal while running, and returns to the `tab5:/$` prompt on exit.

### 4.1 Lua — command `lua`

- `lua` with no arguments starts an interactive REPL.
- `lua <file.lua> [args...]` runs a script from the file system.
- Use the current stable release from https://www.lua.org/ .
- Standard libraries: base, string, table, math, utf8, coroutine, io, os.
- `io` and `os` must read and write files on `/sd` and `/flash`.
- `os.exit()` and end-of-script return to the CLI prompt.

### 4.2 BASIC — command `basic`

- Interpreter: Stefan's IoT BASIC.
  Source: https://github.com/slviajero/tinybasic
  License: GPL-3.0. Owner accepts this for personal use.
- Language set: the full set (`BASICFULL`).
- `basic` with no arguments starts the interactive line-numbered environment.
- `basic <file.bas>` loads and runs a program.
- LOAD and SAVE must work against `/sd`.
- GPIO commands (`PINM`, `DWRITE`, `DREAD`, `AWRITE`, `AREAD`) must work on the Tab5 connector pins.
- I2C via the Wire stream (`OPEN &7`, etc.) must work on the Grove port.
- PEEK and POKE keep Stefan's semantics: BASIC memory, not CPU or peripheral addresses.
- Owner's background is DEC BASIC-PLUS. Document dialect differences found during testing in DECISIONS.md.

### 4.3 Editors — commands `vi` and `edit`

Two editors are required.

- `vi <file>`: a small modal editor in the vi style.
  Insert mode, command mode, cursor movement by arrows and h/j/k/l, `:w`, `:q`, `:wq`, `:q!`.
- `edit <file>`: a small modeless editor in the nano/kilo style.
  Typing inserts text; Ctrl-key commands save, quit, and find.
- Both create the file if it does not exist.
- Both must handle files on `/sd` and `/flash`.
- Both must work with the Tab5 Keyboard, including arrow keys and Esc.

### 4.4 Web fetch — command `wget`

- `wget <url>` saves the response body to the current directory, named from the last path segment of the URL.
- `wget <url> -O <path>` saves to the given path.
- HTTP and HTTPS are both required.
- HTTPS validates server certificates against a CA bundle.
- Prints a progress indication and a final byte count.
- Prints the HTTP status and exits cleanly on errors (DNS failure, connection failure, non-2xx status, write failure).

### 4.5 On-board C compiler — EXPERIMENT

- Candidate: rcc700, a small C compiler for the ESP32-P4.
  Source: https://github.com/valdanylchuk/rcc700
- This item is an experiment.
  It is attempted only after 4.1 through 4.4 pass.
  It may fail.
  Failure must leave 4.1 through 4.4 intact and working.
- Known risk: rcc700 was built for BreezyBox, which loads compiled programs with an ELF loader and exports firmware symbols for them.
  ThinClient has neither.

## 5. Behavior common to all new programs

- **Return to prompt.** Every program exits back to `tab5:/$`.
- **Interrupt.** Ctrl-C stops a running Lua script or BASIC program and returns to its own prompt or to the CLI.
- **Memory.** Free heap after a program exits must match free heap before it started, within a small tolerance recorded in DECISIONS.md.
  Each program is tested over at least 20 start/exit cycles.
- **Esc key.** ThinClient uses Esc to move focus to the menu bar, except during SSH sessions, where Esc goes to the remote program.
  While `vi`, `edit`, `lua`, or `basic` is running, Esc must go to the program, the same way it does for SSH.
- **Pin safety.** Lua and BASIC may touch only the pins that ThinClient's MicroPython GPIO API accepts.
  Reuse that pin table; do not create a second one.
  Reference: docs/PYTHON.md and docs/GPIO.md in the ThinClient repo.
- **Help.** Each new command appears in `help` and has a `man` entry.

## 6. Constraints

- Do not reflash, modify, or replace the ESP32-C6 firmware.
- Do not commit Wi-Fi passwords, SSH credentials, or `data/profiles.local.json`.
- Do not remove or change existing ThinClient behavior except the Esc routing in section 5.
- Keep new code in new files where possible, so upstream changes can be merged later.
- Record every license of every added third-party component in THIRD_PARTY_LICENSES.md.

## 7. Out of scope

- General SPI access from BASIC (Stefan's BASIC supports SPI only as RAM storage).
- Hardware-register PEEK and POKE.
- Pipes, redirection, or quoting in the CLI.
- Any change to SSH, SCP, MicroPython, Wi-Fi, BLE, or the image viewer.
- Running programs in the background or more than one at a time.

## 8. Definition of done

- Every command in section 4.1 through 4.4 passes its acceptance tests in PLAN.md on the owner's Tab5.
- Every existing ThinClient feature in section 2 still works.
- DECISIONS.md records versions, choices, test results, and known limitations.
- Section 4.5 has either passed its tests or been closed with a written failure report.
