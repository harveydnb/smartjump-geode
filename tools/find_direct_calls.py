#!/usr/bin/env python3
"""Find x86-64 E8 direct-call references to selected function RVAs.

Read-only and pinned to the user's exact GeometryDash.exe SHA-256.
Indirect virtual calls are not included.
"""
import hashlib
import pathlib
import struct
import sys

EXPECTED = "FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0"
TEXT_RVA = 0x1000
TEXT_RAW = 0x400
TEXT_SIZE = 5321216
DEFAULT_TARGETS = [0x388D80, 0x385B30, 0x39D3A0]

def find_calls(data, targets):
    found = {target: [] for target in targets}
    for raw in range(TEXT_RAW, TEXT_RAW + TEXT_SIZE - 4):
        if data[raw] != 0xE8:
            continue
        caller = TEXT_RVA + raw - TEXT_RAW
        dest = caller + 5 + struct.unpack_from("<i", data, raw + 1)[0]
        if dest in found:
            found[dest].append(caller)
    return found

def main():
    if len(sys.argv) < 2:
        raise SystemExit("Usage: python tools/find_direct_calls.py GeometryDash.exe [target_RVA ...]")
    data = pathlib.Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != EXPECTED:
        raise SystemExit("Unexpected executable SHA-256; refusing to apply known RVAs.")
    targets = [int(value, 0) for value in sys.argv[2:]] or DEFAULT_TARGETS
    for target, callers in find_calls(data, targets).items():
        print(f"RVA 0x{target:X}: {len(callers)} possible E8 direct calls")
        for caller in callers:
            print(f"  0x{caller:X}")
    print("Raw opcode matches require instruction-boundary validation. No indirect/vtable calls included.")

if __name__ == "__main__":
    main()
