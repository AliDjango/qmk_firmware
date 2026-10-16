# VIA owns Raw HID and delegates telemetry packets to via_command_kb().
VIA_ENABLE = yes
RAW_ENABLE = yes
WPM_ENABLE = yes
LTO_ENABLE = yes
BOOTMAGIC_ENABLE = no
MOUSEKEY_ENABLE = no
EXTRAKEY_ENABLE = yes
CONSOLE_ENABLE = no
COMMAND_ENABLE = no
NKRO_ENABLE = yes
BACKLIGHT_ENABLE = no
AUDIO_ENABLE = no
RGBLIGHT_ENABLE = no
SWAP_HANDS_ENABLE = no
OLED_ENABLE = yes
RGB_MATRIX_ENABLE = yes
NO_USB_STARTUP_CHECK = no
USB_WAIT_FOR_ENUMERATION = no
SRC += usb_recovery.c \
       lib/rgb_state_reader.c \
       lib/layer_state_reader.c \
       lib/logo_reader.c \
       lib/keylogger.c

SRC += brightness_popup.c
