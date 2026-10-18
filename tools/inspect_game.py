#!/usr/bin/env python3
"""Read-only PE64 RTTI locator for the exact user-supplied GeometryDash.exe.

Run: python tools/inspect_game.py /path/to/GeometryDash.exe
Does not execute or modify the game. Requires only Python 3.
"""
import hashlib
import pathlib
import struct
import sys

EXPECTED_SHA256 = "FC5A16C292278BC2E8E078FB1D5023C2BD658322DD72712767EA70C2DD9EC6D0"
NAMES = ("PlayerObject", "PlayLayer")

def u16(b, off): return struct.unpack_from("<H", b, off)[0]
def u32(b, off): return struct.unpack_from("<I", b, off)[0]
def u64(b, off): return struct.unpack_from("<Q", b, off)[0]

def main(path):
    data = pathlib.Path(path).read_bytes()
    sha = hashlib.sha256(data).hexdigest().upper()
    print("SHA-256:", sha)
    if sha != EXPECTED_SHA256:
        raise SystemExit("Unknown executable; refusing to assume known offsets")
    if data[:2] != b"MZ":
        raise SystemExit("Not a PE file")
    pe = u32(data, 0x3c)
    if data[pe:pe + 4] != b"PE\\0\\0" or u16(data, pe + 4) != 0x8664:
        raise SystemExit("Not Windows x64 PE")
    optional = pe + 24
    if u16(data, optional) != 0x20b:
        raise SystemExit("Not PE32+")
    image_base = u64(data, optional + 24)
    section_start = optional + u16(data, pe + 20)
    sections = []
    for i in range(u16(data, pe + 6)):
        off = section_start + 40 * i
        name = data[off:off + 8].rstrip(b"\\0").decode("ascii", "replace")
        sections.append((name, u32(data, off + 12), u32(data, off + 16), u32(data, off + 20)))
    def to_rva(file_offset):
        for name, rva, raw_size, raw_offset in sections:
            if raw_offset <= file_offset < raw_offset + raw_size:
                return rva + file_offset - raw_offset
        return None
    print("Image base:", hex(image_base))
    for name in NAMES:
        signature = (".?AV" + name + "@@").encode("ascii")
        pos = data.find(signature)
        if pos < 0:
            print(name, "RTTI type descriptor not found")
            continue
        # MSVC TypeDescriptor: two pointer-sized fields before decorated name.
        descriptor_file = pos - 16
        descriptor_rva = to_rva(descriptor_file)
        if descriptor_rva is None:
            print(name, "RTTI location not mapped")
            continue
        # MSVC x64 CompleteObjectLocator typically refers to TypeDescriptor by 32-bit image-relative RVA.
        encoded = struct.pack("<I", descriptor_rva)
        candidates = []
        cursor = 0
        while True:
            cursor = data.find(encoded, cursor)
            if cursor < 0: break
            if to_rva(cursor) is not None:
                candidates.append(hex(to_rva(cursor)))
            cursor += 1
        print(f"{name}: type descriptor RVA={descriptor_rva:#x}, candidate 32-bit references={candidates[:40]}")
        print("  NOTE: candidate references are NOT validated function addresses or hooks.")
if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python tools/inspect_game.py GeometryDash.exe")
    main(sys.argv[1])
