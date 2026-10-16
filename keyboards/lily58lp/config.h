#pragma once

#define OLED_FONT_H "lib/font_8x8.c"
#define OLED_FONT_WIDTH 8
#define OLED_FONT_HEIGHT 8

#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 500U

// The RP2040 PIO driver swaps TX/RX on the USB primary for this wiring.
#define SERIAL_USART_PIN_SWAP
#define SERIAL_USART_FULL_DUPLEX
#define SERIAL_USART_TX_PIN GP0
#define SERIAL_USART_RX_PIN GP1

#define I2C_DRIVER I2CD0 // I2CD1 = I2C1
#define I2C1_SDA_PIN GP12
#define I2C1_SCL_PIN GP13



//#define WS2812_LED_COUNT 58

//#define RGBLIGHT_LED_COUNT 58


//
#define MASTER_RIGHT



/* any side can be master by default, enable split sync to support it */
#define EE_HANDS
