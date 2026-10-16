#include "keycodes.h"
#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H
// #include "lib/bongocat.c"
#include "lib/logo.c"
#include "transactions.h"
#include "usb_recovery.h"
#include "brightness_popup.h"
#include "raw_hid.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum layer_number {
  _PROGDVORAK = 0,
  _QWERTY,
  _LOWER,
  _RAISE,
  _ADJUST,
  _NUMPAD,
};

#define L_BASE 0
#define L_LOWER (1 << 1)
#define L_RAISE (1 << 2)
#define L_ADJUST (1 << 3)
#define L_NUMPAD (1 << 4)

#define L_ADJUST_TRI (L_ADJUST | L_RAISE | L_LOWER )

// // Home‐Row Mod-Taps on QWERTY
// Left-hand home row mods
#define HOME_A LGUI_T(KC_A)
#define HOME_S LALT_T(KC_S)
#define HOME_D LCTL_T(KC_D)
#define HOME_F LSFT_T(KC_F)

// Right-hand home row mods
#define HOME_J RSFT_T(KC_J)
#define HOME_K RCTL_T(KC_K)
#define HOME_L LALT_T(KC_L)
#define HOME_SCLN RGUI_T(KC_SCLN)

// KC_LCTL, MT_LCTL_A, MT_LALT_S, MT_LGUI_D, MT_LSFT_F,    KC_G,                     KC_H,    MT_RSFT_J,    MT_RGUI_K,    MT_RALT_L,    MT_RCTL_SCLN, KC_QUOT,
static uint8_t tap_count = 0;
static uint16_t last_tap_time = 0;
enum custom_keycodes {
  CKC_OLED_TOGGLE = QK_KB_0,
  CKC_LANG_TG,
  CKC_OLED_BRIGHTNESS_UP,
  CKC_OLED_BRIGHTNESS_DOWN
};
bool oled_enabled = true;
static uint8_t oled_level = OLED_BRIGHTNESS;
static bool oled_settings_pending = true;
static uint32_t last_oled_settings_sync;

#define BRIGHTNESS_NOTICE_MS 2000U
enum brightness_notice_kind { NOTICE_NONE, NOTICE_OLED, NOTICE_RGB, NOTICE_RGB_POWER, NOTICE_RGB_EFFECT, NOTICE_RGB_SPEED, NOTICE_RGB_HUE, NOTICE_RGB_SAT, NOTICE_BASE_LAYER };
static uint8_t brightness_notice_kind;
static uint8_t brightness_notice_percent;
static uint16_t brightness_notice_sequence;
static uint32_t brightness_notice_started;
static bool brightness_notice_was_visible;
static uint8_t rgb_notice_pending;
static uint8_t last_rgb_effect;
static uint8_t last_base_layer;

static void show_brightness_notice(uint8_t kind, uint8_t value, uint8_t maximum) {
    brightness_notice_kind = kind;
    brightness_notice_percent = (kind == NOTICE_RGB_EFFECT || kind == NOTICE_BASE_LAYER) ? value : maximum ? (uint16_t)value * 100U / maximum : 0;
    brightness_notice_sequence++;
    brightness_notice_started = timer_read32();
    oled_settings_pending = true;
}


typedef struct {
    uint8_t brightness;
    uint8_t enabled;
    uint8_t notice_kind;
    uint8_t notice_percent;
    uint16_t notice_sequence;
    uint16_t notice_remaining_ms;
} oled_settings_t;

static void oled_settings_receive(uint8_t in_len, const void *in_data,
                                  uint8_t out_len, void *out_data) {
    if (in_len != sizeof(oled_settings_t)) return;
    const oled_settings_t *settings = in_data;
    oled_level = settings->brightness;
    oled_enabled = settings->enabled != 0;
    if (settings->notice_sequence != brightness_notice_sequence) {
        brightness_notice_sequence = settings->notice_sequence;
        brightness_notice_kind = settings->notice_kind;
        brightness_notice_percent = settings->notice_percent;
        uint16_t remaining = settings->notice_remaining_ms;
        if (remaining > BRIGHTNESS_NOTICE_MS) remaining = BRIGHTNESS_NOTICE_MS;
        // Duplicate refreshes must not restart the notification timer.
        brightness_notice_started = timer_read32() - (BRIGHTNESS_NOTICE_MS - remaining);
    }
}


// FOR HID
typedef struct {
    uint8_t cpu_temp;
    uint8_t cpu_percent;
    uint8_t gpu_temp;
    uint8_t gpu_percent;
    uint16_t ram_usage;
    uint16_t ram_percent;
    uint16_t vram_usage;
    uint16_t vram_percent;
    uint16_t room_temp_1;
    uint16_t room_temp_2;
    bool    is_hid_driver_connected;
} system_status_t;
// Global variable, initialized to all zeros
system_status_t g_system_status = {0};
static uint32_t last_hid_sync = 0;
static uint32_t last_status_sync = 0;
static bool status_sync_pending = false;

//FOR SLAVE-MASTER COMMS
typedef struct {
    uint8_t ack; // optional: confirmation or other info
} slave_to_master_t;
system_status_t slave_system_status = {0};
#define SLAVE_OLED_PAGE_COUNT 3
#define SLAVE_OLED_PAGE_INTERVAL 5000 // 2 seconds per page
static uint8_t slave_oled_current_page = 0;
static uint32_t slave_oled_last_page_update = 0;

#define MAX_PACKET_SIZE 32
#define CHUNK_SIZE 30  // 32 - 2 bytes (type + index)
#define PAYLOAD_TIMEOUT 2000  // ms
#define MAX_LARGE_PAYLOAD_SIZE (CHUNK_SIZE * 256U) // Packet indices are one byte.

static uint8_t *dynamic_payload_buffer = NULL;
static uint16_t total_payload_size = 0;
static uint16_t total_packets = 0;
static uint16_t expected_packet_index = 0;
static uint32_t last_packet_time = 0;

static void reset_payload_transfer(void) {
    free(dynamic_payload_buffer);
    dynamic_payload_buffer = NULL;
    total_payload_size = 0;
    total_packets = 0;
    expected_packet_index = 0;
}

//extern uint8_t __StackLimit;
//extern uint8_t __bss_end__;
//extern uint8_t __StackTop;

//static size_t get_free_ram(void) {
//    // Heap ends at bottom of stack
//    uint8_t *heap_end = &__bss_end__;
//    uint8_t *stack_ptr = (uint8_t *)__get_MSP();
//
//    return (stack_ptr - heap_end);
//}


const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/* DVORAK
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | ESC  |   1  |   2  |   3  |   4  |   5  |                    |   6  |   7  |   8  |   9  |   0  |  `   |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | Tab  |   ;  |   ,  |   .  |   P  |   Y  |                    |   F  |   G  |   C  |   R  |   L  |  /  |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |LCTRL |   A  |   O  |   E  |   U  |   I  |-------.    ,-------|   D  |   H  |   T  |   N  |   S  |  -   |
 * |------+------+------+------+------+------|   [   |    |    ]  |------+------+------+------+------+------|
 * |LShift|   '  |   Q  |   J  |   K  |   X  |-------|    |-------|   B  |   M  |   W  |   V  |   Z  |RShift|
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LGUI | LOWER| LALT | /Space  /       \Enter \  |BackSP| RAISE| RGUI |
 *                   |      |      |      |/       /         \      \ |      |      | DEL  |
 *                   `----------------------------'           '------''--------------------'
 */
  [_PROGDVORAK] = LAYOUT(
    KC_ESC, KC_1, KC_2, KC_3, KC_4,KC_5,                           KC_6, KC_7, KC_8, KC_9, KC_0, KC_GRV,
    KC_TAB, KC_SCLN, KC_COMM, KC_DOT, KC_P, KC_Y,                   KC_F, KC_G, KC_C, KC_R, KC_L, KC_SLASH,
    KC_LCTL,KC_A, KC_O, KC_E, KC_U, KC_I,                           KC_D,KC_H, KC_T, KC_N, KC_S, RCTL_T(KC_MINUS),
    KC_LSFT, KC_QUOT, KC_Q, KC_J, KC_K, KC_X, KC_LBRC,     KC_RBRC, KC_B, KC_M, KC_W, KC_V, KC_Z, KC_RSFT,
                       KC_LGUI,KC_LALT,MO(_LOWER), KC_SPC, KC_ENT,     MO(_RAISE),KC_BSPC, KC_RGUI
  ),
  /* QWERTY
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | ESC  |   1  |   2  |   3  |   4  |   5  |                    |   6  |   7  |   8  |   9  |   0  |  `   |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | Tab  |   Q  |   W  |   E  |   R  |   T  |                    |   Y  |   U  |   I  |   O  |   P  |  -   |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |LCTRL |   A  |   S  |   D  |   F  |   G  |-------.    ,-------|   H  |   J  |   K  |   L  |   ;  |  '   |
 * |------+------+------+------+------+------|   [   |    |    ]  |------+------+------+------+------+------|
 * |LShift|   Z  |   X  |   C  |   V  |   B  |-------|    |-------|   N  |   M  |   ,  |   .  |   /  |RShift|
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LGUI | LOWER| LALT | /Space  /       \Enter \  |BackSP| RAISE| RGUI |
 *                   |      |      |      |/       /         \      \ |      |      | DEL  |
 *                   `----------------------------'           '------''--------------------'
 */

 [_QWERTY] = LAYOUT(
  KC_ESC,   KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                     KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_GRV,
  KC_TAB,   KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                     KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_MINS,
  KC_LCTL,  KC_A,   KC_S,    KC_D,    KC_F,    KC_G,                     KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, RCTL_T(KC_QUOT),
  KC_LSFT,  KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_LBRC,  KC_RBRC,  KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,  KC_RSFT,
                        KC_LGUI,KC_LALT,MO(_LOWER), KC_SPC, KC_ENT,     MO(_RAISE),KC_BSPC, KC_RGUI
 ),
/* LOWER
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |  F1  |  F2  |  F3  |  F4  |  F5  |  F6  |                    |  F7  |  F8  |  F9  | F10  | F11  | F12  |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |   `  |   !  |   @  |   #  |   $  |   %  |                    |   ^  |   &  |   *  |   ~  |  `   |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------.    ,-------|   |   |  )    |   ]   |  =    |      |      |
 * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
 * |      | MPRV | NEXT | NEXT |      |      |-------|    |-------|   \   |   _  |   +  |   {  |   }  |   |  |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LAlt | LOWER|LGUI  | /Space  /       \Enter \  |BackSP| RAISE| RGUI |
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'       ----''--------------------'
 */
[_LOWER] = LAYOUT(
  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,                     KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,
  _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC,                    KC_CIRC, KC_AMPR, KC_ASTR, KC_GRV,  KC_TILD, _______,
  _______, LCTL(KC_X), KC_LCBR, KC_LBRC, KC_LPRN, KC_EQUAL,                   KC_PIPE, KC_RPRN, KC_RBRC, KC_RCBR, _______, _______,
  _______, LCTL(KC_C), LCTL(KC_V), KC_UNDS, KC_PLUS, KC_SLASH, KC_DELETE, KC_DELETE, KC_BSLS, _______, _______, _______, _______, _______,
                             _______, _______, _______,  _______, _______,  _______, _______, _______
),
  // _______, KC_EXLM, KC_AT,   KC_HASH, KC_DLR,  KC_PERC,                   KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_TILD,
  // _______, KC_MPRV, KC_MNXT, KC_MPLY, KC_VOLD, KC_VOLU, _______, _______, _______, KC_UNDS, KC_PLUS, KC_LCBR, KC_RCBR, KC_PIPE,



/* RAISE
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |      |      |      |      |      |      |                    | PRSC |SCRLCK|PAUSE | INSRT| HOME | PGUP |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |   1  |   2  |   3  |   4  |   5  |                    |MUSIC-|MSCPLY|MUSIC+|DELETE|  END | PGDN |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |   6  |   7  |   8  |   9  |   0  |-------.    ,-------| CAPS | Left | Down |  Up  |Right |      |
 * |------+------+------+------+------+------|   [   |    | DELETE|------+------+------+------+------+------|
 * |      |  +   |  -   |   =  |   [  |   ]  |-------|    |-------|WWWREF| VOL+ | VOL- | MUTE |      |      |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LAlt | LOWER|LGUI  | /Space  /       \Enter \  |BackSP| RAISE| RGUI |
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */

[_RAISE] = LAYOUT(
  _______, _______, _______, _______, _______, _______,                        KC_PSCR, KC_SCRL, KC_PAUSE, KC_INS, KC_HOME, KC_PGUP,
  _______, KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                        KC_MPRV, KC_MPLY, KC_MNXT, KC_DEL, KC_END, KC_PGDN,
  _______, KC_6,    KC_7,    KC_8,    KC_9,    KC_0,                        KC_CAPS, KC_LEFT, KC_DOWN,   KC_UP, KC_RGHT, _______,
  _______, KC_PLUS, KC_MINS, KC_EQL,  KC_LBRC, KC_RBRC,_______, KC_DELETE, KC_WREF, KC_VOLU, KC_VOLD,  KC_MUTE,   _______,  _______,
                             _______, _______, _______,  _______, _______,  _______, _______, _______
),
/* ADJUST -- hold LOWER + RAISE
 *
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | BOOT |  --  |  --  |  --  |REBOOT| SLEEP|                    |  --  |REBOOT|  --  |  --  |  --  | BOOT |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |  TR  |MYCOMP|MONBR+|QWERTY|  --  |OLED+ |                    |EFFECT|RGBBR+| HUE+ | SAT+ |SPEED+|  TR  |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |  TR  | CALC |MONBR-|DVORAK|BASETG|OLED- |-------.    ,-------|EFFT- |RGBBR-| HUE- | SAT- |SPEED-|  TR  |
 * |------+------+------+------+------+------|OLEDTG|    |RGBTOG|------+------+------+------+------+------|
 * |  TR  |  --  |  --  |  --  |  --  |  --  |-------|    |-------|  --  |  --  |  --  |  --  |NKROTG|  TR  |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   |  TR  |  TR  |  TR  | /  TR   /       \  TR  \ |  TR  |  TR  |  TR  |
 *                   `----------------------------'           '----------------------------'
 *
 * TR = transparent (inherits a lower layer); -- = disabled.
 * MONBR+/- adjusts the host monitor; OLED+/- adjusts both OLED displays.
 * RGBBR+/- adjusts keyboard RGB brightness. EFFECT/EFFT- cycles effects.
 * QWERTY/DVORAK selects the base; BASETG toggles between the two bases.
 * OLEDTG/RGBTOG toggles both OLEDs / keyboard RGB; NKROTG toggles NKRO.
 * OLED brightness and RGB brightness/power, effect, speed, hue, and saturation changes show a centered
 * two-second popup on both vertical OLEDs; popups add no key assignments.
 */
  [_ADJUST] = LAYOUT(
  QK_BOOT, XXXXXXX, XXXXXXX, XXXXXXX, QK_RBT, KC_SLEP,                   XXXXXXX, QK_RBT, XXXXXXX, XXXXXXX, XXXXXXX, QK_BOOT,
  _______, KC_MYCM, KC_BRIU, DF(_QWERTY), XXXXXXX, CKC_OLED_BRIGHTNESS_UP,                    RM_NEXT, RM_VALU, RM_HUEU, RM_SATU, RM_SPDU,_______,
  _______, KC_CALC, KC_BRID,  DF(_PROGDVORAK), CKC_LANG_TG, CKC_OLED_BRIGHTNESS_DOWN,                    RM_PREV, RM_VALD, RM_HUED, RM_SATD, RM_SPDD,_______,
  _______, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, CKC_OLED_TOGGLE, RM_TOGG, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, NK_TOGG, _______,
                             _______, _______, _______, _______, _______,  _______, _______, _______
  ),
  /* NUMPAD
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |   F1 | KP / | KP * | KP - | KP + | KP = |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      | KPNUM| KP 7 | KP 8 | KP 9 | BSPC |                    |      |      |      |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      | KP 0 | KP 4 | KP 5 | KP 6 | KPEQ |-------.    ,-------|      | LEFT | DOWN |  UP  | RIGHT|      |
 * |------+------+------+------+------+------|   F5  |    |       |------+------+------+------+------+------|
 * |      | KP . | KP 1 | KP 1 | KP 3 |KPENT |-------|    |-------|      |      |      |      |      |      |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LAlt | LOWER|LGUI  | /Space  /       \Enter \  |BackSP| RAISE| RGUI |
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_NUMPAD] = LAYOUT(
  KC_F1,  KC_KP_SLASH,    KC_KP_ASTERISK,   KC_KP_MINUS,   KC_KP_PLUS,   KC_KP_EQUAL,                   XXXXXXX,   XXXXXXX,   XXXXXXX,   XXXXXXX,  XXXXXXX,  XXXXXXX,
  _______, KC_NUM, KC_KP_7, KC_KP_8, KC_KP_9, KC_BSPC,                   XXXXXXX, XXXXXXX, XXXXXXX, _______, _______, _______,
  _______, KC_KP_0, KC_KP_4, KC_KP_5, KC_KP_6, KC_KP_EQUAL,                   XXXXXXX, KC_LEFT, KC_UP, KC_DOWN,   KC_RGHT, XXXXXXX,
  _______, KC_KP_DOT, KC_KP_1, KC_KP_2, KC_KP_3, KC_KP_ENTER, KC_F5, XXXXXXX,XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX,
                             _______, _______, _______, _______, _______,  _______, _______, _______
  )
};

layer_state_t layer_state_set_user(layer_state_t state) {
  return update_tri_layer_state(state, _LOWER, _RAISE, _ADJUST);
}

void housekeeping_task_user(void) {
    if (is_keyboard_master()) {
        uint8_t base = get_highest_layer(default_layer_state);
        if (base != last_base_layer) {
            last_base_layer = base;
            if (base == _QWERTY || base == _PROGDVORAK) {
                show_brightness_notice(NOTICE_BASE_LAYER, base, 0);
            }
        }
    }
    // Also catch mode changes made through firmware APIs, not just cycle keys.
    if (is_keyboard_master() && rgb_matrix_get_mode() != last_rgb_effect) {
        last_rgb_effect = rgb_matrix_get_mode();
        rgb_notice_pending = NOTICE_RGB_EFFECT;
    }
    if (rgb_notice_pending != NOTICE_NONE) {
        uint8_t kind = rgb_notice_pending;
        rgb_notice_pending = NOTICE_NONE;
        if (kind == NOTICE_RGB_EFFECT) {
            show_brightness_notice(kind, rgb_matrix_get_mode(), 0);
        } else if (kind == NOTICE_RGB_POWER) {
            show_brightness_notice(kind, rgb_matrix_is_enabled() ? 1 : 0, 1);
        } else if (kind == NOTICE_RGB_SPEED) {
            show_brightness_notice(kind, rgb_matrix_get_speed(), 255);
        } else if (kind == NOTICE_RGB_HUE) {
            show_brightness_notice(kind, rgb_matrix_get_hue(), 255);
        } else if (kind == NOTICE_RGB_SAT) {
            show_brightness_notice(kind, rgb_matrix_get_sat(), 255);
        } else {
            show_brightness_notice(kind, rgb_matrix_get_val(), RGB_MATRIX_MAXIMUM_BRIGHTNESS);
        }
    }
    lily58lp_usb_recovery_task();
    if (dynamic_payload_buffer && timer_elapsed32(last_packet_time) > PAYLOAD_TIMEOUT) {
        reset_payload_transfer();
    }
    if (is_keyboard_master()) {
        // Retry changes promptly; periodically refresh after a secondary restart.
        uint32_t oled_sync_elapsed = timer_elapsed32(last_oled_settings_sync);
        if ((oled_settings_pending && oled_sync_elapsed >= 100) || oled_sync_elapsed >= 1000) {
            last_oled_settings_sync = timer_read32();
            uint32_t notice_elapsed = timer_elapsed32(brightness_notice_started);
            oled_settings_t settings = {
                .brightness = oled_level,
                .enabled = oled_enabled,
                .notice_kind = brightness_notice_kind,
                .notice_percent = brightness_notice_percent,
                .notice_sequence = brightness_notice_sequence,
                .notice_remaining_ms = notice_elapsed < BRIGHTNESS_NOTICE_MS ? BRIGHTNESS_NOTICE_MS - notice_elapsed : 0,
            };
            oled_settings_pending = !transaction_rpc_send(USER_SYNC_OLED_SETTINGS, sizeof(settings), &settings);
        }
        if (g_system_status.is_hid_driver_connected && timer_elapsed32(last_hid_sync) > 60000) {
            g_system_status.is_hid_driver_connected = false;
            status_sync_pending = true;
        }
        // Coalesce host updates and limit split RPC attempts, including retries.
        if (status_sync_pending && timer_elapsed32(last_status_sync) >= 100) {
            last_status_sync = timer_read32();
            if (transaction_rpc_send(USER_SYNC_SYSTEM_STATUS, sizeof(g_system_status), &g_system_status)) {
                status_sync_pending = false;
            }
        }
    }
}

//SSD1306 OLED update loop, make sure to enable OLED_ENABLE=yes in rules.mk
#ifdef OLED_ENABLE
static uint32_t splash_start_time;
static bool     splash_done = false;


oled_rotation_t oled_init_user(oled_rotation_t rotation) {
  splash_start_time = timer_read32();
  splash_done       = false;
  if (is_keyboard_master()){
    return OLED_ROTATION_270;  // flips the display 180 degrees if offhand
    } else {
    return OLED_ROTATION_270;
    }
  return rotation;
}


void user_sync_system_status_slave_handler(
    uint8_t in_buflen, const void* in_data,
    uint8_t out_buflen, void* out_data
) {
    if (in_buflen == sizeof(system_status_t)) {
        memcpy(&slave_system_status, in_data, sizeof(system_status_t));
    }
}


void keyboard_post_init_user(void) {
    last_rgb_effect = rgb_matrix_get_mode();
    last_base_layer = get_highest_layer(default_layer_state);
    transaction_register_rpc(USER_SYNC_SYSTEM_STATUS, user_sync_system_status_slave_handler);
    transaction_register_rpc(USER_SYNC_OLED_SETTINGS, oled_settings_receive);
}

// When you add source files to SRC in rules.mk, you can use functions.
const char *read_layer_state(void);
const char *read_logo(void);
void set_keylog(uint16_t keycode, keyrecord_t *record);
const char *read_keylog(void);
const char *read_keylogs(void);

// const char *read_mode_icon(bool swap);
// const char *read_host_led_state(void);
// void set_timelog(void);
// const char *read_timelog(void);

bool oled_task_user(void) {
    if (brightness_notice_kind != NOTICE_NONE && timer_elapsed32(brightness_notice_started) < BRIGHTNESS_NOTICE_MS) {
        oled_set_brightness(oled_level);
        oled_on();
        if (brightness_notice_kind == NOTICE_BASE_LAYER) {
            base_layer_popup_render(brightness_notice_percent == _PROGDVORAK,
                                    timer_elapsed32(brightness_notice_started), BRIGHTNESS_NOTICE_MS);
        } else if (brightness_notice_kind == NOTICE_RGB_EFFECT) {
            rgb_effect_popup_render(brightness_notice_percent,
                                    timer_elapsed32(brightness_notice_started), BRIGHTNESS_NOTICE_MS);
        } else if (brightness_notice_kind == NOTICE_RGB_SPEED || brightness_notice_kind == NOTICE_RGB_HUE || brightness_notice_kind == NOTICE_RGB_SAT) {
            rgb_setting_popup_render(brightness_notice_kind == NOTICE_RGB_SPEED ? "SPEED" :
                                     brightness_notice_kind == NOTICE_RGB_HUE ? "HUE" : "SAT",
                                     brightness_notice_percent, timer_elapsed32(brightness_notice_started), BRIGHTNESS_NOTICE_MS);
        } else {
        brightness_popup_render(brightness_notice_kind == NOTICE_OLED,
                                brightness_notice_kind == NOTICE_RGB_POWER,
                                brightness_notice_percent,
                                timer_elapsed32(brightness_notice_started),
                                BRIGHTNESS_NOTICE_MS);
        }
        brightness_notice_was_visible = true;
        return false;
    }
    if (brightness_notice_was_visible) {
        oled_clear();
        brightness_notice_was_visible = false;
    }
    if (!oled_enabled) {               // if oled is not enabled
        if (is_oled_on()) oled_off();  // turn oled off
        return false;                  // do nothing else
    }
  oled_set_brightness(oled_level);
  if (!splash_done) {
    //oled_set_brightness(128);
    // Calculate elapsed time since init
    uint32_t elapsed = timer_elapsed32(splash_start_time);
    if (elapsed < 2500) {
        // Still within first 5 seconds: draw splash
//        if (is_keyboard_master()) oled_write_raw_P(ALISH_LOGO_270, sizeof(ALISH_LOGO_270));
//        else oled_write_raw_P(ALISH_LOGO_0, sizeof(ALISH_LOGO_0));
        oled_write_raw_P(ALISH_LOGO_270, sizeof(ALISH_LOGO_270));
    } else {
        // 5 seconds have passed: bail out of splash
        splash_done = true;
        oled_clear();
        oled_render_dirty(true);
    }
} else {
  if (last_input_activity_elapsed() > OLED_TIMEOUT) {
      oled_off();
  } else {
      oled_on();
  }
  // display_housekeeping_task();
    if (is_keyboard_master()) {
        oled_clear();

        // Example: display on OLED
        oled_set_cursor(0,0);
        oled_write_number_16x16(g_system_status.vram_usage,0,0);
        oled_set_cursor(0,4);


        //uint32_t free = get_free_ram(); 247176
        //char buf[32];
        //snprintf(buf, sizeof(buf), "fr: %lu", free);
        //oled_write(buf, false);

        // oled_write_ln(get_u8_str(g_system_status.cpu_percent, ' '), false);
        // oled_write_ln(get_u8_str(g_system_status.cpu_temp, ' '), false);
        // oled_write_ln(get_u8_str(g_system_status.ram_percent / 10, ' '), false);
        // oled_write_ln(get_u8_str(g_system_status.ram_usage, ' '), false);
        // oled_write_ln(g_system_status.is_hid_driver_connected ? PSTR("Y") : PSTR("N"), false);


        // oled_set_cursor(0,1);
        // uint8_t n = get_current_wpm();
        // char    wpm_counter[4];
        // wpm_counter[3] = '\0';
        // wpm_counter[2] = '0' + n % 10;
        // wpm_counter[1] = (n /= 10) % 10 ? '0' + (n) % 10 : (n / 10) % 10 ? '0' : ' ';
        // wpm_counter[0] = n / 10 ? '0' + n / 10 : ' ';
        // oled_write_P(PSTR("WPM:"), false);
        // oled_write(wpm_counter, false);
        // oled_set_cursor(0,3);
        // char buffer[16];
        // snprintf(buffer, sizeof(buffer), "oled: %d", OLED_TIMEOUT);
        // oled_write_ln(buffer, false);
        led_t led_state = host_keyboard_led_state();
        if(led_state.caps_lock) {
          // oled_set_cursor(0,10);
          // oled_write_raw_P(CAPS_LOGO, sizeof(CAPS_LOGO));
          oled_write_image_16x16(CAPS_LOGO,0,8);
		  // oled_set_cursor(0,10);
          // oled_write_ln("CAPS",false);
        }
        if(led_state.num_lock) {
          // oled_set_cursor(2,10);
          // oled_write_raw_P(NUM_LOGO, sizeof(NUM_LOGO));
		  oled_write_image_16x16(NUM_LOGO,2,8);
          // oled_set_cursor(0,11);
          // oled_write_ln("NumLk",false);
        }
		if(	g_system_status.is_hid_driver_connected){
			oled_write_image_16x16(SETTINGS_LOGO,0,5);
		}
		if(!keymap_config.nkro){
			oled_write_image_16x16(GHOST_LOGO,2,5);
		}
        // if(led_state.scroll_lock) {
          // oled_set_cursor(3,6);
          // oled_write_raw_P(NUM_LOGO, sizeof(NUM_LOGO));
        //   oled_set_cursor(0,9);
        //   oled_write_ln("ScrLk",false);
        // }
        // char lock_status[4]; // C, N, S, null terminator

        // lock_status[0] = led_state.caps_lock   ? 'C' : ' ';
        // lock_status[1] = led_state.num_lock    ? 'N' : ' ';
        // lock_status[2] = led_state.scroll_lock ? 'S' : ' ';
        // lock_status[3] = '\0'; // End string

        // oled_write_ln(lock_status, false);

        oled_set_cursor(0,12);
        switch (get_highest_layer(layer_state)) {
          case _RAISE:
            oled_write_raw_P(bmp_big_R, sizeof(bmp_big_R));
            break;
          case _LOWER:
            oled_write_raw_P(bmp_big_L, sizeof(bmp_big_L));
            break;
          case _ADJUST:
            oled_write_raw_P(bmp_big_A, sizeof(bmp_big_A));
            break;
          case _NUMPAD:
            oled_write_raw_P(bmp_big_N, sizeof(bmp_big_N));
            break;
          default:
          switch (get_highest_layer(default_layer_state)) {
            case _QWERTY:
                oled_write_raw_P(QW_LOGO, sizeof(QW_LOGO));
                break;
            case _PROGDVORAK:
                oled_write_raw_P(DV_LOGO, sizeof(DV_LOGO));
                break;
            default:
                oled_write_raw_P(bmp_big_U, sizeof(bmp_big_U));
                break;
            }
          }
        // oled_set_cursor(0,3);
        // oled_write_ln(read_layer_state(), false);
        // char buffer[16];
        // snprintf(buffer, sizeof(buffer), "Lines: %d", oled_max_lines());
        // oled_set_cursor(0,5);
        // oled_write_ln(buffer, false);
        // char buffer2[16];
        // snprintf(buffer, sizeof(buffer2), "chars: %d", oled_max_chars());
        // oled_set_cursor(0,7);
        // oled_write_ln(buffer, false);


    } else {
        // Only show pages if HID driver is connected
        if (!slave_system_status.is_hid_driver_connected) {
            oled_clear();
            oled_write_ln("Waiting for HID...", false);
            oled_write_P(slave_system_status.is_hid_driver_connected ? PSTR("Y") : PSTR("N"), false);
            oled_write_P(PSTR("\n"), false);
            return false;
        }

        // Slide show page switching
        if (timer_elapsed32(slave_oled_last_page_update) > SLAVE_OLED_PAGE_INTERVAL) {
            slave_oled_current_page = (slave_oled_current_page + 1) % SLAVE_OLED_PAGE_COUNT;
            slave_oled_last_page_update = timer_read32();
            oled_clear(); // clear previous page
        }

        //char buf[32];
        oled_clear();
        switch (slave_oled_current_page) {
            case 0: // CPU temp
                oled_set_cursor(0,0);
                oled_write_raw_P(cpu_icon, sizeof(cpu_icon));
                oled_write_number_16x16(slave_system_status.cpu_percent,0,5);
                oled_set_cursor(0,7);
                oled_write_raw_P(percent_icon, sizeof(percent_icon));
                oled_write_number_16x16(slave_system_status.cpu_temp,0,10);
                oled_set_cursor(0,12);
                oled_write_raw_P(degrees_celcius_icon, sizeof(degrees_celcius_icon));
                // snprintf(buf, sizeof(buf), "CPU: %dC", slave_system_status.cpu_temp);
                // oled_write_ln(buf, false);
                break;
            case 9: // GPU temp
                oled_set_cursor(0,0);
                oled_write_raw_P(gpu_icon, sizeof(gpu_icon));
                oled_write_number_16x16(slave_system_status.gpu_percent,0,5);
                oled_set_cursor(0,7);
                oled_write_raw_P(percent_icon, sizeof(percent_icon));
                oled_write_number_16x16(slave_system_status.gpu_temp,0,10);
                oled_set_cursor(0,12);
                oled_write_raw_P(degrees_celcius_icon, sizeof(degrees_celcius_icon));
                break;
            case 1: // RAM usage
                oled_set_cursor(0,0);
                oled_write_raw_P(ram_icon, sizeof(ram_icon));
                oled_write_number_16x16(slave_system_status.ram_percent,0,5);
                oled_set_cursor(0,7);
                oled_write_raw_P(percent_icon, sizeof(percent_icon));
                oled_write_number_16x16(slave_system_status.ram_usage,0,10);
                oled_set_cursor(0,12);
                oled_write_raw_P(gb_icon, sizeof(gb_icon));
                break;
            case 2: // Room temp
                oled_set_cursor(0,0);
                oled_write_raw_P(temp_icon, sizeof(temp_icon));
                oled_write_number_16x16(slave_system_status.room_temp_1 / 10,0,5);
                oled_set_cursor(0,7);
                oled_write_raw_P(degrees_celcius_icon, sizeof(percent_icon));
                oled_write_number_16x16(slave_system_status.room_temp_2 / 10,0,10);
                oled_set_cursor(0,12);
                oled_write_raw_P(degrees_celcius_icon, sizeof(degrees_celcius_icon));
                break;
        }
      // render_bongocat();
        // Example: display on OLED
        /*oled_set_cursor(0,0);
        char buf[64];
        snprintf(buf, sizeof(buf),
        "CPU:%dC\nGPU:%dC\nRAM:%d%%\nVRAM:%d%%\nROOM:%dC",
            slave_system_status.cpu_temp,
            slave_system_status.gpu_temp,
            slave_system_status.ram / 10,
            slave_system_status.ram / 10,
            z
        );
        oled_write_ln(buf, false);
*/
    }
  }
    return false;
}
// bool process_record_user(uint16_t keycode, keyrecord_t *record) {
//   if (record->event.pressed) {
//       extern uint32_t tap_timer;
//       tap_timer = timer_read32();
//   }
//   return true;
// }
#endif // OLED_DRIVER_ENABLE

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Let QMK handle RGB first, then read its final value from housekeeping.
    if (keycode == RM_VALU || keycode == RM_VALD || keycode == RM_TOGG || keycode == RM_ON || keycode == RM_OFF || keycode == RM_NEXT || keycode == RM_PREV ||
        keycode == RM_SPDU || keycode == RM_SPDD || keycode == RM_HUEU || keycode == RM_HUED || keycode == RM_SATU || keycode == RM_SATD) {
        uint8_t kind = (keycode == RM_NEXT || keycode == RM_PREV) ? NOTICE_RGB_EFFECT :
                       (keycode == RM_VALU || keycode == RM_VALD) ? NOTICE_RGB :
                       (keycode == RM_SPDU || keycode == RM_SPDD) ? NOTICE_RGB_SPEED :
                       (keycode == RM_HUEU || keycode == RM_HUED) ? NOTICE_RGB_HUE :
                       (keycode == RM_SATU || keycode == RM_SATD) ? NOTICE_RGB_SAT : NOTICE_RGB_POWER;
#ifdef RGB_TRIGGER_ON_KEYDOWN
        if (record->event.pressed) rgb_notice_pending = kind;
#else
        if (!record->event.pressed) rgb_notice_pending = kind;
#endif
    }
  switch (keycode) {
      case MO(_LOWER):
          if (record->event.pressed) {
              uint16_t now = timer_read();

              // If last tap was over 1.5s ago, reset count
              if (timer_elapsed(last_tap_time) > 250) {
                  tap_count = 0;
              }

              tap_count++;
              last_tap_time = now;

              if (tap_count >= 5) {
                  // Toggle the layer
                  if (layer_state_is(_NUMPAD)) {
                      layer_off(_NUMPAD);
                  } else {
                      layer_on(_NUMPAD);
                  }

                  tap_count = 0; // Reset counter after switching
                  return false;
              }
          }
          break;
        case CKC_OLED_TOGGLE:
            if (record->event.pressed) {
                    oled_enabled = !oled_enabled;
                    oled_settings_pending = true;
                }
            break;
        case CKC_OLED_BRIGHTNESS_UP:
        case CKC_OLED_BRIGHTNESS_DOWN:
            if (record->event.pressed) {
                int16_t next = oled_level + (keycode == CKC_OLED_BRIGHTNESS_UP ? 16 : -16);
                // Keep the display visible; the existing toggle turns it off.
                oled_level = next < 16 ? 16 : next > 255 ? 255 : (uint8_t)next;
                show_brightness_notice(NOTICE_OLED, oled_level, 255);
            }
            return false;
        case CKC_LANG_TG:
            if (record->event.pressed) {
                // single‑key toggle between bases
                if (get_highest_layer(default_layer_state) == _PROGDVORAK) {
                    set_single_default_layer(_QWERTY);
                } else {
                    set_single_default_layer(_PROGDVORAK);
                }
            }
            break;
          // return false; // Skip normal processing of this key
  }
  return true;
}

void oled_render_boot(bool bootloader) {
  oled_clear();
      if (bootloader) {
          oled_write_P(PSTR("Awaiting New Firmware "), false);
      } else {
          oled_write_P(PSTR("Rebooting "), false);
      }

  oled_render_dirty(true);
}

bool shutdown_user(bool jump_to_bootloader) {
  oled_render_boot(jump_to_bootloader);
  return jump_to_bootloader;
}

// bool process_record_user(uint16_t keycode, keyrecord_t *record) {
//   if (record->event.pressed) {
// #ifdef OLED_ENABLE
//     set_keylog(keycode, record);
// #endif
//     // set_timelog();
//   }

//   switch (keycode) {
//     case L_BASE:
//       if (record->event.pressed) {
//         set_single_persistent_default_layer(_QWERTY);
//       }
//       return false;
//       break;
//     case L_LOWER:
//       if (record->event.pressed) {
//         layer_on(_LOWER);
//         update_tri_layer(_LOWER, _RAISE, _ADJUST);
//       } else {
//         layer_off(_LOWER);
//         update_tri_layer(_LOWER, _RAISE, _ADJUST);
//       }
//       return false;
//       break;
//     case L_RAISE:
//       if (record->event.pressed) {
//         layer_on(_RAISE);
//         update_tri_layer(_LOWER, _RAISE, _ADJUST);
//       } else {
//         layer_off(_RAISE);
//         update_tri_layer(_LOWER, _RAISE, _ADJUST);
//       }
//       return false;
//       break;
//     case L_ADJUST:
//         if (record->event.pressed) {
//           layer_on(_ADJUST);
//         } else {
//           layer_off(_ADJUST);
//         }
//         return false;
//         break;
//   }
//   return true;
// }


void handle_normal_packet(uint8_t *data, uint8_t length) {
    if (length < 2) return; // must be at least type + 1 payload byte

    // Normal Vial / legacy packets
    if (length >= 7 && memcmp(&data[1], "QWERTY", 6) == 0) {
        set_single_default_layer(_QWERTY);
    } else if (length >= 7 && memcmp(&data[1], "DVORAK", 6) == 0) {
        set_single_default_layer(_PROGDVORAK);
    } else if (length >= 17) {
        // Example: system status (offset by 1 for packet_type)
        g_system_status.cpu_temp     = data[1];
        g_system_status.cpu_percent  = data[2];
        g_system_status.gpu_temp     = data[3];
        g_system_status.gpu_percent  = data[4];
        g_system_status.ram_usage    = (uint16_t)data[5] | ((uint16_t)data[6] << 8);
        g_system_status.ram_percent  = (uint16_t)data[7] | ((uint16_t)data[8] << 8);
        g_system_status.vram_usage   = (uint16_t)data[9] | ((uint16_t)data[10] << 8);
        g_system_status.vram_percent = (uint16_t)data[11] | ((uint16_t)data[12] << 8);
        g_system_status.room_temp_1  = (uint16_t)data[13] | ((uint16_t)data[14] << 8);
        g_system_status.room_temp_2  = (uint16_t)data[15] | ((uint16_t)data[16] << 8);
        g_system_status.is_hid_driver_connected = true;
        last_hid_sync = timer_read32();
        status_sync_pending = true;
    }
}
void process_large_payload(uint8_t *data, uint16_t length){

}

static void telemetry_receive(uint8_t *data, uint8_t length) {
    if (length < 2) return;


    uint8_t packet_type = data[0];
    uint32_t now = timer_read32();

    // oled_set_cursor(0,4);
    // char buf[33];
    // memcpy(buf, data, 32);
    // buf[32] = '\0';
    // oled_write(buf, false); // safe
    // g_system_status.vram_usage = data[0];

    // Check for timeout
    if (dynamic_payload_buffer && now - last_packet_time > PAYLOAD_TIMEOUT) {
        // Timeout reached, reset transfer
        reset_payload_transfer();
    }

    if (packet_type == 0x86 && length >= 3) {
        // Payload size packet
        reset_payload_transfer();
        total_payload_size = data[1] | (data[2] << 8);
        if (total_payload_size == 0 || total_payload_size > MAX_LARGE_PAYLOAD_SIZE) {
            reset_payload_transfer();
            return;
        }
        total_packets = (total_payload_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

        dynamic_payload_buffer = malloc(total_payload_size);
        if (!dynamic_payload_buffer) {
            reset_payload_transfer();
            return;
        }
        expected_packet_index = 0;
        last_packet_time = now;

        // Send ACK for size packet
        // This handler owns telemetry acknowledgments; VIA must not send another.
        memset(data, 0, length);
        data[0] = 0x81;
        data[1] = 0xFF;
        raw_hid_send(data, length);

    } else if (packet_type == 0x87) {
        uint8_t packet_index = data[1];

        // Only accept the packet we are expecting
        if (!dynamic_payload_buffer || expected_packet_index >= total_packets || packet_index != expected_packet_index) return;

        uint16_t offset = packet_index * CHUNK_SIZE;
        if (offset >= total_payload_size) return;
        uint8_t chunk_size = (total_payload_size - offset > CHUNK_SIZE) ? CHUNK_SIZE : total_payload_size - offset;
        if (length - 2 < chunk_size) return;

        memcpy(&dynamic_payload_buffer[offset], &data[2], chunk_size);
        expected_packet_index++;
        last_packet_time = now;

        // Send ACK
        memset(data, 0, length);
        data[0] = 0x82;
        data[1] = packet_index;
        raw_hid_send(data, length);

        if (expected_packet_index >= total_packets) {
            // Full payload received
            process_large_payload(dynamic_payload_buffer, total_payload_size);

            reset_payload_transfer();
        }

    } else if (packet_type == 0x88) {
        // One-way telemetry/layout updates leave the shared IN endpoint to VIA.
        handle_normal_packet(data, length);
    } else if (packet_type == 0x85) {
        // Normal Vial/legacy packets
        handle_normal_packet(data, length);
        // Preserve the legacy command echo for the host sender.
        raw_hid_send(data, length);
    }
}

// VIA handles ordinary commands; retain the existing host telemetry protocol.
bool via_command_kb(uint8_t *data, uint8_t length) {
    // USB Raw HID reports are fixed-size. Do not pass truncated reports to VIA,
    // whose command decoder accesses fields throughout the 32-byte buffer.
    if (length != MAX_PACKET_SIZE) return true;
    switch (data[0]) {
        case 0x85:
        case 0x86:
        case 0x87:
        case 0x88:
            telemetry_receive(data, length);
            return true;
        default:
            return false;
    }
}
