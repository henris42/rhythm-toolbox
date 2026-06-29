#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H
//
// TinyUSB config for the composite CDC (stdio) + MIDI + picotool-reset device.
// Used only when TOOLBOX_USB_MIDI is on; it takes over the USB device from
// pico_stdio_usb's default descriptors (PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=0).
// CFG_TUSB_MCU / CFG_TUSB_OS come from the SDK build.
//
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE

#define CFG_TUD_CDC             1
#define CFG_TUD_CDC_RX_BUFSIZE  64
#define CFG_TUD_CDC_TX_BUFSIZE  64
#define CFG_TUD_CDC_EP_BUFSIZE  64

#define CFG_TUD_MIDI            1
#define CFG_TUD_MIDI_RX_BUFSIZE 128
#define CFG_TUD_MIDI_TX_BUFSIZE 128

// The reset interface uses its own usbd driver, but the MS-OS-2.0 descriptor
// (for driverless reset on Windows) is served via tud_vendor_control_xfer_cb,
// so the vendor class must be enabled.
#define CFG_TUD_VENDOR          1
#define CFG_TUD_VENDOR_RX_BUFSIZE 256
#define CFG_TUD_VENDOR_TX_BUFSIZE 256

#endif
