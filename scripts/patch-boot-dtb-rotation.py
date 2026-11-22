#!/usr/bin/env python3
"""Patch the s5k5e9 (macro) camera rotation in a lisa boot image's appended
DTB, in place, without rebuilding the kernel. Alignment is calculated relative
to the FDT structure block, which may itself be unaligned in the boot image."""
import struct, sys
from pathlib import Path
import libfdt

BEGIN, END, PROP, NOP, ENDNODE = 1, 9, 3, 4, 2

def main():
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    img = bytearray(src.read_bytes())
    base = img.find(b'\xd0\x0d\xfe\xed')
    if base < 0:
        raise SystemExit('no appended DTB found')
    dtb = bytearray(img[base:base + struct.unpack_from('>I', img, base + 4)[0]])
    tot, offs, offst, offmem, ver, lcv, cpu, szst, szstruct = \
        struct.unpack_from('>9I', dtb, 4)
    p, end, strings = offs, offs + szstruct, offst
    path, props = [], []
    while p < end:
        tag = struct.unpack_from('>I', dtb, p)[0]
        if tag == BEGIN:
            p += 4
            e = dtb.find(b'\0', p)
            path.append(dtb[p:e].decode() or '/')
            p += ((e - p) + 4) & ~3
        elif tag == ENDNODE:
            p += 4
            if path: path.pop()
        elif tag == NOP:
            p += 4
        elif tag == PROP:
            ln, nameoff = struct.unpack_from('>II', dtb, p + 4)
            ne = dtb.find(b'\0', strings + nameoff)
            name = dtb[strings + nameoff:ne].decode()
            voff = p + 12
            props.append(('/'.join(path), name, voff, bytes(dtb[voff:voff + ln])))
            p = voff + ((ln + 3) & ~3)
        elif tag == END:
            break
        else:
            raise SystemExit(f'bad FDT tag {tag} at {p}')
    nodes = {n: v.split(b'\0')[0].decode() for n, k, _, v in props if k == 'compatible'}
    macro = [n for n, c in nodes.items() if c == 'samsung,s5k5e9']
    if len(macro) != 1:
        raise SystemExit(f'expected one s5k5e9 node, got {macro}')
    rot = [(n, o, v) for n, k, o, v in props if n == macro[0] and k == 'rotation']
    if len(rot) != 1:
        raise SystemExit(f'expected one rotation on {macro[0]}, got {len(rot)}')
    n, voff, val = rot[0]
    old = struct.unpack('>I', val[:4])[0]
    print(f'DTB at {base:#x}, node {n}, rotation {old} at DTB+{voff:#x}')
    if old not in (90, 270):
        raise SystemExit(f'unexpected rotation {old}')
    struct.pack_into('>I', dtb, voff, 90)
    img[base:base + tot] = dtb
    fdt = libfdt.Fdt(bytes(dtb))
    got = struct.unpack('>I', bytes(fdt.getprop(fdt.path_offset(n), 'rotation'))[:4])[0]
    print(f'verified: rotation now {got}; writing {dst}')
    dst.write_bytes(img)

if __name__ == '__main__':
    main()
