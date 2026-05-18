#!/usr/bin/env python3
"""Convert GenomeIndia (9,768 individuals) per-chromosome AF TSV files into
a single bgzipped, tabix-indexed VCF.

Source format (one file per autosome, no header):
    chrom  pos  id  ref  alt  AF

The release ships only an alternate allele frequency. AC and AN are
synthesized so the varFreqsAll combined-track pipeline can pick them
up via INFO/AC and INFO/AF.

    AN = 2 * 9768 = 19536  (autosomal, biallelic; constant)
    AC = round(AF * AN)

Per the README the underlying calls were filtered to NS_GT >= 98% (>=98%
of samples called at the site), so AN slightly overstates the called
allele count for some sites. The combined track only displays AC/AF for
context so this approximation is acceptable; the methods section notes it.

Usage:
    python3 genomeindiaToVcf.py <input_dir> <output.vcf.gz>
"""

import argparse
import gzip
import os
import subprocess
import sys

N_SAMPLES = 9768
AN = 2 * N_SAMPLES  # 19536, autosomal biallelic

AUTOSOMES = [f"chr{i}" for i in range(1, 23)]


def convert(input_dir, output_vcf):
    body_path = output_vcf + ".unsorted.body"
    total = 0
    skipped = 0

    with open(body_path, "w") as out:
        for chrom in AUTOSOMES:
            tsv = os.path.join(
                input_dir,
                f"GI_9768_CBR-NIBMG_JointCall_AF_{chrom}.tsv")
            if not os.path.exists(tsv):
                print(f"ERROR: missing {tsv}", file=sys.stderr)
                sys.exit(1)

            n_chrom = 0
            with open(tsv) as f:
                for line in f:
                    parts = line.rstrip("\n").split("\t")
                    if len(parts) < 6:
                        skipped += 1
                        continue
                    c, pos, vid, ref, alt, af_str = parts[:6]

                    if c != chrom:
                        # Source files are per-chromosome; cross-chrom rows
                        # would indicate corruption.
                        print(f"ERROR: {tsv} has row with chrom={c}",
                              file=sys.stderr)
                        sys.exit(1)

                    try:
                        af = float(af_str)
                    except ValueError:
                        skipped += 1
                        continue

                    if af <= 0 or af > 1:
                        # AF outside (0,1] is meaningless; skip
                        skipped += 1
                        continue

                    ac = int(round(af * AN))
                    if ac < 1:
                        ac = 1  # rounding floor: an observed alt allele
                    if ac > AN:
                        ac = AN

                    info = f"AN={AN};AC={ac};AF={af:.6g}"

                    if vid == "" or vid == ".":
                        vid = "."

                    out.write(f"{chrom}\t{pos}\t{vid}\t{ref}\t{alt}"
                              f"\t.\tPASS\t{info}\n")
                    n_chrom += 1
                    total += 1
            print(f"  {chrom}: {n_chrom:,} variants",
                  file=sys.stderr)

    print(f"Body written: {total:,} variants ({skipped} skipped)",
          file=sys.stderr)

    # Write header to a separate file
    hdr_path = output_vcf + ".unsorted.hdr"
    with open(hdr_path, "w") as h:
        h.write("##fileformat=VCFv4.2\n")
        h.write("##source=GenomeIndia 9768 (Bhattacharyya et al. 2025)\n")
        h.write('##INFO=<ID=AN,Number=1,Type=Integer,'
                'Description="Total allele number (2 * 9768 = 19536; '
                'autosomal, biallelic; assumes 100% call rate)">\n')
        h.write('##INFO=<ID=AC,Number=A,Type=Integer,'
                'Description="Synthesized alternate allele count '
                '(round(AF * AN); AN=19536)">\n')
        h.write('##INFO=<ID=AF,Number=A,Type=Float,'
                'Description="Alternate allele frequency from GenomeIndia '
                'summary stats">\n')
        for chrom in AUTOSOMES:
            h.write(f"##contig=<ID={chrom}>\n")
        h.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")

    # Concatenate header + body, then bgzip-sort with bcftools
    unsorted_vcf = output_vcf + ".unsorted.vcf"
    with open(unsorted_vcf, "w") as out:
        for p in (hdr_path, body_path):
            with open(p) as f:
                for line in f:
                    out.write(line)

    print("Sorting + bgzipping...", file=sys.stderr)
    # Files are already in chrom order; just sort by position within each
    # chrom. Easiest: bcftools sort handles both.
    subprocess.run(
        ["bcftools", "sort", "-Oz", "-T", "/data/tmp", "-m", "8G",
         "-o", output_vcf, unsorted_vcf],
        check=True)
    subprocess.run(["tabix", "-p", "vcf", output_vcf], check=True)

    os.remove(hdr_path)
    os.remove(body_path)
    os.remove(unsorted_vcf)

    print(f"Done: {output_vcf}", file=sys.stderr)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input_dir",
                   help="Dir with GI_9768_CBR-NIBMG_JointCall_AF_chr*.tsv")
    p.add_argument("output_vcf", help="Output .vcf.gz")
    args = p.parse_args()
    convert(args.input_dir, args.output_vcf)


if __name__ == "__main__":
    main()
