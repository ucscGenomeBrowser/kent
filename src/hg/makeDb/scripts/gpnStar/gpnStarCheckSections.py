#!/usr/bin/env python3
"""Report bigWig fixedStep sections whose header end-start disagrees with itemCount.

The GPN-Star bigWigs from Hugging Face have such sections (end overshoots by 6 before
each coverage gap), which makes kent bigWigAverageOverBed and bigWigCorrelate abort.

usage: gpnStarCheckSections.py file.bw nShow
"""
import struct, sys, zlib
f = open(sys.argv[1], "rb"); nShow = int(sys.argv[2])
h = f.read(64)
magic, ver, zl, chromTree, dataOff, indexOff = struct.unpack("<IHHQQQ", h[:32])
uncompBuf = struct.unpack("<I", h[52:56])[0]
f.seek(indexOff); rh = f.read(48)
def leaves(off):
    f.seek(off); isLeaf, _, cnt = struct.unpack("<BBH", f.read(4))
    if isLeaf:
        items = [struct.unpack("<IIIIQQ", f.read(32))[4:] for _ in range(cnt)]
        for it in items: yield it
    else:
        kids = [struct.unpack("<IIIIQ", f.read(24))[4] for _ in range(cnt)]
        for k in kids: yield from list(leaves(k))
shown = bad = total = 0
for off, size in list(leaves(indexOff + 48)):
    f.seek(off); raw = f.read(size)
    buf = zlib.decompress(raw) if uncompBuf else raw
    chromId, start, end, step, span, typ, _, cnt = struct.unpack("<IIIIIBBH", buf[:24])
    payload = len(buf) - 24
    total += 1
    mismatch = (typ == 3 and step == 1 and span == 1 and end - start != cnt)
    bad += mismatch
    if shown < nShow or (mismatch and bad <= 3):
        print(f"type={typ} start={start} end={end} end-start={end-start} itemCount={cnt} payloadFloats={payload//4} {'MISMATCH' if mismatch else ''}")
        shown += 1
print(f"sections={total} fixedStep step1 sections with end-start != itemCount: {bad}")
