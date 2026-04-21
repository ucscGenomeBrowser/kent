#!/usr/bin/env python3
"""Convert HGSVC2 v2.0 integrated annotation TSVs (insdel + inv) to merged BED9+.

Usage:
    lrSvHgsvc2TsvToBed.py insdel.tsv.gz inv.tsv.gz output.bed

The two TSVs share an envelope (chrom, pos, end, svtype, svlen, MERGE_AC /
MERGE_SAMPLES, cytoband, REF_SD, REF_TRF, REFSEQ_*, PLI/LOEUF, WIN_500/2K)
but diverge on type-specific fields: insdel has POP_*_AF population allele
frequencies, inv has RGN_REF_INNER. Both columns are emitted; type-specific
ones are empty where not applicable.

Source:
    https://ftp.1000genomes.ebi.ac.uk/vol1/ftp/data_collections/HGSVC2/release/v2.0/integrated_callset/
Paper:
    Ebert et al. 2021, Science, PMID 33632895.
"""

import csv
import gzip
import sys

SV_COLORS = {
    "DEL": "200,0,0",      # red
    "INS": "0,0,200",      # blue
    "INV": "230,140,0",    # orange
}


def openTsv(path):
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path, "rt")


def na(val):
    if val is None:
        return ""
    v = val.strip()
    if v in ("", "NA", ".", "nan"):
        return ""
    return v


def toInt(s):
    if not s or s in ("NA", "."):
        return 0
    try:
        return int(float(s))
    except ValueError:
        return 0


def toFloat(s):
    if not s or s in ("NA", "."):
        return 0.0
    try:
        return float(s)
    except ValueError:
        return 0.0


def countSamples(mergeSamples):
    """Return unique-sample count, stripping -h1/-h2/-un haplotype suffixes."""
    if not mergeSamples:
        return 0
    uniq = set()
    for p in mergeSamples.split(","):
        p = p.strip()
        if not p:
            continue
        for suf in ("-h1", "-h2", "-un", "-1", "-2"):
            if p.endswith(suf):
                p = p[: -len(suf)]
                break
        uniq.add(p)
    return len(uniq)


def emit(outF, row, typeExtra):
    chrom = row["#CHROM"]
    if not chrom.startswith("chr"):
        chrom = "chr" + chrom
    # HGSVC2 TSV coords are 0-based half-open (matches HGSVC3).
    chromStart = toInt(row["POS"])
    chromEnd = toInt(row["END"])
    if chromEnd <= chromStart:
        chromEnd = chromStart + 1

    svType = row["SVTYPE"]
    svLen = abs(toInt(row["SVLEN"]))
    color = SV_COLORS.get(svType, "100,100,100")

    alleleCount = toInt(row.get("MERGE_AC", "0"))
    sampleCount = countSamples(row.get("MERGE_SAMPLES", ""))

    bed = [
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
        str(alleleCount),
        str(sampleCount),
        na(row.get("BAND", "")),
        f"{toFloat(row.get('REF_SD', '0')):.6f}",
        na(row.get("REF_TRF", "")),
        str(toInt(row.get("REFSEQ_CDS", "0"))),
        str(toInt(row.get("REFSEQ_UTR3", "0"))),
        str(toInt(row.get("REFSEQ_UTR5", "0"))),
        str(toInt(row.get("REFSEQ_INTRON", "0"))),
        str(toInt(row.get("REFSEQ_NCRNA", "0"))),
        str(toInt(row.get("REFSEQ_UP_5K", "0"))),
        str(toInt(row.get("REFSEQ_DN_5K", "0"))),
        na(row.get("PLI_MAX", "")),
        na(row.get("LOEUF_MIN", "")),
        typeExtra.get("popAllAf", ""),
        typeExtra.get("popAfrAf", ""),
        typeExtra.get("popAmrAf", ""),
        typeExtra.get("popEasAf", ""),
        typeExtra.get("popEurAf", ""),
        typeExtra.get("popSasAf", ""),
        typeExtra.get("regionRefInner", ""),
        row.get("MERGE_SAMPLES", "") or "",
        na(row.get("DISC_CLASS", "")),
        na(row.get("WIN_500", "")),
        na(row.get("WIN_2K", "")),
    ]
    outF.write("\t".join(bed) + "\n")


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    insdelPath, invPath, outPath = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(outPath, "w") as outF:
        with openTsv(insdelPath) as fIn:
            for row in csv.DictReader(fIn, delimiter="\t"):
                emit(outF, row, {
                    "popAllAf": na(row.get("POP_ALL_AF", "")),
                    "popAfrAf": na(row.get("POP_AFR_AF", "")),
                    "popAmrAf": na(row.get("POP_AMR_AF", "")),
                    "popEasAf": na(row.get("POP_EAS_AF", "")),
                    "popEurAf": na(row.get("POP_EUR_AF", "")),
                    "popSasAf": na(row.get("POP_SAS_AF", "")),
                })

        with openTsv(invPath) as fIn:
            for row in csv.DictReader(fIn, delimiter="\t"):
                emit(outF, row, {
                    "regionRefInner": na(row.get("RGN_REF_INNER", "")),
                })


if __name__ == "__main__":
    main()
