#!/usr/bin/env python3
# Build of the ENIGMA PP4/BP5 (BRCAmla) track with the Zanti et al. 2025
# case-control LR (ccLR, PMID 40413188) replacing the Parsons iCOGS
# case-control component (refs #37886).
#
# Revision for refs #38467:
#  - Variants are matched by a left-normalized (chrom, pos, ref, alt) key
#    instead of by HGVS name string, so the same molecular event written two
#    ways (c.4574_4575del / c.4574_4575delAA, dup / ins, del15 / del) becomes
#    one item and its evidence is multiplied together.
#  - Every item is drawn the way the UCSC ClinVar track draws it: SNVs on the
#    base, deletions on the deleted bases shifted left, delins on the replaced
#    bases, insertions and dups as the 2 bp flanking the left-shifted
#    insertion point.
#  - Names are written in current HGVS style (no spelled-out deleted or
#    duplicated bases; dup instead of ins when the insertion copies the
#    adjacent sequence).
#  - Zanti rows with a symbolic allele (the single <CNV> row) are dropped.
#
# hgvsToVcf mis-converts insertions with an intronic offset (refs #38469), so
# Zanti rows are keyed from Zanti's own VCF columns, not from their HGVSc.
#
# Inputs are the pre-Zanti track (archive/v1.1, bed9+12) and Zanti
# Supplementary Data 4. Do not point CUR38 at /gbdb/.../BRCAmfa.bb: since the
# #37886 release that file is this script's own output.
# Outputs go to WORK; copying them onto the staging filenames is done at
# release, see the makedoc (makeDb/doc/enigma.txt).

import openpyxl, re, subprocess, sys, functools

WORK = "/hive/data/inside/enigmaTracksData/rm38467"
XLSX = "/hive/data/inside/enigmaTracksData/zantiDraft/ZantiSuppData4.xlsx"
CUR38 = "/hive/data/inside/enigmaTracksData/archive/v1.1/BRCAmfaHg38.bb"   # pre-Zanti track
ASM = {"38": {"db": "hg38", "twoBit": "/hive/data/genomes/hg38/hg38.2bit",
              "sizes": "/cluster/data/hg38/chrom.sizes", "out": "BRCAmfaZantiHg38"},
       "19": {"db": "hg19", "twoBit": "/hive/data/genomes/hg19/hg19.2bit",
              "sizes": "/cluster/data/hg19/chrom.sizes", "out": "BRCAmfaZantiHg19"}}

TX = {"BRCA1": "NM_007294.4", "BRCA2": "NM_000059.4"}

def bash(cmd):
    r = subprocess.run(cmd, shell=True, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, universal_newlines=True)
    if r.returncode != 0:
        sys.exit("CMD FAILED: %s\n%s" % (cmd, r.stdout))
    return r.stdout

def fnum(x):
    """Parse a float from a track field; return None if blank/non-numeric."""
    if x is None:
        return None
    s = str(x).strip()
    if s == "" or s.upper() == "NULL":
        return None
    try:
        return float(s)
    except ValueError:
        return None

# Gap-free ACMG bands (Tavtigian/ACMG, matching the Zanti thresholds).
def assignACMGcode(lr):
    if lr is None:
        return "Not informative"
    if lr >= 350:      return "PP4 - Pathogenic - Very strong"
    if lr >= 18.7:     return "PP4 - Pathogenic - Strong"
    if lr >= 4.33:     return "PP4 - Pathogenic - Moderate"
    if lr >= 2.08:     return "PP4 - Pathogenic - Supporting"
    if lr <= 0.0029:   return "BP5 - Benign - Very strong"
    if lr <= 0.053:    return "BP5 - Benign - Strong"
    if lr <= 0.231:    return "BP5 - Benign - Moderate"
    if lr <= 0.48:     return "BP5 - Benign - Supporting"
    return "Not informative"

def assignRGB(lr):
    if lr is None:      return "91,91,91"
    if lr >= 2.08:      return "128,64,13"   # brown  -> PP4
    if lr <= 0.48:      return "252,157,3"   # orange -> BP5
    return "91,91,91"                        # grey   -> no evidence

def fmt(v):
    """5 significant figures, so small LRs are not shown as 0.0 (refs #38467)."""
    return "" if v is None else "%.5g" % v

# ---------------------------------------------------------------------------
# Variant normalization and ClinVar-style spans.
# ---------------------------------------------------------------------------
class RefMismatch(Exception):
    pass

@functools.lru_cache(None)
def refChunk(asm, chrom, start):
    out = bash("twoBitToFa %s:%s:%d-%d stdout" % (ASM[asm]["twoBit"], chrom, start, start + 10000))
    return out.split("\n", 1)[1].replace("\n", "").upper()

def refBase(asm, chrom, pos1):
    """Reference base at 1-based position."""
    s = (pos1 - 1) // 10000 * 10000
    return refChunk(asm, chrom, s)[pos1 - 1 - s]

def normalize(asm, chrom, pos, ref, alt):
    """Trim, left-shift and re-anchor a VCF-style variant. Returns (chrom, pos, ref, alt),
    1-based. Pure indels come back anchored on the preceding base; SNVs, MNVs and
    complex delins come back trimmed and unanchored. Raises RefMismatch if ref does not
    match the genome."""
    ref, alt = ref.upper(), alt.upper()
    for i, b in enumerate(ref):
        if refBase(asm, chrom, pos + i) != b:
            raise RefMismatch("%s:%d %s>%s" % (chrom, pos, ref, alt))
    while len(ref) > 1 and len(alt) > 1 and ref[-1] == alt[-1]:
        ref, alt = ref[:-1], alt[:-1]
    while len(ref) > 1 and len(alt) > 1 and ref[0] == alt[0]:
        ref, alt, pos = ref[1:], alt[1:], pos + 1
    if len(ref) == 1 and len(alt) == 1:
        return (chrom, pos, ref, alt)
    if ref[0] == alt[0]:
        ref, alt, pos = ref[1:], alt[1:], pos + 1
    if ref and alt:
        return (chrom, pos, ref, alt)
    # pure insertion or deletion of seq before pos: shift left through repeats
    while True:
        prev = refBase(asm, chrom, pos - 1)
        if ref and ref[-1] == prev:
            ref, pos = prev + ref[:-1], pos - 1
        elif alt and alt[-1] == prev:
            alt, pos = prev + alt[:-1], pos - 1
        else:
            break
    prev = refBase(asm, chrom, pos - 1)
    return (chrom, pos - 1, prev + ref, prev + alt)

def clinvarSpan(key):
    """0-based [start, end) the way the ClinVar track draws the variant."""
    chrom, pos, ref, alt = key
    if len(ref) + len(alt) > 2 and ref[0] == alt[0]:
        if len(ref) == 1:
            return chrom, pos - 1, pos + 1          # insertion/dup: 2 flanking bases
        return chrom, pos, pos + len(ref) - 1       # deletion: deleted bases
    return chrom, pos - 1, pos - 1 + len(ref)       # SNV, MNV, delins: replaced bases

def isPureInsertion(key):
    return len(key[2]) == 1 and len(key[3]) > 1 and key[2][0] == key[3][0]

def hgvsToKeys(asm, names):
    """Run hgvsToVcf on a list of HGVS names; return {name: normalized key}."""
    fn = "%s/hgvsIn.%s.txt" % (WORK, asm)
    with open(fn, "w") as fh:
        fh.write("\n".join(sorted(set(names))) + "\n")
    keys = {}
    for line in bash("hgvsToVcf %s %s stdout" % (ASM[asm]["db"], fn)).splitlines():
        if line.startswith("#"):
            continue
        f = line.split("\t")
        if f[6] != "PASS":
            sys.exit("hgvsToVcf %s flagged %s: %s" % (asm, f[2], f[6]))
        m = re.search(r"(del|dup)(\d+)$", f[2])
        if m and abs(len(f[3]) - len(f[4])) != int(m.group(2)):
            sys.exit("Base count in %s does not match its range" % f[2])
        keys[f[2]] = normalize(asm, f[0], int(f[1]), f[3], f[4])
    return keys

# ---------------------------------------------------------------------------
# 1. Pre-Zanti multifactorial track (bed9+12). Columns (0-based): 9 LLR,
#    10 ACMGcode, 11 famHist, 12 cooc, 13 seg, 14 path, 15 caseControl
#    (Parsons, dropped), 16 caputo, 17 parsons, 18 li, 19 easton, 20 mouseOver.
# ---------------------------------------------------------------------------
cur = {}
for line in bash("bigBedToBed %s stdout" % CUR38).splitlines():
    f = line.split("\t")
    if len(f) != 21:
        sys.exit("Expected 21 columns (bed9+12) in %s, got %d" % (CUR38, len(f)))
    if f[3] in cur:
        sys.exit("Name appears twice in %s: %s" % (CUR38, f[3]))
    cur[f[3]] = {"famHist": f[11], "cooc": f[12], "seg": f[13], "path": f[14],
                 "caputo": f[16], "parsons": f[17], "li": f[18], "easton": f[19]}

# ---------------------------------------------------------------------------
# 2. Zanti Supplementary Data 4.  header at row index 4, data from index 5.
# ---------------------------------------------------------------------------
wb = openpyxl.load_workbook(XLSX, read_only=True)
ws = wb["Supplementary Data 4"]
C = dict(CHR=2, POS19=3, POS38=4, REF=5, ALT=6, GENE=9, HGVSC=12, HGVSP=13,
         BRIDGES=27, CARRIERS=30, UKB=33, CCLR=37, SUGG=38, PS4=42)

zan = []          # list of Zanti records
zanDropped = 0    # N/A / None / no computable LR
zanSymbolic = 0   # symbolic alleles such as <CNV>
for i, r in enumerate(ws.iter_rows(values_only=True)):
    if i < 5 or r is None or r[C["GENE"]] is None:
        continue
    gene = str(r[C["GENE"]]).strip()
    if gene not in TX:
        continue
    cclr = fnum(r[C["CCLR"]])
    sugg = ("" if r[C["SUGG"]] is None else str(r[C["SUGG"]]).strip())
    if cclr is None or sugg in ("N/A", "None"):
        zanDropped += 1
        continue
    ref = str(r[C["REF"]]).strip()
    alt = str(r[C["ALT"]]).strip()
    if ref.startswith("<") or alt.startswith("<"):
        zanSymbolic += 1
        continue
    hgvsc = None if r[C["HGVSC"]] is None else str(r[C["HGVSC"]]).strip()
    chrom = "chr%s" % str(r[C["CHR"]]).strip()
    if hgvsc and hgvsc.startswith("c."):
        name = "%s:%s" % (TX[gene], re.sub(r"\s+", "", hgvsc))
    else:
        name = "%s:%s:%s:%s>%s" % (gene, chrom, str(r[C["POS38"]]).strip(), ref, alt)
    zan.append({"gene": gene, "chrom": chrom, "ref": ref, "alt": alt, "name": name,
                "hgvsc": name if name.startswith("NM_") else None,
                "pos38": r[C["POS38"]], "pos19": r[C["POS19"]],
                "hgvsp": "" if r[C["HGVSP"]] is None else str(r[C["HGVSP"]]).strip(),
                "ccLR": cclr, "sugg": sugg,
                "bridges": r[C["BRIDGES"]], "carriers": r[C["CARRIERS"]], "ukb": r[C["UKB"]],
                "ps4": "" if r[C["PS4"]] is None else str(r[C["PS4"]]).strip()})

# ---------------------------------------------------------------------------
# 3. Keys for both assemblies. Pre-Zanti items: from their HGVS names via
#    hgvsToVcf (none are intronic insertions). Zanti rows: from Zanti's own VCF
#    columns; fall back to the HGVSc only where Zanti's REF does not match the
#    genome. Cross-check Zanti VCF against HGVSc and report disagreements.
# ---------------------------------------------------------------------------
def intronicIns(name):
    return bool(re.search(r"[+-]\d+_.*ins|_\d+[+-]\d+ins", name)) and "delins" not in name

curKey, zanKey = {}, {}
refFallback, crossBad = [], []
for asm in ("38", "19"):
    hk = hgvsToKeys(asm, list(cur) + [z["hgvsc"] for z in zan if z["hgvsc"]])
    missing = [n for n in cur if n not in hk]
    if missing:
        sys.exit("hgvsToVcf %s could not convert pre-Zanti names: %s" % (asm, missing))
    curKey[asm] = {n: hk[n] for n in cur}
    zanKey[asm] = []
    for z in zan:
        try:
            k = normalize(asm, z["chrom"], int(float(str(z["pos" + asm]))), z["ref"], z["alt"])
        except RefMismatch:
            if not z["hgvsc"] or z["hgvsc"] not in hk:
                sys.exit("Zanti REF mismatch with no usable HGVSc: %s" % z["name"])
            k = hk[z["hgvsc"]]
            refFallback.append((asm, z["name"]))
        else:
            if z["hgvsc"] and z["hgvsc"] in hk and hk[z["hgvsc"]] != k:
                if not intronicIns(z["hgvsc"]):
                    sys.exit("Zanti VCF and HGVSc disagree on %s (%s): %s vs %s"
                             % (z["name"], asm, k, hk[z["hgvsc"]]))
                if asm == "38":
                    crossBad.append(z["name"])
        zanKey[asm].append(k)

# ---------------------------------------------------------------------------
# 4. Group by hg38 key. Each group's hg19 key must also agree.
# ---------------------------------------------------------------------------
groups = {}        # hg38 key -> {"cur": [names], "zan": [records], "k19": set()}
for n in cur:
    g = groups.setdefault(curKey["38"][n], {"cur": [], "zan": [], "k19": set()})
    g["cur"].append(n)
    g["k19"].add(curKey["19"][n])
for z, k38, k19 in zip(zan, zanKey["38"], zanKey["19"]):
    g = groups.setdefault(k38, {"cur": [], "zan": [], "k19": set()})
    g["zan"].append(z)
    g["k19"].add(k19)
for k, g in groups.items():
    if len(g["zan"]) > 1:
        sys.exit("Two Zanti rows share one variant %s: %s" % (k, [z["name"] for z in g["zan"]]))
    if len(g["k19"]) != 1:
        sys.exit("Group %s has inconsistent hg19 keys: %s" % (k, g["k19"]))

# ---------------------------------------------------------------------------
# 5. Names. Strip spelled-out bases and counts from del/dup names. For pure
#    insertions named with ins, ask vcfToHgvs whether HGVS calls it a dup.
# ---------------------------------------------------------------------------
def stripSeq(name):
    return re.sub(r"(del|dup)([ACGT]+|\d+)$", r"\1", name)

insKeys = [k for k, g in groups.items() if isPureInsertion(k)
           and any(re.search(r"ins[ACGT]+$", n) and "delins" not in n
                   for n in g["cur"] + [z["name"] for z in g["zan"]])]
dupName = {}
if insKeys:
    fn = WORK + "/insKeys.vcf"
    with open(fn, "w") as fh:
        fh.write("##fileformat=VCFv4.2\n#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")
        for i, k in enumerate(insKeys):
            fh.write("%s\t%d\tk%d\t%s\t%s\t.\t.\t.\n" % (k[0], k[1], i, k[2], k[3]))
    for line in bash("vcfToHgvs hg38 %s stdout" % fn).splitlines():
        f = line.split("\t")
        if len(f) < 9 or f[5] not in TX.values():
            continue
        k = insKeys[int(f[2][1:])]
        if re.search(r"dup[ACGT]*$", f[8]):
            dupName[k] = stripSeq(f[8])

renamed = []
def canonicalName(k, g):
    names = g["cur"] + [z["name"] for z in g["zan"]]
    if k in dupName:
        return dupName[k]
    stripped = sorted(set(stripSeq(n) for n in names))
    if len(stripped) != 1:
        sys.exit("Group %s names differ after stripping: %s" % (k, names))
    return stripped[0]

# ---------------------------------------------------------------------------
# 6. Merge evidence and write one item per group, both assemblies.
# ---------------------------------------------------------------------------
def product(vals):
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    p = 1.0
    for v in vals:
        p *= v
    return p

def mergeList(lists):
    """Multiply comma lists of per-source LRs position by position (NULL = absent) and
    write them with fmt()."""
    lists = [l for l in lists if l]
    if not lists:
        return ""
    cols = [l.split(",") for l in lists]
    if len(set(len(c) for c in cols)) != 1:
        sys.exit("Per-source lists of different lengths: %s" % lists)
    out = []
    for vals in zip(*cols):
        p = product([fnum(v) for v in vals])
        out.append("NULL" if p is None else fmt(p))
    return ",".join(out)

conflicts = []
merged = []
sourceTwice = []
rows = {"38": [], "19": []}
counts = {"groups": 0, "merged": 0, "both": 0, "zanOnly": 0, "mfOnly": 0}
for k, g in groups.items():
    curs = [cur[n] for n in g["cur"]]
    z = g["zan"][0] if g["zan"] else None
    counts["groups"] += 1
    if len(g["cur"]) + len(g["zan"]) > 1:
        counts["merged"] += 1
    if curs and z:
        counts["both"] += 1
    elif z:
        counts["zanOnly"] += 1
    else:
        counts["mfOnly"] += 1
    for src in ("caputo", "parsons", "li", "easton"):
        if sum(1 for c in curs if c[src]) > 1:
            sourceTwice.append((g["cur"], src))

    per = {t: product([fnum(c[t]) for c in curs]) for t in ("famHist", "cooc", "seg", "path")}
    mf = product(list(per.values()))
    cc = z["ccLR"] if z else None
    combined = product([mf, cc])
    if mf is not None and cc is not None:
        if (cc >= 2.08 and mf <= 0.48) or (cc <= 0.48 and mf >= 2.08):
            conflicts.append((canonicalName(k, g), k, fmt(mf), fmt(cc), fmt(combined)))

    name = canonicalName(k, g)
    oldNames = g["cur"] + [x["name"] for x in g["zan"]]
    if len(oldNames) > 1:
        merged.append((name, oldNames, fmt(combined), assignACMGcode(combined)))
    elif oldNames[0] != name:
        renamed.append((oldNames[0], name))

    code = assignACMGcode(combined)
    rgb = assignRGB(combined)
    mouse = ("<b>HGVSc:</b> %s<br><b>Combined LR:</b> %s<br>"
             "<b>ACMG Code:</b> %s" % (name, fmt(combined), code))
    tail = [fmt(combined), code,
            fmt(per["famHist"]), fmt(per["cooc"]), fmt(per["seg"]), fmt(per["path"]),
            fmt(cc),
            fmt(fnum(z["bridges"])) if z else "",
            fmt(fnum(z["carriers"])) if z else "",
            fmt(fnum(z["ukb"])) if z else "",
            z["sugg"] if z else "",
            mergeList([c["caputo"] for c in curs]),
            mergeList([c["parsons"] for c in curs]),
            mergeList([c["li"] for c in curs]),
            mergeList([c["easton"] for c in curs]),
            z["hgvsp"] if z else "", mouse]
    for asm, key in (("38", k), ("19", next(iter(g["k19"])))):
        chrom, start, end = clinvarSpan(key)
        rows[asm].append("\t".join([chrom, str(start), str(end), name, "0", ".",
                                    str(start), str(end), rgb] + tail))

names = [r.split("\t")[3] for r in rows["38"]]
if len(names) != len(set(names)):
    dupNames = sorted(set(n for n in names if names.count(n) > 1))
    sys.exit("Output names are not unique: %s" % dupNames[:20])

for asm in ("38", "19"):
    bed = "%s/%s.bed" % (WORK, ASM[asm]["out"])
    with open(bed, "w") as fh:
        fh.write("\n".join(rows[asm]) + "\n")
    bash("bedSort %s %s" % (bed, bed))

# ---------------------------------------------------------------------------
# 7. autoSql and bigBed (same layout as #37886, so trackDb is unchanged).
# ---------------------------------------------------------------------------
AS = '''table BRCAmla
"BRCA1/BRCA2 multifactorial likelihood analysis (PP4/BP5), with Zanti et al. 2025 case-control LR"
   (
   string chrom;       "Reference sequence chromosome or scaffold"
   uint   chromStart;  "Start position in chromosome"
   uint   chromEnd;    "End position in chromosome"
   string name;        "HGVS Nucleotide"
   uint score;         "Not used, all 0"
   char[1] strand;     "Not used, all ."
   uint thickStart;    "Same as chromStart"
   uint thickEnd;      "Same as chromEnd"
   uint reserved;      "RGB value"
   string LLR;         "Combined LR score (product of available evidence)"
   string ACMGcode;    "PP4/BP5 code and strength from the combined LR"
   string familyHistoryCombinedLR;   "Combined family-history LR. Blank if none."
   string cooccurrenceCombinedLR;    "Combined co-occurrence LR. Blank if none."
   string segregationCombinedLR;     "Combined segregation LR. Blank if none."
   string pathologyCombinedLR;       "Combined pathology LR. Blank if none."
   string caseControlLR;             "Case-control LR from Zanti et al. 2025 (ccLR). Blank if none."
   string bridgesLR;                 "Zanti BRIDGES dataset ccLR"
   string carriersLR;                "Zanti CARRIERS dataset ccLR"
   string ukbLR;                     "Zanti UK Biobank ccLR"
   string zantiSuggestedCode;        "Zanti standalone suggested ACMG/AMP evidence (case-control only)"
   string caputoLRs;    "Caputo et al scores (family, co-occurrence, segregation, pathology)"
   string parsonsLRs;   "Parsons et al scores (family, co-occurrence, segregation, pathology)"
   string liLRs;        "Li et al scores (family)"
   string eastonLRs;    "Easton et al scores (family, co-occurrence, segregation)"
   string HGVSp;        "HGVS protein change"
   string _mouseOver;   "Field only used as mouseOver"
   )'''
with open(WORK + "/BRCAmlaZanti.as", "w") as fh:
    fh.write(AS)

for asm in ("38", "19"):
    out = ASM[asm]["out"]
    bash("bedToBigBed -as=%s/BRCAmlaZanti.as -type=bed9+17 -tab %s/%s.bed %s %s/%s.bb"
         % (WORK, WORK, out, ASM[asm]["sizes"], WORK, out))

# ---------------------------------------------------------------------------
# 8. Report.
# ---------------------------------------------------------------------------
print("=== BUILD SUMMARY ===")
print("Pre-Zanti items: %d   Zanti rows used: %d" % (len(cur), len(zan)))
print("Zanti rows dropped: %d N/A or no computable LR, %d symbolic allele"
      % (zanDropped, zanSymbolic))
print("Zanti rows keyed from HGVSc because Zanti REF does not match the genome: %d %s"
      % (len(refFallback), refFallback))
print("Zanti intronic insertions where hgvsToVcf disagrees with Zanti VCF (#38469): %d"
      % len(crossBad))
print("Output items per assembly: %d" % counts["groups"])
print("Merged groups: %d   Renamed singletons: %d" % (counts["merged"], len(renamed)))
print("Membership: both=%d  Zanti-only=%d  multifactorial-only=%d"
      % (counts["both"], counts["zanOnly"], counts["mfOnly"]))
print("Groups with the same source on two items: %s" % sourceTwice)
print("Direction conflicts (MF vs ccLR opposite): %d" % len(conflicts))
with open(WORK + "/directionConflicts.tsv", "w") as fh:
    fh.write("name\tvariant\tmultifactorialProduct\tzantiCcLR\tcombinedLR\n")
    for c in conflicts:
        fh.write("%s\t%s:%d:%s>%s\t%s\t%s\t%s\n" % ((c[0],) + c[1] + c[2:]))
with open(WORK + "/mergedGroups.tsv", "w") as fh:
    fh.write("name\toldNames\tcombinedLR\tACMGcode\n")
    for m in sorted(merged):
        fh.write("%s\t%s\t%s\t%s\n" % (m[0], ",".join(m[1]), m[2], m[3]))
with open(WORK + "/renamed.tsv", "w") as fh:
    fh.write("oldName\tnewName\n")
    for r in sorted(renamed):
        fh.write("%s\t%s\n" % r)
print("Wrote directionConflicts.tsv, mergedGroups.tsv, renamed.tsv")
