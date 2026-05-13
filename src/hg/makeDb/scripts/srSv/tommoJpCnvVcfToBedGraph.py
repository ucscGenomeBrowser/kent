#!/usr/bin/env python3
"""Convert the ToMMo 48KJPN CNV Frequency VCF to two bedGraph files.

Emits per-1 kb-bin absolute carrier counts:
  - Loss track: number of samples with CN < 2 (CN0, CN1)
  - Gain track: number of samples with CN > 2 (CN3, CN4, CN5)
Bins where every sample is CN=2 (no CNV observed) are omitted.

Usage:
    lrSvTommoJpCnvVcfToBedGraph.py input.vcf.gz outLoss.bg outGain.bg
"""

import gzip
import sys


def parseInfo(infoStr):
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d


def parseAltList(altStr):
    out = []
    for a in altStr.split(","):
        a = a.strip().lstrip("<").rstrip(">")
        if a.startswith("CN"):
            try:
                out.append(int(a[2:]))
                continue
            except ValueError:
                pass
        out.append(-1)
    return out


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outLoss, outGain = sys.argv[1], sys.argv[2], sys.argv[3]
    opener = gzip.open if inPath.endswith(".gz") else open

    kept = 0
    with opener(inPath, "rt") as fIn, \
         open(outLoss, "w") as fLoss, open(outGain, "w") as fGain:
        for line in fIn:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            altList = parseAltList(fields[4])
            info = parseInfo(fields[7])

            end = int(info.get("END", pos))
            scParts = info.get("SC", "").split(",")

            lossCount = 0
            gainCount = 0
            for i, cn in enumerate(altList):
                try:
                    sc = int(scParts[i])
                except (ValueError, IndexError):
                    sc = 0
                if 0 <= cn < 2:
                    lossCount += sc
                elif cn > 2:
                    gainCount += sc

            if lossCount == 0 and gainCount == 0:
                continue

            chromStart = pos - 1
            chromEnd = end
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            if lossCount > 0:
                fLoss.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{lossCount}\n")
            if gainCount > 0:
                fGain.write(f"{chrom}\t{chromStart}\t{chromEnd}\t{gainCount}\n")
            kept += 1

    print(f"Wrote {kept} bins with any CNV carrier", file=sys.stderr)


if __name__ == "__main__":
    main()
