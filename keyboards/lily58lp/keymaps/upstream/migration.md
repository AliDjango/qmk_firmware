# Migration build record

- QMK: 0.34.6-1-g7a1bbf37c5
- Commit: 7a1bbf37c5c07da4ea0a162bb139083a46ef40a1
- Latest remote stable tag checked: 0.34.6
- Target: lily58lp:upstream
- Build: successful (arm-none-eabi-gcc 15.2.0)
- UF2: /home/alish/qmk_firmware/lily58lp_upstream.uf2
- SHA256: 8e7094aea77e63716e63017c9baebd0f5d31808aa2b14418b76e3723d68f6830
- Backup: /home/alish/qmk_firmware/.migration-backups/lily58lp-20261016-140231
- Hardware flash/acceptance: pending; no device flashed.

The previous keyboard and the synced source before porting are in the backup.
Previous dependency commits are in submodules-before.txt. The four clean,
mismatched dependencies were aligned with the parent QMK commit to resolve
ChibiOS configuration incompatibility. The original Vial checkout was not edited.

## Split communication correction

Restored SERIAL_USART_PIN_SWAP after inspecting the RP2040 PIO driver.
The first migration incorrectly removed this required hardware setting.
Both halves must be reflashed with the corrected build.

Corrected generic build: successful. Hardware verification remains pending.

## OLED brightness correction

Removed the per-frame oled_set_brightness(0) call copied from the source.
OLED_BRIGHTNESS is now 128. Build passed; display behavior needs hardware confirmation.

## OLED controls

Added brightness up/down keys on Adjust and dedicated split settings RPC.
Brightness and OLED toggle synchronize to both halves without host telemetry.
Build passed; both halves require reflashing. Hardware validation pending.

## Brightness feedback

Added a synchronized two-second OLED/RGB percentage notice. RGB percentage
uses the configured LED brightness cap. Build passed; hardware validation pending.
Both halves need reflashing because the OLED settings RPC payload changed.

## RGB power feedback

RGB toggle/on/off keys show RGB ON or RGB OFF on both OLEDs for two seconds.
Build passed; hardware validation pending.

## Lower OLED default

Changed OLED_BRIGHTNESS from 128 to 96. Rebuild through the flash helpers
to apply it; the existing UF2/checksum above predates this configuration change.

## Portrait popup design

Replaced text-only popups with a centered pixel renderer for 32 x 128 OLEDs.
Includes sun/power icons, large values, a segmented meter, and countdown line.
Build passed; visual confirmation on hardware pending.

## RGB effect names

Added portrait effect-name popups using the enabled QMK effect list.
Both cycle keys and firmware API mode changes trigger the synchronized notice.
Build passed; hardware appearance remains to be checked.
