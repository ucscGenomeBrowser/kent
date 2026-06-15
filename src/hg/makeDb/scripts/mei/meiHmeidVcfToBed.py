#!/usr/bin/env python3
"""Convert the HMEID v1.1 site-level MEI VCF to a bed9+ file.

Input: MEI.GRCh38.HMEIDv1.1.vcf.gz from the HMEID database
(http://bigdata.ibp.ac.cn/HMEID/download/), which contains 36,699
non-reference Mobile Element Insertions called by MELT v2.1.5 on
5,675 short-read whole genomes: 2,998 Chinese samples from the NyuWa
cohort (~26.2x coverage) and 2,677 samples from the 1000 Genomes
Project (~7.4x coverage).

The VCF is site-level (no FORMAT / per-sample columns); INFO carries
per-cohort and per-1KGP-super-population AC/AN/AF as well as the MELT
fields SVTYPE (ALU/LINE1/SVA/HERVK), SVLEN, TSD and ASSESS.

Insertions are drawn as a 1 bp anchor block, matching the lrSv / HGSVC3
/ DeepMEI MEI tracks:
  chromStart = POS - 1   (1-based VCF anchor -> 0-based)
  chromEnd   = chromStart + 1
"""

import gzip
import sys


COLOR_BY_CLASS = {
    "Alu":   "0,114,178",     # blue (Okabe-Ito)
    "L1":    "213,94,0",      # vermillion
    "SVA":   "0,158,115",     # bluish green
    "HERVK": "204,121,167",   # reddish purple
}

SVTYPE_TO_CLASS = {
    "ALU":   "Alu",
    "LINE1": "L1",
    "SVA":   "SVA",
    "HERVK": "HERVK",
}


def parseInfo(infoField):
    fields = {}
    for tok in infoField.split(";"):
        if not tok:
            continue
        if "=" in tok:
            k, v = tok.split("=", 1)
            fields[k] = v
        else:
            fields[tok] = True
    return fields


def getInt(info, key, default=0):
    v = info.get(key)
    if v is None or v == "" or v == ".":
        return default
    return int(v)


def getFloat(info, key, default=0.0):
    v = info.get(key)
    if v is None or v == "" or v == ".":
        return default
    return float(v)


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: meiHmeidVcfToBed.py input.vcf.gz chrom.sizes output.bed")

    inFn, sizesFn, outFn = sys.argv[1:]

    chromSizes = {}
    with open(sizesFn) as f:
        for line in f:
            chrom, size = line.rstrip("\n").split("\t")
            chromSizes[chrom] = int(size)

    nIn = 0
    nOut = 0
    nSkippedChrom = 0
    nSkippedBoundary = 0
    nSkippedSvType = 0
    classCounts = {}

    opener = gzip.open if inFn.endswith(".gz") else open
    out = open(outFn, "w")

    with opener(inFn, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue

            nIn += 1
            row = line.rstrip("\n").split("\t")
            chrom = row[0]
            pos = int(row[1])
            info = parseInfo(row[7])

            svType = info.get("SVTYPE", "")
            cls = SVTYPE_TO_CLASS.get(svType)
            if cls is None:
                nSkippedSvType += 1
                sys.stderr.write(f"WARN: skipping unknown SVTYPE {svType} at {chrom}:{pos}\n")
                continue

            chromStart = pos - 1
            chromEnd = chromStart + 1

            if chrom not in chromSizes:
                nSkippedChrom += 1
                sys.stderr.write(f"WARN: skipping {chrom}:{pos}, chrom not in chrom.sizes\n")
                continue
            if chromEnd > chromSizes[chrom]:
                nSkippedBoundary += 1
                sys.stderr.write(
                    f"WARN: skipping {chrom}:{pos}, end {chromEnd} > chromSize {chromSizes[chrom]}\n")
                continue

            altAC = getInt(info, "AC")
            AN = getInt(info, "AN")
            altAF = getFloat(info, "AF")

            svLen = getInt(info, "SVLEN", -1)
            tsd = info.get("TSD", ".")
            if tsd in ("null", "", "NULL"):
                tsd = "."
            assess = getInt(info, "ASSESS")

            score = max(0, min(1000, int(round(altAF * 1000))))
            color = COLOR_BY_CLASS[cls]
            name = f"{cls}-{altAC}"
            classCounts[cls] = classCounts.get(cls, 0) + 1

            fields = [
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                str(score),
                ".",
                str(chromStart),
                str(chromEnd),
                color,
                cls,
                str(svLen),
                tsd,
                str(assess),
                str(altAC),
                str(AN),
                f"{altAF:.6f}",
                str(getInt(info, "Nyuwa_AC")),
                str(getInt(info, "Nyuwa_AN")),
                f"{getFloat(info, 'Nyuwa_AF'):.6f}",
                str(getInt(info, "1KGP_AC")),
                str(getInt(info, "1KGP_AN")),
                f"{getFloat(info, '1KGP_AF'):.6f}",
                str(getInt(info, "AFR_AC")),
                str(getInt(info, "AFR_AN")),
                f"{getFloat(info, 'AFR_AF'):.6f}",
                str(getInt(info, "AMR_AC")),
                str(getInt(info, "AMR_AN")),
                f"{getFloat(info, 'AMR_AF'):.6f}",
                str(getInt(info, "EAS_AC")),
                str(getInt(info, "EAS_AN")),
                f"{getFloat(info, 'EAS_AF'):.6f}",
                str(getInt(info, "EUR_AC")),
                str(getInt(info, "EUR_AN")),
                f"{getFloat(info, 'EUR_AF'):.6f}",
                str(getInt(info, "SAS_AC")),
                str(getInt(info, "SAS_AN")),
                f"{getFloat(info, 'SAS_AF'):.6f}",
            ]
            out.write("\t".join(fields) + "\n")
            nOut += 1

    out.close()

    classStr = ", ".join(f"{k} {v}" for k, v in sorted(classCounts.items()))
    sys.stderr.write(
        f"Read {nIn} records, wrote {nOut}, "
        f"skipped {nSkippedSvType} (unknown SVTYPE) + {nSkippedChrom} (chrom missing) + "
        f"{nSkippedBoundary} (off chrom end)\n"
        f"Class distribution: {classStr}.\n")


if __name__ == "__main__":
    main()
