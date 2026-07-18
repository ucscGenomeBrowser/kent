#!/usr/bin/env python3
"""Convert the NIH CARD long-read SV bigBed to the canonical lrSv BED9+.

The NIH CARD Long-Read Initiative distributes its SV catalogue as a ready-made
bigBed (see the source repo). That bigBed does not follow the lrSv supertrack
conventions: it stores a signed svLen (negative for DEL), no separate insLen,
and uses a ColorBrewer palette rather than the supertrack's shared flat colors.
This script re-derives the canonical columns so the CARD subtrack matches its
siblings (svLen = reference span, insLen = inserted length, shared svColor()).

Input is the provider bigBed dumped to BED with bigBedToBed, i.e. 15 columns:
    chrom start end name score strand thickStart thickEnd reserved
    svType svLen(signed) alleleFreq carrierCount nabecCount hbccCount

Usage:
    bigBedToBed NIH_CARD_longReadSVs.bb stdin | lrSvCardBbToBed.py /dev/stdin out.bed

Source:
    https://github.com/meredith705/card_genome_browserTrack (hg38/NIH_CARD_longReadSVs.bb)
Paper:
    Billingsley et al. 2024, bioRxiv 2024.12.16.628723.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lrSvCommon import svName, normalizeSvType, svColor

# The provider emits a single "DUP:TANDEM" record; fold it to the canonical DUP.
TYPE_FIX = {"DUP:TANDEM": "DUP"}


def fmtAf(raw):
    """Re-emit an allele frequency as a compact float string."""
    try:
        return f"{float(raw):g}"
    except (TypeError, ValueError):
        return "0"


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inPath, outPath = sys.argv[1], sys.argv[2]

    nIn = 0
    nBig = 0          # SVs > 1 Mb, kept but counted for the build report
    typeCounts = {}
    with open(inPath) as fIn, open(outPath, "w") as fOut:
        for line in fIn:
            if not line.strip():
                continue
            nIn += 1
            f = line.rstrip("\n").split("\t")
            chrom = f[0]
            chromStart = int(f[1])
            chromEnd = int(f[2])
            svTypeRaw = f[9]
            svLenSigned = int(f[10])
            alleleFreq = fmtAf(f[11])
            carrierCount = int(f[12])
            nabecCount = int(f[13])
            hbccCount = int(f[14])

            svType = normalizeSvType(TYPE_FIX.get(svTypeRaw, svTypeRaw))

            # Canonical svLen is the feature's span on the reference; for INS
            # that is 1 bp, and the inserted-sequence length lives in insLen.
            svLen = chromEnd - chromStart
            if svType == "INS":
                insLen = abs(svLenSigned)
            else:
                insLen = 0

            # CARD publishes genotyped carrier counts, not allele counts; the
            # carrier count is the closest integer to the supertrack's AC.
            ac = carrierCount

            featLen = insLen if svType == "INS" else svLen
            name = svName(svType, featLen, ac)
            color = svColor(svType)

            if max(svLen, insLen) > 1000000:
                nBig += 1
            typeCounts[svType] = typeCounts.get(svType, 0) + 1

            row = [
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
                alleleFreq,
                str(nabecCount),
                str(hbccCount),
            ]
            fOut.write("\t".join(row) + "\n")

    typeStr = ", ".join(f"{k}={v:,}" for k, v in sorted(typeCounts.items()))
    print(f"CARD: {nIn:,} input records written; by type: {typeStr}", file=sys.stderr)
    print(f"CARD: {nBig:,} records with svLen or insLen > 1 Mb kept "
          f"(large ONT/assembly calls; not dropped)", file=sys.stderr)


if __name__ == "__main__":
    main()
