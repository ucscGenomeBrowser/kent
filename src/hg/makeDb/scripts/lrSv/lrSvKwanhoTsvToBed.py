#!/usr/bin/env python3
"""Convert the Kim 2026 PD long-read SV catalog (media-13.txt) to BED9+.

Usage:
    lrSvKwanhoTsvToBed.py media-13.txt output.bed

The source TSV has thousands-separator commas inside quoted numeric fields
(e.g. "10,889"), so we parse it with the csv module.
"""

import csv
import re
import sys

SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "DUP": "0,160,0",      # green
    "INV": "230,140,0",    # orange
}


def toInt(s):
    if s is None or s == "":
        return 0
    s = s.replace(",", "")
    try:
        return int(float(s))
    except ValueError:
        return 0


def toFloat(s):
    if s is None or s == "":
        return 0.0
    s = s.strip().rstrip("%")
    s = s.replace(",", "")
    try:
        return float(s)
    except ValueError:
        return 0.0


def pctToFrac(s):
    """'5.90%' -> 0.059; bare numbers left unchanged."""
    if s is None or s == "":
        return 0.0
    s = s.strip()
    if s.endswith("%"):
        try:
            return float(s[:-1].replace(",", "")) / 100.0
        except ValueError:
            return 0.0
    try:
        return float(s.replace(",", ""))
    except ValueError:
        return 0.0


_NAME_RE = re.compile(r"'([^']+)'")


def carrierList(cell):
    """Parse a carrier tuple cell like "('BN0651', 'BN1554')" into "BN0651,BN1554"."""
    if not cell:
        return "", 0
    names = _NAME_RE.findall(cell)
    return ",".join(names), len(names)


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]

    with open(inPath, newline="") as fIn, open(outPath, "w") as fOut:
        reader = csv.DictReader(fIn, delimiter="\t")
        for row in reader:
            chrom = row["Chromosome"]
            if not chrom.startswith("chr"):
                chrom = "chr" + chrom

            chromStart = toInt(row["Start"])
            chromEnd = toInt(row["End"])
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            svType = row["SV type"]
            svLen = abs(toInt(row["SV length"]))

            color = SV_COLORS.get(svType, "100,100,100")

            pdStr, nPd = carrierList(row.get("PD CARRIERS", ""))
            hcStr, nHc = carrierList(row.get("HC CARRIERS", ""))
            ilbdStr, nIlbd = carrierList(row.get("ILBD CARRIERS", ""))

            bedRow = [
                chrom,
                str(chromStart),
                str(chromEnd),
                row["ID"],
                "0",
                ".",
                str(chromStart),
                str(chromEnd),
                color,
                svType,
                str(svLen),
                row.get("Size bin", ""),
                str(toInt(row.get("qual", "0"))),
                str(toInt(row.get("SUPP", "0"))),
                row.get("SUPP VEC", ""),
                f"{toFloat(row.get('MISSING RATE', '0')):.6f}",
                f"{pctToFrac(row.get('CASE RATE', '0')):.6f}",
                f"{pctToFrac(row.get('CONTROL RATE', '0')):.6f}",
                f"{toFloat(row.get('DIFFERENTIAL RATE', '0')):.6f}",
                f"{toFloat(row.get('AF PD', '0')):.6f}",
                f"{toFloat(row.get('AF HC', '0')):.6f}",
                f"{toFloat(row.get('AF ILBD', '0')):.6f}",
                str(toInt(row.get("AC PD", "0"))),
                str(toInt(row.get("AC HC", "0"))),
                str(toInt(row.get("AC ILBD", "0"))),
                str(toInt(row.get("AN PD", "0"))),
                str(toInt(row.get("AN HC", "0"))),
                str(toInt(row.get("AN ILBD", "0"))),
                str(nPd),
                str(nHc),
                str(nIlbd),
                str(toInt(row.get("LD SNPS COUNT", "0"))),
                str(toInt(row.get("TOTAL SNPS NEARBY", "0"))),
                f"{toFloat(row.get('AVG MAP QUALITY', '0')):.3f}",
                f"{toFloat(row.get('AVG READS PER SAMPLE', '0')):.3f}",
                pdStr,
                hcStr,
                ilbdStr,
            ]
            fOut.write("\t".join(bedRow) + "\n")


if __name__ == "__main__":
    main()
