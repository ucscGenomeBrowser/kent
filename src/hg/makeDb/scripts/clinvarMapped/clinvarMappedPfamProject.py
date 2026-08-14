#!/usr/bin/env python3
"""clinvarMappedPfamProject.py - project ClinVar coding variants onto every other
gene that shares the same Pfam domain, at the equivalent HMM match-state column.

For each ClinVar variant already assigned to a MANE codon (clinvarCodons.tsv) we
look up which Pfam domain column the source residue occupies (alnCols.tsv). Every
other instance of that domain that has a residue in the same column is an
equivalent position; we map that residue back to its gene's genomic codon and emit
one bed12+ feature there. The Pfam HMM is the shared coordinate hub, so this links
all genes carrying a domain, not only annotated paralog pairs.

Inputs (in dataDir):
  clinvarCodons.tsv  ensg enst sym aaPos refRes chrom gPos vcv variantId
                     clinSignCode clinSign stars molConseq protChange
  pfam/alnCols.tsv   ensg pfamAcc pfamName aaPos matchCol
  maneMeta.tsv       enst ensg sym refTx refProt chrom strand geneType
  maneSelect.gp      genePred
  geneProt.faa       protein per gene (">ENSG...")
Output (-o): unsorted bed12+ (see clinvarMappedPfam.as)

Usage: clinvarMappedPfamProject.py <dataDir> -o out.bed [--maxFanout N] [-j N]
"""
import argparse, sys, os, array, multiprocessing as mp
from Bio.Align import substitution_matrices

BLOSUM = substitution_matrices.load("BLOSUM62")

RGB = {
    "PG": "210,0,0",     "LP": "210,0,0",
    "BN": "0,210,0",     "LB": "0,210,0",
    "VUS": "0,0,128",
    "CF": "137,121,212",
    "OT": "128,128,128", "RF": "128,128,128",
}

prot = {}          # ensg -> protein sequence
cdsG = {}          # ensg -> array('i') genomic coords in transcription order
strandOf = {}
symOf = {}
chromOf = {}
byRes = {}         # (ensg, aaPos) -> list of (pfamAcc, pfamName, col)
col2inst = {}      # (pfamAcc, col) -> list of (ensg, aaPos)
maxFanout = 0

def loadFasta(path):
    d, name, buf = {}, None, []
    for line in open(path):
        if line[0] == ">":
            if name: d[name] = "".join(buf)
            name = line[1:].split()[0]; buf = []
        else: buf.append(line.strip())
    if name: d[name] = "".join(buf)
    return d

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

def shorten(s, n):
    return s if len(s) <= n else s[:n - 3] + "..."

def resClass(a, b):
    if a == b: return "identical"
    try:
        return "similar" if BLOSUM[a, b] > 0 else "different"
    except (KeyError, IndexError):
        return "different"

def codonBlocks(coords):
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

RANK = {"identical": 0, "similar": 1, "different": 2}

def projectChunk(lines):
    outRows = []
    for line in lines:
        f = line.rstrip("\n").split("\t")
        srcEnsg, srcSym, aaPos, refRes = f[0], f[2], int(f[3]), f[4]
        vcv, clinCode, clinSign, stars, molConseq, protChange = \
            f[7], f[9], f[10], f[11], f[12], f[13]
        srcChrom, srcGpos = f[5], f[6]
        cols = byRes.get((srcEnsg, aaPos))
        if not cols: continue                          # source residue not in any domain
        # gather unique candidate targets across all domains/columns of this residue
        cand = {}                                      # (tgtEnsg, tgtAaPos) -> (pfamAcc, pfamName, col)
        for (pfamAcc, pfamName, col) in cols:
            for (tgtEnsg, tgtAaPos) in col2inst.get((pfamAcc, col), ()):
                if tgtEnsg == srcEnsg: continue        # skip self
                cand.setdefault((tgtEnsg, tgtAaPos), (pfamAcc, pfamName, col))
        if not cand: continue
        # rank each candidate by residue conservation vs the source residue
        ranked = []
        for (tgtEnsg, tgtAaPos), (pfamAcc, pfamName, col) in cand.items():
            pprot = prot.get(tgtEnsg, "")
            if tgtAaPos > len(pprot): continue
            tgtRes = pprot[tgtAaPos - 1]
            rc = resClass(refRes, tgtRes)
            ranked.append((RANK[rc], tgtEnsg, tgtAaPos, tgtRes, rc, pfamAcc, pfamName, col))
        # keep the most conserved first (identical > similar > different), deterministic
        ranked.sort(key=lambda x: (x[0], x[1], x[2]))
        if maxFanout:
            ranked = ranked[:maxFanout]

        rgb = RGB.get(clinCode, "0,0,0")
        try: starsN = int(stars)
        except ValueError: starsN = 0
        score = min(1000, starsN * 250)
        sourceLocus = "%s:%s" % (srcChrom, int(srcGpos) + 1)
        pChange = protChange or ("p.%s%d" % (refRes, aaPos))
        name = shorten("%s %s" % (srcSym, pChange), 120)
        for (_rank, tgtEnsg, tgtAaPos, tgtRes, rc, pfamAcc, pfamName, col) in ranked:
            g = cdsG.get(tgtEnsg)
            base = 3 * (tgtAaPos - 1)
            if g is None or base + 3 > len(g): continue
            coords = [g[base], g[base + 1], g[base + 2]]
            start, end, sizes, starts = codonBlocks(coords)
            tgtSym = symOf.get(tgtEnsg, tgtEnsg)
            row = [chromOf[tgtEnsg], start, end, name, score, strandOf[tgtEnsg],
                   start, end, rgb, len(sizes),
                   ",".join(map(str, sizes)) + ",", ",".join(map(str, starts)) + ",",
                   srcSym, tgtSym, shorten(pChange, 200), aaPos, refRes, tgtRes, rc,
                   clinSign, clinCode, starsN, molConseq,
                   pfamName, pfamAcc, col, vcv, sourceLocus]
            outRows.append("\t".join(map(str, row)))
    return outRows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataDir")
    ap.add_argument("-o", required=True)
    ap.add_argument("--maxFanout", type=int, default=25,
                    help="cap targets per source variant, keeping the most conserved (0 = unlimited)")
    ap.add_argument("-j", type=int, default=32)
    args = ap.parse_args()
    D = args.dataDir

    global prot, cdsG, strandOf, symOf, chromOf, byRes, col2inst, maxFanout
    maxFanout = args.maxFanout
    prot = loadFasta(os.path.join(D, "geneProt.faa"))
    cdsG = buildCdsG(os.path.join(D, "maneSelect.gp"))
    enst2ensg = {}
    for line in open(os.path.join(D, "maneMeta.tsv")):
        f = line.rstrip("\n").split("\t")
        enst2ensg[f[0]] = f[1]
        strandOf[f[1]] = f[6]; symOf[f[1]] = f[2]; chromOf[f[1]] = f[5]
    cdsG = {enst2ensg[k]: v for k, v in cdsG.items() if k in enst2ensg}

    nrow = 0
    for line in open(os.path.join(D, "pfam", "alnCols.tsv")):
        f = line.rstrip("\n").split("\t")
        ensg, pfamAcc, pfamName, aaPos, col = f[0], f[1], f[2], int(f[3]), int(f[4])
        byRes.setdefault((ensg, aaPos), []).append((pfamAcc, pfamName, col))
        col2inst.setdefault((pfamAcc, col), []).append((ensg, aaPos))
        nrow += 1
    sys.stderr.write("indexed %d residue-column rows, %d residues, %d columns\n"
                     % (nrow, len(byRes), len(col2inst)))

    lines = open(os.path.join(D, "clinvarCodons.tsv")).readlines()
    sys.stderr.write("projecting %d variant-codon rows on %d workers (maxFanout=%d)\n"
                     % (len(lines), args.j, maxFanout))
    chunk = 20000
    chunks = [lines[i:i + chunk] for i in range(0, len(lines), chunk)]
    nout = 0
    with open(args.o, "w") as out, mp.Pool(args.j) as pool:
        for rows in pool.imap_unordered(projectChunk, chunks):
            for r in rows: out.write(r + "\n")
            nout += len(rows)
    sys.stderr.write("wrote %d projected features\n" % nout)

if __name__ == "__main__":
    main()
