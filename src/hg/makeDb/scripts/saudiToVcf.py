#!/usr/bin/env python3
"""Convert Saudi Arabian variant frequency TSV file to VCF format."""

import sys, gzip

INPUT_FILE = "51297884.tsv.gz"
OUTPUT_FILE = "saudi_variants.vcf"

def main():
    with gzip.open(INPUT_FILE, 'r') as infile, open(OUTPUT_FILE, 'w') as outfile:
        # Write VCF header
        outfile.write("##fileformat=VCFv4.2\n")
        outfile.write("##source=SaudiArabianVariantFrequencies\n")
        outfile.write("##INFO=<ID=AN,Number=1,Type=Integer,Description=\"Total allele count\">\n")
        outfile.write("##INFO=<ID=AC,Number=A,Type=Integer,Description=\"Alternative allele count\">\n")
        outfile.write("##INFO=<ID=AF,Number=A,Type=Float,Description=\"Allele frequency\">\n")
        outfile.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")

        for line in infile:
            # Skip comment lines and header
            if line.startswith("##") or line.startswith("#") or line.strip() == "":
                continue

            fields = line.strip().split('\t')
            if len(fields) < 6:
                continue

            chrom, pos, ref, alt, an, ac = fields[:6]

            # Calculate allele frequency
            try:
                af = int(ac) / int(an) if int(an) > 0 else 0
            except ValueError:
                continue

            # Build INFO field
            info = f"AN={an};AC={ac};AF={af:.6f}"

            # Write VCF line: CHROM POS ID REF ALT QUAL FILTER INFO
            outfile.write(f"{chrom}\t{pos}\t.\t{ref}\t{alt}\t.\tPASS\t{info}\n")

    print(f"Converted {INPUT_FILE} to {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
