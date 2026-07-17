#!/usr/bin/env python3
"""clinvarMappedParalogAlnPsl.py - turn the paralog protein alignments into PSL so the
alignment that produced each variant projection can be viewed as a track.

For every annotated paralog pair we emit two PSL records (one per orientation):
query = one gene's CDS, target = the other gene's genome. Blocks come from the
same per-pair protein alignment used for variant projection (3 nt per aligned
residue), broken at alignment gaps and at intron / split-codon boundaries, so
the alignment track is exactly the evidence for the map track.

Inputs (in dataDir): alnBlocks.tsv, maneSelect.gp, maneMeta.tsv, geneProt.faa
Output (-o): PSL (21 cols). Feed to pslToBigPsl -fa=<faOut> | bedToBigBed.

With --faOut/--twoBit it also writes a query CDS FASTA (keyed by gene symbol =
the PSL qName), built from the same cdsG coordinates so each sequence length
equals the PSL qSize exactly. This lets the bigPsl show base match/mismatch.

Usage: clinvarMappedParalogAlnPsl.py <dataDir> <chromSizes> -o out.psl
                               [--faOut cds.fa --twoBit genome.2bit]
"""
import argparse, sys, os, array

COMP = {"A": "T", "C": "G", "G": "C", "T": "A", "N": "N"}

def writeCdsFasta(twoBit, faOut):
    import py2bit
    tb = py2bit.open(twoBit)
    with open(faOut, "w") as fh:
        for ensg, coords in cdsG.items():
            chrom = chromOf[ensg]; minus = strandOf[ensg] == "-"
            lo, hi = coords[0], coords[0]
            for c in coords:
                if c < lo: lo = c
                if c > hi: hi = c
            block = tb.sequence(chrom, lo, hi + 1).upper()
            seq = []
            for c in coords:
                b = block[c - lo]
                seq.append(COMP.get(b, "N") if minus else (b if b in COMP else "N"))
            fh.write(">%s\n%s\n" % (symOf[ensg], "".join(seq)))
    tb.close()

cdsG = {}; chromOf = {}; strandOf = {}; symOf = {}; chromSizes = {}

def buildCdsG(gpPath, enst2ensg):
    out = {}
    for line in open(gpPath):
        f = line.rstrip("\n").split("\t")
        enst, strand = f[0], f[2]
        cdsStart, cdsEnd = int(f[5]), int(f[6])
        if cdsStart >= cdsEnd: continue
        exStarts = [int(x) for x in f[8].rstrip(",").split(",")]
        exEnds   = [int(x) for x in f[9].rstrip(",").split(",")]
        segs = [(max(s, cdsStart), min(e, cdsEnd)) for s, e in zip(exStarts, exEnds)
                if max(s, cdsStart) < min(e, cdsEnd)]
        a = array.array('i')
        if strand == "+":
            for (s, e) in segs: a.extend(range(s, e))
        else:
            for (s, e) in reversed(segs): a.extend(range(e - 1, s - 1, -1))
        g = enst2ensg.get(enst)
        if g: out[g] = a
    return out

def buildPsl(qEnsg, tEnsg, qAaStarts, tAaStarts, aaSizes):
    """query = qEnsg CDS, target = tEnsg genome."""
    gT = cdsG[tEnsg]; tStrand = strandOf[tEnsg]
    qSize = len(cdsG[qEnsg]); tChrom = chromOf[tEnsg]; tSize = chromSizes[tChrom]
    # nucleotide walk in transcription order -> ungapped blocks
    blocks = []  # [qStartNt, tGfirst, size]  transcription order
    cur = None; prevQ = None; prevT = None
    for qa, ta, sz in zip(qAaStarts, tAaStarts, aaSizes):
        for k in range(sz):
            qAa = qa + k; tAa = ta + k
            base = 3 * tAa
            if base + 3 > len(gT):
                if cur: blocks.append(cur); cur = None
                prevQ = prevT = None; continue
            for f in range(3):
                qNt = 3 * qAa + f; tG = gT[base + f]
                cont = (cur is not None and qNt == prevQ + 1 and
                        (tG == prevT + 1 if tStrand == "+" else tG == prevT - 1))
                if cont:
                    cur[2] += 1
                else:
                    if cur: blocks.append(cur)
                    cur = [qNt, tG, 1]
                prevQ = qNt; prevT = tG
    if cur: blocks.append(cur)
    if not blocks: return None

    if tStrand == "+":
        strand = "+"
        qStarts = [b[0] for b in blocks]
        tStarts = [b[1] for b in blocks]
        sizes   = [b[2] for b in blocks]
        # PSL qStart/qEnd are + strand query coords, matching the qStarts list
        qStart, qEnd = qStarts[0], qStarts[-1] + sizes[-1]
    else:
        strand = "-"
        rc = []
        for q0, tGfirst, L in blocks:
            rc.append((tGfirst - (L - 1), qSize - (q0 + L), L))  # (tStart, qStart_rc, L)
        rc.sort()
        tStarts = [x[0] for x in rc]; qStarts = [x[1] for x in rc]; sizes = [x[2] for x in rc]
        # qStarts are reverse-strand coords, but PSL qStart/qEnd stay in + coords
        qStart = qSize - (qStarts[-1] + sizes[-1])
        qEnd   = qSize - qStarts[0]
    tStart, tEnd = tStarts[0], tStarts[-1] + sizes[-1]
    match = sum(sizes)
    qNI = qBI = tNI = tBI = 0
    for i in range(1, len(sizes)):
        qg = qStarts[i] - (qStarts[i - 1] + sizes[i - 1])
        tg = tStarts[i] - (tStarts[i - 1] + sizes[i - 1])
        if qg > 0: qNI += 1; qBI += qg
        if tg > 0: tNI += 1; tBI += tg
    return [match, 0, 0, 0, qNI, qBI, tNI, tBI, strand,
            symOf[qEnsg], qSize, qStart, qEnd, tChrom, tSize, tStart, tEnd,
            len(sizes), ",".join(map(str, sizes)) + ",",
            ",".join(map(str, qStarts)) + ",", ",".join(map(str, tStarts)) + ","]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataDir"); ap.add_argument("chromSizes")
    ap.add_argument("-o", required=True)
    ap.add_argument("--faOut"); ap.add_argument("--twoBit")
    args = ap.parse_args()
    D = args.dataDir

    enst2ensg = {}
    for line in open(os.path.join(D, "maneMeta.tsv")):
        f = line.rstrip("\n").split("\t")
        enst2ensg[f[0]] = f[1]
        chromOf[f[1]] = f[5]; strandOf[f[1]] = f[6]; symOf[f[1]] = f[2]
    for line in open(args.chromSizes):
        c, s = line.split()[:2]; chromSizes[c] = int(s)
    global cdsG
    cdsG = buildCdsG(os.path.join(D, "maneSelect.gp"), enst2ensg)
    sys.stderr.write("cdsG for %d genes\n" % len(cdsG))

    if args.faOut:
        if not args.twoBit:
            sys.exit("--faOut requires --twoBit")
        writeCdsFasta(args.twoBit, args.faOut)
        sys.stderr.write("wrote query CDS FASTA to %s\n" % args.faOut)

    n = out = 0
    with open(args.o, "w") as fh:
        for line in open(os.path.join(D, "alnBlocks.tsv")):
            f = line.rstrip("\n").split("\t")
            a, b = f[0], f[1]
            if a not in cdsG or b not in cdsG: continue
            aStarts = [int(x) for x in f[7].split(",")]
            bStarts = [int(x) for x in f[8].split(",")]
            sizes   = [int(x) for x in f[9].split(",")]
            for (q, t, qs, ts) in ((a, b, aStarts, bStarts), (b, a, bStarts, aStarts)):
                psl = buildPsl(q, t, qs, ts, sizes)
                if psl:
                    fh.write("\t".join(map(str, psl)) + "\n"); out += 1
            n += 1
    sys.stderr.write("read %d pairs, wrote %d psl records\n" % (n, out))

if __name__ == "__main__":
    main()
