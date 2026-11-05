#!/usr/bin/env python3
"""Exact comparison of two bigWig files of fixedStep step=1 span=1 sections.

For every chromosome, compares the raw float32 bytes in position order and the merged
intervals that have data. Chromosomes are matched by name, since files written by
different tools can number and order them differently. Only the Python standard library
is used. Exits non-zero, with a message, on any difference or if no chromosome is found.

usage: gpnStarCompare.py a.bw b.bw
"""
import hashlib, struct, sys, zlib

def chromNames(f, off):
    " read the chromosome B+ tree, return dict chromId -> name "
    f.seek(off)
    magic, blockSize, keySize, valSize, itemCount, _ = struct.unpack("<IIIIQQ", f.read(32))
    names = {}
    def node(pos):
        f.seek(pos)
        isLeaf, _, n = struct.unpack("<BBH", f.read(4))
        if isLeaf:
            for _ in range(n):
                key = f.read(keySize).rstrip(b"\0").decode()
                cid, size = struct.unpack("<II", f.read(8))
                names[cid] = key
        else:
            kids = []
            for _ in range(n):
                f.read(keySize)
                kids.append(struct.unpack("<Q", f.read(8))[0])
            for k in kids:
                node(k)
    node(off + 32)
    return names

def leaves(f, off):
    f.seek(off)
    isLeaf, _, n = struct.unpack("<BBH", f.read(4))
    if isLeaf:
        return [struct.unpack("<IIIIQQ", f.read(32))[4:] for _ in range(n)]
    kids = [struct.unpack("<IIIIQ", f.read(24))[4] for _ in range(n)]
    return [leaf for k in kids for leaf in leaves(f, k)]

def digest(path):
    " return dict chromName -> (sha256 of values in order, merged covered intervals) "
    f = open(path, "rb")
    hdr = f.read(64)
    chromTree, dataOff, indexOff = struct.unpack("<QQQ", hdr[8:32])
    compressed = struct.unpack("<I", hdr[52:56])[0] > 0
    names = chromNames(f, chromTree)
    res = {}
    # the index lists sections in chromosome then start order, so hash as we read
    for off, size in leaves(f, indexOff + 48):
        f.seek(off)
        buf = f.read(size)
        if compressed:
            buf = zlib.decompress(buf)
        cid, start, end, step, span, typ, _, n = struct.unpack("<IIIIIBBH", buf[:24])
        if typ != 3 or step != 1 or span != 1:
            sys.exit("%s: section type %d step %d span %d not handled" % (path, typ, step, span))
        if len(buf) - 24 != 4 * n:
            sys.exit("%s: section at %s:%d holds %d bytes for %d items"
                     % (path, names[cid], start, len(buf) - 24, n))
        chrom = names[cid]
        h, ivs = res.setdefault(chrom, (hashlib.sha256(), []))
        if ivs and start < ivs[-1][1]:
            sys.exit("%s: sections out of order or overlapping on %s at %d" % (path, chrom, start))
        h.update(buf[24:])
        if ivs and ivs[-1][1] == start:
            ivs[-1][1] = start + n
        else:
            ivs.append([start, start + n])
    return {c: (h.hexdigest(), ivs) for c, (h, ivs) in res.items()}

def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = digest(sys.argv[1]), digest(sys.argv[2])
    if not a:
        sys.exit("FAIL %s: no chromosomes with data" % sys.argv[1])
    if sorted(a) != sorted(b):
        sys.exit("FAIL chromosome sets differ: %s vs %s" % (sorted(a), sorted(b)))
    for c in sorted(a):
        if a[c][1] != b[c][1]:
            sys.exit("FAIL %s: covered intervals differ" % c)
        if a[c][0] != b[c][0]:
            sys.exit("FAIL %s: values differ" % c)
    print("OK %s identical float32 values on %d chromosomes" % (sys.argv[1], len(a)))

if __name__ == "__main__":
    main()
