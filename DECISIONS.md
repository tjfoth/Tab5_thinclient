# Decisions and Test Log

Shared record for the Tab5 ThinClient fork (see REQUIREMENTS.md and PLAN.md).
Newest entries go at the end of each section.

## Owner decisions

- 2026-11-08: The fork is public on purpose, so others can use it.
- 2026-11-08: The Tab5 connects to the owner's Mac, not the build Pi. The Pi builds; the Mac flashes and runs device tests.
- 2026-11-08: No flash backup of the device is taken.
- 2026-11-08: The supported keyboard is the Tab5 Keyboard (A164) on Ext.Port1. Upstream BLE keyboard code stays but is not tested.
- 2026-11-08: MIT, BSD, Apache-2.0, LGPL and GPL-3.0 are approved for any use in this project.
- 2026-11-08: A mesh radio client (Meshtastic or MeshCore, through a Heltec node) is added as an experiment. The Heltec model is decided in that phase.

## Phase 0 — Host setup, baseline build

### Build host

- Raspberry Pi 5, Raspberry Pi OS (Debian 12), Linux 6.12, aarch64.
- PlatformIO platform: pioarduino `espressif32` 55.03.312 (`stable` URL in platformio.ini).
- Arduino core 3.3.12, ESP-IDF libs 5.5.5, toolchain riscv32-esp GCC 14.2.0, esptool 5.4.0.
- Libraries: M5Unified 0.3.0, M5GFX 0.3.0, M5Unit-KEYBOARD 0.1.0, ArduinoJson 7.4.3, LibSSH-ESP32 5.10.0.
  These meet the ST7121 minimums (M5Unified >= 0.2.23, M5GFX >= 0.2.30).
- The aarch64 toolchain risk in PLAN.md did not happen: the unmodified firmware builds on the Pi.

### Known issue: pioarduino Core 6.2.0.post1 breaks the second build

- On the first build, the platform installs the PyPI package `pioarduino` (>= 6.2.0) over PlatformIO Core.
  The newest release, 6.2.0.post1, reports Core version `6.2.0-post.1`.
  PlatformIO ranks that below 6.2.0, so the platform's own `engines: >=6.2.0` check fails on every later build, and PlatformIO removes the platform:
  `IncompatiblePlatform: Development platform 'espressif32' is not compatible with PlatformIO Core v6.2.0-post.1 and depends on PlatformIO Core >=6.2.0.`
- Workaround (host only, no repo change): `~/.platformio/penv/bin/pip install 'pioarduino==6.2.0'`.
  Verified: two builds in a row succeed and the version stays at 6.2.0.

### Firmware size

- `firmware.bin` 3,454,304 bytes; app partition `app0` is 0x400000 (4,194,304) bytes.
- Free in app partition: about 740 KB (82% used).
- This is the budget for Lua, BASIC, wget, editors, file commands and the mesh client.
  Changing `partitions.csv` needs the owner's approval (PLAN.md rule 4); `littlefs` is 0xBF0000 (about 12 MB) and could give up space.

### Flash procedure (Mac)

Bundle: `local/phase0/` (git-ignored), copied to the Mac with scp.
Command, taken from `pio run -t upload -v`:

```
esptool --chip esp32p4 --port <PORT> --baud 1500000 --before default-reset --after hard-reset \
  write-flash -z --flash-mode dio --flash-freq 80m --flash-size detect \
  0x2000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

NVS (0x9000) and LittleFS (0x410000) are not written, so saved Wi-Fi and SSH profiles stay.

### Status

- [x] PlatformIO installed, fork cloned, baseline build passes.
- [x] Chip revision: ESP32-P4 revision v1.3, 400 MHz dual core, USB mode USB-Serial/JTAG (esptool v5.6.0 `chip-id`, 2026-11-08).
- [x] Flashed from the Mac (Pi-built firmware, owner confirmed 2026-11-08; esptool output not kept).
- [x] B1–B6 pass on the Pi build, 2026-11-08:
  - B1 passes: `help` prints the list, `status` shows `stage=loop`, and the owner saw the `tab5:/$` prompt on screen.
  - B2 passes: `python -c print('hello')` prints `hello`.
  - B3 passes: serial `sd ls /`, and `ls /sd` typed at the `tab5:/$` prompt lists the card.
  - B4 passes: saved profile Studio1 connects, ip 192.168.1.198.
  - B5 passes: the saved SSH profile auto-connects to RPI4Server-1 (192.168.1.216); `ssh send` ran `ls -a ~` and `term dump` shows the listing (`local/phase0/b5b6.sh`).
  - B6 passes: `scp put` /b6_out.txt to /tmp/b6_test.txt and `scp get` back to /b6_back.txt; both files hold the same 20 bytes.
  - Upstream quirk: after leaving SSH (`exit`), the local screen accepted keys (`events` counted up) but showed nothing typed. `clear` fixed it. Likely cause, not confirmed: the local terminal was scrolled up, and `drawTerminal()` hides the input line unless `terminal.atBottom()`.
  - Serial harness: `cli <command>` (not listed in `help`) runs a `tab5:/$` command and echoes its output over serial. `term dump` reads only the SSH screen.
  - Serial harness: `serial_command.py` must set RTS off before DTR. Setting DTR off first while RTS is on resets the ESP32-P4 (reset reason 11) on every command.

## Next

Phase 0 passed 2026-11-08. Next: Phase 1 Step A (GUD assessment, no integration code).
