#!/usr/bin/env python3
"""Convert TRExplorer catalog TSV to BED9+ format for bigBed conversion.

Reads the gzipped TSV catalog and writes a tab-separated BED file to stdout.

Usage:
    trexplorerToBed.py <input.tsv.gz> > trexplorer.bed
"""

import gzip
import sys

PERIOD_COLORS = {
    1: "255,0,0",       # mono: red
    2: "0,0,255",       # di: blue
    3: "0,128,0",       # tri: green
    4: "255,165,0",     # tetra: orange
    5: "128,0,128",     # penta: purple
    6: "70,130,180",    # hexa: steel blue
}
DEFAULT_COLOR = "128,128,128"  # gray for period > 6


def truncateMotif(motif, maxLen=25):
    """Truncate motif to maxLen characters with '..' in the middle."""
    if len(motif) <= maxLen:
        return motif
    keepLen = maxLen - 2
    leftLen = (keepLen + 1) // 2
    rightLen = keepLen - leftLen
    return motif[:leftLen] + ".." + motif[-rightLen:]


def parseHistogram(histStr):
    """Parse histogram like '3x:11,4x:3809' into (sizes, counts) strings.

    Preserves the original order so sizes and counts stay aligned.
    Returns two comma-separated strings: sizes and counts.
    """
    if not histStr:
        return "", ""
    sizes = []
    counts = []
    for entry in histStr.split(","):
        parts = entry.split(":")
        if len(parts) == 2:
            sizeStr = parts[0]
            countStr = parts[1]
            # Remove trailing 'x' from size (e.g. '4x' -> '4')
            if sizeStr.endswith("x"):
                sizeStr = sizeStr[:-1]
            sizes.append(sizeStr)
            counts.append(countStr)
    return ",".join(sizes), ",".join(counts)


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile = sys.argv[1]

    print("Processing TRExplorer catalog...", file=sys.stderr)
    count = 0

    with gzip.open(inFile, "rt") as f:
        header = f.readline().rstrip("\n").split("\t")
        col = {name: i for i, name in enumerate(header)}

        for line in f:
            fields = line.rstrip("\n").split("\t")

            chrom = fields[col["chrom"]]
            start = fields[col["start_0based"]]
            end = fields[col["end_1based"]]

            refMotif = fields[col["ReferenceMotif"]]
            canMotif = fields[col["CanonicalMotif"]]
            motifSize = int(fields[col["MotifSize"]])
            numRepeats = int(fields[col["NumRepeatsInReference"]])
            purity = float(fields[col["ReferenceRepeatPurity"]])
            regionSize = int(end) - int(start)
            source = fields[col["Source"]]
            region = fields[col["ReferenceRegion"]]
            locusId = fields[col["LocusId"]]

            diseaseLocus = fields[col["KnownDiseaseAssociatedLocus"]]
            diseaseMotif = fields[col["KnownDiseaseAssociatedMotif"]]

            # Gene annotation: prefer MANE, then Gencode
            geneRegion = fields[col["ManeGeneRegion"]]
            geneName = fields[col["ManeGeneName"]]
            if not geneRegion:
                geneRegion = fields[col["GencodeGeneRegion"]]
                geneName = fields[col["GencodeGeneName"]]

            # Name: truncated motif x repeat count
            name = truncateMotif(refMotif) + "x" + str(numRepeats)

            # Score: purity scaled to 0-1000
            score = str(int(round(purity * 1000)))

            # Color by motif size
            color = PERIOD_COLORS.get(motifSize, DEFAULT_COLOR)

            # Allele histograms
            tenKSizes, tenKCounts = parseHistogram(fields[col["TenK10K_AlleleHistogram"]])
            tenKNum = fields[col["TenK10K_NumCalledAlleles"]] or "0"

            hprcSizes, hprcCounts = parseHistogram(fields[col["HPRC256_AlleleHistogram"]])
            hprcNum = fields[col["HPRC256_NumCalledAlleles"]] or "0"

            aouNum = fields[col["AoU1027_NumCalledAlleles"]] or "0"

            outFields = [
                chrom, start, end,
                name, score, ".",
                start, end, color,
                locusId,
                refMotif, canMotif,
                str(motifSize), str(numRepeats), f"{purity:.6f}",
                str(regionSize), source, region,
                diseaseLocus, diseaseMotif,
                geneRegion, geneName,
                tenKSizes, tenKCounts, tenKNum,
                hprcSizes, hprcCounts, hprcNum,
                aouNum,
            ]

            print("\t".join(outFields))
            count += 1
            if count % 1000000 == 0:
                print(f"  Processed {count} loci...", file=sys.stderr)

    print(f"Done. Wrote {count} records.", file=sys.stderr)


if __name__ == "__main__":
    main()
