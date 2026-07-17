#!/usr/bin/env python3
"""clinvarMappedParalogAlign.py - global pairwise protein alignment for annotated
paralog pairs.

For every unordered paralog pair (already annotated as paralogs by Ensembl and
pre-filtered to >=20% identity) we do a global Needleman-Wunsch alignment of the
two MANE Select proteins (BLOSUM62, gap open 11 / extend 1) and reduce it to a
list of ungapped aligned blocks in protein coordinates. Those blocks are the
shared substrate for variant projection and for the bigPsl alignment track.

Inputs:
  geneProt.faa    protein per gene, header ">ENSG..."
  pairs.ge20.tsv  geneA geneB symA symB pidAB pidBA maxPid   (>=20% id pairs)
Output (stdout or -o):
  geneA geneB lenA lenB nIdent matchCols blockCount aStarts bStarts blockSizes
  coords are 0-based protein positions; aStarts/bStarts/blockSizes are csv.

Usage: clinvarMappedParalogAlign.py <faa> <pairs> -o <out> [-j N]
"""
import argparse, sys, multiprocessing as mp
import parasail

VALID = set("ACDEFGHIKLMNPQRSTVWY")
MATRIX = parasail.blosum62
GAP_OPEN, GAP_EXT = 11, 1

prot = {}  # ensg -> sanitized seq (module global, inherited by workers via fork)

def sanitize(seq):
    seq = seq.rstrip("*").upper()
    return "".join(c if c in VALID else "X" for c in seq)

def loadFasta(path):
    d, name, buf = {}, None, []
    with open(path) as fh:
        for line in fh:
            if line.startswith(">"):
                if name is not None:
                    d[name] = sanitize("".join(buf))
                name = line[1:].split()[0]; buf = []
            else:
                buf.append(line.strip())
    if name is not None:
        d[name] = sanitize("".join(buf))
    return d

def alignPair(pair):
    a, b = pair
    qs, ts = prot.get(a), prot.get(b)
    if not qs or not ts:
        return None
    res = parasail.nw_trace_striped_16(qs, ts, GAP_OPEN, GAP_EXT, MATRIX)
    q = res.traceback.query   # aligned query with '-'
    t = res.traceback.ref     # aligned target with '-'
    ai = bi = 0
    aStarts, bStarts, sizes = [], [], []
    curLen = 0; nIdent = 0
    for qc, tc in zip(q, t):
        if qc != '-' and tc != '-':
            if curLen == 0:
                aStarts.append(ai); bStarts.append(bi)
            curLen += 1
            if qc == tc:
                nIdent += 1
            ai += 1; bi += 1
        else:
            if curLen:
                sizes.append(curLen); curLen = 0
            if qc != '-': ai += 1
            if tc != '-': bi += 1
    if curLen:
        sizes.append(curLen)
    if not sizes:
        return None
    matchCols = sum(sizes)
    return (a, b, len(qs), len(ts), nIdent, matchCols, len(sizes),
            ",".join(map(str, aStarts)), ",".join(map(str, bStarts)),
            ",".join(map(str, sizes)))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("faa"); ap.add_argument("pairs")
    ap.add_argument("-o", default="-"); ap.add_argument("-j", type=int, default=32)
    args = ap.parse_args()

    global prot
    prot = loadFasta(args.faa)
    sys.stderr.write("loaded %d proteins\n" % len(prot))

    pairs = []
    with open(args.pairs) as fh:
        for line in fh:
            f = line.rstrip("\n").split("\t")
            pairs.append((f[0], f[1]))
    sys.stderr.write("aligning %d pairs on %d workers\n" % (len(pairs), args.j))

    out = sys.stdout if args.o == "-" else open(args.o, "w")
    n = ok = 0
    with mp.Pool(args.j) as pool:
        for r in pool.imap_unordered(alignPair, pairs, chunksize=200):
            n += 1
            if r:
                ok += 1
                out.write("\t".join(map(str, r)) + "\n")
            if n % 10000 == 0:
                sys.stderr.write("  %d/%d\n" % (n, len(pairs)))
    if out is not sys.stdout:
        out.close()
    sys.stderr.write("aligned %d pairs (%d skipped for missing seq)\n" % (ok, n - ok))

if __name__ == "__main__":
    main()
