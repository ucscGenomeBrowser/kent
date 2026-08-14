#!/usr/bin/env python3
"""Convert the Chirmade 2026 SVatalog sv_annotations.tsv to BED9+.

Source: https://zenodo.org/records/13367574 (sv_annotations.tsv)
Paper:  Chirmade et al. 2026, Heredity (Edinb), PMID 41203876

Coordinates in the source TSV are 1-based closed (End-Start+1 == Length).
Translate to BED-style 0-based half-open (chromStart = Start - 1,
chromEnd = End).

Usage:
    lrSvChirmade101TsvToBed.py sv_annotations.tsv output.bed
"""

import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor


def na(val):
    """Return '' for missing ('NA' or empty) source values."""
    if val is None:
        return ""
    v = val.strip()
    if v == "" or v == "NA":
        return ""
    return v


def toInt(s):
    if not s:
        return 0
    try:
        return int(float(s))
    except ValueError:
        return 0


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]

    seen = set()
    nIn = 0
    nDup = 0
    with open(inPath, newline="") as fIn, open(outPath, "w") as fOut:
        reader = csv.DictReader(fIn, delimiter="\t")
        for row in reader:
            nIn += 1
            chrom = row["Chromosome"]
            if not chrom.startswith("chr"):
                chrom = "chr" + chrom

            # 1-based closed -> 0-based half-open
            chromStart = toInt(row["Start"]) - 1
            chromEnd = toInt(row["End"])
            if chromEnd <= chromStart:
                chromEnd = chromStart + 1

            svTypeRaw = row["Type"]  # lowercase del/ins/dup/inv/complex
            svType = normalizeSvType(svTypeRaw)
            srcLen = abs(toInt(row["Length"]))
            svLen = chromEnd - chromStart
            if svType in ("INS", "MEI"):
                insLen = srcLen
            else:
                insLen = 0
            color = svColor(svType)

            # Chirmade catalog is site-level without AC. Use -1 as placeholder
            # so svName drops the :AC suffix.
            ac = -1

            featLen = insLen if svType in ("INS", "MEI") else svLen
            name = svName(svType, featLen, ac)

            bedRow = [
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
                str(toInt(row.get("GC (%)", "0"))),
                na(row.get("Cytoband", "")),
                str(toInt(row.get("Gene Count", "0"))),
                na(row.get("Gene Name(s)", "")),
                na(row.get("Gene at Start", "")),
                na(row.get("Gene at End", "")),
                na(row.get("Exon Name", "")),
                na(row.get("CDS Name", "")),
                na(row.get("Dark Genes % Overlap", "")),
                na(row.get("ClinGen Haploinsufficient", "")),
                na(row.get("ClinGen Triplosensitive", "")),
                na(row.get("gnomAD O/E LoF Upper", "")),
                na(row.get("gnomAD O/E Mis Upper", "")),
                na(row.get("gnomAD pLI", "")),
                na(row.get("gnomAD pRec", "")),
                na(row.get("Repeat % Overlap", "")),
                na(row.get("Dirty Region % Overlap", "")),
                na(row.get("Chromosome Region", "")),
                na(row.get("CGD", "")),
                na(row.get("OMIM Pheno", "")),
                na(row.get("OMIM Inh", "")),
                na(row.get("ClinGen Region", "")),
                na(row.get("Decipher Region", "")),
                na(row.get("ClinVar VarID", "")),
                na(row.get("gnomAD AF Max 90% RO", "")),
                na(row.get("gnomAD Population AF Max 90% RO", "")),
                na(row.get("gnomAD Hom/Ref Frequency 90% RO", "")),
                na(row.get("gnomAD Het Frequency 90% RO", "")),
                na(row.get("gnomAD Hom/Alt Frequency 90% RO", "")),
                na(row.get("DGV % Overlap", "")),
                na(row.get("DGV 50% RO", "")),
            ]
            line_out = "\t".join(bedRow)
            if line_out in seen:
                nDup += 1
                continue
            seen.add(line_out)
            fOut.write(line_out + "\n")

    print(f"Chirmade101: {nIn:,} input records, {nDup:,} duplicate rows dropped, "
          f"{nIn - nDup:,} written", file=sys.stderr)


if __name__ == "__main__":
    main()
