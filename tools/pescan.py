#!/usr/bin/env python3
"""Small PE helper for checking hook signatures against a game build.

Usage:
  pescan.py <exe> bytes <rva_hex> [count]      hex dump at an RVA
  pescan.py <exe> find <hex bytes...>          all .text matches of a pattern
  pescan.py <exe> qword <rva_hex> [count]      qwords at an RVA (vtable dump)
  pescan.py <exe> info                         image base / size / sections

Townfall 4:3 fork, by it-was-katsumata (2026). GPL-3.0, see LICENSE.
"""

import mmap
import struct
import sys


def load(path):
    f = open(path, "rb")
    d = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    assert d[pe : pe + 4] == b"PE\0\0"
    nsects = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    image_base = struct.unpack_from("<Q", d, opt + 24)[0]
    size_of_image = struct.unpack_from("<I", d, opt + 56)[0]
    sect_off = opt + struct.unpack_from("<H", d, pe + 20)[0]
    sections = []
    for i in range(nsects):
        s = sect_off + 40 * i
        name = d[s : s + 8].rstrip(b"\0").decode()
        vsz, va, rsz, raw = struct.unpack_from("<IIII", d, s + 8)
        sections.append((name, va, vsz, raw, rsz))
    return d, image_base, size_of_image, sections


def rva_to_off(sections, rva):
    for _, va, vsz, raw, rsz in sections:
        if va <= rva < va + max(vsz, rsz):
            return raw + (rva - va)
    raise ValueError("RVA 0x%X not mapped" % rva)


def main():
    path, mode = sys.argv[1], sys.argv[2]
    d, base, size, sections = load(path)
    if mode == "info":
        print("image base 0x%X size 0x%X" % (base, size))
        for s in sections:
            print("  %-8s va 0x%08X vsz 0x%08X raw 0x%08X rsz 0x%08X" % s)
    elif mode == "bytes":
        rva = int(sys.argv[3], 16)
        n = int(sys.argv[4]) if len(sys.argv) > 4 else 48
        off = rva_to_off(sections, rva)
        print("0x%08X: %s" % (rva, " ".join("%02X" % b for b in d[off : off + n])))
    elif mode == "qword":
        rva = int(sys.argv[3], 16)
        n = int(sys.argv[4]) if len(sys.argv) > 4 else 8
        off = rva_to_off(sections, rva)
        for i in range(n):
            q = struct.unpack_from("<Q", d, off + 8 * i)[0]
            print("[%3d] 0x%X (rva 0x%X)" % (i, q, q - base if q >= base else 0))
    elif mode == "find":
        pat = bytes(int(x, 16) for x in sys.argv[3:])
        text = next(s for s in sections if s[0] == ".text")
        _, va, vsz, raw, rsz = text
        start, end = raw, raw + rsz
        hits = []
        i = d.find(pat, start, end)
        while i != -1 and len(hits) < 20:
            hits.append(va + (i - raw))
            i = d.find(pat, i + 1, end)
        print("%d match(es): %s" % (len(hits), " ".join("0x%X" % h for h in hits)))


if __name__ == "__main__":
    main()
