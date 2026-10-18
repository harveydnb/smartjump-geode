#!/usr/bin/env python3
"""Read-only, exact-build x64 vtable classifier for Geometry Dash 2.2081.

Usage: python tools/classify_player_vtable.py GeometryDash.exe
Requires only Python 3. Does not inject, patch, or rely on GD 2.1 offsets.
"""
import hashlib
import pathlib
import struct
import sys

SHA256 = "FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0"
BASE = 0x140000000
TABLES = {"PlayerObject": 0x5C8270, "PlayLayer": 0x5C9790, "GameObject": 0x5563B0}
SECTIONS = [
    (0x1000, 0x5133EC, 0x400),
    (0x515000, 0x6B9B42, 0x513600),
]
def raw(rva):
    for start, end, offset in SECTIONS:
        if start <= rva < end:
            return offset + rva - start
    raise ValueError(f"RVA outside mapped sections: {rva:x}")
def u64(data, rva):
    return struct.unpack_from("<Q", data, raw(rva))[0]
def resolved(data, rva):
    # Follow x86-64 E9 rel32 thunks, up to three jumps.
    hops = []
    for _ in range(3):
        p = raw(rva)
        if data[p] != 0xE9:
            break
        disp = struct.unpack_from("<i", data, p + 1)[0]
        target = rva + 5 + disp
        hops.append(target)
        rva = target
    return rva, hops
def main(path):
    data = pathlib.Path(path).read_bytes()
    if hashlib.sha256(data).hexdigest().upper() != SHA256:
        raise SystemExit("Unsupported executable SHA-256; refusing offsets.")
    slots = {}
    for name, table in TABLES.items():
        entries = []
        for slot in range(160):
            pointer = u64(data, table + slot * 8)
            rva = pointer - BASE
            if not 0x1000 <= rva < 0x5133EC:
                break
            final, hops = resolved(data, rva)
            entries.append((rva, final, hops))
        slots[name] = entries
        print(f"{name}: {len(entries)} executable vtable slots")
    print("\nPlayerObject entries differing from GameObject (including thunk resolution):")
    for i, (rva, final, hops) in enumerate(slots["PlayerObject"]):
        other = slots["GameObject"]
        if i < len(other) and final == other[i][1]:
            continue
        print(f"slot={i:3d} entry=0x{rva:06X} target=0x{final:06X} thunk={bool(hops)}")
    print("\nNot a confirmed input or collision hook. Validate function behavior before patching.")
if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tools/classify_player_vtable.py GeometryDash.exe")
    main(sys.argv[1])
