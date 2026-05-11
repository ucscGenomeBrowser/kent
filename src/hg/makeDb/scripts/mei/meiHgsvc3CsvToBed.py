#!/usr/bin/env python3
"""Convert HGSVC3 MEI_Callset_GRCh38 CSV to a bed9+ file.

The HGSVC3 MEI_Callset_GRCh38 callset (Logsdon et al. 2025, Nature) lists
novel Mobile Element Insertions identified in 65 long-read assembled
samples relative to the GRCh38 reference. Each row of the input CSV is
VCF-like (CHROM, POS, REF, ALT, QUAL, FILTER, INFO, FORMAT, <65 sample
columns>, Caller_Count, TE_Designation, L1ME-AID, PALMER, L1ME-AID_INFO,
PALMER_INFO, PAVMergedCalls).

For BED, insertions are drawn as a 1bp anchor block at the position
where the insertion would attach in the reference:
  chromStart = POS - 1   (1-based VCF anchor -> 0-based)
  chromEnd   = chromStart + 1

Genotypes are encoded "h1|h2|h3" where each haplotype is 0 (reference,
no insertion), 1 (insertion present), or "." (missing).
"""

import csv
import gzip
import sys


COLOR_BY_CLASS = {
    "Alu":   "0,114,178",   # blue (Okabe-Ito)
    "L1":    "213,94,0",    # vermillion (Okabe-Ito)
    "SVA":   "0,158,115",   # bluish green (Okabe-Ito)
    "HERVK": "204,121,167", # reddish purple (Okabe-Ito)
    "snRNA": "0,0,0",       # black (Okabe-Ito)
    "Other": "0,0,0",
}


def shortClass(teDesignation):
    """Map 'SINE/Alu' -> 'Alu', 'LINE/L1' -> 'L1', 'Retroposon/SVA' -> 'SVA',
    'HERVK' -> 'HERVK', 'snRNA' -> 'snRNA'."""
    if "/" in teDesignation:
        return teDesignation.split("/", 1)[1]
    return teDesignation


def parseInfo(infoField):
    fields = {}
    for tok in infoField.split(";"):
        if "=" in tok:
            k, v = tok.split("=", 1)
            fields[k] = v
        else:
            fields[tok] = True
    return fields


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: meiHgsvc3CsvToBed.py input.csv.gz chrom.sizes output.bed")

    inFn, sizesFn, outFn = sys.argv[1:]
    chromSizes = {}
    with open(sizesFn) as f:
        for line in f:
            chrom, size = line.rstrip("\n").split("\t")
            chromSizes[chrom] = int(size)

    nIn = 0
    nOut = 0
    nSkippedChrom = 0
    nSkippedBoundary = 0

    opener = gzip.open if inFn.endswith(".gz") else open
    out = open(outFn, "w")

    with opener(inFn, "rt") as f:
        reader = csv.reader(f)
        header = next(reader)
        # Locate column indices by name (more robust than positional).
        idx = {name: i for i, name in enumerate(header)}
        # Sample columns are everything between FORMAT and Caller_Count.
        firstSampleCol = idx["FORMAT"] + 1
        lastSampleCol = idx["Caller_Count"]
        sampleNames = header[firstSampleCol:lastSampleCol]

        for row in reader:
            nIn += 1
            chrom = row[idx["CHROM"]]
            pos = int(row[idx["POS"]])             # 1-based VCF position of anchor
            ref = row[idx["REF"]]
            alt = row[idx["ALT"]]
            info = parseInfo(row[idx["INFO"]])
            sampleGts = row[firstSampleCol:lastSampleCol]
            teDesignation = row[idx["TE_Designation"]]

            # Insertion length: prefer SVLEN from INFO, else len(ALT)-len(REF).
            if "SVLEN" in info:
                svLen = abs(int(info["SVLEN"]))
            else:
                svLen = len(alt) - len(ref)
            if svLen <= 0:
                sys.stderr.write(f"WARN: non-positive svLen at {chrom}:{pos}\n")
                continue

            # Anchor-base BED interval (1bp wide).
            chromStart = pos - 1
            chromEnd = chromStart + 1

            if chrom not in chromSizes:
                nSkippedChrom += 1
                sys.stderr.write(f"WARN: skipping record at {chrom}:{pos}, chrom not in chrom.sizes\n")
                continue
            if chromEnd > chromSizes[chrom]:
                nSkippedBoundary += 1
                sys.stderr.write(
                    f"WARN: skipping record at {chrom}:{pos}, end {chromEnd} > chromSize {chromSizes[chrom]}\n")
                continue

            # Per-record genotype tallying.
            altAC = 0
            AN = 0
            carrierCount = 0
            sampleCount = 0
            carrierSamples = []
            for sampleName, gt in zip(sampleNames, sampleGts):
                hapCalls = gt.split("|")
                hapAlt = 0
                hapCalled = 0
                for h in hapCalls:
                    if h == ".":
                        continue
                    AN += 1
                    hapCalled += 1
                    if h == "1":
                        altAC += 1
                        hapAlt += 1
                    elif h != "0":
                        sys.stderr.write(f"WARN: unexpected GT '{h}' at {chrom}:{pos} sample {sampleName}\n")
                if hapCalled > 0:
                    sampleCount += 1
                if hapAlt > 0:
                    carrierCount += 1
                    carrierSamples.append(sampleName)

            altAF = (altAC / AN) if AN > 0 else 0.0
            score = max(0, min(1000, int(round(altAF * 1000))))

            cls = shortClass(teDesignation)
            color = COLOR_BY_CLASS.get(cls, COLOR_BY_CLASS["Other"])

            refSd = float(info.get("REF_SD", 0)) if info.get("REF_SD", False) else 0.0
            refTrf = "True" if info.get("REF_TRF", False) else "False"
            sourceSample = info.get("SAMPLE", "")
            callerCount = int(row[idx["Caller_Count"]]) if row[idx["Caller_Count"]] else 0
            l1meAid = "Yes" if row[idx["L1ME-AID"]] == "1" else "No"
            palmer = "Yes" if row[idx["PALMER"]] == "1" else "No"

            # Inserted DNA: VCF convention puts the anchor base at ALT[0] (= REF[0]).
            insertSeq = alt[1:] if len(alt) > 1 else ""

            # Name format matches lrSv tracks: INS-<svLen>:<carrierCount>.
            name = f"INS-{svLen}:{carrierCount}"

            out.write("\t".join([
                chrom,
                str(chromStart),
                str(chromEnd),
                name,
                str(score),
                ".",
                str(chromStart),
                str(chromEnd),
                color,
                cls,
                teDesignation,
                str(svLen),
                str(altAC),
                str(AN),
                f"{altAF:.4f}",
                str(carrierCount),
                str(sampleCount),
                sourceSample,
                str(callerCount),
                l1meAid,
                palmer,
                f"{refSd:.2f}",
                refTrf,
                ",".join(carrierSamples),
                insertSeq,
            ]) + "\n")
            nOut += 1

    out.close()
    sys.stderr.write(
        f"Read {nIn} records, wrote {nOut}, skipped {nSkippedChrom} (chrom missing) + "
        f"{nSkippedBoundary} (off chrom end)\n")


if __name__ == "__main__":
    main()
