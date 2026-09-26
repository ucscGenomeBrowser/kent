#!/usr/bin/env python3
"""Extract a position range from a bgzipped VCF that has no index.

The SPARK WGS 2026_08 GLnexus pVCFs come without .tbi/.csi files, and at
60-85 GB per 2.5 Mb chunk, indexing or streaming a chunk from the start just to
get one gene takes hours. This script instead bisects over the compressed file:
it seeks to a byte offset, finds the next BGZF block, decompresses until the
first complete record and reads its POS. That finds the block where the range
starts in a few dozen seeks, and the range is then streamed from there.

Output on stdout: the VCF header followed by all records with
start <= POS < end (1-based POS, as in the VCF). The file must hold a single
chromosome and be sorted by POS, which is true for the SPARK chunks.

Usage:
    sparkWgs45kPvcfSlice.py in.vcf.gz start end > out.vcf
    sparkWgs45kPvcfSlice.py --noHeader in.vcf.gz start end
"""

import argparse
import os
import sys
import zlib

BGZF_MAGIC = b"\x1f\x8b\x08\x04\x00\x00\x00\x00\x00\xff\x06\x00BC\x02\x00"
READSIZE = 1 << 20


def blocksFrom(fh, offset):
    """Yield decompressed BGZF blocks starting at a block boundary."""
    fh.seek(offset)
    buf = b""
    while True:
        if len(buf) < 18:
            more = fh.read(READSIZE)
            if not more:
                return
            buf += more
            continue
        if not buf.startswith(BGZF_MAGIC):
            sys.exit("not a BGZF block at offset %d" % offset)
        bsize = int.from_bytes(buf[16:18], "little") + 1
        while len(buf) < bsize:
            more = fh.read(READSIZE)
            if not more:
                sys.exit("truncated BGZF block at offset %d" % offset)
            buf += more
        data = zlib.decompress(buf[18:bsize - 8], -15)
        buf = buf[bsize:]
        offset += bsize
        if data:
            yield data


def nextBlockOffset(fh, offset):
    """Return the offset of the first BGZF block at or after offset, verified by
    checking that another block header follows it."""
    fh.seek(offset)
    buf = fh.read(4 * 65536)
    i = 0
    while True:
        i = buf.find(BGZF_MAGIC, i)
        if i < 0:
            return None
        bsize = int.from_bytes(buf[i + 16:i + 18], "little") + 1
        nxt = buf[i + bsize:i + bsize + len(BGZF_MAGIC)]
        if nxt == BGZF_MAGIC or (i + bsize == len(buf)) or len(nxt) < len(BGZF_MAGIC):
            return offset + i
        i += 1


def firstPosAfter(fh, blockOffset):
    """Return POS of the first record that starts inside or after this block."""
    pending = b""
    seenNewline = False
    for data in blocksFrom(fh, blockOffset):
        if not seenNewline:
            j = data.find(b"\n")
            if j < 0:
                continue
            seenNewline = True
            data = data[j + 1:]
        pending += data
        if pending.startswith(b"#"):
            j = pending.find(b"\n")
            if j < 0:
                continue
            pending = pending[j + 1:]
            continue
        k = pending.find(b"\t", pending.find(b"\t") + 1)
        if k > 0:
            return int(pending.split(b"\t", 2)[1])
    return None


def findStartBlock(fh, fileSize, start):
    """Bisect for a block offset whose first record has POS < start, as late as possible."""
    lo, hi = 0, fileSize
    best = 0
    while hi - lo > 4 * 65536:
        mid = (lo + hi) // 2
        blk = nextBlockOffset(fh, mid)
        pos = firstPosAfter(fh, blk) if blk is not None else None
        if pos is None or pos >= start:
            hi = mid
        else:
            best = blk
            lo = mid
    return best


def writeHeader(fh, out):
    pending = b""
    for data in blocksFrom(fh, 0):
        pending += data
        j = pending.find(b"\n#CHROM")
        if j >= 0:
            k = pending.find(b"\n", j + 1)
            if k >= 0:
                out.write(pending[:k + 1])
                return
    sys.exit("no #CHROM line found")


def streamRange(fh, blockOffset, start, end, out):
    """Write records with start <= POS < end, starting the scan at blockOffset."""
    atLineStart = blockOffset == 0
    line = []            # pieces of the current record, only kept if it is in range
    keep = None          # None: POS not known yet for the current line
    head = b""           # first bytes of the current line, to read POS
    nOut = 0
    for data in blocksFrom(fh, blockOffset):
        i = 0
        n = len(data)
        while i < n:
            j = data.find(b"\n", i)
            piece = data[i:] if j < 0 else data[i:j + 1]
            i = n if j < 0 else j + 1
            if not atLineStart:
                if j >= 0:
                    atLineStart = True
                continue
            if keep is None:
                head += piece[:64]
                f = head.split(b"\t", 2)
                if len(f) == 3 or j >= 0:
                    if head.startswith(b"#"):
                        keep = False
                    else:
                        pos = int(f[1])
                        if pos >= end:
                            return nOut
                        keep = pos >= start
            if keep:
                line.append(piece)
            if j >= 0:
                if keep:
                    out.write(b"".join(line))
                    nOut += 1
                elif keep is None:
                    pass
                line = []
                keep = None
                head = b""
    return nOut


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inVcf")
    parser.add_argument("start", type=int, help="1-based, inclusive")
    parser.add_argument("end", type=int, help="1-based, exclusive")
    parser.add_argument("--noHeader", action="store_true")
    args = parser.parse_args()

    fh = open(args.inVcf, "rb")
    out = sys.stdout.buffer
    if not args.noHeader:
        writeHeader(fh, out)
    size = os.path.getsize(args.inVcf)
    blk = findStartBlock(fh, size, args.start)
    nOut = streamRange(fh, blk, args.start, args.end, out)
    out.flush()
    sys.stderr.write("%s:%d-%d startBlock=%d records=%d\n" %
                     (args.inVcf, args.start, args.end, blk, nOut))


if __name__ == "__main__":
    main()
