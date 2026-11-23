# /// script
# dependencies = ["pandas", "xlrd", "openpyxl"]
# ///
"""Convert the cancerhotspots.org hotspot tables (v2: Chang et al. 2017, v3: the
164 new hotspots of Bandlamudi et al. 2026) to a bed12+ file for the
cancerHotspots bigBed track.

The tables give hotspots as gene + amino acid position, so each one is mapped
to hg38 through an Ensembl 115 transcript: the transcript the v2 MAF used for
the gene (Ensembl 75), if it still exists and has the right reference amino
acid, otherwise the Ensembl canonical transcript, otherwise any protein-coding
transcript of the gene with the right reference amino acid. For v2 hotspots,
the mapped codon is checked against the hg19 positions of the hotspot
mutations in the table, lifted to hg38. Splice-site hotspots (X<pos>_splice)
have no codon; they are placed on the lifted hg19 mutation positions.

Usage: cancerHotspotsToBed.py hotspots_v2.xls hotspots_v3.xlsx mafGeneTx.tsv \
           ensembl.gff3.gz hg38.2bit hg19ToHg38.over.chain.gz outBed
Writes a summary to stderr.
"""
import sys, gzip, re, subprocess, tempfile, os, warnings
from collections import defaultdict, Counter
import pandas as pd

warnings.filterwarnings("ignore")

# tissue names of v2 and v3 tables -> one label. v3 uses other spellings.
TISSUES = {
    "adrenalgland": "Adrenal gland", "adrenal_gland": "Adrenal gland",
    "ampullaofvater": "Ampulla of Vater", "ampulla_of_vater": "Ampulla of Vater",
    "biliarytract": "Biliary tract", "biliary_tract": "Biliary tract",
    "bladder": "Bladder", "blood": "Blood", "bone": "Bone", "bowel": "Bowel",
    "breast": "Breast", "cervix": "Cervix", "cnsbrain": "CNS/Brain", "brain": "CNS/Brain",
    "esophagusstomach": "Esophagus/Stomach", "stomach": "Esophagus/Stomach",
    "eye": "Eye", "headandneck": "Head and neck", "head_neck": "Head and neck",
    "kidney": "Kidney", "liver": "Liver", "lung": "Lung", "lymph": "Lymphoid",
    "ovaryfallopiantube": "Ovary/Fallopian tube", "ovary": "Ovary/Fallopian tube",
    "pancreas": "Pancreas", "penis": "Penis", "peritoneum": "Peritoneum",
    "pleura": "Pleura", "prostate": "Prostate", "skin": "Skin",
    "softtissue": "Soft tissue", "soft_tissue": "Soft tissue", "testis": "Testis",
    "thymus": "Thymus", "thyroid": "Thyroid", "unk": "Unknown", "uterus": "Uterus",
    "vulvavagina": "Vulva/Vagina", "vulva": "Vulva/Vagina",
}

# 2017 gene symbols whose Ensembl gene ID changed since, old symbol -> Ensembl 115 symbol
ALIASES = {
    "HIST1H3B": "H3C2", "HIST1H3C": "H3C3",
}

COLORS = {"single residue": "216,38,38", "in-frame indel": "0,0,255",
          "splice site": "0,107,27"}
NEWCOLOR = "189,0,157"   # v3 hotspots, all single residue

CODON = {}
bases = "TCAG"
aas = "FFLLSSSSYY**CC*WLLLLPPPPHHQQRRRRIIIMTTTTNNKKSSRRVVVVAAAADDEEGGGG"
i = 0
for a in bases:
    for b in bases:
        for c in bases:
            CODON[a + b + c] = aas[i]
            i += 1

warnCounts = Counter()

def warn(msg):
    warnCounts[msg.split(":")[0]] += 1
    print("warning: " + msg, file=sys.stderr)

def revComp(s):
    return s[::-1].translate(str.maketrans("ACGTNacgtn", "TGCANtgcan"))

def parseGff(fname):
    " return gene symbol -> list of tx, tx -> dict(chrom, strand, cds, canon, gene) "
    genes = {}          # ENSG -> symbol
    txs = {}
    with gzip.open(fname, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue
            row = line.rstrip("\n").split("\t")
            if len(row) < 9:
                continue
            ftype = row[2]
            attrs = dict(x.split("=", 1) for x in row[8].split(";") if "=" in x)
            fid = attrs.get("ID", "")
            if fid.startswith("gene:"):
                genes[fid.split(":")[1]] = attrs.get("Name", "")
            elif fid.startswith("transcript:"):
                # mRNA, but also NMD and other transcript types that have a CDS
                tid = attrs["ID"].split(":")[1]
                txs[tid] = dict(chrom="chr" + row[0] if row[0] != "MT" else "chrM",
                                strand=row[6], gene=attrs["Parent"].split(":")[1],
                                canon="Ensembl_canonical" in attrs.get("tag", ""),
                                cds=[], phase=0)
            elif ftype == "CDS":
                tid = attrs["Parent"].split(":")[1]
                txs[tid]["cds"].append((int(row[3]) - 1, int(row[4]), int(row[7]) if row[7] != "." else 0))
    symToTx = defaultdict(list)
    for tid, t in txs.items():
        if not t["cds"]:
            continue
        t["cds"].sort(reverse=(t["strand"] == "-"))
        t["phase"] = t["cds"][0][2]
        t["sym"] = genes.get(t["gene"], "")
        symToTx[t["sym"]].append(tid)
        symToTx[t["gene"]].append(tid)   # also by ENSG
    return symToTx, txs

def loadCdsSeqs(txs, tids, twoBit):
    " fill in t['seq'] (CDS, coding strand) and t['prot'] for the given transcripts "
    with tempfile.TemporaryDirectory() as tmp:
        bedName = os.path.join(tmp, "cds.bed")
        with open(bedName, "w") as bed:
            for tid in tids:
                t = txs[tid]
                for i, (s, e, ph) in enumerate(t["cds"]):
                    bed.write("%s\t%d\t%d\t%s.%d\n" % (t["chrom"], s, e, tid, i))
        faName = os.path.join(tmp, "cds.fa")
        subprocess.run(["twoBitToFa", "-bed=" + bedName, twoBit, faName], check=True)
        parts = {}
        name = None
        for line in open(faName):
            if line.startswith(">"):
                name = line[1:].strip()
                parts[name] = []
            else:
                parts[name].append(line.strip().upper())
    for tid in tids:
        t = txs[tid]
        seq = []
        for i in range(len(t["cds"])):
            s = "".join(parts[tid + "." + str(i)])
            seq.append(revComp(s) if t["strand"] == "-" else s)
        seq = "".join(seq)[t["phase"]:]
        t["seq"] = seq
        t["prot"] = "".join(CODON.get(seq[i:i + 3], "X") for i in range(0, len(seq) - 2, 3))

def cdsToGenome(t, cdsStart, cdsEnd):
    " cds nucleotide range (0-based, after phase offset) -> list of genomic (start, end) "
    blocks = []
    pos = -t["phase"]   # cds coordinate of start of current exon piece
    for s, e, ph in t["cds"]:
        l = e - s
        a, b = max(cdsStart, pos), min(cdsEnd, pos + l)
        if a < b:
            if t["strand"] == "+":
                blocks.append((s + (a - pos), s + (b - pos)))
            else:
                blocks.append((e - (b - pos), e - (a - pos)))
        pos += l
    return sorted(blocks)

def refOk(t, checks):
    " checks: list of (aaPos 1-based, aa letter). True if all match "
    p = t.get("prot", "")
    for pos, aa in checks:
        if pos < 1 or pos > len(p) or p[pos - 1] != aa:
            return False
    return True

def candidates(sym, maf, symToTx, txs):
    " all Ensembl 115 transcripts of a gene: by symbol, by the MAF's ENSG and the MAF's transcript "
    cands = set(symToTx.get(sym, []))
    if sym in maf:
        mafTid, gid = maf[sym]
        cands.update(symToTx.get(gid, []))
        if mafTid in txs:
            cands.add(mafTid)
    if sym in ALIASES:
        cands.update(symToTx.get(ALIASES[sym], []))
    return sorted(cands)

INDELPAD = 6   # indel positions in the table can be a few bp left of the residue range

def liftScore(t, h, lift, delta=0):
    " number of mutations whose lifted position is in the codon(s) of the hotspot, shifted by delta residues "
    blocks = cdsToGenome(t, (h["aaStart"] + delta - 1) * 3, (h["aaEnd"] + delta) * 3)
    if not blocks:
        return 0
    pad = INDELPAD if h["type"] == "in-frame indel" else 0
    lo, hi = blocks[0][0] - pad, blocks[-1][1] + pad
    return sum(n for c, p, n in lift if c == t["chrom"] and lo <= p < hi)

def genomeToResidue(t, chrom, gpos):
    " genomic 0-based position -> 1-based residue number on the transcript, or None "
    if chrom != t["chrom"]:
        return None
    pos = -t["phase"]
    for s, e, ph in t["cds"]:
        if s <= gpos < e:
            off = gpos - s if t["strand"] == "+" else e - 1 - gpos
            return (pos + off) // 3 + 1
        pos += e - s
    return None

def pickTx(h, cands, lift, maf, geneChoice, txs):
    """ return (tid, delta, how). delta is the difference between the residue
    number on the transcript and the one in the table.
    v2 hotspots: the transcript and residue shift where the codon(s) contain
    most of the lifted hg19 mutations and the reference amino acid(s) match.
    The table numbers residues on Ensembl 75 transcripts, some genes have
    other isoforms now (MYD88 L265 is L273 on the current main transcript).
    v3 hotspots: no shift, the transcript picked for the gene's v2 hotspots,
    then the canonical transcript.
    Ties go to: no shift, canonical, the MAF transcript ID, smallest shift, longest. """
    mafTid = maf.get(h["sym"], ("", ""))[0]
    def tieKey(tid):
        return (tid == mafTid, txs[tid]["canon"], len(txs[tid]["prot"]))
    if lift:
        opts = []
        for tid in cands:
            t = txs[tid]
            deltas = {0}
            for c, p, n in lift:
                r = genomeToResidue(t, c, p)
                if r is not None:
                    for d in range(h["aaStart"] - h["aaEnd"] - 2, 3):
                        deltas.add(r - h["aaStart"] + d if h["type"] == "in-frame indel" else r - h["aaStart"])
            for d in deltas:
                if refOk(t, [(p + d, aa) for p, aa in h["checks"]]):
                    sc = liftScore(t, h, lift, d)
                    # indels in repeats: the positions can be far from the residue
                    # range, keep the published numbering if the residues match
                    if sc > 0 or (d == 0 and h["type"] == "in-frame indel"):
                        opts.append(((d == 0, sc, txs[tid]["canon"], tid == mafTid, -abs(d), len(txs[tid]["prot"])), tid, d))
        if opts:
            key, tid, d = max(opts)
            return tid, d, "best match to hg19 positions" + (" (residue shifted)" if d else "")
        return None, 0, None
    good = [tid for tid in cands if refOk(txs[tid], h["checks"])]
    if not good:
        return None, 0, None
    prev = geneChoice.get(h["sym"])
    if prev and prev.most_common(1)[0][0] in good:
        return prev.most_common(1)[0][0], 0, "same as v2 hotspots of gene"
    best = max(good, key=lambda tid: (txs[tid]["canon"], tid == mafTid, len(txs[tid]["prot"])))
    return best, 0, "canonical" if txs[best]["canon"] else "other transcript"

def liftPositions(posList, chain):
    """ posList: list of (key, chrom, pos1, count) on hg19 -> dict key -> list of
    (chrom, pos0, count) on hg38, and the number of positions that did not lift """
    res = defaultdict(list)
    with tempfile.TemporaryDirectory() as tmp:
        inB, outB, unB = [os.path.join(tmp, x) for x in ("in.bed", "out.bed", "un.bed")]
        with open(inB, "w") as f:
            for i, (key, chrom, pos, n) in enumerate(posList):
                f.write("chr%s\t%d\t%d\t%d\n" % (chrom, pos - 1, pos, i))
        subprocess.run(["liftOver", inB, chain, outB, unB], check=True, stderr=subprocess.DEVNULL)
        nOk = 0
        for line in open(outB):
            c, s, e, i = line.split()
            key, chrom, pos, n = posList[int(i)]
            res[key].append((c, int(s), n))
            nOk += 1
    return res, len(posList) - nOk

def fmtQ(q):
    try:
        q = float(q)
    except ValueError:
        return ""
    if q == 0:
        return "0"
    return "%.2g" % q

def tissueFields(samples):
    " 'skin:88|lung:4' -> (filter list, display string). Sums duplicates. "
    cnt = Counter()
    for x in str(samples).split("|"):
        if ":" not in x:
            continue
        name, n = x.rsplit(":", 1)
        if name not in TISSUES:
            warn("unknown tissue: %s" % name)
        cnt[TISSUES.get(name, name)] += int(n)
    items = sorted(cnt.items(), key=lambda x: (-x[1], x[0]))
    return ",".join(k for k, v in items), ", ".join("%s (%d)" % (k, v) for k, v in items)

def mergeTissues(sampleStrs):
    return "|".join(str(s) for s in sampleStrs if str(s) != "nan")

def main():
    v2File, v3File, mafTxFile, gffFile, twoBit, chain, outFile = sys.argv[1:]

    maf = {}    # gene symbol -> (most common transcript ID, gene ID) in the MAF
    for line in open(mafTxFile):
        sym, tid, gid, n = line.rstrip("\n").split("\t")
        maf[sym] = (tid, gid)

    symToTx, txs = parseGff(gffFile)

    v2s = pd.read_excel(v2File, "SNV-hotspots", dtype=str)
    v2i = pd.read_excel(v2File, "INDEL-hotspots", dtype=str)
    v3r = pd.read_excel(v3File, "Hotspot_Residues", dtype=str)
    v3s = pd.read_excel(v3File, "SNV_Variants", dtype=str)

    # ---- collect hotspots as dicts
    hotspots = []
    # v2 single residue and splice: one row per variant amino acid, group by residue
    for (sym, pos), df in v2s.groupby(["Hugo_Symbol", "Amino_Acid_Position"], sort=False):
        r = df.iloc[0]
        ref = r.Reference_Amino_Acid.split(":")[0]
        varCnt = Counter()
        for v in df.Variant_Amino_Acid:
            for x in v.split("|"):
                a, n = x.rsplit(":", 1)
                varCnt[a] += int(n)
        isSplice = ref == "splice"
        h = dict(sym=sym, version="v2", type="splice site" if isSplice else "single residue",
                 tumors=int(r.Mutation_Count), q=fmtQ(r.qvalue),
                 samples=mergeTissues(df.Samples), oncokb="yes" if (df.inOncokb.str.upper() == "TRUE").any() else "no",
                 genPos=r.Genomic_Position, cohorts="")
        if isSplice:
            h["aaStart"] = h["aaEnd"] = int(re.sub("[^0-9]", "", pos.split("_")[0]))
            h["residue"] = pos.replace("_", " ")
            h["checks"] = []
            h["variants"] = ", ".join("%s (%d)" % (a, n) for a, n in varCnt.most_common())
        else:
            h["aaStart"] = h["aaEnd"] = int(pos)
            h["residue"] = ref + pos
            h["checks"] = [(int(pos), ref)]
            h["variants"] = ", ".join("%s%s%s (%d)" % (ref, pos, a, n) for a, n in varCnt.most_common())
        hotspots.append(h)

    # v2 in-frame indels: one row per indel, group by residue range
    for (sym, pos), df in v2i.groupby(["Hugo_Symbol", "Amino_Acid_Position"], sort=False):
        r = df.iloc[0]
        a, b = (pos.split("-") + [pos])[:2]
        checks = set()
        varCnt = Counter()
        for v in df.Variant_Amino_Acid:
            for x in v.split("|"):
                name, n = x.rsplit(":", 1)
                varCnt[name] += int(n)
                for aa, p in re.findall(r"([A-Z])(\d+)", name.split("ins")[0] if "delins" not in name else name.split("delins")[0]):
                    checks.add((int(p), aa))
        h = dict(sym=sym, version="v2", type="in-frame indel", aaStart=int(a), aaEnd=int(b),
                 residue=pos, checks=sorted(checks), tumors=int(r.Mutation_Count), q=fmtQ(r.qvalue),
                 samples=mergeTissues(df.Samples), oncokb="yes" if (df.inOncokb.str.upper() == "TRUE").any() else "no",
                 genPos=r.Genomic_Position, cohorts="",
                 variants=", ".join("%s (%d)" % (a, n) for a, n in varCnt.most_common()))
        hotspots.append(h)

    # v3: the 164 new residues; variants from the SNV_Variants sheet
    v3var = defaultdict(Counter)
    for r in v3s[v3s.Hotspot_Version == "v3"].itertuples():
        v3var[(r.Hugo_Symbol, r.Codon_Position)][r.Variant_Amino_Acid] += int(r.Mutation_Count)
    for r in v3r.to_dict("records"):
        sym, codon, pos = r["Hugo_Symbol"], r["Codon"], r["Codon_Position"]
        ref = codon[0]
        varCnt = v3var[(sym, pos)]
        tl, td = tissueFields(r["Samples"])
        cohorts = "MSK-IMPACT %s of %s, other GENIE %s of %s, TCGA %s of %s" % (
            r["# mut in MSK"], r["# total samples in MSK"], r["# mut in non-MSK-GENIE"],
            r["# total samples in non-MSK-GENIE"], r["# mut in TCGA"], r["# total samples in TCGA"])
        h = dict(sym=sym, version="v3", type="single residue", aaStart=int(pos), aaEnd=int(pos),
                 residue=codon, checks=[(int(pos), ref)],
                 tumors=sum(int(x.split(":")[1]) for x in r["Samples"].split("|")),
                 q=fmtQ(r["Q value"]), samples=r["Samples"],
                 oncokb="yes" if str(r["In OncoKB v3.14"]) == "OncoKB" else "no",
                 genPos="", cohorts=cohorts,
                 variants=", ".join("%s%s (%d)" % (codon, a, n) for a, n in varCnt.most_common()))
        hotspots.append(h)

    # ---- sequences of all candidate transcripts of all hotspot genes
    cands = {}
    for h in hotspots:
        cands[h["sym"]] = candidates(h["sym"], maf, symToTx, txs)
    loadCdsSeqs(txs, sorted(set(t for l in cands.values() for t in l)), twoBit)

    # ---- lift the v2 hg19 positions, format chrom:pos_count|...
    posList = []
    for i, h in enumerate(hotspots):
        for x in h["genPos"].split("|") if h["genPos"] else []:
            loc, n = x.rsplit("_", 1)
            chrom, p = loc.split(":")
            posList.append((i, chrom, int(p), int(n)))
    lifted, nUnlifted = liftPositions(posList, chain)
    if nUnlifted:
        warn("hg19 positions that did not lift: %d" % nUnlifted)

    howCount = Counter()
    geneChoice = defaultdict(Counter)
    out = open(outFile, "w")
    nOut = 0
    for i, h in enumerate(hotspots):
        name = "%s %s" % (h["sym"], h["residue"])
        lift = lifted.get(i, [])
        tid = None
        txResidue = ""
        if h["type"] == "splice site":
            # no codon: one 1 bp block for each lifted mutation position
            if not lift:
                warn("splice hotspot without lifted positions: %s" % name)
                continue
            chroms = set(c for c, p, n in lift)
            if len(chroms) != 1:
                warn("splice hotspot lifted to several chroms: %s" % name)
                continue
            chrom = chroms.pop()
            blocks = sorted(set((p, p + 1) for c, p, n in lift))
            tids = [t for t in cands[h["sym"]] if txs[t]["canon"]]
            tid = tids[0] if tids else ""
            strand = txs[tid]["strand"] if tid else "."
            tid = ""   # the residue number is from an older transcript, do not show one
            howCount["splice: lifted hg19 positions"] += 1
        else:
            tid, delta, how = pickTx(h, cands[h["sym"]], lift, maf, geneChoice, txs)
            if tid is None:
                warn("no transcript with matching reference amino acid: %s %s" % (name, h["checks"]))
                continue
            howCount[how] += 1
            if h["version"] == "v2":
                geneChoice[h["sym"]][tid] += 1
            t = txs[tid]
            blocks = cdsToGenome(t, (h["aaStart"] + delta - 1) * 3, (h["aaEnd"] + delta) * 3)
            if delta:
                if h["type"] == "in-frame indel":
                    txResidue = "%d-%d" % (h["aaStart"] + delta, h["aaEnd"] + delta)
                else:
                    txResidue = h["residue"][0] + str(h["aaStart"] + delta)
                warn("residue number shifted: %s is %s on %s" % (name, txResidue, tid))
            chrom, strand = t["chrom"], t["strand"]
            # v2: most lifted mutation positions must be in the mapped codon.
            # A few are not, they are dinucleotide changes that start in the codon before.
            if lift:
                total = sum(n for c, p, n in lift)
                inCodon = liftScore(t, h, lift, delta)
                if inCodon * 2 <= total:
                    warn("most hg19 mutation positions outside of mapped codon: %s %s %d of %d" % (name, tid, inCodon, total))
        start, end = blocks[0][0], blocks[-1][1]
        tl, td = tissueFields(h["samples"])
        color = NEWCOLOR if h["version"] == "v3" else COLORS[h["type"]]
        version = "v2 (Chang et al. 2017)" if h["version"] == "v2" else "v3 (Bandlamudi et al. 2026)"
        top3 = ", ".join(td.split(", ")[:3])
        row = [chrom, start, end, name, 0, strand, start, end, color, len(blocks),
               ",".join(str(e - s) for s, e in blocks) + ",",
               ",".join(str(s - start) for s, e in blocks) + ",",
               h["sym"], h["residue"], h["type"], version, h["tumors"], h["q"], h["variants"],
               td, tl, top3, h["oncokb"], h["cohorts"], tid or "", txResidue]
        out.write("\t".join(str(x) for x in row) + "\n")
        nOut += 1

    print("hotspots in input: %d (v2 single residue/splice %d, v2 indel %d, v3 %d)" % (
        len(hotspots), v2s.groupby(["Hugo_Symbol", "Amino_Acid_Position"]).ngroups,
        v2i.groupby(["Hugo_Symbol", "Amino_Acid_Position"]).ngroups, len(v3r)), file=sys.stderr)
    print("features written: %d" % nOut, file=sys.stderr)
    print("transcript choice: %s" % dict(howCount), file=sys.stderr)
    print("warnings: %s" % dict(warnCounts), file=sys.stderr)

main()
