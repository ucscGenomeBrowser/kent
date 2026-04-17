#!/usr/bin/env python3
"""Convert 1KG ONT SVAN VCF to BED9+ for bigBed, adding allele counts from phased VCF.

Usage:
    lrSv1kgOntVcfToBed.py svan.vcf.gz output.bed chrom.sizes [phased.vcf.gz]

The optional phased VCF provides AC, AN, and AF per variant (matched by ID).
SVs without a match get alleleCount=-1, alleleNumber=-1, alleleFreq=-1.
"""

import gzip
import sys

# Colors by SV class (R,G,B)
SV_COLORS = {
    "INS": "0,0,200",      # blue
    "DEL": "200,0,0",      # red
    "COMPLEX": "230,140,0", # orange
}

def parseInfo(infoStr):
    """Parse INFO field into a dict."""
    d = {}
    for item in infoStr.split(";"):
        if "=" in item:
            k, v = item.split("=", 1)
            d[k] = v
        else:
            d[item] = True
    return d

def getSvClass(varId):
    """Extract SV class (INS, DEL, COMPLEX) from the variant ID."""
    if "-INS-" in varId:
        return "INS"
    elif "-DEL-" in varId:
        return "DEL"
    elif "-COMPLEX-" in varId:
        return "COMPLEX"
    return "OTHER"

def loadPhasedAC(phasedFile):
    """Load AC, AN, AF from phased VCF, keyed by variant ID."""
    acMap = {}
    opener = gzip.open if phasedFile.endswith(".gz") else open
    with opener(phasedFile, "rt") as f:
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.split("\t", 9)
            varId = fields[2]
            info = parseInfo(fields[7])
            ac = int(info.get("AC", "0"))
            an = int(info.get("AN", "0"))
            af = float(info.get("AF", "0"))
            acMap[varId] = (ac, an, af)
    return acMap

def main():
    if len(sys.argv) < 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile = sys.argv[1]
    outFile = sys.argv[2]
    chromSizesFile = sys.argv[3]
    phasedFile = sys.argv[4] if len(sys.argv) > 4 else None
    opener = gzip.open if inFile.endswith(".gz") else open

    # Load chrom sizes for clipping
    chromSizes = {}
    with open(chromSizesFile) as f:
        for line in f:
            c, s = line.strip().split("\t")
            chromSizes[c] = int(s)

    # Load allele counts from phased VCF
    acMap = {}
    if phasedFile:
        print(f"Loading allele counts from {phasedFile}...", file=sys.stderr)
        acMap = loadPhasedAC(phasedFile)
        print(f"Loaded {len(acMap)} variants from phased VCF", file=sys.stderr)

    skipped = 0
    matched = 0
    unmatched = 0
    with opener(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        for line in fIn:
            if line.startswith("#"):
                continue

            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[1])
            varId = fields[2]
            ref = fields[3]
            info = parseInfo(fields[7])

            svClass = getSvClass(varId)

            # BED coordinates: 0-based half-open
            chromStart = pos - 1
            chromEnd = chromStart + len(ref)

            # SV length
            svLen = int(info.get("INS_LEN", info.get("DEL_LEN", "0")))

            # Readable name: e.g. "DEL 258bp" or "INS Alu 330bp"
            famName = info.get("FAM_N", "")
            if famName:
                name = f"{svClass} {famName} {svLen}bp"
            else:
                name = f"{svClass} {svLen}bp"

            # For INS, the item spans only the anchor base; expand by 1 for visibility
            if svClass == "INS" and chromEnd <= chromStart + 1:
                chromEnd = chromStart + 1

            # Insertion/deletion type
            insType = info.get("ITYPE_N", info.get("DTYPE_N", ""))

            # Transposon family
            family = info.get("FAM_N", "")

            # Percent resolved
            try:
                percResolved = float(info.get("PERC_RESOLVED", "0"))
            except ValueError:
                percResolved = 0.0

            # TSD length
            try:
                tsdLen = int(info.get("TSD_LEN", "0"))
            except ValueError:
                tsdLen = 0

            # Poly-A length
            try:
                polyaLen = int(info.get("POLYA_LEN", "0"))
            except ValueError:
                polyaLen = 0

            # Conformation
            conformation = info.get("CONFORMATION", "")

            # Retrotransposon length
            try:
                rtLen = int(info.get("RT_LEN", "0"))
            except ValueError:
                rtLen = 0

            # VNTR motif count
            try:
                nbMotifs = int(info.get("NB_MOTIFS", "0"))
            except ValueError:
                nbMotifs = 0

            # Source gene (for processed pseudogenes)
            srcGene = info.get("SRC_GENE", "")

            # Number of exons retrotransposed
            try:
                nbExons = int(info.get("NB_EXONS", "0"))
            except ValueError:
                nbExons = 0

            # Non-canonical MEI flag
            notCanonical = "Yes" if info.get("NOT_CANONICAL") else ""

            # Strand
            strand = info.get("STRAND", ".")
            if strand not in ("+", "-"):
                strand = "."

            color = SV_COLORS.get(svClass, "100,100,100")

            # Allele counts from phased VCF
            if varId in acMap:
                ac, an, af = acMap[varId]
                matched += 1
            else:
                ac, an, af = -1, -1, -1.0
                unmatched += 1

            # Clip to chrom sizes; skip records on unknown chroms
            if chrom not in chromSizes:
                skipped += 1
                continue
            if chromEnd > chromSizes[chrom]:
                skipped += 1
                continue

            row = [
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                "0",
                strand,
                str(chromStart),   # thickStart
                str(chromEnd),     # thickEnd
                color,
                svClass,
                str(svLen),
                insType,
                family,
                f"{percResolved:.2f}",
                str(tsdLen),
                str(polyaLen),
                conformation,
                str(rtLen),
                str(nbMotifs),
                srcGene,
                str(nbExons),
                notCanonical,
                str(ac),
                str(an),
                f"{af:.4f}" if af >= 0 else "-1.0000",
            ]
            fOut.write("\t".join(row) + "\n")

    print(f"Skipped {skipped} records outside chrom sizes", file=sys.stderr)
    if acMap:
        print(f"Allele counts: {matched} matched, {unmatched} unmatched "
              f"({100*matched/(matched+unmatched):.1f}% matched)", file=sys.stderr)

if __name__ == "__main__":
    main()
