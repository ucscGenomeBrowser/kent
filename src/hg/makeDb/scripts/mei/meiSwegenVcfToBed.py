#!/usr/bin/env python3
"""Convert the SweGen MELT MEI VCF (GRCh37) to a bed9+ file with hg19 coords.

Input: MELT_SWEGEN.20180314.ALU_HERVK_LINE1_SVA.vcf from SweGen
(https://swefreq.nbis.se/dataset/SweGen), site-level MELT v2.0.2 output
on 1,000 Swedish whole-genome samples. The VCF has no per-sample
columns; INFO carries cohort allele counts and frequencies:
  MELT_AN  = number of allele observations (despite the name, this is
             allele count AC, not allele number)
  MELT_AF  = alt-allele frequency
Plus the MELT fields SVTYPE (ALU/LINE1/SVA/HERVK), SVLEN, TSD, ASSESS,
MEIINFO (subfamily), and INTERNAL (gene context).

The VCF uses GRCh37 contigs (no "chr" prefix). This script adds the
"chr" prefix; coordinates remain GRCh37 and must be lifted to GRCh38
downstream with liftOver before bigBed conversion.

Insertions are drawn as a 1 bp anchor block:
  chromStart = POS - 1   (1-based VCF anchor -> 0-based)
  chromEnd   = chromStart + 1
"""

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


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: meiSwegenVcfToBed.py input.vcf output.bed")

    inFn, outFn = sys.argv[1:]

    nIn = 0
    nOut = 0
    nSkippedSvType = 0
    nSkippedSeen = 0
    classCounts = {}

    out = open(outFn, "w")

    with open(inFn) as f:
        for line in f:
            if line.startswith("#"):
                continue
            nIn += 1
            row = line.rstrip("\n").split("\t")
            chrom = row[0]
            if not chrom.startswith("chr"):
                chrom = "chr" + chrom
            pos = int(row[1])
            filterStatus = row[6]
            info = parseInfo(row[7])

            svType = info.get("SVTYPE", "")
            cls = SVTYPE_TO_CLASS.get(svType)
            if cls is None:
                nSkippedSvType += 1
                sys.stderr.write(f"WARN: skipping unknown SVTYPE {svType} at {chrom}:{pos}\n")
                continue

            chromStart = pos - 1
            chromEnd = chromStart + 1

            svLen = int(info.get("SVLEN", -1))
            tsd = info.get("TSD", ".")
            if tsd in ("null", "", "NULL"):
                tsd = "."
            assess = int(info.get("ASSESS", 0))

            meiInfo = info.get("MEINFO", "")
            meiSubfamily = meiInfo.split(",", 1)[0] if meiInfo else "."
            if not meiSubfamily or meiSubfamily in ("null",):
                meiSubfamily = "."

            internal = info.get("INTERNAL", "")
            if internal in ("null,null", "null", "", ".") :
                internalGene = ""
            else:
                internalGene = internal

            ac = int(info.get("MELT_AN", 0))
            af = float(info.get("MELT_AF", 0))

            score = max(0, min(1000, int(round(af * 1000))))
            color = COLOR_BY_CLASS[cls]
            name = f"{cls}-{ac}"
            classCounts[cls] = classCounts.get(cls, 0) + 1

            out.write("\t".join([
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
                meiSubfamily,
                tsd,
                str(assess),
                internalGene,
                str(ac),
                f"{af:.6f}",
                filterStatus,
            ]) + "\n")
            nOut += 1

    out.close()

    classStr = ", ".join(f"{k} {v}" for k, v in sorted(classCounts.items()))
    sys.stderr.write(
        f"Read {nIn} records, wrote {nOut}, "
        f"skipped {nSkippedSvType} (unknown SVTYPE)\n"
        f"Class distribution: {classStr}.\n")


if __name__ == "__main__":
    main()
