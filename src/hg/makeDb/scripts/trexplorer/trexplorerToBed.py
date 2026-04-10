#!/usr/bin/env python3
"""Convert TRExplorer catalog TSV to BED9+ format for bigBed conversion.

Reads the gzipped TSV catalog and writes a tab-separated BED file to stdout.

Usage:
    trexplorerToBed.py <input.tsv.gz> > trexplorer.bed
"""

import gzip
import sys

HET_BINS = [
    (0.1, "0,0,180"),       # het < 0.1: dark blue (nearly monomorphic)
    (0.3, "70,130,230"),    # het 0.1-0.3: medium blue (low diversity)
    (0.5, "180,130,200"),   # het 0.3-0.5: light purple (moderate diversity)
    (0.7, "230,100,80"),    # het 0.5-0.7: salmon (high diversity)
    (999, "180,0,0"),       # het >= 0.7: dark red (very high diversity)
]
NO_DATA_COLOR = "128,128,128"  # gray when no allele freq data


def hetToColor(het):
    """Map heterozygosity value to an RGB color string."""
    if het < 0:
        return NO_DATA_COLOR
    for threshold, color in HET_BINS:
        if het < threshold:
            return color
    return HET_BINS[-1][1]


def computeHet(histStr1, histStr2):
    """Compute expected heterozygosity from one or two histogram strings.

    Pools allele counts from both histograms, then computes
    het = 1 - sum(p_i^2). Histograms are space-separated size=count pairs.
    """
    from collections import defaultdict
    counts = defaultdict(int)
    for histStr in (histStr1, histStr2):
        if not histStr:
            continue
        for pair in histStr.split():
            if "=" in pair:
                size, count = pair.split("=", 1)
                try:
                    counts[size] += int(count)
                except ValueError:
                    pass
    if not counts:
        return -1.0
    total = sum(counts.values())
    if total == 0:
        return -1.0
    sumPSq = sum((c / total) ** 2 for c in counts.values())
    return round(1.0 - sumPSq, 3)


def truncateMotif(motif, maxLen=25):
    """Truncate motif to maxLen characters with '..' in the middle."""
    if len(motif) <= maxLen:
        return motif
    keepLen = maxLen - 2
    leftLen = (keepLen + 1) // 2
    rightLen = keepLen - leftLen
    return motif[:leftLen] + ".." + motif[-rightLen:]


def parseHistogram(histStr):
    """Parse histogram like '3x:11,4x:3809' into logfmt string.

    Returns space-separated size=count pairs, e.g. '3=11 4=3809'.
    Preserves the original order.
    """
    if not histStr:
        return ""
    pairs = []
    for entry in histStr.split(","):
        parts = entry.split(":")
        if len(parts) == 2:
            sizeStr = parts[0]
            countStr = parts[1]
            # Remove trailing 'x' from size (e.g. '4x' -> '4')
            if sizeStr.endswith("x"):
                sizeStr = sizeStr[:-1]
            pairs.append(sizeStr + "=" + countStr)
    return " ".join(pairs)


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

            # Allele histograms (logfmt: space-separated size=count pairs)
            tenKHist = parseHistogram(fields[col["TenK10K_AlleleHistogram"]])
            tenKNum = fields[col["TenK10K_NumCalledAlleles"]] or "0"

            hprcHist = parseHistogram(fields[col["HPRC256_AlleleHistogram"]])
            hprcNum = fields[col["HPRC256_NumCalledAlleles"]] or "0"

            aouNum = fields[col["AoU1027_NumCalledAlleles"]] or "0"

            # Compute heterozygosity pooling TenK + HPRC histograms
            het = computeHet(tenKHist, hprcHist)
            color = hetToColor(het)

            outFields = [
                chrom, start, end,
                name, score, ".",
                start, end, color,
                locusId,
                refMotif, canMotif,
                str(motifSize), str(numRepeats), f"{purity:.3f}",
                str(regionSize), source,
                diseaseLocus, diseaseMotif,
                geneRegion, geneName,
                str(het),
                tenKHist, tenKNum,
                hprcHist, hprcNum,
                aouNum,
            ]

            print("\t".join(outFields))
            count += 1
            if count % 1000000 == 0:
                print(f"  Processed {count} loci...", file=sys.stderr)

    print(f"Done. Wrote {count} records.", file=sys.stderr)


if __name__ == "__main__":
    main()
