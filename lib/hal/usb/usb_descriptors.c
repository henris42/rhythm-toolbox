// Composite USB descriptors: CDC (stdio) + MIDI + picotool reset interface.
// Used when TOOLBOX_USB_MIDI is on (PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=0).
// Mirrors the SDK's stdio_usb descriptors and adds a MIDI interface.
#include <string.h>
#include "tusb.h"
#include "pico/unique_id.h"
#include "pico/usb_reset_interface.h"

#define USBD_VID 0x2E8A   // Raspberry Pi (keeps the usbipd VID match working)
#define USBD_PID 0x000A

// Reset MUST be interface 2: the SDK's MS-OS-2.0 descriptor (reset_interface.c)
// hardcodes interface 2 for driverless WinUSB access (needed so picotool can
// reset over Windows/usbipd). So: CDC = 0/1, reset = 2, MIDI = 3/4.
enum { ITF_CDC = 0, ITF_RESET = 2, ITF_MIDI = 3, ITF_MAX = 5 };
enum { STRID_LANG = 0, STRID_MANUF, STRID_PRODUCT, STRID_SERIAL, STRID_CDC, STRID_MIDI, STRID_RESET };

#define EPNUM_CDC_CMD  0x81
#define EPNUM_CDC_OUT  0x02
#define EPNUM_CDC_IN   0x82
#define EPNUM_MIDI_OUT 0x03
#define EPNUM_MIDI_IN  0x83

#define TUD_RPI_RESET_DESC_LEN 9
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MIDI_DESC_LEN + TUD_RPI_RESET_DESC_LEN)

#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, \
    RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx,

static const tusb_desc_device_t desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    // 0x0210 so Windows requests the BOS / MS-OS-2.0 descriptor (driverless
    // reset). The BOS + MS-OS descriptors come from the SDK's reset_interface.c.
    .bcdUSB             = 0x0210,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USBD_VID,
    .idProduct          = USBD_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = STRID_MANUF,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t desc_cfg[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_MAX, STRID_LANG, CONFIG_TOTAL_LEN, 0x00, 250),
    TUD_CDC_DESCRIPTOR(ITF_CDC, STRID_CDC, EPNUM_CDC_CMD, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    TUD_RPI_RESET_DESCRIPTOR(ITF_RESET, STRID_RESET)
    TUD_MIDI_DESCRIPTOR(ITF_MIDI, STRID_MIDI, EPNUM_MIDI_OUT, EPNUM_MIDI_IN, 64),
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t s[32];
    static char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    const char *str;

    if (index == STRID_LANG) {
        s[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
        s[1] = 0x0409;
        return s;
    }
    switch (index) {
    case STRID_MANUF:   str = "Drum Machine Toolbox"; break;
    case STRID_PRODUCT: str = "Drum Machine";         break;
    case STRID_SERIAL:  pico_get_unique_board_id_string(serial, sizeof(serial)); str = serial; break;
    case STRID_CDC:     str = "Board CDC";            break;
    case STRID_MIDI:    str = "Drum Machine MIDI";    break;
    case STRID_RESET:   str = "Reset";               break;
    default:            return NULL;
    }
    uint8_t len = (uint8_t)strlen(str);
    if (len > 31) len = 31;
    for (uint8_t i = 0; i < len; i++) s[1 + i] = str[i];
    s[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));
    return s;
}
