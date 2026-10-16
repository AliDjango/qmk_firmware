#include "brightness_popup.h"
#include "oled_driver.h"
#include "rgb_matrix.h"
#include <stdio.h>
#include <string.h>

// Logical portrait coordinates after OLED_ROTATION_270: 32 x 128.
#define WIDTH OLED_DISPLAY_HEIGHT
#define HEIGHT OLED_DISPLAY_WIDTH

static void pixel(int x, int y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) oled_write_pixel(x, y, true);
}

static void line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x0 += sx; }
        if (twice <= dx) { error += dx; y0 += sy; }
    }
}

// Five rows of three bits; this compact font keeps three-digit values on one line.
static uint16_t glyph(char c) {
    switch (c) {
        case '0': return 0x7B6F;
        case '1': return 0x2492;
        case '2': return 0x73E7;
        case '3': return 0x73CF;
        case '4': return 0x5BC9;
        case '5': return 0x79CF;
        case '6': return 0x79EF;
        case '7': return 0x7249;
        case '8': return 0x7BEF;
        case '9': return 0x7BCF;
        case 'R': return 0x6BAD;
        case 'G': return 0x796F;
        case 'B': return 0x6BAE;
        case 'O': return 0x7B6F;
        case 'L': return 0x4927;
        case 'E': return 0x79E7;
        case 'D': return 0x6B6E;
        case 'N': return 0x5F7D;
        case 'F': return 0x79E4;
        case '%': return 0x52A5;
        case 'A': return 0x2BED;
        case 'C': return 0x7927;
        case 'H': return 0x5BED;
        case 'I': return 0x7497;
        case 'J': return 0x126F;
        case 'K': return 0x5BAD;
        case 'M': return 0x5FED;
        case 'P': return 0x6BA4;
        case 'Q': return 0x7B79;
        case 'S': return 0x79CF;
        case 'T': return 0x7492;
        case 'U': return 0x5B6F;
        case 'V': return 0x5B6A;
        case 'W': return 0x5BFD;
        case 'X': return 0x5AAD;
        case 'Y': return 0x5A92;
        case 'Z': return 0x72A7;
        default: return 0;
    }
}

static void text(const char *s, int y, int scale) {
    int width = ((int)strlen(s) * 4 - 1) * scale;
    int x = (WIDTH - width) / 2;
    for (; *s; ++s, x += 4 * scale) {
        uint16_t bits = glyph(*s);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (!(bits & (1U << (14 - row * 3 - col)))) continue;
                for (int yy = 0; yy < scale; ++yy)
                    for (int xx = 0; xx < scale; ++xx)
                        pixel(x + col * scale + xx, y + row * scale + yy);
            }
        }
    }
}

static void icon(bool power, bool enabled) {
    int cx = WIDTH / 2 - 1, cy = 43;
    // Two-pixel ring. The power icon leaves a gap for its upright stem.
    for (int y = -9; y <= 9; ++y)
        for (int x = -9; x <= 9; ++x) {
            int radius = x * x + y * y;
            if (radius >= 49 && radius <= 81 && (!power || y >= -3 || x < -3 || x > 3))
                pixel(cx + x, cy + y);
        }
    if (power) {
        line(cx, cy - 12, cx, cy - 1);
        line(cx + 1, cy - 12, cx + 1, cy - 1);
        if (!enabled) line(cx - 10, cy + 10, cx + 10, cy - 10);
    } else {
        line(cx, cy - 14, cx, cy - 12);
        line(cx, cy + 12, cx, cy + 14);
        line(cx - 14, cy, cx - 12, cy);
        line(cx + 12, cy, cx + 14, cy);
        line(cx - 10, cy - 10, cx - 8, cy - 8);
        line(cx + 8, cy - 8, cx + 10, cy - 10);
        line(cx - 10, cy + 10, cx - 8, cy + 8);
        line(cx + 8, cy + 8, cx + 10, cy + 10);
    }
}

static void value_popup_render(const char *label, bool power, uint8_t percent, uint32_t elapsed, uint16_t duration) {
    oled_clear();
    text(label, 10, strlen(label) > 4 ? 1 : 2);
    line(WIDTH / 2 - 4, 24, WIDTH / 2 + 3, 24);
    icon(power, percent != 0);
    if (power) {
        text(percent ? "ON" : "OFF", 70, 2);
    } else {
        char value[4];
        snprintf(value, sizeof(value), "%u", (unsigned)(percent > 100 ? 100 : percent));
        text(value, 68, 2);
        text("%", 84, 1);
        // Ten discrete, centered brightness segments, filling from left to right.
        int start = (WIDTH - 29) / 2;
        int filled = (percent + 5) / 10;
        for (int i = 0; i < 10; ++i) {
            int x = start + i * 3;
            if (i < filled) {
                line(x, 98, x, 104);
                line(x + 1, 98, x + 1, 104);
            } else {
                line(x, 104, x + 1, 104);
            }
        }
    }
    // A quiet countdown line at the foot of the portrait display.
    int track = WIDTH - 12;
    int remaining = elapsed < duration ? (uint32_t)track * (duration - elapsed) / duration : 0;
    if (remaining) line((WIDTH - remaining) / 2, HEIGHT - 10, (WIDTH - remaining) / 2 + remaining - 1, HEIGHT - 10);
}

void brightness_popup_render(bool oled, bool power, uint8_t percent, uint32_t elapsed, uint16_t duration) {
    value_popup_render(oled ? "OLED" : "RGB", power, percent, elapsed, duration);
}

void rgb_setting_popup_render(const char *label, uint8_t percent, uint32_t elapsed, uint16_t duration) {
    value_popup_render(label, false, percent, elapsed, duration);
}

// Generate names from QMK's enabled effect list: IDs remain correct when
// animations are enabled/disabled or their enum order changes upstream.
static const char *effect_name(uint8_t mode) {
    switch (mode) {
        case RGB_MATRIX_NONE: return "NONE";
#define RGB_MATRIX_EFFECT(name, ...) case RGB_MATRIX_##name: return #name;
#include "rgb_matrix_effects.inc"
#undef RGB_MATRIX_EFFECT
        default: return "CUSTOM";
    }
}

void rgb_effect_popup_render(uint8_t mode, uint32_t elapsed, uint16_t duration) {
    oled_clear();
    text("RGB", 10, 2);
    line(WIDTH / 2 - 4, 24, WIDTH / 2 + 3, 24);
    // Opposing arrows echo the effect-cycle controls.
    line(7, 37, 25, 37);
    line(25, 37, 21, 33);
    line(25, 37, 21, 41);
    line(24, 49, 6, 49);
    line(6, 49, 10, 45);
    line(6, 49, 10, 53);

    // Eight compact glyphs fit the portrait width. Split at underscores and
    // wrap longer words without losing any part of the actual effect name.
    const char *name = effect_name(mode);
    int rows = 0;
    for (const char *word = name; *word;) {
        int length = 0;
        while (word[length] && word[length] != '_') ++length;
        rows += (length + 7) / 8;
        word += length;
        if (*word == '_') ++word;
    }
    int y = 65 + (48 - rows * 8) / 2;
    while (*name) {
        char part[9];
        int count = 0;
        while (*name && *name != '_' && count < 8) part[count++] = *name++;
        part[count] = '\0';
        if (count) { text(part, y, 1); y += 8; }
        if (*name == '_') ++name;
    }
    int track = WIDTH - 12;
    int remaining = elapsed < duration ? (uint32_t)track * (duration - elapsed) / duration : 0;
    if (remaining) line((WIDTH - remaining) / 2, HEIGHT - 10, (WIDTH - remaining) / 2 + remaining - 1, HEIGHT - 10);
}

void base_layer_popup_render(bool dvorak, uint32_t elapsed, uint16_t duration) {
    oled_clear();
    text("BASE", 10, 2);
    line(WIDTH / 2 - 4, 24, WIDTH / 2 + 3, 24);
    // A compact keyboard icon centered above the selected layout.
    line(3, 35, 28, 35);
    line(3, 53, 28, 53);
    line(3, 35, 3, 53);
    line(28, 35, 28, 53);
    for (int y = 39; y <= 43; y += 4)
        for (int x = 7; x <= 23; x += 4)
            line(x, y, x + 1, y);
    line(10, 48, 21, 48);
    // Two-letter layout labels fit at four times the compact font size.
    text(dvorak ? "DV" : "QW", 70, 4);
    int track = WIDTH - 12;
    int remaining = elapsed < duration ? (uint32_t)track * (duration - elapsed) / duration : 0;
    if (remaining) line((WIDTH - remaining) / 2, HEIGHT - 10, (WIDTH - remaining) / 2 + remaining - 1, HEIGHT - 10);
}

void boot_status_popup_render(bool awaiting_firmware) {
    oled_clear();
    text("BOOT", 12, 2);
    line(WIDTH / 2 - 4, 27, WIDTH / 2 + 3, 27);

    // Firmware chip icon.
    line(8, 40, 23, 40);
    line(8, 60, 23, 60);
    line(8, 40, 8, 60);
    line(23, 40, 23, 60);
    line(12, 46, 19, 46);
    line(12, 50, 19, 50);
    line(12, 54, 19, 54);
    for (int x = 11; x <= 20; x += 3) {
        line(x, 37, x, 40);
        line(x, 60, x, 63);
    }

    if (awaiting_firmware) {
        text("WAITING", 78, 1);
        text("FIRMWARE", 92, 1);
    } else {
        text("REBOOT", 78, 1);
        text("SYSTEM", 92, 1);
    }
}

void hid_waiting_popup_render(void) {
    oled_clear();
    text("HID", 12, 2);
    line(WIDTH / 2 - 4, 27, WIDTH / 2 + 3, 27);

    // USB plug with an open connection to show the missing host link.
    line(12, 42, 19, 42);
    line(12, 42, 12, 49);
    line(19, 42, 19, 49);
    line(12, 49, 15, 53);
    line(19, 49, 16, 53);
    line(15, 53, 15, 60);
    line(16, 53, 16, 60);
    line(11, 39, 11, 43);
    line(20, 39, 20, 43);
    line(8, 52, 10, 50);
    line(21, 50, 23, 52);

    text("WAITING", 78, 1);
    text("FOR HOST", 92, 1);
}
