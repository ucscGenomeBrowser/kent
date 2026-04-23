#!/usr/bin/env python3
"""Convert the Gustafson 2024 1000G ONT Jasmine-merged SV VCF to BED9+.

Usage:
    lrSvGustafsonVcfToBed.py input.vcf.gz output.bed

Source:
    https://s3.amazonaws.com/1000g-ont/Gustafson_etal_2024_preprint_SUPPLEMENTAL/
    20240423_jasmine_intrasample_noBND_custom_suppvec_alphanumeric_header_JASMINE.vcf.gz
Paper:
    Gustafson et al. 2024, bioRxiv / Genome Res, PMID 39358015.
"""

import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType

SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "DUP": "0,160,0",      # green
    "INV": "230,140,0",    # orange
}

# Jasmine END on chrM can overshoot by one base; clip to chrM length.
CHRM_LEN = 16569


def openVcf(path):
    """Open a local .vcf.gz via gzip; everything else as plain text."""
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path, "rt")


def parseInfo(infoStr):
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]

    with openVcf(inPath) as fIn, open(outPath, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            rowName = fields[2]
            info = parseInfo(fields[7])

            svTypeRaw = info.get("SVTYPE", ".")
            svType = normalizeSvType(svTypeRaw)
            end = int(info.get("END", pos))
            try:
                svLenRaw = int(float(info.get("SVLEN", "0")))
            except ValueError:
                svLenRaw = 0
            try:
                supp = int(info.get("SUPP", "0"))
            except ValueError:
                supp = 0
            try:
                varCalls = int(info.get("VARCALLS", "0"))
            except ValueError:
                varCalls = 0
            precise = 1 if "PRECISE" in info else 0
            strands = info.get("STRANDS", "")
            if strands == "??":
                strands = ""

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1
            if chrom == "chrM" and chromEnd > CHRM_LEN:
                chromEnd = CHRM_LEN

            svLen = chromEnd - chromStart
            if svType in ("INS", "MEI"):
                insLen = abs(svLenRaw)
            else:
                insLen = 0

            # Gustafson callset is site-level without AC; 2 * SUPP
            # is the diploid carrier upper bound.
            ac = supp * 2

            color = SV_COLORS.get(svType, "100,100,100")

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
                str(supp),
                str(varCalls),
                str(precise),
                strands,
            ]
            fOut.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
