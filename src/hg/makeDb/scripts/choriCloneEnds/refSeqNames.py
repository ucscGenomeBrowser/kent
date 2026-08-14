#!/usr/bin/env python3
"""Build a RefSeq-accession to UCSC-chrom-name map from an NCBI assembly
report (e.g. GCF_000002035.6_GRCz11_assembly_report.txt).  The report
already has the UCSC-style name in column 10; we just emit
`<refSeqAcc>\\t<ucscName>` for rows that have a RefSeq accession.
Writes tab-separated pairs to stdout."""

import sys


def main(path):
    with open(path) as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 10:
                continue
            refseq = parts[6]
            ucsc = parts[9]
            if not refseq or refseq == "na":
                continue
            if not ucsc or ucsc == "na":
                continue
            print(f"{refseq}\t{ucsc}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write(
            "usage: refSeqNames.py <GCF_xxxxxx.assembly.txt>\n")
        sys.exit(1)
    main(sys.argv[1])
