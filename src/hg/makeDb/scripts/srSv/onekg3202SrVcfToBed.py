#!/usr/bin/env python3
"""Convert 1KG 3,202-sample GATK-SV short-read VCF to BED9+.

Short-read comparator track for the lrSv collection.

Source:
    https://ftp.1000genomes.ebi.ac.uk/vol1/ftp/data_collections/1000G_2504_high_coverage/working/20210124.SV_Illumina_Integration/1KGP_3202.gatksv_svtools_novelins.freeze_V3.wAF.vcf.gz
Paper:
    Byrska-Bishop et al. 2022, Cell, PMID 36055201.

Usage:
    lrSv1kg3202SrVcfToBed.py input.vcf.gz output.bed
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
    "CPX": "140,0,200",    # purple
    "CTX": "100,100,100",  # grey
    "CNV": "150,80,0",     # brown
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


def toInt(s):
    if not s:
        return 0
    try:
        return int(float(s))
    except ValueError:
        return 0


def toFloat(s):
    if not s:
        return 0.0
    try:
        return float(s)
    except ValueError:
        return 0.0


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
            rowName = fields[2]
            filt = fields[6]
            info = parseInfo(fields[7])

            svTypeRaw = info.get("SVTYPE", ".")
            svType = normalizeSvType(svTypeRaw)
            end = int(info.get("END", pos))
            srcSvLen = abs(toInt(info.get("SVLEN", "0")))

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            # Translocations: the END is on chr2; cap the item width to 1 bp
            # on the chromosome-1 side.
            chr2 = info.get("CHR2", "")
            if svType == "CTX" and chr2 and chr2 != chrom:
                chromEnd = chromStart + 1

            svLen = chromEnd - chromStart
            if svType in ("INS", "MEI"):
                insLen = srcSvLen
            else:
                insLen = 0

            color = SV_COLORS.get(svType, "100,100,100")

            ac = toInt(info.get("AC", "0"))
            an = toInt(info.get("AN", "0"))

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
                f"{toFloat(info.get('AF', '0')):.6f}",
                f"{toFloat(info.get('POPMAX_AF', '0')):.6f}",
                f"{toFloat(info.get('AFR_AF', '0')):.6f}",
                f"{toFloat(info.get('AMR_AF', '0')):.6f}",
                f"{toFloat(info.get('ASN_AF', '0')):.6f}",
                f"{toFloat(info.get('EUR_AF', '0')):.6f}",
                f"{toFloat(info.get('SAN_AF', '0')):.6f}",
                str(toInt(info.get("N_HET", "0"))),
                str(toInt(info.get("N_HOMALT", "0"))),
                info.get("ALGORITHMS", ""),
                info.get("SOURCE", ""),
                filt,
                chr2 if svType == "CTX" else "",
            ]
            fOut.write("\t".join(row) + "\n")


if __name__ == "__main__":
    main()
