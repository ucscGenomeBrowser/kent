#!/usr/bin/env python3
"""Convert WebSTR CSV data to BED9+ format for bigBed conversion.

Reads hg38_repeats_withlinks.csv.gz and hg38_afreqs.csv.gz from the input
directory and writes a tab-separated BED file to stdout.

Usage:
    webstrToBed.py <inputDir> > webstr.bed
"""

import csv
import gzip
import sys
from collections import defaultdict

HET_BINS = [
    (0.1, "0,0,180"),       # het < 0.1: dark blue (nearly monomorphic)
    (0.3, "70,130,230"),    # het 0.1-0.3: medium blue (low diversity)
    (0.5, "180,130,200"),   # het 0.3-0.5: light purple (moderate diversity)
    (0.7, "230,100,80"),    # het 0.5-0.7: salmon (high diversity)
    (999, "180,0,0"),       # het >= 0.7: dark red (very high diversity)
]
NO_DATA_COLOR = "128,128,128"       # medium gray when no allele freq data
MONOMORPHIC_COLOR = "200,200,200"   # light gray when het == 0 (fixed allele)


def truncateMotif(motif, maxLen=25):
    """Truncate motif to maxLen characters with '..' in the middle."""
    if len(motif) <= maxLen:
        return motif
    keepLen = maxLen - 2
    leftLen = (keepLen + 1) // 2
    rightLen = keepLen - leftLen
    return motif[:leftLen] + ".." + motif[-rightLen:]


def hetToColor(het):
    """Map heterozygosity value to an RGB color string."""
    if het < 0:
        return NO_DATA_COLOR
    if het == 0:
        return MONOMORPHIC_COLOR
    for threshold, color in HET_BINS:
        if het < threshold:
            return color
    return HET_BINS[-1][1]


def computeHet(af):
    """Compute expected heterozygosity from allele freqs pooled across populations.

    Pools allele frequencies weighted by sample count, then computes
    het = 1 - sum(p_i^2).
    """
    if af is None:
        return -1.0
    totalN = 0
    weightedFreqs = defaultdict(float)
    for cohort in COHORT_ORDER:
        entry = af[cohort]
        n = entry["n"]
        if n == 0:
            continue
        totalN += n
        for allele, freq in zip(entry["alleles"], entry["freqs"]):
            weightedFreqs[allele] += float(freq) * n
    if totalN == 0:
        return -1.0
    sumPSq = sum((wf / totalN) ** 2 for wf in weightedFreqs.values())
    return round(1.0 - sumPSq, 3)


COHORT_ORDER = ["AFR", "AMR", "EAS", "EUR", "SAS"]
COHORT_MAP = {
    "1000 Genomes AFR": "AFR",
    "1000 Genomes AMR": "AMR",
    "1000 Genomes EAS": "EAS",
    "1000 Genomes EUR": "EUR",
    "1000 Genomes SAS": "SAS",
}

def loadAlleleFreqs(inDir):
    """Load allele frequency data, grouped by repeatid and cohort."""
    freqs = defaultdict(lambda: {c: {"alleles": [], "freqs": [], "n": 0} for c in COHORT_ORDER})
    path = f"{inDir}/hg38_afreqs.csv.gz"
    with gzip.open(path, "rt") as f:
        reader = csv.reader(f)
        header = next(reader)  # skip header
        for row in reader:
            cohort_raw, allele, freq, n, repeatid = row
            cohort = COHORT_MAP.get(cohort_raw)
            if cohort is None:
                continue
            entry = freqs[repeatid][cohort]
            entry["alleles"].append(allele)
            entry["freqs"].append(freq)
            entry["n"] = int(n)
    return freqs

def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inDir = sys.argv[1]

    print("Loading allele frequencies...", file=sys.stderr)
    afreqs = loadAlleleFreqs(inDir)
    print(f"  Loaded frequencies for {len(afreqs)} repeats", file=sys.stderr)

    print("Processing repeats...", file=sys.stderr)
    repeatsPath = f"{inDir}/hg38_repeats_withlinks.csv.gz"
    count = 0
    with gzip.open(repeatsPath, "rt") as f:
        reader = csv.reader(f)
        header = next(reader)  # skip header
        for row in reader:
            repeatid, panel, chrom, motif, start, end, period, numcopies, _webstr_link = row
            # Source coordinates are 1-based; convert start to 0-based for BED
            start = str(int(start) - 1)

            # Compute heterozygosity pooled across populations
            af = afreqs.get(repeatid)
            het = computeHet(af)
            color = hetToColor(het)
            score = max(0, int(het * 1000)) if het >= 0 else 0

            # BED9 fields
            fields = [
                chrom,
                start,
                end,
                truncateMotif(motif) + "x" + numcopies,  # name
                str(score),     # score (het * 1000)
                ".",            # strand
                start,          # thickStart
                end,            # thickEnd
                color,          # itemRgb
                motif,
                period,
                numcopies,
                repeatid,
                str(het),       # het field
            ]
            for cohort in COHORT_ORDER:
                if af and af[cohort]["alleles"]:
                    entry = af[cohort]
                    pairs = [a + "=" + f for a, f in zip(entry["alleles"], entry["freqs"])]
                    fields.append(" ".join(pairs))
                    fields.append(str(entry["n"]))
                else:
                    fields.append("")
                    fields.append("0")

            print("\t".join(fields))
            count += 1
            if count % 500000 == 0:
                print(f"  Processed {count} repeats...", file=sys.stderr)

    print(f"Done. Wrote {count} records.", file=sys.stderr)

if __name__ == "__main__":
    main()
