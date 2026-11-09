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

## Phase 1 Step A — GUD assessment (2026-11-08)

No integration code written. Waiting for owner approval (PLAN.md Phase 1 Step A.5).

### REQUIREMENTS.md 4.6.8 answers

1. **USB-C speed: full speed (12 Mbit/s).** Schematic (Tab5_Schematics_PDF.pdf): the USB-C data pair (USBIN_DP/DM) runs through R111/R112 (33 Ω) to USB_DEVICE_DP/DM on ESP32-P4 pins 52/53 = GPIO24/25 = USB1P1_0, the full-speed FSLS PHY 0.
   The high-speed OTG pins (pins 49/50, USB2_OTG_D±) are USB_HOST_DP/DM, which go to the USB-A host port.
   Does not rule the mode out: gud-esp32s3 is also full speed. It does cap throughput at about 1 MB/s (gud-esp32s3 README).
   A full uncompressed 720×1280 RGB565 frame is 1.84 MB, so a full-screen change takes about 2 s uncompressed; LZ4 and partial (damage) rectangles make desktop and terminal use faster.
2. **Serial console shares the port: yes.** The console is USB-Serial/JTAG (esptool: "USB mode: USB-Serial/JTAG") on FSLS PHY 0, the same PHY as USB-C.
   To run GUD, the full-speed OTG controller must take PHY 0 (`usb_wrap_ll_phy_select(0)` sets `LP_SYS.usb_ctrl.sw_usb_phy_sel`), which moves USB-Serial/JTAG to PHY 1 (GPIO26/27, used for I2S on the Tab5, no connector).
   So the serial console and esptool's automatic reset are unavailable while the mode runs. A Tab5 restart restores them.
3. **pioarduino USB layer: not usable as is; TinyUSB underneath is.**
   Arduino's `USB`/`USBHID`/`USBVendor` classes start the device on TinyUSB rhport 1 (`esp32-hal-tinyusb.c`, `tusb_init(1, …)`: "P4 OTG HS is rhport 1"), the high-speed controller wired to USB-A.
   The precompiled TinyUSB (`libarduino_tinyusb.a`, framework 3.3.12) is configured with rhport 0 = device and rhport 1 = host, and its ESP32-P4 port maps rhport 0 to the full-speed controller (`dwc2_esp32.h`: 7 endpoints, 5 IN, which is enough for GUD bulk OUT plus two HID IN).
   `usbd_app_driver_get_cb` (needed for GUD's custom class driver) and the `tud_descriptor_*_cb` callbacks are weak symbols, so the firmware can supply its own.
   The firmware already starts TinyUSB itself for the USB-A host (`src/Tab5KeyboardInput.cpp:757`). GUD would do the same on rhport 0, as gud-esp32s3's `usb_glue.c` does.
   Does not rule the mode out. Running device (rhport 0) and host (rhport 1) at once in one TinyUSB instance is untested on this board.
4. **Orange Pi `CONFIG_DRM_GUD`: not yet known.** The Orange Pi Zero 2W has not arrived yet (2026-11-08). Owner to run `modinfo gud` on the Orange Pi. gud-esp32s3 reports Armbian usually has it (`=m`) and Debian 6.12+ does not.
5. **Orange Pi USB host port: not yet known.** Official page refused connections; linux-sunxi returned 403. Owner to check which Type-C port enumerates a device (`lsusb` / `dmesg -w` while plugging in a USB stick).
6. **Charger: controllable, current draw not yet measured.**
   The Tab5 USB-C port has 5.1 kΩ pull-downs on CC1/CC2 (R52/R53), so it is a power sink only: the Orange Pi will supply its VBUS.
   USB_5VIN feeds an IP2326 (5 V to 2-cell boost charger).
   Its enable (CHG_EN) and 500/1000 mA step (nCHG_QC_EN) are on I/O expander 1 (PI4IOE5V6408), driven by M5Unified `M5.Power.setBatteryCharge()` and `setChargeCurrent()`.
   Plan: turn charging off on entering the mode, back on when leaving.
   Still unknown: how much the Tab5 draws from VBUS with charging off, and how much the Orange Pi's port can supply. Needs a USB power meter between the two.
   Does not rule the mode out yet.

### Licenses

| Source | License | Notes |
|---|---|---|
| notro/gud-pico | MIT | `LICENSE.md` |
| Ponywka/gud-esp32s3 | MIT | `main/gud/*` SPDX MIT, from gud-pico (Noralf Trønnes). `host/gud-module` is Linux `drivers/gpu/drm/gud`, SPDX MIT |
| nhattran02/generic-usb-display-device | MIT | Same author and near-identical tree to gud-esp32s3 (Vladimir Avtsenov) |
| freand76/stm32-usb-gud-display | MIT | Reference only |
| TinyUSB | MIT | Already in the framework; nothing to vendor |

All MIT, compatible with the fork. Vendored files keep their headers and get a THIRD_PARTY.md entry.

### Porting gaps (gud-esp32s3 → ThinClient)

1. USB bring-up: replace `USB.begin()` with a direct `tusb_init(0, device, full speed)` after switching PHY 0 to the OTG controller; supply our own descriptors (GUD vendor 1d50:614d, HID keyboard, HID multitouch).
2. HID keyboard is new: gud-esp32s3 has touch only. The Tab5 Keyboard driver runs in Character mode (`cfg.mode = Character`), so the mode needs raw key events, either via the M5Unit-KEYBOARD library's raw mode or a reverse map. USB-A keyboard reports can pass through as they arrive.
3. Framebuffer: ThinClient draws into an 8-bit 1280×720 sprite (boot log: "sprite depth=8"). GUD needs RGB565 at 720×1280 portrait, so the mode writes rectangles straight to the panel through M5GFX (or its DSI framebuffer), not through the sprite. 1.84 MB in PSRAM for a receive buffer, plus a 64 KB internal-RAM LZ4 window.
4. Touch: map M5.Touch points (5 contacts) to gud-esp32s3's descriptor in 720×1280 coordinates.
5. Console loss (item 2): any serial output while the mode runs must not block (HWCDC with no host).
6. Existing host start calls `init_usb_hal(true)`, the Arduino device-PHY helper, rather than `init_usb_hal_host`. Check this does not clash once a device PHY is also created.

### Other finding

On every boot with a saved SSH profile, the firmware connects to the active profile as soon as Wi-Fi is up (`src/main.cpp:5890`, `:5925`), uploads a helper, and mounts the Tab5's storage on that host as `~/sd` and `~/usb`. There is no setting to turn this off. The screen then shows the remote shell, not `tab5:/$`. Owner to decide whether B1's "boots to `tab5:/$`" should change, or auto-connect become optional.

## Next

Owner: review the Phase 1 Step A assessment; answer 4.6.8 items 4 and 5 on the Orange Pi; measure item 6 with a USB power meter. Then approve or stop Phase 1.
