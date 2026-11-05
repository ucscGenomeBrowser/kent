#!/usr/bin/env python3
"""Convert one chromosome of the gnomAD long-read (lrGS) release 0.1 VCFs to
two BED files: structural variants (>= 50 bp) and tandem repeats.

The release has two cohorts with separate VCFs: All of Us (1,027 samples,
sites-only) and HPRC/HGSVC (292 samples, with genotypes). Both are read with
bcftools query (sample columns are never parsed) and merged:
  - SVs by exact allele (POS, REF, ALT); a merged SV keeps per-cohort values.
  - TRs by TRID (the TR locus); per-cohort allele lists are kept.

Usage: gnomadLrToBed.py <chrom> <aouVcf> <hgsvcVcf> <outSvBed> <outTrBed>
"""

import sys, os, math, hashlib, subprocess

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lrSv"))
from lrSvCommon import svColor, shortLen

MINSVLEN = 50
SELECT = f'allele_type="trv" || (allele_type!="snv" && abs(allele_length)>={MINSVLEN})'

BASEFIELDS = ["POS", "ID", "REF", "ALT", "FILTER", "INFO/allele_type", "INFO/allele_subtype",
    "INFO/allele_length", "INFO/AC", "INFO/AN", "INFO/AF", "INFO/nhomalt", "INFO/grpmax",
    "INFO/AF_grpmax", "INFO/SUB_FAMILY", "INFO/ORIGIN", "INFO/REGION", "INFO/SOURCE",
    "INFO/gnomAD_V4_match_ID", "INFO/dbSNP_ID", "INFO/TRID", "INFO/MOTIFS",
    "INFO/HOMOPOLYMER_TRV", "INFO/gnomAD_STR"]

# SVAnnotate gene consequences, in order of severity, with display labels
GENEEFFECTS = [("PREDICTED_LOF", "LoF"), ("PREDICTED_COPY_GAIN", "copy gain"),
    ("PREDICTED_INTRAGENIC_EXON_DUP", "intragenic exon dup"),
    ("PREDICTED_PARTIAL_EXON_DUP", "partial exon dup"), ("PREDICTED_TSS_DUP", "TSS dup"),
    ("PREDICTED_DUP_PARTIAL", "partial dup"), ("PREDICTED_PARTIAL_DISPERSED_DUP", "partial dispersed dup"),
    ("PREDICTED_INV_SPAN", "inversion span"), ("PREDICTED_MSV_EXON_OVERLAP", "multiallelic exon overlap"),
    ("PREDICTED_BREAKEND_EXONIC", "breakend exonic"), ("PREDICTED_PROMOTER", "promoter"),
    ("PREDICTED_UTR", "UTR"), ("PREDICTED_INTRONIC", "intronic"),
    ("PREDICTED_NEAREST_TSS", "nearest TSS")]

POPS = ["afr", "amr", "asj", "eas", "nfe", "sas"]
POPNAMES = {"afr": "African/African American", "amr": "Admixed American", "asj": "Ashkenazi Jewish",
    "eas": "East Asian", "nfe": "European (non-Finnish)", "sas": "South Asian"}

COHORTS = [("aou", "All of Us", "all_of_us"), ("hg", "HPRC/HGSVC", "hgsvc_hprc")]

TYPECOLORS = {"del": svColor("DEL"), "ins": svColor("INS"), "dup": svColor("DUP")}
TRCOLOR = "230,159,0"    # Okabe-Ito orange, as on the gnomAD browser

GNOMADURL = "https://long-read-beta.gnomad.broadinstitute.org"

def headerInfoIds(vcf):
    " return set of INFO IDs in the VCF header "
    out = subprocess.run(["bcftools", "view", "-h", vcf], capture_output=True, text=True, check=True).stdout
    ids = set()
    for line in out.splitlines():
        if line.startswith("##INFO=<ID="):
            ids.add(line[len("##INFO=<ID="):].split(",")[0])
    return ids

def readCohort(vcf, chrom):
    """ yield one dict per selected VCF record. Fields not in this VCF's header
    (e.g. population AFs absent from All of Us) are empty. """
    hdr = headerInfoIds(vcf)
    fields = list(BASEFIELDS) + ["INFO/" + g for g, _ in GENEEFFECTS] + ["INFO/AF_" + p for p in POPS]
    present = [f for f in fields if not f.startswith("INFO/") or f[5:] in hdr]
    fmt = "\t".join("%" + f for f in present) + "\n"
    cmd = ["bcftools", "query", "-r", chrom, "-i", SELECT, "-f", fmt, vcf]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True, bufsize=1 << 20)
    for line in proc.stdout:
        vals = line.rstrip("\n").split("\t")
        assert len(vals) == len(present), line[:200]
        rec = {f.replace("INFO/", ""): ("" if v == "." else v) for f, v in zip(present, vals)}
        yield rec
    if proc.wait() != 0:
        sys.exit("bcftools failed: %s" % " ".join(cmd))

def afColor(af):
    """ light grey (AF <= 1e-4) to red (AF = 1), log scale. Mimics the
    gnomAD haplotype-count scale, but on allele frequency. """
    if af is None or af <= 0:
        t = 0.0
    else:
        t = min(1.0, max(0.0, (math.log10(af) + 4) / 4))
    lo, hi = (210, 210, 210), (200, 0, 0)
    return ",".join(str(round(lo[i] + t * (hi[i] - lo[i]))) for i in range(3))

def fmtAf(x):
    return "%.4g" % x if x is not None else ""

def toInt(s):
    return int(s) if s not in ("", None) else None

def toFloat(s):
    return float(s) if s not in ("", None) else None

def geneEffects(rec):
    " 'LoF: A, B; intronic: C' "
    parts = []
    for tag, label in GENEEFFECTS:
        v = rec.get(tag, "")
        if v:
            parts.append("%s: %s" % (label, v.replace(",", " ")))
    return "; ".join(parts)

def popAfs(rec):
    parts = []
    for p in POPS:
        v = rec.get("AF_" + p, "")
        if v != "":
            parts.append("%s %s" % (p, fmtAf(float(v))))
    return " ".join(parts)

def combined(cohortVals):
    " cohortVals: list of (ac, an). Return AC, AN, AF over cohorts that have the variant "
    ac = sum(a for a, n in cohortVals if a is not None)
    an = sum(n for a, n in cohortVals if n is not None)
    af = ac / an if an else None
    return ac, an, af

def linkField(recs, idFunc):
    " 'variantId?...&lr_cohort=x|label' for each cohort, comma-separated, for urls $$ "
    links = []
    for key, label, urlCohort in COHORTS:
        r = recs.get(key)
        if r:
            links.append("%s?dataset=gnomad_r4_lr&lr_cohort=%s|%s" % (idFunc(r), urlCohort, label))
    return ",".join(links)

def mouseOver(head, af, ac, an, cohortLines, filters, extra):
    """ HTML mouseover: header line, combined AF, one line per cohort that has the
    variant, the FILTER values if not PASS, then extra lines """
    lines = [head, "<b>AF</b>: %s (%d/%d)" % (fmtAf(af) if af is not None else "0", ac, an)]
    lines += cohortLines
    if filters and filters != ["PASS"]:
        lines.append("<b>Filter</b>: " + ", ".join(filters))
    lines += [e for e in extra if e]
    return "<br>".join(lines)

def cohortLine(label, af, ac, an, sizeStr=None):
    line = "<b>%s</b>: %s (%s/%s)" % (label, fmtAf(af) if af is not None else "0", ac, an)
    if sizeStr:
        line += ", sizes " + sizeStr
    return line

def trunc(s, maxLen):
    return s if len(s) <= maxLen else s[:maxLen - 3] + "..."

def svCoords(rec, chrom):
    """ 0-based half-open coordinates. DEL: the deleted bases (REF minus the padding
    base). INS: the 1-bp anchor base. DUP (tandem): the duplicated reference
    segment from ORIGIN if it is on this chromosome, else the anchor base. """
    pos = int(rec["POS"])
    atype = rec["allele_type"]
    refLen = len(rec["REF"])
    if atype == "del":
        return pos, pos - 1 + refLen
    if atype == "dup" and rec["ORIGIN"].startswith(chrom + ":"):
        o = rec["ORIGIN"].split(":", 1)[1].rsplit("_", 1)[0]
        s, e = o.split("-")
        return int(s) - 1, int(e)
    return pos - 1, pos

def svName(atype, alen):
    return "%s-%s" % (atype.upper(), shortLen(abs(alen)))

def writeSvs(chrom, svs, ofh):
    " svs: key -> {cohortKey: rec} "
    rows = []
    for key, recs in svs.items():
        first = recs.get("hg") or recs.get("aou")
        atype = first["allele_type"]
        alen = int(first["allele_length"])
        start, end = svCoords(first, chrom)
        acan = []
        cohortCols = []
        cohortLines = []
        for ck, label, _ in COHORTS:
            r = recs.get(ck)
            if r:
                ac, an, af = toInt(r["AC"]), toInt(r["AN"]), toFloat(r["AF"])
                acan.append((ac, an))
                cohortCols += [str(ac), str(an), fmtAf(af), r["nhomalt"], r["FILTER"]]
                cohortLines.append(cohortLine(label, af, ac, an))
            else:
                cohortCols += ["", "", "", "", ""]
        ac, an, af = combined(acan)
        filters = sorted(set(f for r in recs.values() for f in r["FILTER"].split(";") if f))
        cohorts = ",".join(label for ck, label, _ in COHORTS if ck in recs)
        hg = recs.get("hg", {})
        subtype = first["allele_subtype"]
        genes = geneEffects(first)
        head = "<b>%s</b> %s bp %s %s" % (atype.upper(), "{:,}".format(abs(alen)), subtype, first["SUB_FAMILY"])
        mo = mouseOver(head, af, ac, an, cohortLines, filters,
            ["<b>Genes</b>: " + trunc(genes, 200) if genes else ""])
        row = [chrom, str(start), str(end), svName(atype, alen), "0", ".", str(start), str(end),
            TYPECOLORS[atype],
            atype.upper(), str(abs(alen)), subtype, first["SUB_FAMILY"], cohorts, ",".join(filters),
            str(ac), str(an), fmtAf(af) if af is not None else "0"] + cohortCols + [
            hg.get("grpmax", ""), fmtAf(toFloat(hg.get("AF_grpmax", ""))), popAfs(hg) if hg else "",
            first["ORIGIN"], first["REGION"], genes,
            first["gnomAD_V4_match_ID"], first["dbSNP_ID"], first["ID"],
            linkField(recs, lambda r: r["ID"] + "~1"),
            afColor(af), mo]
        rows.append(row)
    rows.sort(key=lambda r: (int(r[1]), int(r[2])))
    for row in rows:
        ofh.write("\t".join(row) + "\n")
    return len(rows)

def trAlleles(rec):
    """ return (altAc, nAltAlleles, sizeStr). sizeStr lists each ALT allele
    as 'length diff vs REF in bp:AC', e.g. '-3:2 +3:5', sorted by length """
    refLen = len(rec["REF"]) - 1
    alts = rec["ALT"].split(",") if rec["ALT"] else []
    acs = [int(x) for x in rec["AC"].split(",")] if rec["AC"] else [0] * len(alts)
    bySize = {}
    for alt, ac in zip(alts, acs):
        d = (len(alt) - 1) - refLen
        bySize[d] = bySize.get(d, 0) + ac
    sizeStr = " ".join("%+d:%d" % (d, c) for d, c in sorted(bySize.items()) if c > 0)
    return sum(acs), len(alts), sizeStr

def writeTrs(chrom, trs, ofh):
    rows = []
    for trid, recs in trs.items():
        first = recs.get("hg") or recs.get("aou")
        pos = int(first["POS"])
        start = pos
        end = pos - 1 + len(first["REF"])
        acan = []
        cohortCols = []
        cohortLines = []
        for ck, label, _ in COHORTS:
            r = recs.get(ck)
            if r:
                altAc, nAlt, sizeStr = trAlleles(r)
                an = toInt(r["AN"])
                cAf = altAc / an if an else None
                acan.append((altAc, an))
                cohortCols += [str(altAc), str(an), fmtAf(cAf), str(nAlt), sizeStr, r["FILTER"]]
                cohortLines.append(cohortLine(label, cAf, altAc, an, trunc(sizeStr, 120)))
            else:
                cohortCols += ["", "", "", "", "", ""]
        ac, an, af = combined(acan)
        filters = sorted(set(f for r in recs.values() for f in r["FILTER"].split(";") if f))
        cohorts = ",".join(label for ck, label, _ in COHORTS if ck in recs)
        motifs = first["MOTIFS"]
        name = trunc(motifs, 20)
        head = "<b>%s</b> repeat, %d bp in reference" % (trunc(motifs, 60), end - start)
        mo = mouseOver(head, af, ac, an, cohortLines, filters, [])
        row = [chrom, str(start), str(end), name, "0", ".", str(start), str(end), TRCOLOR,
            motifs, str(end - start), first["SOURCE"], cohorts, ",".join(filters),
            str(ac), str(an), fmtAf(af) if af is not None else "0"] + cohortCols + [
            "yes" if first["HOMOPOLYMER_TRV"] == "1" else "", first["REGION"],
            first["gnomAD_STR"], first["TRID"], afColor(af), mo]
        rows.append(row)
    rows.sort(key=lambda r: (int(r[1]), int(r[2])))
    for row in rows:
        ofh.write("\t".join(row) + "\n")
    return len(rows)

def main():
    chrom, aouVcf, hgVcf, outSv, outTr = sys.argv[1:6]
    svs, trs = {}, {}
    counts = {}
    for (ck, label, _), vcf in zip(COHORTS, [aouVcf, hgVcf]):
        for rec in readCohort(vcf, chrom):
            atype = rec["allele_type"]
            if atype == "trv":
                trid = rec["TRID"]
                if ck in trs.get(trid, {}):
                    sys.exit("duplicate TRID in one cohort: %s" % trid)
                trs.setdefault(trid, {})[ck] = rec
                counts[(ck, "trv")] = counts.get((ck, "trv"), 0) + 1
            elif atype in TYPECOLORS:
                assert "," not in rec["ALT"], "multiallelic SV " + rec["ID"]
                key = (rec["POS"], hashlib.md5((rec["REF"] + ">" + rec["ALT"]).encode()).hexdigest())
                if ck in svs.get(key, {}):
                    sys.exit("duplicate SV allele in one cohort: %s" % rec["ID"])
                svs.setdefault(key, {})[ck] = rec
                counts[(ck, atype)] = counts.get((ck, atype), 0) + 1
            else:
                sys.exit("unexpected allele_type %s in %s" % (atype, rec["ID"]))
    with open(outSv, "w") as ofh:
        nSv = writeSvs(chrom, svs, ofh)
    with open(outTr, "w") as ofh:
        nTr = writeTrs(chrom, trs, ofh)
    for (ck, t), n in sorted(counts.items()):
        print("%s\tinput\t%s\t%s\t%d" % (chrom, ck, t, n))
    print("%s\toutput\tsv\tall\t%d" % (chrom, nSv))
    print("%s\toutput\ttr\tall\t%d" % (chrom, nTr))

main()
