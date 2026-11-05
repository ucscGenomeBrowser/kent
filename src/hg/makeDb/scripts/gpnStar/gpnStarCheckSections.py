#!/usr/bin/env python3
"""Check the data sections of a bigWig file.

Reports sections whose data length does not match their item count, and fixedStep
step=1 span=1 sections whose header end-start disagrees with the item count. The
GPN-Star bigWigs from Hugging Face have the second problem (end overshoots by 6 before
each coverage gap, a pyBigWig bug), which makes kent bigWigAverageOverBed and
bigWigCorrelate abort.

usage: gpnStarCheckSections.py file.bw nShow
"""
import struct, sys, zlib

ITEM_BYTES = {1: 12, 2: 8, 3: 4}   # bedGraph, variableStep, fixedStep

f = open(sys.argv[1], "rb")
nShow = int(sys.argv[2])
h = f.read(64)
indexOff = struct.unpack("<Q", h[24:32])[0]
compressed = struct.unpack("<I", h[52:56])[0] > 0

def leaves(off):
    f.seek(off)
    isLeaf, _, cnt = struct.unpack("<BBH", f.read(4))
    if isLeaf:
        return [struct.unpack("<IIIIQQ", f.read(32))[4:] for _ in range(cnt)]
    kids = [struct.unpack("<IIIIQ", f.read(24))[4] for _ in range(cnt)]
    return [leaf for k in kids for leaf in leaves(k)]

shown = badEnd = badLen = total = 0
for off, size in leaves(indexOff + 48):
    f.seek(off)
    buf = f.read(size)
    if compressed:
        buf = zlib.decompress(buf)
    chromId, start, end, step, span, typ, _, cnt = struct.unpack("<IIIIIBBH", buf[:24])
    total += 1
    lenOk = (len(buf) - 24 == ITEM_BYTES.get(typ, -1) * cnt)
    endOk = not (typ == 3 and step == 1 and span == 1 and end - start != cnt)
    badLen += not lenOk
    badEnd += not endOk
    if shown < nShow or ((not lenOk or not endOk) and badEnd + badLen <= 3):
        print(f"type={typ} start={start} end={end} itemCount={cnt} dataBytes={len(buf) - 24}"
              f"{'' if lenOk else ' BADLENGTH'}{'' if endOk else ' BADEND'}")
        shown += 1
print(f"sections with data length != item count: {badLen}")
print(f"sections={total} fixedStep step1 sections with end-start != itemCount: {badEnd}")
if total == 0 or badLen:
    sys.exit(1)
