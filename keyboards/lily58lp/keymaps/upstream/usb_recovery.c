#include "usb_recovery.h"

#include "keyboard.h"
#include "matrix.h"
#include "timer.h"
#include "usb_main.h"

// Allow the host's reset/address/configuration retries to finish first.
#define USB_RECOVERY_TIMEOUT_MS 10000U
// A held local key provides recovery from a stuck suspend without repeatedly
// disconnecting USB during normal laptop sleep.
#define USB_RECOVERY_KEY_HOLD_MS 3000U

typedef enum {
    RECOVERY_NONE,
    RECOVERY_UNCONFIGURED,
    RECOVERY_SUSPENDED_KEY,
} recovery_reason_t;

static bool local_key_pressed(void) {
    uint8_t first_row = is_keyboard_left() ? 0 : MATRIX_ROWS / 2;
    for (uint8_t row = first_row; row < first_row + MATRIX_ROWS / 2; ++row) {
        if (matrix_get_row(row)) {
            return true;
        }
    }
    return false;
}

void lily58lp_usb_recovery_task(void) {
    static recovery_reason_t waiting_for = RECOVERY_NONE;
    static uint32_t waiting_since;

    // Role detection has finished before either caller runs. A secondary half
    // deliberately has USB stopped; only the primary may restart it.
    if (!is_keyboard_master()) {
        return;
    }

    bool restart = false;
    bool key_pressed = local_key_pressed();
    uint32_t now = timer_read32();

    // Keep the eligibility check and detach together: configuration can
    // complete in the USB interrupt while this task is running.
    chSysLock();
    usbstate_t state = USB_DRIVER.state;
    usbstate_t saved_state = USB_DRIVER.saved_state;

    // A hub can stop polling after failed enumeration, leaving the device
    // suspended and blocking the normal main loop. Its saved state tells us
    // whether it suspended before configuration or during normal host sleep.
    bool unconfigured = state == USB_READY || state == USB_SELECTED;
    if (state == USB_SUSPENDED) {
        unconfigured = saved_state == USB_READY || saved_state == USB_SELECTED;
    }

    recovery_reason_t reason = RECOVERY_NONE;
    if (unconfigured) {
        reason = RECOVERY_UNCONFIGURED;
    } else if (state == USB_SUSPENDED && saved_state == USB_ACTIVE && key_pressed) {
        reason = RECOVERY_SUSPENDED_KEY;
    }
    uint32_t timeout = reason == RECOVERY_SUSPENDED_KEY ? USB_RECOVERY_KEY_HOLD_MS : USB_RECOVERY_TIMEOUT_MS;

    if (reason == RECOVERY_NONE) {
        waiting_for = RECOVERY_NONE;
    } else if (reason != waiting_for) {
        waiting_for = reason;
        waiting_since = now;
    } else if ((uint32_t)(now - waiting_since) >= timeout) {
        usbDisconnectBus(&USB_DRIVER);
        restart = true;
    }
    chSysUnlock();

    if (restart) {
        // QMK restarts the peripheral and endpoint queues and toggles D+ to
        // request fresh enumeration without requiring a cable unplug.
        restart_usb_driver(&USB_DRIVER);
        waiting_since = timer_read32();
    }
}

// ChibiOS invokes this while its suspend loop blocks housekeeping.
// Its loop scans the matrix, so a continuously held local key is visible here.
void suspend_power_down_user(void) {
    lily58lp_usb_recovery_task();
}
