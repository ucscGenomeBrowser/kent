#!/usr/bin/env python3
"""Convert the SFARI SPARK WGS 2026_08 variant frequency table to a sites-only VCF.

The SPARK WGS release (DS0000135, 45,178 individuals) does not ship a sites
VCF, only SPARK.WGS.2026_08.gatk.pvcf_variant_frequencies.tsv, which was made
with  bcftools query -f '%CHROM\\t%POS\\t%REF\\t%ALT\\t%AF\\n'  from the
GLnexus pVCFs. So there is no AC or AN, only a comma-separated AF per ALT.

AN estimate: the AF values are rounded to 6 decimal places. All low AFs in the
file are exact multiples of 1/90356 = 1/(2 * 45178), on chrX and chrY as well
(e.g. every one of the 179 million AF=1 allele counts is 1.1e-05, never 1.2e-05,
which an AN lower than ~87,000 would give). So the AF denominator is at or very
close to the full sample count everywhere, and we set AN=90356 on every record
and AC=round(AF*AN). With 6 decimals, that rounding is exact to +-0.05 alleles.

Multiallelic records are split into one record per ALT. Alleles with AC <= 1
(singletons) are dropped. Output is an unsorted, not-yet-normalized VCF body
on stdout (use -H to also print the header); left-alignment, sorting and
bgzip are done by sparkWgs45kToVcf.sh. Counts are printed to stderr.

Usage:
    sparkWgs45kToVcf.py [-H] [--chromSizes FILE] in.tsv > out.vcf
"""

import argparse
import sys

NSAMPLES = 45178
AN = 2 * NSAMPLES
MINAC = 2   # drop alleles with an AC below this, i.e. singletons


def printHeader(out, chromSizes):
    out.write("##fileformat=VCFv4.2\n")
    out.write("##source=SPARK.WGS.2026_08.gatk.pvcf_variant_frequencies.tsv, "
              "converted by sparkWgs45kToVcf.py\n")
    out.write('##INFO=<ID=AC,Number=A,Type=Integer,Description="Estimated '
              'alternate allele count, round(AF*AN)">\n')
    out.write('##INFO=<ID=AN,Number=1,Type=Integer,Description="Estimated total '
              'number of alleles, 2 x %d samples; not provided by the source">\n' % NSAMPLES)
    out.write('##INFO=<ID=AF,Number=A,Type=Float,Description="Alternate allele '
              'frequency from the SPARK pVCF, rounded to 6 decimals">\n')
    for line in open(chromSizes):
        chrom, size = line.split()[:2]
        if "_" in chrom:
            continue
        out.write("##contig=<ID=%s,length=%s>\n" % (chrom, size))
    out.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("inTsv", help="input tsv, '-' for stdin")
    parser.add_argument("-H", "--header", action="store_true", help="print the VCF header")
    parser.add_argument("--chromSizes", default="/hive/data/genomes/hg38/chrom.sizes")
    args = parser.parse_args()

    out = sys.stdout
    if args.header:
        printHeader(out, args.chromSizes)

    ifh = sys.stdin if args.inTsv == "-" else open(args.inTsv)
    inRecs = inAlleles = outRecs = skipSingle = skipStar = 0
    for line in ifh:
        if line.startswith("chrom\t"):
            continue
        chrom, pos, ref, alts, afs = line.rstrip("\n").split("\t")
        inRecs += 1
        alts = alts.split(",")
        afs = afs.split(",")
        if len(alts) != len(afs):
            sys.exit("ALT/AF count mismatch: %s" % line)
        for alt, af in zip(alts, afs):
            inAlleles += 1
            if alt == "*":
                skipStar += 1
                continue
            ac = int(round(float(af) * AN))
            if ac < MINAC:
                skipSingle += 1
                continue
            out.write("%s\t%s\t.\t%s\t%s\t.\t.\tAC=%d;AN=%d;AF=%s\n" %
                      (chrom, pos, ref, alt, ac, AN, af))
            outRecs += 1

    sys.stderr.write("%s\tinputRecords=%d inputAlleles=%d singletonsDropped=%d "
                     "spanningDelDropped=%d outputRecords=%d\n" %
                     (args.inTsv, inRecs, inAlleles, skipSingle, skipStar, outRecs))


if __name__ == "__main__":
    main()
