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
- 2026-11-15: USB display mode (GUD) is all or none: it must work at high speed on all three hosts (Radxa Zero 3W, Orange Pi Zero 2W, Orange Pi Zero 3W), or it is not pursued. If the Radxa cannot be made to work, Phase 1 closes.
- 2026-11-13: An ambient info display app is to be added to the fork. Not yet in REQUIREMENTS.md or PLAN.md: what it shows, where its data comes from, how it starts and stops, and which phase it belongs to are still to be defined by the owner.

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
   Measurement 2026-11-09 (owner, USB meter on the Tab5 USB-C, fed by the Mac, firmware running, charging at its default setting): 4.9 V, 1.9 A, 9 W. Battery charge level not recorded. With charging on, this is far more than a USB 2.0 port is required to supply (500 mA).

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

2026-11-09: owner asked for a setting. Added `system.sshAutoConnect` (default on, so existing behaviour is unchanged): CONF screen row "SSH Auto", local CLI `ssh auto [on|off]`, serial `ssh auto [on|off]`, and `sshAuto=` in serial `status`. It gates the connect that runs when Wi-Fi comes up, at boot or on Wi-Fi reconnect. Builds on the Pi; not yet tested on the device (bundle `local/sshauto/`).
Same bundle: SSH and Wi-Fi password fields now show the password while that field is selected, and stars otherwise (owner request). Serial output is unchanged.
Same bundle: `power` and `power charge on|off` (local CLI and serial) report battery mV, mA, level, charge state and VBUS via M5Unified, and switch charging with `M5.Power.setBatteryCharge()` until restart. `local/sshauto/test_power.sh` records meter readings with charging on and off (question 6).

### 2026-11-09: GUD moves to the USB-A port (high speed)

Owner decision: GUD is only worth doing at USB high speed (480 Mbit/s). The USB-C port is full speed (12 Mbit/s, item 1), so the mode would use the Tab5's USB-A port instead.

Measured 2026-11-09 (owner, Mac `system_profiler SPUSBHostDataType`, Tab5 USB-C on a Mac USB 3.1 port): "USB JTAG/serial debug unit", 303a:1001, Link Speed 12 Mb/s. Confirms item 1: the limit is on the Tab5 side.

Hosts: Orange Pi Zero 2W, Orange Pi Zero 3W, Radxa Zero 3W, possibly Raspberry Pi 4.

Schematic findings (Tab5_Schematics_PDF.pdf, pages 4 and 5):
- Data: USB-A J10 pins 2/3 (EXT_USB_DM/DP) go through common-mode choke FT2, ESD array D21 and 33 Ω resistors to USB_HOST_DM/DP on ESP32-P4 pins 49/50, the high-speed OTG controller. Arduino on the P4 already uses this controller as a USB device by default (rhport 1, item 3).
- Power: J10 VCC is SYS_USB5V, from load switch U27 (MT9700, VIN = SYS_INT5V, R84 5.1 kΩ current-limit set). Its EN is USB5V_EN on I/O expander 0x44 bit 3, which M5Unified drives with `M5.Power.setExtOutput(enable, ext_USB)`.
- Not verified: whether the MT9700 blocks reverse current from the host's VBUS while disabled. Not relied on.

Decisions:
- An inline USB power blocker (owner has one) sits between the Tab5 and the host every time, so the two 5 V supplies are never connected. The firmware also turns USB5V_EN off while the mode runs.
- No USB-A keyboard. The Tab5's own keyboard is sent to the host as a USB HID keyboard.
- The serial console on USB-C keeps working in this mode (unlike the USB-C plan, item 2).

Open:
1. Host port role per board. On an A-to-C cable, the cable's Rp resistor tells the C end to be a device. A host port fixed as `dr_mode = host` ignores that; an OTG port may not. Check each board without the Tab5 attached: `dr_mode` under /proc/device-tree and /sys/class/usb_role. RPi 4 USB-A ports need an A-to-A cable, acceptable only with the blocker.
2. With no VBUS, the P4 device controller must be told to assume a session is valid instead of sensing VBUS.
3. Real throughput, measured once frames are drawn.
4. `lsusb` on the host only shows the Tab5 once the firmware presents a device on USB-A; the current firmware runs USB-A as a host.

USB-A device test 2026-11-09/10 (test sketch `tests/usba_device/`, bundle `local/usbatest/`; Tab5 USB-A → power blocker → A-to-C cable → Radxa Zero 3W USB 3.0 port, `usb@fd000000`, dr_mode host; the Radxa's other port `usb@fcc00000` is otg):
- Tab5 USB-A 5 V (USB5V_EN, expander 0x44 pin 3) read back off. Note: `M5.Power.getExtOutput()` on Tab5 reads EXT5V_EN (expander 0x43 pin 2), not USB-A.
- The P4 HS controller reports B-session valid at boot with nothing connected, so VBUS sensing is not a blocker; the GOTGCTL B-valid override changed nothing.
- High-speed build: the Radxa logs "new high-speed USB device", then `device descriptor read/64, error -71` and "not accepting address", so it never enumerates. Same with and without the override.
- Full-speed build (DCFG devspd = 1): enumerates as 303a:4002 "Tab5 USB-A test", cdc_acm ttyACM0, 12M; 23 MB received at 288 kB/s (CDC overhead).
- Conclusion: USB-A device mode and the firmware work; the 480 Mbit/s failure is most likely signal integrity in the blocker/cable path. Next: check the cable alone at high speed (Radxa gadget → Mac), then a high-speed test without the blocker once the Radxa host-port VBUS can be controlled.
- Cable check 2026-11-10: the same A-to-C cable, Radxa OTG port in gadget mode (NCM) → Mac: Link Speed 480 Mb/s. Owner to confirm the power blocker was in that chain; if it was, the blocker and cable pass high speed and the fault is on the Tab5 side.
- Tab5-side suspect (unverified): Arduino `init_usb_hal()` on the P4 (`esp32-hal-tinyusb.c`) creates the PHY with `USB_PHY_TARGET_INT` ("internal FSLS PHY") while TinyUSB runs the device on the high-speed controller (rhport 1), whose PHY is UTMI. Arduino's own host path uses `USB_PHY_TARGET_UTMI`. Test build `usba_test_utmi.bin` (`./flash.sh <port> utmi`) deletes that PHY and creates a UTMI device PHY at high speed, then reconnects. To run 2026-11-10.
- Ports on 2026-11-15 (owner): every run up to and including the 10:06 PM `dmesg` was with the cable in the Radxa's OTG port (`fcc00000`, in device mode), so those runs could not enumerate and say nothing about either image. The only USB lines then were `dwc3 fcc00000.usb: device reset` (twice; what drove the reset is unexplained). The 10:10 PM run was in the host ("Data") port (`fd000000`): no `lsusb` entry, no ttyACM, and no new USB lines in `dmesg`, so nothing at all was presented on the bus. The owner did not say which image was on the Tab5 for that run (`utmi2` was the one supplied), and the Tab5 screen text was not recorded.
- UTMI test 2026-11-15 (`usba_test_utmi.bin`, in the OTG port, see above): the Tab5 did not appear in `lsusb` and no ttyACM was created. `dmesg` was not captured (the script ran it without sudo) and the Tab5 screen text was not recorded.
  This result does not test the PHY suspect: the build was wrong. It created the UTMI PHY after `USB.begin()`, and creating that PHY resets the high-speed controller (`_usb_utmi_ll_reset_register` sets `rst_en_usb_otg20`, `hal/usb_utmi_ll.h`), which wipes TinyUSB's device setup. Cause inferred from the header, not confirmed on the device.
  Replacement: `usba_test_utmi2.bin` (`./flash.sh <port> utmi2`) creates the UTMI PHY before `USB.begin()` and leaves Arduino's own PHY in place. The old image is removed from the bundle. `host_check.sh` now runs `sudo dmesg`. Not yet run.
- `utmi2` on the host port, 2026-11-15 (owner photo of the Tab5 screen): title "HIGH-SPEED + UTMI v2", "waiting for host", "UTMI PHY before USB start: ESP_OK", "VBUS seen before override: no", B-valid override on, session valid. Nothing on the bus (see ports note). Suspected, not confirmed: Arduino's `tinyusb_driver_install()` returns without calling `tusb_init()` when its own `init_usb_hal()` fails, and the earlier `usb_new_phy()` call may make it fail.
- Control 2026-11-15, 10:27 PM: full-speed image on the host port, same blocker and cable: enumerates as 303a:4002, cdc_acm, 12M, 289 kB/s; Tab5 shows "Speed: FULL". So the setup is good and the fault is in the `utmi2` image.
- `usba_test_utmi3.bin` (`./flash.sh <port> utmi3`): calls ESP-IDF's `_usb_utmi_hal_init()` directly before `USB.begin()` and does not call `usb_new_phy()`, so Arduino's start-up runs as in the plain high-speed build. Reason to expect a difference: `sys_otg_suspendm` (HP_SYSTEM, "PHY not suspended") resets to 0 per `hp_system_struct.h`, and nothing in TinyUSB's P4 port sets it (`dwc2_phy_init()` is empty, comment "maybe usb_utmi_hal_init()"). That `_usb_utmi_hal_init()` sets it is from memory of the ESP-IDF source, not checked. All images built from now on also show: `USB.begin` result, whether the TinyUSB stack is up, `suspendm` at boot and now, soft-disconnect and enumerated speed. `usba_test.bin` was rebuilt with these lines; `usba_test_fs.bin` was not. Not yet run.
- `utmi3` result 2026-11-15, 10:37 PM (host port, blocker inline): same failure as the plain high-speed build on 2026-11-10. `dmesg`: "new high-speed USB device", `device descriptor read/64, error -71` (x4), port power cycle, "Device not responding to setup address", "unable to enumerate USB device". Tab5 shows "waiting for host"; the diagnostic lines were not recorded.
  Conclusion: the Tab5 starts the high-speed handshake (chirp) but packets after it are corrupted. Running the vendor PHY init made no difference, so PHY setup is no longer the main suspect. Next suspect: the signal path (blocker, cable, Tab5 USB-A layout) at 480 Mbit/s.
- Blocker check 2026-11-15 (owner): Mac USB-C → power blocker → USB-C → Radxa OTG port (NCM gadget): Mac reports "Radxa Zero 3W NCM", Link Speed 480 Mb/s. Owner confirmed the blocker was in line and is the same blocker used in the Tab5 tests.
  With the 2026-11-10 cable check (same A-to-C cable at 480 Mb/s), the blocker and the cable each pass high speed; they have not been tested together.
  Conclusion: the high-speed failure is most likely on the Tab5 side (P4 high-speed device mode on this board, or the USB-A wiring at 480 Mbit/s).
  Options: (1) test the Tab5 against another host (e.g. an Orange Pi) to rule out the Radxa host port; (2) GUD at full speed (about 1 MB/s); (3) close Phase 1 with a failure report.
- Mac as host, 2026-11-15 (owner): Tab5 USB-A → power blocker (in the same orientation as the 480 Mb/s Mac → Radxa check: Mac on the host side; a USB-A male-to-male adapter made that possible) → MacBook. Exact cables and adapters in the chain not recorded. Image `utmi3`. The Mac lists "Tab5 USB-A test", Link Speed 480 Mb/s; Tab5 shows CONNECTED, "Speed: HIGH (480 Mbit/s)", `USB.begin: ok stack: up`, `suspendm boot=1 now=1`, `softdiscon=0 enumspd=0`.
  **The Tab5 USB-A port works as a high-speed device.** The Radxa failure is in what differed there: the Radxa host port, the A-to-C cable, or the blocker's orientation (which side faced the Tab5 in the Radxa tests is not recorded).
  `suspendm boot=1` means the PHY was already out of suspend at boot, so v3's PHY init probably made no difference; the plain high-speed image has not been tried on the Mac.
  An earlier attempt the same night, with the blocker turned the other way round, showed nothing on the Mac; the Tab5 screen was not recorded, so it is inconclusive.
- Radxa recheck 2026-11-15, 11:17 PM: the exact chain that ran at 480 Mb/s on the Mac (same blocker orientation and adapters, Mac end moved to the Radxa host "Data" port `fd000000`), image `utmi3`: same `error -71` sequence and "unable to enumerate USB device"; Tab5 "waiting for host". In the OTG port (gadget mode) nothing new is logged, as expected.
  **Conclusion: high-speed failure is specific to the Radxa Zero 3W host port with this device.** The Tab5, blocker, cable and adapters are cleared. Full speed works on that port. Unknown whether it is signal quality on the Radxa side or an interaction between its xHCI controller and the P4. Untried: a USB 2.0 hub between Radxa and Tab5; the Orange Pi hosts; the Radxa OTG port in host mode (needs a Radxa config change, owner approval per PLAN.md rule 4).
- Mac as host: an A-to-C cable with its C end in the Mac does not work (the cable's Rp makes the Mac a sink), so the Mac can only test this with an A-to-A cable and a C-to-A dongle.

Radxa Zero 3W (rdx3mobile-1, kernel 6.1.84-10-rk2410-nocsf): no `gud` module in the distro kernel. Built out-of-tree from v6.1.84 `drivers/gpu/drm/gud` sources; `modprobe gud` succeeded 2026-11-09.

SSH auto-connect test 2026-11-15 (`local/sshauto/test_sshauto.sh`, owner's Mac): **pass.** `ssh auto off` → saved; after restart `sshAuto=off ssh=disconnected`. `ssh auto on` → saved; after restart `sshAuto=on ssh=connected`. Local CLI `ssh auto` reports on. Default on at boot also confirmed after reflashing.
Mac note: the serial scripts need pyserial; the owner uses a venv at `~/tab5venv` (`PATH=~/tab5venv/bin:$PATH ./script.sh`).
Seen in the same run, not investigated: after the auto-connect, upstream's storage bridge printed `Tab5 storage setup failed: remote exit -1` and `Python/FUSE preparation failed`, asking for `sudo sh ~/.tab5/bin/tab5-server-setup.sh` on the SSH host. That is upstream's ~/sd ~/usb mount on the server; the auto-connect change does not touch it. Not known whether it also happens without the change.
Later the same night (owner): on RPI4Server-1, `~/sd` and `~/usb` exist in /home/pi and `ls ~/sd` lists the Tab5's SD card, so the mount works. The firmware retries the storage bridge every 10 s while SSH is connected (`loop()`, `startStorageBridgeAsync`), so the failure was most likely transient, possibly from the test's restarts and serial traffic during setup. Not confirmed.
`power` test 2026-11-15 (`local/sshauto/test_power.sh`, USB meter between Mac and Tab5 USB-C): **pass.**

| State | Tab5 `power` | Meter (V, A, W) |
|---|---|---|
| Charging, default | 8290 mV, 732 mA, charging | 4.9, 1.9, 9 |
| `power charge off` | 8163 mV, 0 mA, idle | 5.2, 0.27, 1.38 |
| `power charge on` | 8278 mV, 734 mA, charging | 4.9, 1.9, 9 |

- Charging accounts for about 1.6 A of the 1.9 A; the Tab5 itself, running with Wi-Fi and SSH connected and charging off, draws about 0.27 A (1.4 W).
- `vbus=-1mV` in every reading: M5Unified does not report VBUS on the Tab5. Remove the field or label it unsupported.
- `level` moved 100 → 97 → 96 → 100 within a minute as charging stopped and started, so it is read from the voltage and is unreliable while charging current flows.
- This answers item 6 (charger current), now moot with Phase 1 closed.

Password display 2026-11-15 (owner, on the device): **pass** on both the SSH edit screen and the Wi-Fi edit screen (password plain while its field is selected, stars otherwise). The Wi-Fi edit screen is reached only through WIFI → Scan → a network; upstream has no Edit button for saved Wi-Fi profiles, although the README says "edit".
The whole `local/sshauto/` bundle is now tested.

### Phase 1 closed — failure report (2026-11-15)

Status: **closed, not pursued.** No GUD integration code was written; `main` carries none. Step C was never approved.

Owner requirement: GUD at USB high speed (480 Mbit/s), direct cable plus the power blocker only, working on all three hosts (Radxa Zero 3W, Orange Pi Zero 2W, Orange Pi Zero 3W). No hub (the point was to carry less).

Why it closed:
1. The USB-C port is full speed only (12 Mbit/s), wired to the P4's FSLS PHY (item 1 above). Owner ruled full speed out (2026-11-09).
2. The USB-A port works as a high-speed device: 480 Mb/s with a MacBook as host, through the blocker (2026-11-15).
3. The Radxa Zero 3W host port does not enumerate the Tab5 at high speed (`device descriptor read/64, error -71`, "unable to enumerate USB device"), with the exact chain that works on the Mac. It does enumerate at full speed. Its OTG port is the owner's gadget-mode link and was not tried as host.
4. A hub might work around it but is not acceptable (owner, 2026-11-15). With the Radxa failing, the all-or-none rule closes the phase. The Orange Pis were not tested.

What was learned, for anyone picking this up:
- Arduino on the P4 runs its USB device stack on the high-speed controller (rhport 1), which is wired to USB-A; a plain `USB.begin()` reaches 480 Mbit/s with no PHY changes. The `utmi2`/`utmi3` PHY experiments were not needed.
- With VBUS removed by a blocker, set the DWC2 B-valid override (GOTGCTL `bvalidoven`/`bvalidovval`); the controller then connects.
- Turn the Tab5's own USB-A 5 V off with `M5.Power.setExtOutput(false, ext_USB)` (expander 0x44 pin 3). `getExtOutput()` reads a different pin.
- The blocker is direction-sensitive: its host side must face the host.
- Throughput was measured only at full speed (289 kB/s over CDC). The high-speed rate was not measured.
- Test sketch: `tests/usba_device/`. Host check script: `local/usbatest/host_check.sh` (git-ignored).
- Not tried: the plain high-speed image on the Mac (only `utmi3`, which behaved the same as plain on the Radxa); a full-speed GUD build; the Orange Pis; the Radxa OTG port in host mode.

The Radxa `gud.ko` built on 2026-11-09 is unused.

## Next

Owner: flash ThinClient back (`local/sshauto/flash.sh`), then test that bundle on the device (SSH auto, password display, `power` commands; meter readings with charging on and off). Describe the menu bar touch problem.
Next phase is Phase 2 (Lua). Before it starts, the plan needs revising: the MicroPython runner has no keyboard input to a running program and checks Ctrl-C only in `gfx.present()`, so the "hosting layer" must be designed, not copied.
Define the ambient info display app (owner decision 2026-11-13) so it can get a requirements section and a phase.
