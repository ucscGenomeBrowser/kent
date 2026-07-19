#!/usr/bin/env python3
"""clinvarMappedPfamAln.py - place each MANE Pfam domain instance on the genome as
a bed12 footprint (the evidence track for the Pfam projection).

This is the MANE-restricted analog of ucscGenePfam: for every domain hit we map the
matched protein residue range through the transcript's CDS to genomic blocks. Unlike
a paralog pairwise alignment, a domain instance is aligned to an HMM (not a genomic
sequence), so the evidence shown to the user is the domain footprint plus the HMM
column range it covers, not a base-level bigPsl.

Inputs (in dataDir):
  pfam/maneHits.domtbl  hmmsearch --domtblout (target=protein ENSG)
  maneMeta.tsv          enst ensg sym refTx refProt chrom strand geneType
  maneSelect.gp         genePred
Output (-o): unsorted bed12+ (see clinvarMappedPfamAln.as)

Usage: clinvarMappedPfamAln.py <dataDir> -o out.bed
"""
import argparse, sys, os, array

def buildCdsG(gpPath):
    out = {}
    for line in open(gpPath):
        f = line.rstrip("\n").split("\t")
        enst, chrom, strand = f[0], f[1], f[2]
        cdsStart, cdsEnd = int(f[5]), int(f[6])
        if cdsStart >= cdsEnd: continue
        exStarts = [int(x) for x in f[8].rstrip(",").split(",")]
        exEnds   = [int(x) for x in f[9].rstrip(",").split(",")]
        segs = []
        for s, e in zip(exStarts, exEnds):
            cs, ce = max(s, cdsStart), min(e, cdsEnd)
            if cs < ce: segs.append((cs, ce))
        a = array.array('i')
        if strand == "+":
            for (s, e) in segs: a.extend(range(s, e))
        else:
            for (s, e) in reversed(segs): a.extend(range(e - 1, s - 1, -1))
        out[enst] = a
    return out

def blocksFromCoords(coords):
    """genomic coords (any order) -> (start, end, blockSizes, blockStarts)."""
    cs = sorted(coords)
    start, end = cs[0], cs[-1] + 1
    sizes, starts = [], []
    i = 0
    while i < len(cs):
        j = i
        while j + 1 < len(cs) and cs[j + 1] == cs[j] + 1:
            j += 1
        starts.append(cs[i] - start); sizes.append(cs[j] - cs[i] + 1)
        i = j + 1
    return start, end, sizes, starts

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataDir")
    ap.add_argument("-o", required=True)
    args = ap.parse_args()
    D = args.dataDir

    strandOf, symOf, chromOf, enst2ensg, ensg2enst = {}, {}, {}, {}, {}
    for line in open(os.path.join(D, "maneMeta.tsv")):
        f = line.rstrip("\n").split("\t")
        enst2ensg[f[0]] = f[1]; ensg2enst[f[1]] = f[0]
        strandOf[f[1]] = f[6]; symOf[f[1]] = f[2]; chromOf[f[1]] = f[5]
    cdsGe = buildCdsG(os.path.join(D, "maneSelect.gp"))     # keyed by ENST
    cdsG = {enst2ensg[k]: v for k, v in cdsGe.items() if k in enst2ensg}

    nout = nskip = 0
    with open(args.o, "w") as out:
        for line in open(os.path.join(D, "pfam", "maneHits.domtbl")):
            if line.startswith("#"): continue
            f = line.split()
            if len(f) < 23: continue
            ensg, pfamName, pfamAcc = f[0], f[3], f[4]
            score = float(f[13]); iEval = f[12]
            hmmFrom, hmmTo = int(f[15]), int(f[16])
            envFrom, envTo = int(f[19]), int(f[20])
            g = cdsG.get(ensg)
            if g is None: nskip += 1; continue
            base0, base1 = 3 * (envFrom - 1), 3 * envTo      # CDS nt half-open
            if base1 > len(g): base1 = len(g)
            if base0 >= base1: nskip += 1; continue
            coords = [g[i] for i in range(base0, base1)]
            start, end, sizes, starts = blocksFromCoords(coords)
            sc = min(1000, int(score * 2))
            row = [chromOf[ensg], start, end, pfamName, sc, strandOf[ensg],
                   start, end, "20,90,180", len(sizes),
                   ",".join(map(str, sizes)) + ",", ",".join(map(str, starts)) + ",",
                   symOf.get(ensg, ensg), pfamName, pfamAcc,
                   envFrom, envTo, hmmFrom, hmmTo, iEval]
            out.write("\t".join(map(str, row)) + "\n")
            nout += 1
    sys.stderr.write("wrote %d domain footprints, skipped %d (no CDS)\n" % (nout, nskip))

if __name__ == "__main__":
    main()
