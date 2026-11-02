# Tab5 ThinClient Fork — Implementation Plan

Companion to REQUIREMENTS.md.
Executor: Claude Code, running on the owner's Raspberry Pi with the Tab5 connected by USB-C.

## 0. Rules for every session

1. **Replay before inventing.** Before writing code for any step, look for a working example of the same thing and follow it.
   The existing MicroPython runner inside ThinClient is the primary pattern for hosting an interpreter.
   ESP32 tinyConsole (https://github.com/spuggy0919/ESP32_tinyConsole) is a working example of Stefan's BASIC embedded in an ESP32 Arduino shell.
   BreezyBox (https://github.com/valdanylchuk/breezybox) contains an MIT-licensed minimal vi under `apps/vi`.
2. **One phase per session.** Finish a phase, pass its tests, commit, then stop.
3. **DECISIONS.md is the shared memory.** Create it in Phase 0.
   At the end of every session, record what was decided, what versions were used, what passed, and what is next.
   Read it at the start of every session.
4. **Stop and ask the owner** before any of these:
   - Changing `partitions.csv`.
   - Erasing flash.
   - Adding a component whose license is not MIT, BSD, Apache, LGPL, or the already-accepted GPL-3.0 for BASIC.
   - Any action that touches the ESP32-C6.
   - Abandoning a phase.
5. **Serial port care.** The firmware's serial API runs at 115200 baud.
   Avoid DTR/RTS toggles when opening the port; they can reset the board.
6. **Verify by screen contents, not assumption.** Use the serial command `term dump` to read the terminal contents after each test.

## Phase 0 — Host setup, safety backup, baseline build

Goal: prove the Pi can build and flash ThinClient unmodified, without losing the working firmware.

Steps:

1. Install PlatformIO Core on the Pi.
2. Clone the owner's GitHub fork of `airpocket-soundman/Tab5_thinclient`.
3. **Back up the device before anything else.**
   Read the full 16 MB flash with esptool (`read_flash 0 0x1000000`) to a dated file.
   Copy it off the Tab5 to the Pi and to the owner's fork as a release asset or a git-ignored local file.
   Record the file name and SHA-256 in DECISIONS.md.
4. Record the ESP32-P4 chip revision that esptool reports.
5. Build: `pio run -e tab5`.
6. Record the M5GFX and M5Unified versions the build resolved.
   M5Stack's Tab5 page says ST7121 units need M5Unified ≥ 0.2.23 and M5GFX ≥ 0.2.30 (https://docs.m5stack.com/en/core/Tab5).
   If the build resolves older versions, stop and report before flashing.
7. Flash: `pio run -e tab5 -t upload`.
   The repo's full-flash script is PowerShell; do not use it.
   If LittleFS profile data must be written, use the PlatformIO filesystem upload target and record the command.
8. Run baseline tests B1–B6.

Baseline tests (must pass now and after every later phase):

- B1. Device boots to `tab5:/$` and `help` prints the command list.
- B2. `python -c print('hello')` prints `hello`.
- B3. `ls /sd` lists the SD card.
- B4. Wi-Fi connects from a saved profile.
- B5. `ssh user@host` reaches a test host and runs `ls`.
- B6. `scp get` and `scp put` move a file both ways.

Exit criteria: B1–B6 pass on a firmware built on the Pi.

**Known risk:** PlatformIO's ESP32-P4 toolchain on aarch64 Linux is unverified.
If the build fails for toolchain reasons, stop and report.
Do not attempt workarounds without the owner's approval.

## Phase 1 — Program hosting plumbing, then Lua

Goal: one reusable way to run a foreground program, proven with Lua.

Steps:

1. Read how ThinClient runs MicroPython: console input, console output, Ctrl-C, and teardown.
2. Factor or copy that into a small hosting layer that later programs share.
3. Add Esc passthrough while a hosted program runs, mirroring how SSH sessions already pass Esc through.
4. Vendor the current stable Lua release from lua.org.
5. Route Lua's output and input through the hosting layer.
6. Point `io` and `os` at the firmware's file system.
7. Register `lua` in the CLI, `help`, and `man`.
8. Check flash usage against `partitions.csv` and record the margin.

Acceptance tests:

- L1. `lua` starts a REPL; `print(1+1)` prints `2`; `os.exit()` returns to `tab5:/$`.
- L2. `lua /sd/test.lua a b` runs a script that prints its arguments.
- L3. A Lua script writes `/sd/out.txt`; `cat /sd/out.txt` shows the content.
- L4. Ctrl-C stops `while true do end` and returns to a prompt.
- L5. 20 start/exit cycles show no free-heap loss beyond the tolerance recorded in DECISIONS.md.
- L6. Esc inside the Lua REPL does not move focus to the menu bar.
- B1–B6 still pass.

## Phase 2 — BASIC

Goal: Stefan's IoT BASIC as `basic`.

Steps:

1. Vendor slviajero/tinybasic; record the commit hash.
2. Configure `BASICFULL`.
3. Follow ESP32 tinyConsole's integration pattern where it applies.
4. Route console I/O through the Phase 1 hosting layer.
5. Map LOAD and SAVE to the firmware's `/sd` path.
6. Restrict GPIO commands to ThinClient's MicroPython pin table.
7. Map the Wire stream to the Grove I2C pins used by the MicroPython GPIO docs.
8. Register `basic` in the CLI, `help`, and `man`.

Acceptance tests:

- BA1. `basic`, then `10 PRINT "HELLO"`, `RUN` prints `HELLO`.
- BA2. `SAVE` to `/sd`, `NEW`, `LOAD`, `LIST` restores the program.
- BA3. `basic /sd/hello.bas` runs a saved program and returns to `tab5:/$`.
- BA4. A FOR/NEXT loop with string functions (`LEFT$`, `MID$`, `LEN`) runs correctly.
- BA5. `PINM`/`DWRITE` toggles an allowed connector pin; an on-board pin is refused.
- BA6. An I2C scan program from the tinybasic `examples/13wire` folder finds a device on the Grove port, if the owner attaches one. If none is attached, record the test as not run.
- BA7. Ctrl-C (or the interpreter's documented break key) stops a running loop.
- BA8. 20 start/exit cycles show no heap loss.
- B1–B6 still pass.

Record observed differences from DEC BASIC-PLUS in DECISIONS.md.

## Phase 3 — wget

Goal: HTTP/HTTPS fetch to file.

Steps:

1. Use the HTTP client and TLS stack already in the ESP32 Arduino core.
2. Enable a CA certificate bundle for HTTPS validation.
3. Stream the body to the file; do not buffer whole files in RAM.
4. Register `wget` in the CLI, `help`, and `man`.

Acceptance tests:

- W1. `wget https://<known small file>` saves it with the right name and size.
- W2. `wget <url> -O /sd/x.txt` saves to the given path.
- W3. A file larger than 1 MB downloads intact; compare SHA-256 with the Pi's copy.
- W4. A bad hostname, a 404, and a write to a missing directory each print an error and return to the prompt.
- W5. Python, Lua, and BASIC still pass L1 and BA1 after wget runs.
- B1–B6 still pass.

## Phase 4 — Editors

Goal: `vi` (modal) and `edit` (modeless).

Steps:

1. `vi`: start from BreezyBox's `apps/vi` (MIT). Confirm its license before vendoring.
2. `edit`: start from antirez's kilo or an equivalent small modeless editor. Confirm its license before vendoring.
3. Replace POSIX terminal calls (raw mode, `read`, `write`) with the Phase 1 hosting layer.
4. Map Tab5 Keyboard arrows, Esc, Backspace, Delete, and Ctrl combinations; record the mapping in DECISIONS.md.
5. Register both in the CLI, `help`, and `man`.

Acceptance tests (run for each editor):

- E1. Open a new file, type three lines, save, quit; `cat` shows the three lines.
- E2. Reopen, move with arrows, change a word in line 2, save; `cat` shows the change.
- E3. Quit without saving discards changes.
- E4. A 500-line file opens, scrolls to the end, and saves intact.
- E5. Esc reaches the editor (essential for `vi`); the menu bar does not take focus.
- E6. A Lua script and a BASIC program edited in the editor then run correctly.
- B1–B6 still pass.

## Phase 5 — EXPERIMENT: on-board C compiler (rcc700)

This phase is an experiment.
It may fail.
Do not start it until Phases 0–4 pass.

Steps:

1. Read rcc700's README and its BreezyBox integration.
2. Write a short assessment in DECISIONS.md: what rcc700 needs from its host (ELF loading, exported symbols, memory regions), and what ThinClient lacks.
3. Stop and present the assessment to the owner before writing integration code.
4. If the owner approves, integrate on a separate branch.

Acceptance tests (only if approved):

- C1. Compile and run a hello-world C file from `/sd`.
- C2. Compile and run a program that uses a loop and a function call.
- C3. Phases 1–4 acceptance tests still pass on the branch.

Stop criteria: if the assessment shows the work needs an ELF loader port plus symbol export across the Arduino framework, or if the branch breaks any earlier phase, close the phase with a written failure report and leave `main` untouched.

## Session start checklist

1. Read REQUIREMENTS.md, PLAN.md, and DECISIONS.md.
2. Confirm which phase is current.
3. Confirm the device is connected and boots to `tab5:/$`.
4. Run B1 before changing anything.
