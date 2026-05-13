#!/usr/bin/env python3
"""Convert the HPRC release-2 wave-decomposed pangenome VCF to BED9+.

Filters to SV-sized alleles (|LEN| >= 50 bp) and inversions (INV flag).
Expands multi-allelic rows so each ALT becomes its own BED row.

Usage:
    lrSvHprc2VcfToBed.py input.vcf.gz output.bed

Source:
    https://humanpangenome.org/hprc-data-release-2/
    Graph/VCF from the HPRC v2.0 minigraph-cactus GRCh38 release
    (see https://github.com/human-pangenomics/hprc_intermediate_assembly/
        blob/main/data_tables/pangenomes/alignments_v2.0.csv).
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

MIN_SV_LEN = 50

SV_COLORS = {
    "INS":     "0,0,200",      # blue
    "DEL":     "200,0,0",      # red
    "CPX":     "140,0,200",    # purple
    "INV":     "230,140,0",    # orange
}


def parseInfo(infoStr):
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d


def splitMulti(s, n):
    """Split a per-allele INFO value; pad with '' if missing."""
    if s is None:
        return [""] * n
    parts = s.split(",")
    if len(parts) < n:
        parts = parts + [""] * (n - len(parts))
    return parts


def deriveType(ref, alt):
    """Derive allele type when TYPE isn't in INFO."""
    dr, da = len(ref), len(alt)
    if dr == da:
        return "snp" if dr == 1 else "mnp"
    if dr == 1 and da > 1:
        return "ins"
    if da == 1 and dr > 1:
        return "del"
    return "complex"


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]
    opener = gzip.open if inPath.endswith(".gz") else open

    with opener(inPath, "rt") as fIn, open(outPath, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            rowId = fields[2]
            ref = fields[3]
            alts = fields[4].split(",")
            info = parseInfo(fields[7])

            n = len(alts)
            typeList = splitMulti(info.get("TYPE"), n)
            lenList = splitMulti(info.get("LEN"), n)
            acList = splitMulti(info.get("AC"), n)
            afList = splitMulti(info.get("AF"), n)

            isInv = "INV" in info
            try:
                nSamples = int(info.get("NS", "0"))
            except ValueError:
                nSamples = 0
            try:
                an = int(info.get("AN", "0"))
            except ValueError:
                an = 0
            try:
                snarlLevel = int(info.get("LV", "0"))
            except ValueError:
                snarlLevel = 0

            for i, alt in enumerate(alts):
                if alt in (".", "*"):
                    continue

                t = (typeList[i] or "").strip()
                if not t:
                    t = deriveType(ref, alt)

                lenStr = (lenList[i] or "").strip()
                try:
                    svLenVal = int(lenStr)
                except ValueError:
                    svLenVal = abs(len(alt) - len(ref))

                # Filter: keep SV-sized alleles plus INV flag records.
                if svLenVal < MIN_SV_LEN and not isInv:
                    continue

                # Normalize type for display (complex -> CPX via lrSvCommon).
                svType = normalizeSvType(t)
                if svType not in ("INS", "DEL", "CPX"):
                    svType = "CPX"
                if isInv:
                    svType = "INV"

                color = SV_COLORS.get(svType, "100,100,100")

                chromStart = pos - 1
                # INS: 1-bp anchor; DEL/CPX/INV: span the REF interval.
                if svType == "INS":
                    chromEnd = chromStart + 1
                else:
                    chromEnd = chromStart + len(ref)
                if chromEnd <= chromStart:
                    chromEnd = chromStart + 1

                try:
                    ac = int(acList[i])
                except ValueError:
                    ac = 0
                try:
                    af = float(afList[i])
                except ValueError:
                    af = 0.0

                svLen = chromEnd - chromStart
                # insLen: for INS use abs(ALT-REF) / svLenVal; for DEL/CPX/INV 0.
                if svType == "INS":
                    insLen = abs(len(alt) - len(ref))
                    if insLen == 0:
                        insLen = svLenVal
                else:
                    insLen = 0

                # ID: VCF ID if informative, else constructed. Truncate
                # very long snarl/ORIGIN-chain IDs to stay within bigBed's
                # 255-char limit.
                if rowId and rowId != ".":
                    baseId = f"{rowId}.{i+1}" if n > 1 else rowId
                else:
                    baseId = f"{chrom}:{pos}:{svType}:{svLenVal}"
                if len(baseId) > 200:
                    baseId = f"{chrom}:{pos}:{svType}:{svLenVal}"

                featLen = insLen if svType in ("INS", "MEI") else svLen
                name = svName(svType, featLen, ac)

                row = [
                    chrom,
                    str(chromStart),
                    str(chromEnd),
                    name,
                    "0",
                    ".",
                    str(chromStart),
                    str(chromEnd),
                    color,
                    svType,
                    str(svLen),
                    str(insLen),
                    str(ac),
                    str(an),
                    f"{af:.6f}",
                    str(nSamples),
                    str(snarlLevel),
                ]
                fOut.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
