#!/usr/bin/env python3
"""Aggregate gnomAD STR individual genotypes into per-locus BED9+ summary.

Reads gnomAD_STR_genotypes TSV and outputs one BED record per unique locus
(Id + Chrom + Start + End), with allele size distributions and sample counts.

Usage:
    gnomadStrToBed.py <input.tsv.gz> > gnomadStr.bed
"""

import csv
import gzip
import sys
from collections import defaultdict, Counter

MOTIF_LEN_COLORS = {
    1: "255,0,0",       # mono: red
    2: "0,0,255",       # di: blue
    3: "0,128,0",       # tri: green
    4: "255,165,0",     # tetra: orange
    5: "128,0,128",     # penta: purple
    6: "70,130,180",    # hexa: steel blue
}
DEFAULT_COLOR = "128,128,128"

def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile = sys.argv[1]

    # Aggregate per locus: key = (Id, chrom, start, end)
    loci = {}  # key -> {locusId, motifs, isAdjacent, alleles, nSamples, nPass, populations}

    print("Loading genotypes...", file=sys.stderr)
    with gzip.open(inFile, "rt") as f:
        reader = csv.DictReader(f, delimiter="\t")
        count = 0
        for row in reader:
            locusKey = (row["Id"], row["Chrom"], row["Start_0based"], row["End"])

            if locusKey not in loci:
                # Use the first motif's length for coloring; may have "/" for multi-motif
                motifField = row["Motif"]
                firstMotif = motifField.split("/")[0]
                loci[locusKey] = {
                    "locusId": row["LocusId"],
                    "motifs": set(),
                    "isAdjacent": row["IsAdjacentRepeat"],
                    "alleles": Counter(),
                    "nSamples": 0,
                    "nPass": 0,
                    "populations": Counter(),
                    "motifLen": len(firstMotif),
                }

            rec = loci[locusKey]
            rec["motifs"].add(row["Motif"])
            rec["nSamples"] += 1
            if row["Filter"] == "PASS":
                rec["nPass"] += 1
            rec["populations"][row["Population"]] += 1

            # Count allele sizes (both alleles)
            a1 = row["Allele1"]
            a2 = row["Allele2"]
            if a1:
                rec["alleles"][a1] += 1
            if a2:
                rec["alleles"][a2] += 1

            count += 1
            if count % 500000 == 0:
                print(f"  Processed {count} genotypes...", file=sys.stderr)

    print(f"Loaded {count} genotypes across {len(loci)} loci.", file=sys.stderr)

    # Output BED records
    for (locusId_raw, chrom, start, end), rec in sorted(loci.items(), key=lambda x: (x[0][1], int(x[0][2]))):
        color = MOTIF_LEN_COLORS.get(rec["motifLen"], DEFAULT_COLOR)
        motifs = ",".join(sorted(rec["motifs"]))

        # Build allele frequency distribution (sorted by allele size numerically)
        alleleCounts = rec["alleles"]
        totalAlleles = sum(alleleCounts.values())
        if totalAlleles > 0:
            # Try to sort numerically, fall back to string sort
            try:
                sortedAlleles = sorted(alleleCounts.keys(), key=lambda x: float(x))
            except ValueError:
                sortedAlleles = sorted(alleleCounts.keys())
            alleleStr = ",".join(sortedAlleles)
            freqStr = ",".join(f"{alleleCounts[a]/totalAlleles:.4f}" for a in sortedAlleles)
        else:
            alleleStr = ""
            freqStr = ""

        # Population summary
        popStr = ",".join(f"{p}:{n}" for p, n in sorted(rec["populations"].items(), key=lambda x: -x[1]))

        fields = [
            chrom,
            start,
            end,
            locusId_raw,        # name
            "0",                # score
            ".",                # strand
            start,              # thickStart
            end,                # thickEnd
            color,              # itemRgb
            rec["locusId"],     # gene/locus
            motifs,             # motif(s)
            rec["isAdjacent"],
            str(rec["nSamples"]),
            str(rec["nPass"]),
            alleleStr,
            freqStr,
            popStr,
        ]
        print("\t".join(fields))

    print(f"Wrote {len(loci)} locus records.", file=sys.stderr)

if __name__ == "__main__":
    main()
