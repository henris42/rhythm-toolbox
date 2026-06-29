cmake_minimum_required(VERSION 3.13)

# Locate the Pico SDK (command line -DPICO_SDK_PATH, env var, or this default).
if(NOT PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "/home/hs/pico-sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK")
    message(STATUS "PICO_SDK_PATH not set; using default '${PICO_SDK_PATH}'")
endif()

# Target the RP2350 (Raspberry Pi Pico 2).
set(PICO_BOARD pico2 CACHE STRING "Board type")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico platform")

include(pico_sdk_import.cmake)

project(drum_machine_toolbox C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---------------------------------------------------------------------------
# Toolbox library: HAL (board) + core engine. Apps link this and never open it.
# ---------------------------------------------------------------------------
add_library(toolbox STATIC
    # HAL (board-specific)
    lib/hal/shiftio.cpp
    lib/hal/input.cpp
    lib/hal/leds.cpp
    lib/hal/extio.cpp
    lib/hal/midi.cpp
    lib/hal/io.cpp
    # core engine (board-agnostic)
    lib/core/clock.cpp
    lib/core/sequencer.cpp
    lib/machine.cpp
)
pico_generate_pio_header(toolbox ${CMAKE_CURRENT_LIST_DIR}/lib/hal/shiftio.pio)
target_include_directories(toolbox PUBLIC lib lib/hal lib/core)

# Embedded C++ hygiene: no exceptions/RTTI (smaller, deterministic; we don't use
# them), no thread-safe static guards (single init context), and warnings on.
# PUBLIC so example apps build the same way.
target_compile_options(toolbox PUBLIC
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    -Wall -Wextra
)
target_link_libraries(toolbox PUBLIC
    pico_stdlib
    hardware_pio
    hardware_clocks
    hardware_timer
    hardware_adc
    hardware_uart
)

# Optional class-compliant USB MIDI. Off keeps the simple CDC-only USB; on makes
# the device a composite CDC (serial) + USB MIDI + picotool-reset, so MIDI works
# over the USB cable as well as the DIN UART. Toggle with -DTOOLBOX_USB_MIDI=ON.
option(TOOLBOX_USB_MIDI "Add class-compliant USB MIDI (composite with USB serial)" ON)
if(TOOLBOX_USB_MIDI)
    target_sources(toolbox PRIVATE lib/hal/usb/usb_descriptors.c)
    target_include_directories(toolbox BEFORE PUBLIC lib/hal/usb)   # our tusb_config.h first
    target_link_libraries(toolbox PUBLIC tinyusb_device pico_stdio_usb pico_unique_id)
    target_compile_definitions(toolbox PUBLIC
        TOOLBOX_USB_MIDI=1
        PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=0
        PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=1
        PICO_STDIO_USB_ENABLE_TINYUSB_INIT=1
        PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR=1
    )
    message(STATUS "toolbox: USB MIDI enabled (composite CDC + MIDI + reset)")
endif()

# ---------------------------------------------------------------------------
# Example apps (one file each — copy and modify).
# ---------------------------------------------------------------------------
add_executable(basic_drums examples/basic_drums.cpp)
target_link_libraries(basic_drums toolbox)
pico_enable_stdio_usb(basic_drums 1)
pico_enable_stdio_uart(basic_drums 0)
pico_add_extra_outputs(basic_drums)
