# Lily58 LP upstream QMK port

Build from the QMK root:

```sh
make lily58lp:upstream -j8
```

The six layers, OLED graphics/status, custom split RPC, RGB Matrix and USB
recovery come from the current Vial keymap. VIA supports persistent remapping
of all six layers and RGB Matrix controls. Existing host telemetry shares the
Raw HID interface through `via_command_kb()`. Vial, VialRGB and OpenRGB
protocols are not enabled. The copied `vial`
keymap is an archive of the source and is not an upstream build target.

## Flashing and handedness

Export the current Vial configuration and keep a confirmed working UF2 before
flashing. Connect only the selected half in BOOTSEL mode. From the QMK root:

```sh
make lily58lp:upstream:uf2-split-left
make lily58lp:upstream:uf2-split-right
```

Run each command with the corresponding half connected. Both halves must run
the matching port. The helper `flash_lily58lp.sh left|right` does the same.
Disconnect USB before changing the split cable. Existing MASTER_RIGHT and
EE_HANDS settings are retained; verify behavior with the usual right-side USB
connection first. No flash is performed by the compile helper.

## Raw HID host compatibility

USB VID/PID: 1209:5800. Raw usage page/id: FF60:0061. Reports are 32 bytes.
A host library may require a leading zero report-ID byte when writing (33 bytes
at the host API; 32 payload bytes reach firmware).

- 0x88: one-way telemetry/base-layer command; no reply (for VIA coexistence).
- 0x85: legacy telemetry/base-layer command; replies with the original report.
- 0x86: little-endian total length; replies 0x81,0xFF and zero padding.
- 0x87: chunk index plus up to 30 bytes; replies 0x82,index and zero padding.

The existing 2-second transfer timeout and 7680-byte transfer cap are preserved.
`process_large_payload()` is empty in the source and remains empty here.
Other commands are handled by VIA (unknown commands receive its unhandled reply).
The host sender must be checked on hardware for
interface selection, acknowledgment handling and reconnect behavior.

## Changes from Vial

RGB_* controls use RM_* keycodes. RGB starts disabled when configuration is
initialized, with solid color selected for subsequent activation. Persisted
settings can override defaults. Six layers are editable in VIA and saved in EEPROM.
The compiled keymap supplies the defaults when VIA storage is initialized.

Vial configuration and unused RGBLight options were removed from this keymap.
USB recovery is retained, including the 10-second enumeration timeout and
3-second held-local-key recovery from suspend. Hardware pins and LED mapping
are preserved. SERIAL_USART_PIN_SWAP is required by this board: the RP2040 PIO driver swaps
the primary TX/RX pins so the split link matches the wiring.

## Hardware acceptance / rollback

Check all 58 keys, all six layers, tri-layer behavior, mod-taps, five-tap numpad,
media/NKRO keys, OLED toggle/timeout, all LEDs, status on both halves, and cold
boot/hub/sleep/resume. Restart the host sender and verify telemetry reconnects.
To roll back, flash the confirmed previous Vial firmware to both halves,
restore handedness if needed, and reload the exported Vial settings.

## OLED brightness keys

Hold Lower + Raise to access Adjust. On the left half, the rightmost key in the
first alphabet row (QWERTY T / Dvorak Y) increases OLED brightness; the key below
it (QWERTY G / Dvorak I) decreases it. Each press changes contrast by 16, bounded
at 16 and 255. Both OLEDs follow the setting, independently of host telemetry.
The existing OLED toggle now controls both displays too. Settings reset to
brightness 96 and enabled on power-up; they are not written to EEPROM.
Both halves must be reflashed because the split transaction list changed.

## Brightness feedback

OLED brightness keys and RGB Matrix brightness keys show a two-second notice
on both displays: OLED or RGB and a percentage. Repeated presses restart the
notice. RGB percentage is relative to the configured brightness cap (80), so
100% respects the existing LED limit. OLED percentage is relative to 255.
Notices are suppressed while OLED is disabled with the OLED toggle. An idle
display may wake temporarily for a notice, then return to its normal state.
Both halves must run this version of the settings RPC.

RGB toggle/on/off keys also show RGB ON or RGB OFF on both OLEDs for two seconds.

## Portrait notification design

Popups use the rotated 32 x 128 canvas: centered compact title, a sun or power
icon, large centered numbers or ON/OFF, a ten-segment brightness meter, and a
shrinking dismissal line. They use pixel drawing so values including 100 stay
on one line despite the regular display's wide font. The two-second duration
and synchronized behavior remain the same.

RGB effect changes show the current effect name on both OLEDs for two seconds.
Names are generated from QMK's enabled effect list and wrap to the portrait
width. Firmware API mode changes are detected as well as cycle keys.

## VIA setup

Flash the matching firmware to both halves. In VIA, enable the Design tab in
Settings and load `keymaps/upstream/via.json` as a V3 definition (V2 disabled),
then authorize the keyboard. This local definition is needed for this custom
build; automatic recognition has not been published.

Layer order: 0 Programmer Dvorak, 1 QWERTY, 2 Lower, 3 Raise, 4 Adjust,
5 Numpad. Custom keycodes expose OLED toggle, base-layout toggle, and OLED
brightness up/down. The five-tap Lower gesture still controls Numpad.
Remappings and macros persist across power cycles; OLED settings remain volatile.
On first use VIA initializes its keymap storage from the compiled defaults.
QMK VIA uses the firmware build date to validate storage: a build from a
different date reinitializes VIA mappings and macros. A same-date rebuild
retains saved mappings, so compiled keymap edits may need a VIA keymap reset.
Export mappings before updating firmware or resetting storage.

Use the updated lily58lp-manager with command `0x88` for one-way status and
layout updates while VIA is connected. It must frame HIDAPI writes with a
leading zero report ID (33 host bytes, 32 firmware bytes). Legacy `0x85`
clients and chunked `0x86`/`0x87` transfers emit replies on VIA's input endpoint
and must be stopped while VIA is in use.

Hardware validation is pending: remap a key on each layer, reconnect and check
persistence, then verify one-way telemetry while VIA is connected.

## Release verification

See [release-check.md](release-check.md) for the verified artifact, software
checks, and hardware acceptance gates. Run the host regression checks with
`python3 keymaps/upstream/tests/verify.py` from the keyboard directory.
