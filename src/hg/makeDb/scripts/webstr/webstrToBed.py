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
            period_int = int(period)
            color = PERIOD_COLORS.get(period_int, DEFAULT_COLOR)

            # Source coordinates are 1-based; convert start to 0-based for BED
            start = str(int(start) - 1)

            # BED9 fields
            fields = [
                chrom,
                start,
                end,
                truncateMotif(motif) + "x" + numcopies,  # name
                "0",            # score
                ".",            # strand
                start,          # thickStart
                end,            # thickEnd
                color,          # itemRgb
                motif,
                period,
                numcopies,
            ]

            # Allele frequency fields for each cohort
            af = afreqs.get(repeatid)
            for cohort in COHORT_ORDER:
                if af and af[cohort]["alleles"]:
                    entry = af[cohort]
                    fields.append(",".join(entry["alleles"]))
                    fields.append(",".join(entry["freqs"]))
                    fields.append(str(entry["n"]))
                else:
                    fields.append("")
                    fields.append("")
                    fields.append("0")

            print("\t".join(fields))
            count += 1
            if count % 500000 == 0:
                print(f"  Processed {count} repeats...", file=sys.stderr)

    print(f"Done. Wrote {count} records.", file=sys.stderr)

if __name__ == "__main__":
    main()
