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
import sys

SV_COLORS = {
    "DEL": "200,0,0",
    "INS": "0,0,200",
}

def na(val):
    """Return empty string for NA values."""
    if val == "NA" or val == "No" or val == "":
        return ""
    return val

def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]

    with gzip.open(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        reader = csv.reader(fIn)
        header = next(reader)

        for row in reader:
            coord = row[0]       # chr1:10627
            svType = row[2]
            svLen = int(row[3])

            # Parse coordinate (1-based position)
            chrom, posStr = coord.split(":")
            pos = int(posStr)

            # BED is 0-based half-open
            chromStart = pos - 1
            if svType == "DEL":
                chromEnd = chromStart + svLen
            else:
                # INS: place at insertion site, 1 bp wide
                chromEnd = chromStart + 1

            name = f"{svType} {svLen}bp"
            color = SV_COLORS.get(svType, "100,100,100")

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
            fOut.write("\t".join(bedRow) + "\n")

if __name__ == "__main__":
    main()
