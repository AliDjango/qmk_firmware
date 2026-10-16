# VIA release verification — 2026-10-16

Status: software checks passed; hardware acceptance pending. Not flashed.

Artifact: `/home/alish/qmk_firmware/lily58lp_upstream.uf2`

SHA256: `e09589a475626d2e04ace2e6a78e16ddb505e1bc51b118aa5ff7664fc040d8df`

## Verified

- Fresh isolated `lily58lp:upstream` build with QMK 0.34.6 and
  arm-none-eabi-gcc 15.2.0; no compiler warnings or errors.
  Build log: `/tmp/lily58lp-via-oneway-release-build.log`.
- Six dynamic layers fit QMK's EEPROM layout and compile-time storage checks.
  RP2040 defaults provide 4096 logical EEPROM bytes; key positions require
  720 bytes (6 layers × 10 rows × 6 columns × 2 bytes), plus VIA metadata.
- UF2 headers, block sequence/count, RP2040 family, payload boundaries and
  flash range verified. Flash ends at `0x1000ff00`, below EEPROM backing
  storage at `0x101fe000` on the default 2 MiB flash configuration.
- Linker reserves approximately 249 KiB of heap. Maximum telemetry allocation
  is 7680 bytes; static storage and reserved stacks fit the linker memory map.
- Actual production telemetry handlers compiled into a host harness, with
  hardware services stubbed. AddressSanitizer, UndefinedBehaviorSanitizer,
  LeakSanitizer and explicit allocation-balance checks passed.
- Tests cover ordinary VIA command passthrough without telemetry replies,
  truncated reports, QWERTY/Dvorak commands, telemetry field decoding,
  acknowledgments, zero/oversize payload rejection, allocation failure,
  1/30/31/7680-byte transfers, out-of-order chunks, interrupted-transfer
  restart, timeout and timer rollover. One-way `0x88` status and layout updates
  are verified to emit no replies; legacy `0x85` echo behavior is retained.
- VIA definition has exactly the firmware's 58 unique matrix positions,
  matching USB IDs and four custom keycodes. Flash/build scripts pass
  `bash -n`.

The host harness verifies local command routing and telemetry behavior. It
does not emulate QMK's complete VIA implementation, EEPROM hardware, USB,
split transport, or OLED timing.

## Required hardware acceptance before a production sign-off

Export current mappings and retain a known working firmware. Flash matching
firmware to both halves using the documented handedness procedure.

- [ ] Load `via.json` as a V3 definition and authorize the keyboard in VIA.
- [ ] Confirm layer 0 is Programmer Dvorak and layer 1 is QWERTY. Remap and
      restore one key on each of the six layers. Check saved mappings after
      a power cycle with the usual right-side USB connection.
- [ ] Verify all 58 keys, home-row mod-taps, Lower/Raise/Adjust tri-layer,
      five-tap Numpad gesture, media keys and NKRO.
- [ ] Exercise the custom OLED toggle/brightness and base-layout toggle
      keycodes from VIA. Check QW/DV and RGB popups on both displays,
      repeated presses, two-second dismissal and display timeout behavior.
- [ ] Check VIA RGB Matrix controls and all LEDs.
- [ ] Run the existing host telemetry sender: status updates, layout commands,
      transfer ACKs, reconnect, and use while VIA is connected. Check that
      the two host programs can share the HID device on the target OS.
- [ ] Cold boot, reconnect, hub connection, sleep/resume and split recovery
      work without repeated USB reconnects or dropped keys.

QMK's stock VIA EEPROM validity includes the build date: installing a build
from a different date resets VIA mappings/macros. Export before updates and
restore afterward. Same-date builds retain saved mappings, so compiled
default changes may require a deliberate keymap reset.

## Repeat software checks

From the keyboard directory:

```sh
python3 keymaps/upstream/tests/verify.py
bash -n compile_lily58lp.sh flash_lily58lp.sh
./compile_lily58lp.sh
```

The host tests default to Clang; set `CC` to select another compiler with
sanitizer runtimes. LeakSanitizer requires execution outside a ptrace sandbox.
Generated host test binaries are temporary and removed after the run.

## Telemetry/VIA coexistence correction

Hardware logs from the earlier build showed desynchronized VIA responses while
telemetry was running; stopping telemetry allowed VIA to load. The earlier
shared-interface arrangement therefore did not pass hardware acceptance.

The updated manager now uses `0x88` for one-way status/layout reports instead
of `0x85`, which the firmware echoes. It also supplies the HIDAPI zero report-ID
prefix and detects incomplete writes. Both sides need this update. Manager:
21 tests passed, Ruff lint/format passed. Firmware sanitizer tests, fresh build
and UF2 validation passed. Simultaneous operation on hardware remains pending.
Legacy `0x85` clients and chunk transfers still produce responses and must not
run alongside VIA. The manager was not restarted and no hardware was flashed.

## OLED status-screen redesign

Redrew the bootloader wait, reboot, and waiting-for-HID screens with the
portrait pixel renderer, centered headings, and matching icons. The HID wait
remains visible until the split status reports a connected host. Build passed;
visual and split behavior need hardware confirmation. The UF2 checksum above
identifies this build.
