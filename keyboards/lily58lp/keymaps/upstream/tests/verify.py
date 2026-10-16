#!/usr/bin/env python3
"""Host checks for the actual firmware telemetry handlers and VIA definition.

Run: python3 keymaps/upstream/tests/verify.py
Requires a C compiler with AddressSanitizer and UndefinedBehaviorSanitizer.
Hardware acceptance is documented separately in readme.md.
"""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

KEYMAP = Path(__file__).resolve().parents[1]
BOARD = KEYMAP.parents[1]
source = (KEYMAP / "keymap.c").read_text()
definition = json.loads((KEYMAP / "via.json").read_text())
board = json.loads(re.sub(r"//[^\n]*", "", (BOARD / "keyboard.json").read_text()))
coordinates = [
    tuple(map(int, item.split(",")))
    for row in definition["layouts"]["keymap"]
    for item in row if isinstance(item, str)
]
expected = {tuple(key["matrix"]) for key in board["layouts"]["LAYOUT"]["layout"]}
assert len(coordinates) == len(set(coordinates)) == 58
assert set(coordinates) == expected
assert definition["vendorId"] == board["usb"]["vid"]
assert definition["productId"] == board["usb"]["pid"]
assert len(definition["customKeycodes"]) == 4

# Compile the production functions unchanged, replacing only hardware services.
status = source[source.index("// FOR HID"):source.index("//FOR SLAVE-MASTER COMMS")]
transfer = source[source.index("#define MAX_PACKET_SIZE"):source.index("//extern uint8_t __StackLimit")]
handlers = source[source.index("void handle_normal_packet"):]
stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static uint32_t now;
static unsigned sends;
static uint8_t reply[32], base;
enum { _PROGDVORAK, _QWERTY };
static uint32_t timer_read32(void) { return now; }
static void set_single_default_layer(uint8_t layer) { base = layer; }
static void raw_hid_send(uint8_t *data, uint8_t length) {
    assert(length == 32);
    memcpy(reply, data, length);
    ++sends;
}
static bool fail_alloc;
static unsigned live_allocations;
static void *test_malloc(size_t size) {
    void *result = fail_alloc ? NULL : malloc(size);
    if (result) ++live_allocations;
    return result;
}
static void test_free(void *ptr) {
    if (ptr) { assert(live_allocations); --live_allocations; }
    free(ptr);
}
#define malloc test_malloc
#define free test_free
'''
checks = r'''
static void start(uint16_t size) {
    uint8_t packet[32] = {0x86, size & 255, size >> 8};
    unsigned before = sends;
    assert(via_command_kb(packet, sizeof(packet)));
    assert(sends == before + 1);
    assert(reply[0] == 0x81 && reply[1] == 0xff);
    for (int i = 2; i < 32; ++i) assert(reply[i] == 0);
}
int main(void) {
    // Standard VIA commands must fall through unchanged, without telemetry ACKs.
    for (int id = 0; id < 256; ++id) {
        if (id == 0x85 || id == 0x86 || id == 0x87 || id == 0x88) continue;
        uint8_t packet[32] = {id};
        unsigned before = sends;
        assert(!via_command_kb(packet, sizeof(packet)));
        assert(packet[0] == id && sends == before);
    }
    assert(via_command_kb(NULL, 0));
    for (int size = 1; size < 32; ++size) {
        uint8_t *short_packet = malloc(size);
        memset(short_packet, 0, size);
        short_packet[0] = 0x86;
        unsigned before = sends;
        assert(via_command_kb(short_packet, size));
        assert(sends == before && !dynamic_payload_buffer);
        free(short_packet);
    }
    uint8_t packet[32] = {0x85};
    memcpy(packet + 1, "QWERTY", 6);
    assert(via_command_kb(packet, 32) && base == _QWERTY);
    assert(memcmp(packet, reply, 32) == 0);
    memcpy(packet + 1, "DVORAK", 6);
    assert(via_command_kb(packet, 32) && base == _PROGDVORAK);
    for (int i = 1; i < 32; ++i) packet[i] = i;
    now = 1234;
    assert(via_command_kb(packet, 32));
    assert(g_system_status.cpu_temp == 1 && g_system_status.gpu_percent == 4);
    assert(g_system_status.ram_usage == 0x0605);
    assert(g_system_status.room_temp_2 == 0x100f);
    assert(g_system_status.is_hid_driver_connected && status_sync_pending);
    assert(last_hid_sync == now);

    for (int size = 0; size <= 7681; size += 7681) {
        uint8_t invalid[32] = {0x86, size & 255, size >> 8};
        unsigned before = sends;
        assert(via_command_kb(invalid, 32));
        assert(sends == before && !dynamic_payload_buffer);
    }
    fail_alloc = true;
    uint8_t allocation[32] = {0x86, 30};
    unsigned before = sends;
    assert(via_command_kb(allocation, 32));
    assert(sends == before && !dynamic_payload_buffer);
    fail_alloc = false;

    // One-way updates share the parser but must never enqueue a VIA-visible reply.
    uint8_t one_way[32] = {0x88};
    memcpy(one_way + 1, "QWERTY", 6);
    before = sends;
    assert(via_command_kb(one_way, 32) && base == _QWERTY && sends == before);
    memcpy(one_way + 1, "DVORAK", 6);
    assert(via_command_kb(one_way, 32) && base == _PROGDVORAK && sends == before);
    for (int i = 1; i < 32; ++i) one_way[i] = i;
    assert(via_command_kb(one_way, 32) && sends == before);
    assert(g_system_status.ram_usage == 0x0605 && g_system_status.cpu_temp == 1);

    // Exercise single-byte, partial final, and maximum 256-packet transfers.
    const uint16_t sizes[] = {1, 30, 31, 7680};
    for (unsigned test = 0; test < sizeof(sizes) / sizeof(sizes[0]); ++test) {
        start(sizes[test]);
        uint16_t count = total_packets;
        for (uint16_t i = 0; i < count; ++i) {
            uint8_t chunk[32] = {0x87, (uint8_t)i};
            memset(chunk + 2, 0xa5, 30);
            before = sends;
            assert(via_command_kb(chunk, 32));
            assert(sends == before + 1 && reply[0] == 0x82 && reply[1] == i);
            for (int j = 2; j < 32; ++j) assert(reply[j] == 0);
        }
        assert(!dynamic_payload_buffer && !total_packets && !expected_packet_index);
    }
    start(31);
    uint8_t out_of_order[32] = {0x87, 1};
    before = sends;
    assert(via_command_kb(out_of_order, 32));
    assert(sends == before && expected_packet_index == 0);
    start(1); // Restart frees the interrupted transfer.
    now += PAYLOAD_TIMEOUT + 1;
    uint8_t expired[32] = {0x87, 0};
    before = sends;
    assert(via_command_kb(expired, 32));
    assert(sends == before && !dynamic_payload_buffer);
    // Timeout arithmetic must also survive timer rollover.
    now = UINT32_MAX - 100;
    start(31);
    now = 2001;
    assert(via_command_kb(expired, 32) && !dynamic_payload_buffer);
    assert(live_allocations == 0);
    puts("PASS: VIA routing, short reports, telemetry, ACKs, bounds, allocation failure, restarts, timeout and rollover");
}
'''
with tempfile.TemporaryDirectory(prefix="lily58lp-verify-") as tmp:
    path = Path(tmp)
    (path / "verify.c").write_text(stubs + status + transfer + handlers + checks)
    subprocess.run([
        os.environ.get("CC", "clang"), "-std=c11", "-g", "-fsanitize=address,undefined",
        "-fno-omit-frame-pointer", str(path / "verify.c"), "-o", str(path / "verify"),
    ], check=True)
    subprocess.run([str(path / "verify")], check=True)
print("PASS: VIA definition matches all 58 physical keys, USB IDs and custom key count")
