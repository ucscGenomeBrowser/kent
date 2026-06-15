#!/usr/bin/env python3
"""Convert AoU 1K long-read SV CSV to BED9+ for bigBed.

Input is a gzipped CSV (media-2.gz) with columns:
  SV_coordinate, SV_ID, SV type, SV length, Mean GenotypePosterior nonref,
  AF(AFR,AMR,EAS,EUR,SAS), Fst(AFR vs Non-AFR), OMIM genes, Disease genes,
  Cancer genes, ACMG genes, OMIM CDS, Disease CDS, Cancer CDS, ACMG CDS,
  Regulatory element, SegDUP, Tandem repeats, Other LR datasets,
  Detected in AoU SR, eQTLs, GWAS, SV-trait associations,
  SR validation, LR assembly-supported, Locityper validation

Usage:
    lrSvAou1kCsvToBed.py input.csv.gz output.bed
"""

import csv
import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor

# AoU Phase-I (2025) SV panel: 1,027 samples => 2,054 alleles (diploid)
# No per-variant AC in the site-level release; approximate AC as
# round(max_popAF * N_alleles) for naming purposes only.
AOU_N_ALLELES = 2054


def encodeNonAscii(s):
    """Replace non-ASCII characters with numeric HTML entities so detail
    pages render them correctly instead of as mojibake. The source CSV has
    gene/trait text with accented names, Greek letters, curly quotes and
    en-dashes (e.g. ö, β, ', –)."""
    return "".join(c if ord(c) < 128 else f"&#{ord(c)};" for c in s)


def na(val):
    """Return empty string for NA values, else the value with any non-ASCII
    characters numeric-entity encoded."""
    if val == "NA" or val == "No" or val == "":
        return ""
    return encodeNonAscii(val)

def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]

    seen = set()
    nIn = 0
    nDup = 0
    with gzip.open(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        reader = csv.reader(fIn)
        header = next(reader)

        for row in reader:
            nIn += 1
            coord = row[0]       # chr1:10627
            svTypeRaw = row[2]
            svType = normalizeSvType(svTypeRaw)
            svLenSrc = int(row[3])

            # Parse coordinate (1-based position)
            chrom, posStr = coord.split(":")
            pos = int(posStr)

            # BED is 0-based half-open
            chromStart = pos - 1
            if svType == "DEL":
                chromEnd = chromStart + svLenSrc
            else:
                # INS: place at insertion site, 1 bp wide
                chromEnd = chromStart + 1

            svLen = chromEnd - chromStart
            # insLen: the source "SV length" represents the INS payload for INS
            # and 0 for DEL (where it equals reference span)
            if svType in ("INS", "MEI"):
                insLen = svLenSrc
            else:
                insLen = 0

            color = svColor(svType)

            # Parse population AFs (column 5): "0.001,0.002,0.003,0.004,0.005"
            afStr = row[5]
            afParts = afStr.split(",")
            try:
                afAfr = float(afParts[0])
                afAmr = float(afParts[1])
                afEas = float(afParts[2])
                afEur = float(afParts[3])
                afSas = float(afParts[4])
            except (ValueError, IndexError):
                afAfr = afAmr = afEas = afEur = afSas = 0.0

            fst = na(row[6])

            # Gene intersections (use gene-level, skip CDS-level which is subset)
            omimGenes = na(row[7])
            diseaseGenes = na(row[8])
            cancerGenes = na(row[9])
            acmgGenes = na(row[10])

            regElement = na(row[15])
            segDup = na(row[16])
            tandemRepeat = na(row[17])
            otherLr = na(row[18])
            detectedSr = na(row[19])

            eqtls = na(row[20])
            gwas = na(row[21])
            traitAssoc = na(row[22])

            # Use max population AF as score (0-1000)
            maxAf = max(afAfr, afAmr, afEas, afEur, afSas)
            score = min(int(round(maxAf * 1000)), 1000)

            # AC: AoU site-level data doesn't publish AC; approximate with
            # round(maxAf * 2054) so the name has something informative.
            ac = int(round(maxAf * AOU_N_ALLELES))

            featLen = insLen if svType in ("INS", "MEI") else svLen
            name = svName(svType, featLen, ac)

            bedRow = [
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                str(score),
                ".",
                str(chromStart),
                str(chromEnd),
                color,
                svType,
                str(svLen),
                str(insLen),
                str(ac),
                f"{afAfr:.6f}",
                f"{afAmr:.6f}",
                f"{afEas:.6f}",
                f"{afEur:.6f}",
                f"{afSas:.6f}",
                fst,
                omimGenes,
                diseaseGenes,
                cancerGenes,
                acmgGenes,
                regElement,
                segDup,
                tandemRepeat,
                otherLr,
                detectedSr,
                eqtls,
                gwas,
                traitAssoc,
            ]
            line_out = "\t".join(bedRow)
            if line_out in seen:
                nDup += 1
                continue
            seen.add(line_out)
            fOut.write(line_out + "\n")

    print(f"AoU 1K: {nIn:,} input records, {nDup:,} duplicate rows dropped, "
          f"{nIn - nDup:,} written", file=sys.stderr)

if __name__ == "__main__":
    main()
