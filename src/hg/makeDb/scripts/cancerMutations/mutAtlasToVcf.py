#!/usr/bin/env python3
"""Convert the Cancer Mutation Atlas atlas.tsv to a sites-only VCF, so
bcftools csq can annotate it. The VCF ID is the line number in atlas.tsv
(header = line 0), mutAtlasToBed.py uses it to join the consequences back."""
import sys

def main():
    if len(sys.argv) != 3:
        sys.exit("usage: mutAtlasToVcf.py atlas.tsv chrom.sizes > out.vcf")
    atlasFn, sizesFn = sys.argv[1:]
    out = sys.stdout
    out.write("##fileformat=VCFv4.2\n")
    for line in open(sizesFn):
        chrom, size = line.split()[:2]
        out.write("##contig=<ID=%s,length=%s>\n" % (chrom, size))
    out.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")
    with open(atlasFn) as fh:
        header = fh.readline().rstrip("\n").split("\t")
        assert header[:4] == ["contig", "start", "ref", "alt"], header
        for lineNo, line in enumerate(fh, start=1):
            chrom, pos, ref, alt = line.split("\t", 4)[:4]
            out.write("%s\t%s\t%d\t%s\t%s\t.\t.\t.\n" % (chrom, pos, lineNo, ref, alt))

main()
