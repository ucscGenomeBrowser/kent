#!/usr/bin/env python3
# Rebuild of the ENIGMA PP4/BP5 (BRCAmla) track with the Zanti et al. 2025
# case-control LR (ccLR, PMID 40413188) replacing the Parsons iCOGS
# case-control component. Reads the current track bigBeds for the other
# evidence types, merges in the ccLRs from Zanti Supplementary Data 4, and
# writes new .bed/.bb into the work dir. The outputs are copied onto the
# staging filenames served by the hub only at release, see the makedoc
# (makeDb/doc/enigma.txt). refs #37886

import openpyxl, re, subprocess, sys

WORK = "/hive/data/inside/enigmaTracksData/zantiDraft"
XLSX = WORK + "/ZantiSuppData4.xlsx"
CUR38 = "/gbdb/hg38/bbi/enigma/BRCAmfa.bb"   # current (Parsons-based) track, hg38
CUR19 = "/gbdb/hg19/bbi/enigma/BRCAmfa.bb"   # current track, hg19
HG38SIZES = "/cluster/data/hg38/chrom.sizes"
HG19SIZES = "/cluster/data/hg19/chrom.sizes"

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

# ---------------------------------------------------------------------------
# 1. Current multifactorial track: reuse its already-combined per-evidence LRs
#    (family/co-occurrence/segregation/pathology) and coordinates for BOTH
#    assemblies. We DROP the existing case-control column (Parsons iCOGS).
#    bb columns (1-based): 1-9 bed9, 10 LLR, 11 ACMGcode, 12 famHist,
#    13 cooc, 14 seg, 15 path, 16 caseControl(Parsons), 17 caputo, 18 parsons,
#    19 li, 20 easton, 21 mouseOver.
# ---------------------------------------------------------------------------
def loadCurrent(bb):
    d = {}
    for line in bash("bigBedToBed %s stdout" % bb).splitlines():
        f = line.split("\t")
        name = f[3]
        d[name] = {
            "chrom": f[0], "start": f[1], "end": f[2],
            "famHist": f[11], "cooc": f[12], "seg": f[13], "path": f[14],
            "caputo": f[16], "parsons": f[17], "li": f[18], "easton": f[19],
        }
    return d

cur38 = loadCurrent(CUR38)
cur19 = loadCurrent(CUR19)

# ---------------------------------------------------------------------------
# 2. Zanti Supplementary Data 4.  header at row index 4, data from index 5.
# ---------------------------------------------------------------------------
wb = openpyxl.load_workbook(XLSX, read_only=True)
ws = wb["Supplementary Data 4"]
C = dict(CHR=2, POS19=3, POS38=4, REF=5, ALT=6, GENE=9, HGVSC=12, HGVSP=13,
         BRIDGES=27, CARRIERS=30, UKB=33, CCLR=37, SUGG=38, PS4=42)

zan = {}          # key -> dict
zan_dropped = 0   # N/A / None / no computable LR
for i, r in enumerate(ws.iter_rows(values_only=True)):
    if i < 5 or r is None or r[C["GENE"]] is None:
        continue
    gene = str(r[C["GENE"]]).strip()
    if gene not in TX:
        continue
    cclr = fnum(r[C["CCLR"]])
    sugg = ("" if r[C["SUGG"]] is None else str(r[C["SUGG"]]).strip())
    if cclr is None or sugg in ("N/A", "None"):
        zan_dropped += 1
        continue
    hgvsc = None if r[C["HGVSC"]] is None else str(r[C["HGVSC"]]).strip()
    chrom = "chr%s" % str(r[C["CHR"]]).strip()
    ref = str(r[C["REF"]]).strip()
    if hgvsc and hgvsc.startswith("c."):
        key = "%s:%s" % (TX[gene], re.sub(r"\s+", "", hgvsc))
        name = key
    else:
        key = "%s:%s:%s:%s>%s" % (gene, chrom, str(r[C["POS38"]]).strip(),
                                  ref, str(r[C["ALT"]]).strip())
        name = key
    rec = {"gene": gene, "chrom": chrom, "ref": ref, "name": name,
           "hgvsp": "" if r[C["HGVSP"]] is None else str(r[C["HGVSP"]]).strip(),
           "ccLR": cclr, "sugg": sugg,
           "bridges": r[C["BRIDGES"]], "carriers": r[C["CARRIERS"]], "ukb": r[C["UKB"]],
           "ps4": "" if r[C["PS4"]] is None else str(r[C["PS4"]]).strip()}
    for asm, col in (("38", "POS38"), ("19", "POS19")):
        p = r[C[col]]
        if p is None or str(p).strip() == "":
            rec["pos" + asm] = None
        else:
            s = int(float(str(p).strip())) - 1
            rec["pos" + asm] = (chrom, str(s), str(s + max(1, len(ref))))
    zan[key] = rec

# ---------------------------------------------------------------------------
# 3. Merge: universe = union of current-track variants and Zanti variants.
#    New combined LR = product(family, co-occ, seg, path) x Zanti ccLR.
# ---------------------------------------------------------------------------
allkeys = set(cur38) | set(zan)
conflicts = []            # (key, MF, ccLR, combined)
n_both = n_zan_only = n_mf_only = n_no_evidence_left = 0
color_count = {}

def mfProduct(cur):
    """Product of the four non-case-control components; None if none present."""
    vals = [fnum(cur[k]) for k in ("famHist", "cooc", "seg", "path")]
    vals = [v for v in vals if v is not None]
    if not vals:
        return None
    p = 1.0
    for v in vals:
        p *= v
    return p

def buildBed(asm, curmap):
    """Emit bed lines for one assembly."""
    global n_both, n_zan_only, n_mf_only, n_no_evidence_left
    lines = []
    for key in allkeys:
        cur = curmap.get(key)
        z = zan.get(key)

        # multifactorial part
        mf = mfProduct(cur) if cur else None
        cc = z["ccLR"] if z else None

        # combined LR
        terms = [t for t in (mf, cc) if t is not None]
        if not terms:
            combined = None
        else:
            combined = 1.0
            for t in terms:
                combined *= t

        # coordinates for this assembly: prefer current-track coords for
        # multifactorial variants, else Zanti's provided coords.
        if cur:
            chrom, start, end = cur["chrom"], cur["start"], cur["end"]
        elif z and z["pos" + asm]:
            chrom, start, end = z["pos" + asm]
        else:
            continue  # variant not placeable on this assembly

        # classify membership (count once, on hg38 pass only)
        if asm == "38":
            if cur and z:   n_both += 1
            elif z:         n_zan_only += 1
            else:           n_mf_only += 1
            if combined is None:
                n_no_evidence_left += 1
            # direction conflict: MF vs ccLR point opposite ways
            if mf is not None and cc is not None:
                if (cc >= 2.08 and mf <= 0.48) or (cc <= 0.48 and mf >= 2.08):
                    conflicts.append((key, round(mf, 5), round(cc, 5),
                                      round(combined, 5)))

        code = assignACMGcode(combined)
        rgb = assignRGB(combined)
        if asm == "38":
            color_count[rgb] = color_count.get(rgb, 0) + 1

        def fmt(v):
            return "" if v is None else str(round(v, 5))

        name = z["name"] if z else key
        hgvsp = z["hgvsp"] if z else ""
        cc_str = fmt(cc)
        bridges = fmt(fnum(z["bridges"])) if z else ""
        carriers = fmt(fnum(z["carriers"])) if z else ""
        ukb = fmt(fnum(z["ukb"])) if z else ""
        sugg = z["sugg"] if z else ""
        caputo = cur["caputo"] if cur else ""
        parsons = cur["parsons"] if cur else ""
        li = cur["li"] if cur else ""
        easton = cur["easton"] if cur else ""

        # mouseOver mirrors the original track: Combined LR + ACMG code only.
        # Per-source scores (including the Zanti ccLR) live in the detail page,
        # not the mouseOver, so no single evidence type is singled out.
        mouse = ("<b>HGVSc:</b> %s<br><b>Combined LR:</b> %s<br>"
                 "<b>ACMG Code:</b> %s" % (name, fmt(combined), code))

        row = [chrom, start, end, name, "0", ".", start, end, rgb,
               fmt(combined), code,
               cur["famHist"] if cur else "", cur["cooc"] if cur else "",
               cur["seg"] if cur else "", cur["path"] if cur else "",
               cc_str, bridges, carriers, ukb, sugg,
               caputo, parsons, li, easton, hgvsp, mouse]
        lines.append("\t".join(row))
    return lines

for asm, curmap, sizes, out in (("38", cur38, HG38SIZES, "BRCAmfaZantiHg38"),
                                ("19", cur19, HG19SIZES, "BRCAmfaZantiHg19")):
    bed = WORK + "/%s.bed" % out
    with open(bed, "w") as fh:
        fh.write("\n".join(buildBed(asm, curmap)) + "\n")
    bash("bedSort %s %s" % (bed, bed))

# ---------------------------------------------------------------------------
# 4. autoSql and bigBed.
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

for out, sizes in (("BRCAmfaZantiHg38", HG38SIZES), ("BRCAmfaZantiHg19", HG19SIZES)):
    bash("bedToBigBed -as=%s/BRCAmlaZanti.as -type=bed9+17 -tab %s/%s.bed %s %s/%s.bb"
         % (WORK, WORK, out, sizes, WORK, out))

# ---------------------------------------------------------------------------
# 5. Report.
# ---------------------------------------------------------------------------
print("=== BUILD SUMMARY ===")
print("Zanti variants dropped (N/A / no computable LR): %d" % zan_dropped)
print("Membership (hg38): both=%d  Zanti-only=%d  multifactorial-only=%d"
      % (n_both, n_zan_only, n_mf_only))
print("Variants left with no evidence at all after CC swap: %d" % n_no_evidence_left)
print("Color counts (hg38): %s" % color_count)
print("Direction conflicts (MF vs ccLR opposite): %d" % len(conflicts))
conflicts.sort(key=lambda x: abs(__import__("math").log10(x[2]) if x[2] > 0 else 0),
               reverse=True)
print("Top conflicts (key, MFproduct, ccLR, combined):")
for c in conflicts[:15]:
    print("   ", c)
with open(WORK + "/directionConflicts.tsv", "w") as fh:
    fh.write("variant\tmultifactorialProduct\tzantiCcLR\tcombinedLR\n")
    for c in conflicts:
        fh.write("%s\t%s\t%s\t%s\n" % c)
print("Conflicts written to directionConflicts.tsv")
