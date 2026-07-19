#!/usr/bin/env python3
"""clinvarMappedPfamAlign.py - reconstruct, for every Pfam domain instance in the
MANE Select proteome, the mapping from protein residue to Pfam HMM match-state
column (the shared coordinate used to line up equivalent positions across genes).

The UCSC ucscGenePfam track keeps only each domain's genomic footprint, not the
per-residue HMM column. We recover the column mapping directly: for every Pfam
family that hits two or more MANE proteins, we fetch the family model, realign the
matched subsequences to it with hmmalign, and read the #=GC RF reference line of
the resulting alignment. All instances of a family are aligned to the same model,
so match column k is the same position in the domain for every gene - that is the
equivalence a residue in gene A and a residue in gene B need in order to be called
"the same position in the domain".

Inputs:
  maneHits.domtbl   hmmsearch --domtblout, Pfam-A.hmm vs geneProt.faa (--cut_ga)
                    target=protein(ENSG), query=model; env coords in the protein
  geneProt.faa      one MANE protein per gene, header ">ENSG..."
  Pfam-A.hmm        hmmpressed + hmmfetch-indexed Pfam-A library

Outputs:
  alnCols.tsv   ensg  pfamAcc  pfamName  aaPos(1-based)  matchCol(1-based)
                one row per match-state residue of every instance
  famStats.tsv  pfamAcc  pfamName  modelLen  nInstances  nGenes

Usage: clinvarMappedPfamAlign.py <dataDir> --hmmDir DIR --hmmerBin DIR [-j N]
"""
import argparse, sys, os, subprocess, tempfile, multiprocessing as mp

HMM = None          # path to Pfam-A.hmm (indexed)
HMMER = None        # dir with hmmfetch / hmmalign
PROT = {}           # ensg -> protein sequence

def loadFasta(path):
    d, name, buf = {}, None, []
    for line in open(path):
        if line[0] == ">":
            if name: d[name] = "".join(buf)
            name = line[1:].split()[0]; buf = []
        else: buf.append(line.strip())
    if name: d[name] = "".join(buf)
    return d

def parseDomtbl(path):
    """Return {pfamName: (pfamAcc, modelLen, [(ensg, envFrom, envTo), ...])}."""
    fams = {}
    for line in open(path):
        if line.startswith("#"): continue
        f = line.split()
        if len(f) < 23: continue
        ensg = f[0]                    # target name  = protein
        pfamName = f[3]                # query name   = model
        pfamAcc = f[4]                 # query accession = PFxxxxx.n
        modelLen = int(f[5])           # qlen = model match-state count
        envFrom, envTo = int(f[19]), int(f[20])   # env coords in the protein
        rec = fams.setdefault(pfamName, [pfamAcc, modelLen, []])
        rec[2].append((ensg, envFrom, envTo))
    return fams

def parsePfamStockholm(text):
    """Parse hmmalign --outformat pfam output (one line per sequence).
    Returns (rfString, {seqId: alignedString})."""
    rf, seqs = [], {}
    for line in text.splitlines():
        if line.startswith("#=GC RF"):
            rf.append(line.split(None, 2)[2].rstrip())
        elif line.startswith("#") or line.startswith("//") or not line.strip():
            continue
        else:
            parts = line.split(None, 1)
            if len(parts) == 2:
                seqs.setdefault(parts[0], []).append(parts[1].rstrip())
    rfStr = "".join(rf)
    seqs = {k: "".join(v) for k, v in seqs.items()}
    return rfStr, seqs

def alignFamily(job):
    """Align one family's instances to its model; return alnCols rows."""
    pfamName, pfamAcc, modelLen, members = job
    # unique id per instance (a gene may carry the domain more than once)
    recs = []
    faLines = []
    for (ensg, envFrom, envTo) in members:
        seq = PROT.get(ensg)
        if not seq: continue
        sub = seq[envFrom - 1:envTo]
        if not sub: continue
        sid = "%s|%d" % (ensg, envFrom)          # env start makes it unique
        recs.append((sid, ensg, envFrom))
        faLines.append(">%s\n%s\n" % (sid, sub))
    if len(recs) < 2:
        return []
    with tempfile.TemporaryDirectory(dir="/dev/shm") as td:
        faPath = os.path.join(td, "seqs.fa")
        hmmPath = os.path.join(td, "model.hmm")
        open(faPath, "w").writelines(faLines)
        with open(hmmPath, "wb") as fh:
            subprocess.run([os.path.join(HMMER, "hmmfetch"), HMM, pfamName],
                           stdout=fh, stderr=subprocess.DEVNULL, check=True)
        p = subprocess.run(
            [os.path.join(HMMER, "hmmalign"), "--amino", "--outformat", "pfam",
             hmmPath, faPath],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, check=True)
        rfStr, seqs = parsePfamStockholm(p.stdout.decode())
    envOf = {sid: (ensg, envFrom) for (sid, ensg, envFrom) in recs}
    rows = []
    for sid, aln in seqs.items():
        if sid not in envOf: continue
        ensg, envFrom = envOf[sid]
        matchCol = 0
        resIdx = 0                                 # residues consumed in this instance
        for c, ch in enumerate(aln):
            isMatchCol = c < len(rfStr) and rfStr[c] != "." and rfStr[c] != "-"
            if isMatchCol:
                matchCol += 1
            if ch.isalpha():
                aaPos = envFrom + resIdx           # 1-based protein position
                resIdx += 1
                if isMatchCol and ch.isupper():    # match-state residue: shared column
                    rows.append((ensg, pfamAcc, pfamName, aaPos, matchCol))
    return rows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataDir")
    ap.add_argument("--hmmDir", required=True, help="dir with pressed/indexed Pfam-A.hmm")
    ap.add_argument("--hmmerBin", required=True, help="dir with hmmfetch/hmmalign")
    ap.add_argument("-j", type=int, default=20)
    args = ap.parse_args()
    D = args.dataDir

    global HMM, HMMER, PROT
    HMM = os.path.join(args.hmmDir, "Pfam-A.hmm")
    HMMER = args.hmmerBin
    PROT = loadFasta(os.path.join(D, "geneProt.faa"))

    fams = parseDomtbl(os.path.join(D, "pfam", "maneHits.domtbl"))
    jobs, stats = [], []
    for pfamName, (pfamAcc, modelLen, members) in fams.items():
        nGenes = len({m[0] for m in members})
        stats.append((pfamAcc, pfamName, modelLen, len(members), nGenes))
        if nGenes >= 2:                            # >=2 genes -> mappable
            jobs.append((pfamName, pfamAcc, modelLen, members))
    with open(os.path.join(D, "pfam", "famStats.tsv"), "w") as fh:
        for pfamAcc, pfamName, modelLen, nInst, nGenes in sorted(stats, key=lambda x: -x[3]):
            fh.write("%s\t%s\t%d\t%d\t%d\n" % (pfamAcc, pfamName, modelLen, nInst, nGenes))
    sys.stderr.write("families total %d, mappable (>=2 genes) %d; aligning on %d workers\n"
                     % (len(fams), len(jobs), args.j))

    nrows = 0
    with open(os.path.join(D, "pfam", "alnCols.tsv"), "w") as out, mp.Pool(args.j) as pool:
        for rows in pool.imap_unordered(alignFamily, jobs, chunksize=8):
            for r in rows:
                out.write("%s\t%s\t%s\t%d\t%d\n" % r)
            nrows += len(rows)
    sys.stderr.write("wrote %d residue-column rows\n" % nrows)

if __name__ == "__main__":
    main()
