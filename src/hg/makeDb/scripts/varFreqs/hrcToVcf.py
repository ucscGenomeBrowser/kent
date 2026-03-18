#!/usr/bin/env python3
"""
Convert HRC.r1-1.GRCh37.wgs.mac5.sites.tab.gz to VCF format,
lifting coordinates from hg19 (GRCh37) to hg38 using UCSC liftOver.
"""

import gzip
import subprocess
import sys
import tempfile
import os

INFILE = "HRC.r1-1.GRCh37.wgs.mac5.sites.tab.gz"
OUTFILE = "hrc.vcf"
CHAIN = "/gbdb/hg19/liftOver/hg19ToHg38.over.chain.gz"
CHROMSIZES = "/hive/data/genomes/hg38/chrom.sizes"
LIFTOVER = "liftOver"
MIN_MATCH = "0.95"

def main():
    inPath = INFILE
    outPath = OUTFILE

    # Step 1: Read input, write BED for liftOver, and store original rows
    print("Reading input and creating BED for liftOver...", file=sys.stderr)
    rows = []  # list of (chrom, pos, id, ref, alt, ac, an, af, ac_ex, an_ex, af_ex, aa)
    bedTmp = tempfile.NamedTemporaryFile(mode="w", suffix=".bed", delete=False)
    with gzip.open(inPath, "rt") as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            chrom, pos = fields[0], int(fields[1])
            chromStr = "chr" + chrom
            # BED is 0-based half-open; VCF POS is 1-based
            bedTmp.write(f"{chromStr}\t{pos - 1}\t{pos}\t{len(rows)}\n")
            rows.append(fields)
    bedTmp.close()
    print(f"  {len(rows)} variants read.", file=sys.stderr)

    # Step 2: Run liftOver
    liftedBed = bedTmp.name + ".lifted"
    unmappedBed = bedTmp.name + ".unmapped"
    print("Running liftOver...", file=sys.stderr)
    subprocess.check_call([
        LIFTOVER, "-minMatch=" + MIN_MATCH,
        bedTmp.name, CHAIN, liftedBed, unmappedBed
    ])

    # Count unmapped
    unmappedCount = 0
    with open(unmappedBed) as fh:
        for line in fh:
            if not line.startswith("#"):
                unmappedCount += 1
    print(f"  {unmappedCount} variants unmapped.", file=sys.stderr)

    # Step 3: Read lifted coordinates. Key = original row index -> (chrom, pos)
    lifted = {}
    with open(liftedBed) as fh:
        for line in fh:
            fields = line.rstrip("\n").split("\t")
            chrom = fields[0]
            pos = int(fields[2])  # 1-based end = our VCF POS
            idx = int(fields[3])
            lifted[idx] = (chrom, pos)
    print(f"  {len(lifted)} variants lifted to hg38.", file=sys.stderr)

    # Step 4: Write VCF using only the "excluding 1000 Genomes" counts,
    # since the varFreqs supertrack already includes 1000 Genomes data separately.
    # Skip variants with AN_EXCLUDING_1000G == 0 (only seen in 1000G samples).
    print("Writing VCF...", file=sys.stderr)
    skipped1kg = 0
    with open(outPath, "w") as out:
        out.write("##fileformat=VCFv4.1\n")
        out.write("##source=HRC.r1-1\n")
        out.write("##liftOver=hg19ToHg38\n")
        with open(CHROMSIZES) as cs:
            for cline in cs:
                chrom_name, chrom_len = cline.rstrip().split("\t")
                out.write(f"##contig=<ID={chrom_name},length={chrom_len}>\n")
        out.write('##INFO=<ID=AC,Number=A,Type=Integer,Description="Allele count (excluding 1000 Genomes samples)">\n')
        out.write('##INFO=<ID=AN,Number=1,Type=Integer,Description="Total allele number (excluding 1000 Genomes samples)">\n')
        out.write('##INFO=<ID=AF,Number=A,Type=Float,Description="Allele frequency (excluding 1000 Genomes samples)">\n')
        out.write('##INFO=<ID=AA,Number=1,Type=String,Description="Ancestral allele">\n')
        out.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")

        written = 0
        for idx in range(len(rows)):
            if idx not in lifted:
                continue
            chrom, pos = lifted[idx]
            fields = rows[idx]
            ac = fields[8]   # AC_EXCLUDING_1000G
            an = fields[9]   # AN_EXCLUDING_1000G
            af = fields[10]  # AF_EXCLUDING_1000G

            # Skip variants only present in 1000 Genomes samples
            if an == "0" or (ac == "0" and af == "0"):
                skipped1kg += 1
                continue

            rsId = fields[2] if fields[2] != "." else "."
            ref = fields[3]
            alt = fields[4]
            aa = fields[11] if len(fields) > 11 else "."

            info = f"AC={ac};AN={an};AF={af}"
            if aa and aa != ".":
                info += f";AA={aa}"

            out.write(f"{chrom}\t{pos}\t{rsId}\t{ref}\t{alt}\t.\tPASS\t{info}\n")
            written += 1

    print(f"  {skipped1kg} variants skipped (only in 1000 Genomes).", file=sys.stderr)
    print(f"  {written} variants written to {outPath}", file=sys.stderr)

    # Cleanup temp files
    os.unlink(bedTmp.name)
    os.unlink(liftedBed)
    os.unlink(unmappedBed)

    print("Done.", file=sys.stderr)

if __name__ == "__main__":
    main()
