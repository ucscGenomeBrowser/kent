#!/usr/bin/env python3
"""Convert Han 945 site-only SV VCF with SUPP_VEC to a VCF with per-sample genotypes.

The input VCF has no sample columns but contains a SUPP_VEC INFO field:
a 945-character binary string where '1' means the sample supports the SV.
This script reconstructs per-sample GT columns (0/1 for carriers, 0/0 for non-carriers).

Sample names are generated as Sample_001 through Sample_945 since
the original VCF does not include sample identifiers.

Usage:
    lrSvHan945SuppVecToVcf.py input.vcf.gz output.vcf
"""

import gzip
import sys

def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    inFile, outFile = sys.argv[1], sys.argv[2]
    nSamples = 945
    sampleNames = [f"Sample_{i+1:03d}" for i in range(nSamples)]

    opener = gzip.open if inFile.endswith(".gz") else open

    with opener(inFile, "rt") as fIn, open(outFile, "w") as fOut:
        for line in fIn:
            if line.startswith("##"):
                fOut.write(line)
                continue

            if line.startswith("#CHROM"):
                # Rewrite header line with sample columns
                baseCols = line.rstrip("\n").split("\t")[:8]
                fOut.write("\t".join(baseCols + ["FORMAT"] + sampleNames) + "\n")
                continue

            fields = line.rstrip("\n").split("\t")
            # fields: CHROM POS ID REF ALT QUAL FILTER INFO
            infoStr = fields[7]

            # Extract SUPP_VEC
            suppVec = ""
            for item in infoStr.split(";"):
                if item.startswith("SUPP_VEC="):
                    suppVec = item.split("=", 1)[1]
                    break

            if len(suppVec) != nSamples:
                print(f"Warning: SUPP_VEC length {len(suppVec)} != {nSamples} "
                      f"at {fields[0]}:{fields[1]}, skipping", file=sys.stderr)
                continue

            # Build genotype columns
            gts = ["0/1" if c == "1" else "0/0" for c in suppVec]

            row = fields[:8] + ["GT"] + gts
            fOut.write("\t".join(row) + "\n")

if __name__ == "__main__":
    main()
