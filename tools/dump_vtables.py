#!/usr/bin/env python3
"""Extract the first virtual-function entries from verified MSVC x64 vtables.

This reads the *exact* user executable and never executes or modifies it.
Outputs function RVAs (not confirmed gameplay hooks).
Usage: python tools/dump_vtables.py GeometryDash.exe
"""
import hashlib
import pathlib
import struct
import sys

EXPECTED = "FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0"
TABLES = {
    "PlayerObject": 0x5C8270,
    "PlayLayer": 0x5C9790,
    "GameObject": 0x5563B0,
}
IMAGE_BASE = 0x140000000
RDATA_RVA, RDATA_RAW = 0x515000, 0x513600
TEXT_START, TEXT_END = 0x1000, 0x51406C

def main(filename):
    data = pathlib.Path(filename).read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != EXPECTED:
        raise SystemExit("Unknown binary hash; cannot use known table RVAs")
    tables = {}
    for name, rva in TABLES.items():
        offset = rva - RDATA_RVA + RDATA_RAW
        entries = []
        for i in range(128):
            if offset + (i + 1) * 8 > len(data):
                break
            pointer = struct.unpack_from("<Q", data, offset + i * 8)[0]
            target = pointer - IMAGE_BASE
            if not TEXT_START <= target < TEXT_END:
                break
            entries.append(target)
        tables[name] = entries
        print(f"{name}: {len(entries)} consecutive code pointers")
    print("\nSlot  PlayerObject   PlayLayer      GameObject")
    for i in range(min(64, max(map(len, tables.values())))):
        cells = [f"0x{v[i]:06X}" if i < len(v) else "-" for v in tables.values()]
        print(f"{i:>4}  {cells[0]:<14} {cells[1]:<14} {cells[2]}")
    print("\nCAUTION: No slot has been validated as a jump or collision function.")
if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tools/dump_vtables.py GeometryDash.exe")
    main(sys.argv[1])
