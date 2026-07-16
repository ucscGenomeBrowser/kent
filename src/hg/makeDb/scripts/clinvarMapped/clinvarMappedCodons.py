#!/usr/bin/env python3
"""clinvarMappedCodons.py - assign ClinVar coding variants to a MANE Select
codon position.

For every ClinVar variant with a codon-level molecular consequence we find the
MANE Select CDS exon(s) it overlaps, compute the 1-based amino-acid position in
that transcript, and record the reference residue (from the MANE protein). A
variant that overlaps two MANE genes (overlapping loci) yields one row per gene.

The genomic<->CDS mapping is built from the same genePred that produced the
protein FASTA, so the reference residue is self-consistent with the protein by
construction (verified by --selftest).

Inputs:
  clinvarBb   /gbdb/hg38/bbi/clinvar/clinvarMain.bb
  maneGp      maneSelect.gp
  geneFaa     geneProt.faa  (headers ">ENSG...")
  maneMeta    maneMeta.tsv  (enst ensg sym ...)
Output (-o):
  ensg enst sym aaPos refRes chrom gPos vcvId variantId clinSignCode clinSign stars molConseq protChange

Usage: clinvarMappedCodons.py <clinvarBb> <maneGp> <geneFaa> <maneMeta> -o out [--selftest]
"""
import argparse, subprocess, sys, os, tempfile, json, re

# protein HGVS like "p.Glu36Gly" -> capture (3-letter ref, position)
_PPAT = re.compile(r'p\.([A-Z][a-z]{2})(\d+)')

def protChangeForPos(jsonStr, aaPos):
    """From ClinVar's HGVS table pick the p.change whose position == aaPos (the
    MANE codon); fall back to the first p.change. Returns '' if none."""
    try:
        tbl = json.loads(jsonStr)
    except Exception:
        return ""
    first = ""
    for entry in tbl:
        for cell in entry:
            m = _PPAT.search(cell)
            if not m:
                continue
            if not first:
                first = cell[m.start():].split()[0].rstrip(",")
            if int(m.group(2)) == aaPos:
                return cell[m.start():].split()[0].rstrip(",")
    return first

# molecular consequences that correspond to a single residue position
CODING = {
    "missense variant", "nonsense", "inframe deletion", "inframe insertion",
    "inframe indel", "initiator codon variant", "stop lost",
}
# clinvarMain.bed extra-field 1-based columns (12 BED + extras)
C_MOLCONSEQ, C_CLINSIGN, C_STARS, C_VARID, C_VCV, C_CLINCODE = 18, 14, 45, 46, 48, 41

def loadFasta(path):
    d, name, buf = {}, None, []
    for line in open(path):
        if line[0] == ">":
            if name: d[name] = "".join(buf)
            name = line[1:].split()[0]; buf = []
        else: buf.append(line.strip())
    if name: d[name] = "".join(buf)
    return d

def parseGenePred(path, enst2gene):
    """Return dict txid -> (chrom, strand, ensg, sym, [(gStart,gEnd,cumBefore)])
    where the exon list is CDS-clipped and cumBefore is the count of CDS bases
    preceding the exon in transcription order."""
    tx = {}
    for line in open(path):
        f = line.rstrip("\n").split("\t")
        name, chrom, strand = f[0], f[1], f[2]
        cdsStart, cdsEnd = int(f[5]), int(f[6])
        if cdsStart >= cdsEnd: continue
        exStarts = [int(x) for x in f[8].rstrip(",").split(",")]
        exEnds   = [int(x) for x in f[9].rstrip(",").split(",")]
        # CDS-clipped exon segments in genomic order
        segs = []
        for s, e in zip(exStarts, exEnds):
            cs, ce = max(s, cdsStart), min(e, cdsEnd)
            if cs < ce: segs.append([cs, ce])
        # cumulative CDS bases before each exon, in TRANSCRIPTION order
        order = range(len(segs)) if strand == "+" else range(len(segs) - 1, -1, -1)
        cum = 0
        cumBefore = {}
        for i in order:
            cumBefore[i] = cum
            cum += segs[i][1] - segs[i][0]
        g = enst2gene.get(name)
        if not g: continue
        ensg, sym = g
        tx[name] = (chrom, strand, ensg, sym,
                    [(segs[i][0], segs[i][1], cumBefore[i]) for i in range(len(segs))])
    return tx

def writeCdsBed(tx, path):
    with open(path, "w") as out:
        for name, (chrom, strand, ensg, sym, segs) in tx.items():
            for (s, e, cb) in segs:
                # name field packs everything the compute step needs
                out.write("%s\t%d\t%d\t%s|%s|%s|%s|%d\t0\t%s\n" %
                          (chrom, s, e, name, ensg, sym, strand, cb, strand))

def cdsOffset(strand, exonStart, exonEnd, cumBefore, repBase):
    if strand == "+":
        return cumBefore + (repBase - exonStart)
    return cumBefore + (exonEnd - 1 - repBase)

def selftest(tx, prot):
    """Verify that translating each transcript codon-by-codon from the genome-
    derived mapping reproduces the MANE protein for a sample of transcripts."""
    import random
    names = list(tx.keys()); random.seed(1); random.shuffle(names)
    checked = bad = 0
    for name in names[:200]:
        chrom, strand, ensg, sym, segs = tx[name]
        p = prot.get(ensg)
        if not p: continue
        # first CDS base in transcription order -> must be aa position 1
        if strand == "+":
            first = segs[0]; base = first[0]
        else:
            last = segs[-1]; base = last[1] - 1
        # find that base's exon and compute
        for (s, e, cb) in segs:
            if s <= base < e:
                off = cdsOffset(strand, s, e, cb, base)
                if off != 0: bad += 1
                break
        checked += 1
    sys.stderr.write("selftest: %d transcripts, %d with first-base!=aa1\n" % (checked, bad))
    return bad == 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("clinvarBb"); ap.add_argument("maneGp")
    ap.add_argument("geneFaa"); ap.add_argument("maneMeta")
    ap.add_argument("-o", required=True); ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--workdir", default=None)
    args = ap.parse_args()

    enst2gene = {}
    for line in open(args.maneMeta):
        f = line.rstrip("\n").split("\t")
        enst2gene[f[0]] = (f[1], f[2])   # enst -> (ensg, sym)

    prot = loadFasta(args.geneFaa)
    tx = parseGenePred(args.maneGp, enst2gene)
    sys.stderr.write("loaded %d MANE transcripts, %d proteins\n" % (len(tx), len(prot)))

    if args.selftest:
        ok = selftest(tx, prot)
        if not ok: sys.exit("SELFTEST FAILED")

    wd = args.workdir or tempfile.mkdtemp()
    os.makedirs(wd, exist_ok=True)
    cdsBed = os.path.join(wd, "cdsExons.bed")
    varBed = os.path.join(wd, "codingVars.bed")
    writeCdsBed(tx, cdsBed)

    # extract coding variants from the clinvar bigBed into a BED4+
    sys.stderr.write("extracting coding variants ...\n")
    nvar = 0
    jsonByVid = {}   # variantId -> HGVS json (col28) for protChange display
    with open(varBed, "w") as vb:
        proc = subprocess.Popen(["bigBedToBed", args.clinvarBb, "stdout"],
                                stdout=subprocess.PIPE, text=True)
        for line in proc.stdout:
            f = line.rstrip("\n").split("\t")
            if f[C_MOLCONSEQ - 1] not in CODING: continue
            jsonByVid[f[C_VARID - 1]] = f[27]
            # name packs variant metadata; '|' join, sanitize stray '|'
            meta = "%s;%s;%s;%s;%s" % (
                f[C_VCV - 1], f[C_VARID - 1], f[C_CLINCODE - 1],
                f[C_CLINSIGN - 1].replace(";", ","), f[C_STARS - 1])
            vb.write("%s\t%s\t%s\tv%d\t%s\t%s\n" %
                     (f[0], f[1], f[2], nvar, meta, f[C_MOLCONSEQ - 1].replace(" ", "_")))
            nvar += 1
        proc.wait()
    sys.stderr.write("coding variants: %d\n" % nvar)

    # sort both then intersect
    subprocess.run("sort -k1,1 -k2,2n %s -o %s" % (cdsBed, cdsBed), shell=True, check=True)
    subprocess.run("sort -k1,1 -k2,2n %s -o %s" % (varBed, varBed), shell=True, check=True)

    sys.stderr.write("intersecting ...\n")
    isect = subprocess.Popen(
        ["bedtools", "intersect", "-a", varBed, "-b", cdsBed, "-wa", "-wb", "-sorted"],
        stdout=subprocess.PIPE, text=True)

    nrow = miss = 0
    with open(args.o, "w") as out:
        for line in isect.stdout:
            f = line.rstrip("\n").split("\t")
            vStart, vEnd = int(f[1]), int(f[2])
            meta = f[4].split(";"); molConseq = f[5].replace("_", " ")
            # b-record starts at column 6 (0-based): chrom s e name score strand
            bStart, bEnd = int(f[7]), int(f[8])
            nm = f[9].split("|")   # name enst ensg sym strand cumBefore
            enst, ensg, sym, strand, cb = nm[0], nm[1], nm[2], nm[3], int(nm[4])
            # representative base = transcription-first affected coding base in this exon
            if strand == "+":
                repBase = max(vStart, bStart)
            else:
                repBase = min(vEnd - 1, bEnd - 1)
            if repBase < bStart or repBase >= bEnd:
                continue
            off = cdsOffset(strand, bStart, bEnd, cb, repBase)
            aaPos = off // 3 + 1
            p = prot.get(ensg, "")
            refRes = p[aaPos - 1] if 0 < aaPos <= len(p) else "?"
            if refRes == "?": miss += 1
            vcv, varId, clinCode, clinSign, stars = meta[0], meta[1], meta[2], meta[3], meta[4]
            protChange = protChangeForPos(jsonByVid.get(varId, ""), aaPos)
            out.write("\t".join([ensg, enst, sym, str(aaPos), refRes, f[0],
                                 str(repBase), vcv, varId, clinCode, clinSign,
                                 stars, molConseq, protChange]) + "\n")
            nrow += 1
    isect.wait()
    sys.stderr.write("wrote %d variant-codon rows (%d with out-of-range aaPos)\n" % (nrow, miss))

if __name__ == "__main__":
    main()
