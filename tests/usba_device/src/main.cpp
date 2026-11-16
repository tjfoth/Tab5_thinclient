// USB-A device test for the M5Stack Tab5.
//
// Question: can the Tab5's USB-A port act as a high-speed (480 Mbit/s) USB device,
// with no 5 V on the cable (inline power blocker)?
//
// What it does:
//   1. Turns off the Tab5's own 5 V output on USB-A (USB5V_EN) as a safety measure.
//   2. Starts a USB serial (CDC) device on the high-speed controller (USB-A).
//   3. Shows on screen: connected or not, negotiated speed, and the rate at which
//      it receives data from the host (host -> Tab5, the same direction GUD uses).
//
// The receive rate is a lower bound: USB serial has more overhead than GUD's bulk transfers.

#include <M5Unified.h>
#include <USB.h>
#include "tusb.h"
#include "esp_private/usb_phy.h"
#include "soc/usb_dwc_struct.h"  // USB_DWC_HS: the high-speed OTG controller (USB-A)
#include "soc/hp_system_struct.h"  // HP_SYSTEM.sys_usbotg20_ctrl.sys_otg_suspendm
#include "hal/usb_utmi_hal.h"

static USBCDC cdc;
static uint8_t rxBuf[16384];
static uint64_t totalBytes = 0;
static uint32_t windowBytes = 0;
static uint32_t windowStart = 0;
static float lastRateMBs = 0.0f;
static float peakRateMBs = 0.0f;
static bool usb5vOffOk = false;
static bool bsesvldBefore = false;
static bool usbBeginOk = false;   // did Arduino's USB.begin() report success?
static int suspendmBoot = -1;     // UTMI PHY "not suspended" bit as found at boot (reset default 0)
#ifdef UTMI_LL_INIT
// v3: run ESP-IDF's own UTMI PHY hardware init (clock on, reset, suspendm = 1) before USB starts,
// without going through usb_new_phy(), so Arduino's own PHY call is not disturbed.
// v2 (UTMI_PHY_FIX) used usb_new_phy() and then presented nothing on the bus (2026-11-15).
static usb_utmi_hal_context_t utmiHal;
#endif
// (bsesvldBefore: did the controller see VBUS, i.e. B-session valid, before the override?)
#ifdef UTMI_PHY_FIX
// Arduino's init_usb_hal() on the P4 creates the PHY with USB_PHY_TARGET_INT ("internal FSLS PHY")
// although TinyUSB runs the device on the high-speed controller, whose PHY is the UTMI PHY.
// Test: also create the UTMI PHY (device mode, high speed) BEFORE TinyUSB starts.
// Order matters: creating the UTMI PHY resets the high-speed controller (rst_en_usb_otg20 in
// usb_utmi_ll.h), so doing it after USB.begin() wipes TinyUSB's setup and the device never appears.
// That is what the first UTMI build did (2026-11-15: nothing enumerated on the Radxa).
static esp_err_t phyNewErr = ESP_FAIL;
static usb_phy_handle_t utmiPhy = nullptr;
#endif

// USB-A 5 V switch (MT9700 U27) enable: I/O expander 0x44 (index 1), pin 3 = USB5V_EN.
// Note: M5.Power.getExtOutput() on Tab5 reads EXT5V_EN (expander 0 pin 2), not this.
static bool usb5vOn() { return M5.getIOExpander(1).getWriteValue(3); }

static const char* speedName()
{
    switch (tud_speed_get()) {
        case TUSB_SPEED_HIGH: return "HIGH (480 Mbit/s)";
        case TUSB_SPEED_FULL: return "FULL (12 Mbit/s)";
        case TUSB_SPEED_LOW:  return "LOW (1.5 Mbit/s)";
        default:              return "unknown";
    }
}

static void drawStatus()
{
    auto& d = M5.Display;
    d.startWrite();
    d.fillRect(0, 120, d.width(), d.height() - 120, TFT_BLACK);
    d.setCursor(20, 140);

    bool mounted = tud_mounted();
    d.setTextColor(mounted ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
    d.printf("USB-A device: %s\n\n", mounted ? "CONNECTED" : "waiting for host");

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(20, d.getCursorY());
    d.printf("Speed: %s\n\n", mounted ? speedName() : "-");
    d.setCursor(20, d.getCursorY());
    d.printf("Host->Tab5 now:  %.2f MB/s\n", lastRateMBs);
    d.setCursor(20, d.getCursorY());
    d.printf("Host->Tab5 peak: %.2f MB/s\n", peakRateMBs);
    d.setCursor(20, d.getCursorY());
    d.printf("Total received:  %.1f MB\n\n", totalBytes / 1048576.0);

    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(20, d.getCursorY());
    d.printf("USB-A 5V (USB5V_EN): %s\n", usb5vOn() ? "ON (!)" : "off");
    d.setCursor(20, d.getCursorY());
    d.printf("turn-off call: %s\n", usb5vOffOk ? "ok" : "FAILED");
    d.setCursor(20, d.getCursorY());
    d.printf("Grove/ext 5V (EXT5V_EN): %s\n", M5.Power.getExtOutput() ? "on" : "off");
    d.setCursor(20, d.getCursorY());
    d.printf("\nVBUS seen before override: %s\n", bsesvldBefore ? "yes" : "no");
    d.setCursor(20, d.getCursorY());
    d.printf("B-valid ovr: %s  session: %s\n",
             USB_DWC_HS.gotgctl_reg.bvalidoven ? "on" : "off",
             USB_DWC_HS.gotgctl_reg.bsesvld ? "valid" : "NOT valid");
    d.setCursor(20, d.getCursorY());
    d.printf("USB.begin: %s  stack: %s\n", usbBeginOk ? "ok" : "FAILED", tud_inited() ? "up" : "DOWN");
    d.setCursor(20, d.getCursorY());
    d.printf("suspendm boot=%d now=%d\n", suspendmBoot, (int)HP_SYSTEM.sys_usbotg20_ctrl.sys_otg_suspendm);
    d.setCursor(20, d.getCursorY());
    d.printf("softdiscon=%d enumspd=%d\n", (int)USB_DWC_HS.dctl_reg.sftdiscon, (int)USB_DWC_HS.dsts_reg.enumspd);
#ifdef UTMI_PHY_FIX
    d.setCursor(20, d.getCursorY());
    d.printf("UTMI PHY before USB start: %s\n", esp_err_to_name(phyNewErr));
#endif
    d.endWrite();
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    suspendmBoot = HP_SYSTEM.sys_usbotg20_ctrl.sys_otg_suspendm;

    // Safety: never drive 5 V onto the USB-A cable. The power blocker is the main protection.
    usb5vOffOk = M5.Power.setExtOutput(false, m5::ext_port_mask_t::ext_USB);

    M5.Display.setRotation(0);  // portrait, 720 x 1280
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setCursor(20, 30);
#if defined(FORCE_FULL_SPEED)
    M5.Display.println("Tab5 USB-A test: FULL-SPEED build");
#elif defined(UTMI_LL_INIT)
    M5.Display.println("Tab5 USB-A test: HIGH-SPEED + UTMI v3");
#elif defined(UTMI_PHY_FIX)
    M5.Display.println("Tab5 USB-A test: HIGH-SPEED + UTMI v2");
#else
    M5.Display.println("Tab5 USB-A test: HIGH-SPEED build");
#endif
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 80);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.println("Restore ThinClient: run its flash.sh again.");
    M5.Display.setTextSize(3);

#ifdef UTMI_PHY_FIX
    {
        usb_phy_config_t c = {};
        c.controller = USB_PHY_CTRL_OTG;
        c.target = USB_PHY_TARGET_UTMI;
        c.otg_mode = USB_OTG_MODE_DEVICE;
        c.otg_speed = USB_PHY_SPEED_HIGH;
        phyNewErr = usb_new_phy(&c, &utmiPhy);
    }
    delay(10);
#endif

#ifdef UTMI_LL_INIT
    _usb_utmi_hal_init(&utmiHal);
    delay(10);
#endif

    cdc.setRxBufferSize(sizeof(rxBuf) * 2);
    cdc.begin();
    USB.VID(0x303A);  // Espressif
    USB.PID(0x4002);  // Espressif's generic TinyUSB CDC id
    USB.manufacturerName("tab5-test");
    USB.productName("Tab5 USB-A test");
    usbBeginOk = USB.begin();

    // The power blocker removes VBUS, so the controller never sees a valid B-session and
    // will not connect. Tell it to assume VBUS is present (DWC2 GOTGCTL B-valid override).
    delay(50);
    bsesvldBefore = USB_DWC_HS.gotgctl_reg.bsesvld;
    USB_DWC_HS.gotgctl_reg.bvalidovval = 1;
    USB_DWC_HS.gotgctl_reg.bvalidoven = 1;

#ifdef FORCE_FULL_SPEED
    // Diagnostic build: disconnect, switch the controller to full speed (12 Mbit/s), reconnect.
    // If this enumerates and the high-speed build does not, the cable path is the problem.
    USB_DWC_HS.dctl_reg.sftdiscon = 1;
    delay(100);
    USB_DWC_HS.dcfg_reg.devspd = 1;  // 0 = high speed, 1 = full speed on the HS PHY
    delay(10);
    USB_DWC_HS.dctl_reg.sftdiscon = 0;
#endif

    windowStart = millis();
    drawStatus();
}

void loop()
{
    // Drain everything the host sends; count it.
    int avail;
    while ((avail = cdc.available()) > 0) {
        size_t n = cdc.read(rxBuf, avail > (int)sizeof(rxBuf) ? sizeof(rxBuf) : avail);
        totalBytes += n;
        windowBytes += n;
    }

    uint32_t now = millis();
    if (now - windowStart >= 1000) {
        lastRateMBs = windowBytes / 1048576.0f * 1000.0f / (now - windowStart);
        if (lastRateMBs > peakRateMBs) peakRateMBs = lastRateMBs;
        windowBytes = 0;
        windowStart = now;
        drawStatus();
    }
    M5.update();
}
