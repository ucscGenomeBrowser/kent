#!/usr/bin/env python3
"""clinvarMappedParalogProject.py - project ClinVar coding variants onto their
paralogs' equivalent residues.

For each ClinVar variant assigned to a MANE codon (clinvarCodons.tsv) and each
annotated paralog of the source gene (alnBlocks.tsv), we look up the aligned
residue in the paralog, map that residue back to the paralog's genomic codon,
and emit one bed12+ feature on the paralog. Everything needed to reproduce the
mapping - source gene, source variant, ClinVar link, residue conservation,
identities - travels on the feature.

Inputs:
  clinvarCodons.tsv  ensg enst sym aaPos refRes chrom gPos vcv variantId
                     clinSignCode clinSign stars molConseq protChange
  alnBlocks.tsv      geneA geneB lenA lenB nIdent matchCols blockCount
                     aStarts bStarts blockSizes   (protein 0-based)
  pairs.ge20.tsv     geneA geneB symA symB pidAB pidBA maxPid
  maneMeta.tsv       enst ensg sym refTx refProt chrom strand geneType
  maneSelect.gp      genePred
  geneProt.faa       protein per gene (">ENSG...")
Output (-o): unsorted bed12+14 (see clinvarMappedParalog.as)

Usage: clinvarMappedParalogProject.py <dataDir> -o out.bed [-j N]
"""
import argparse, sys, os, array, multiprocessing as mp
from Bio.Align import substitution_matrices

BLOSUM = substitution_matrices.load("BLOSUM62")

# colors by clinical significance, matching the UCSC ClinVar track (clinVarToBed)
RGB = {
    "PG": "210,0,0",     "LP": "210,0,0",     # pathogenic / likely pathogenic
    "BN": "0,210,0",     "LB": "0,210,0",     # benign / likely benign
    "VUS": "0,0,128",                          # uncertain
    "CF": "137,121,212",                       # conflicting
    "OT": "128,128,128", "RF": "128,128,128",  # other / risk factor
}

# module globals shared with workers via fork
prot = {}          # ensg -> protein sequence
cdsG = {}          # ensg -> array('i') genomic coords in transcription order
strandOf = {}      # ensg -> '+'/'-'
symOf = {}         # ensg -> symbol
partners = {}      # ensg -> list of (partnerEnsg, srcStarts, srcSizes, tgtStarts, alnPid)
bioPid = {}        # (a,b) sorted -> max BioMart percent id

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
    """ensg -> array of genomic coords for every CDS base in transcription order."""
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
    """Keep bigBed string fields under the 255-char limit (long polyQ ins/dels)."""
    return s if len(s) <= n else s[:n - 3] + "..."

def resClass(a, b):
    if a == b: return "identical"
    try:
        return "similar" if BLOSUM[a, b] > 0 else "different"
    except (KeyError, IndexError):
        return "different"

def codonBlocks(coords):
    """3 genomic coords (any order) -> (start, end, blockSizes, blockStarts)."""
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

def projectChunk(lines):
    outRows = []
    for line in lines:
        f = line.rstrip("\n").split("\t")
        srcEnsg, srcSym, aaPos, refRes = f[0], f[2], int(f[3]), f[4]
        vcv, clinCode, clinSign, stars, molConseq, protChange = \
            f[7], f[9], f[10], f[11], f[12], f[13]
        srcChrom, srcGpos = f[5], f[6]
        plist = partners.get(srcEnsg)
        if not plist: continue
        aa0 = aaPos - 1
        rgb = RGB.get(clinCode, "0,0,0")
        try: starsN = int(stars)
        except ValueError: starsN = 0
        score = min(1000, starsN * 250)
        sourceLocus = "%s:%s" % (srcChrom, int(srcGpos) + 1)
        for (pEnsg, srcStarts, srcSizes, tgtStarts, alnPid) in plist:
            # find block containing aa0 in the source coordinate
            tgt0 = None
            for bi in range(len(srcStarts)):
                s = srcStarts[bi]
                if s <= aa0 < s + srcSizes[bi]:
                    tgt0 = tgtStarts[bi] + (aa0 - s); break
            if tgt0 is None: continue                  # aligns to a gap
            pprot = prot.get(pEnsg, "")
            if tgt0 >= len(pprot): continue
            pRes = pprot[tgt0]
            g = cdsG.get(pEnsg)
            base = 3 * tgt0
            if g is None or base + 3 > len(g): continue
            coords = [g[base], g[base + 1], g[base + 2]]
            start, end, sizes, starts = codonBlocks(coords)
            pChrom = chromOf[pEnsg]
            pChange = protChange or ("p.%s%d" % (refRes, aaPos))
            name = shorten("%s %s" % (srcSym, pChange), 120)
            rc = resClass(refRes, pRes)
            bpid = bioPid.get((srcEnsg, pEnsg) if srcEnsg < pEnsg else (pEnsg, srcEnsg), 0.0)
            row = [pChrom, start, end, name, score, strandOf[pEnsg],
                   start, end, rgb, len(sizes),
                   ",".join(map(str, sizes)) + ",", ",".join(map(str, starts)) + ",",
                   srcSym, shorten(pChange, 200), aaPos, refRes, pRes, rc,
                   clinSign, clinCode, starsN, molConseq,
                   "%.1f" % bpid, "%.1f" % alnPid, vcv, sourceLocus]
            outRows.append("\t".join(map(str, row)))
    return outRows

chromOf = {}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataDir")
    ap.add_argument("-o", required=True); ap.add_argument("-j", type=int, default=32)
    args = ap.parse_args()
    D = args.dataDir

    global prot, cdsG, strandOf, symOf, partners, bioPid, chromOf
    prot = loadFasta(os.path.join(D, "geneProt.faa"))
    cdsG = buildCdsG(os.path.join(D, "maneSelect.gp"))
    for line in open(os.path.join(D, "maneMeta.tsv")):
        f = line.rstrip("\n").split("\t")
        # map by ENSG using this gene's transcript; enst is f[0]
        strandOf[f[1]] = f[6]; symOf[f[1]] = f[2]; chromOf[f[1]] = f[5]
    # cdsG is keyed by ENST; re-key to ENSG (one transcript per gene)
    enst2ensg = {}
    for line in open(os.path.join(D, "maneMeta.tsv")):
        f = line.rstrip("\n").split("\t"); enst2ensg[f[0]] = f[1]
    cdsG = {enst2ensg[k]: v for k, v in cdsG.items() if k in enst2ensg}
    prot = prot   # already ENSG-keyed

    for line in open(os.path.join(D, "pairs.ge20.tsv")):
        f = line.rstrip("\n").split("\t")
        a, b = f[0], f[1]
        bioPid[(a, b) if a < b else (b, a)] = float(f[6])

    # build the partner index from alnBlocks (both orientations)
    n = 0
    for line in open(os.path.join(D, "alnBlocks.tsv")):
        f = line.rstrip("\n").split("\t")
        a, b = f[0], f[1]
        nIdent, matchCols = int(f[4]), int(f[5])
        pid = 100.0 * nIdent / matchCols if matchCols else 0.0
        aStarts = [int(x) for x in f[7].split(",")]
        bStarts = [int(x) for x in f[8].split(",")]
        sizes   = [int(x) for x in f[9].split(",")]
        partners.setdefault(a, []).append((b, aStarts, sizes, bStarts, pid))
        partners.setdefault(b, []).append((a, bStarts, sizes, aStarts, pid))
        n += 1
    sys.stderr.write("indexed %d pairs, %d genes with partners, %d proteins\n"
                     % (n, len(partners), len(prot)))

    lines = open(os.path.join(D, "clinvarCodons.tsv")).readlines()
    sys.stderr.write("projecting %d variant-codon rows on %d workers\n"
                     % (len(lines), args.j))
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
