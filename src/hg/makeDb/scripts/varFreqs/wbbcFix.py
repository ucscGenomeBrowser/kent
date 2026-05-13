#!/usr/bin/env python3
"""Stream-filter and re-header WBBC concat VCF: add proper INFO/contig
lines, drop AC=0 rows. Reads stdin, writes stdout."""
import sys

HG38_CONTIGS = [
    ("chr1",  248956422), ("chr2",  242193529), ("chr3",  198295559),
    ("chr4",  190214555), ("chr5",  181538259), ("chr6",  170805979),
    ("chr7",  159345973), ("chr8",  145138636), ("chr9",  138394717),
    ("chr10", 133797422), ("chr11", 135086622), ("chr12", 133275309),
    ("chr13", 114364328), ("chr14", 107043718), ("chr15", 101991189),
    ("chr16",  90338345), ("chr17",  83257441), ("chr18",  80373285),
    ("chr19",  58617616), ("chr20",  64444167), ("chr21",  46709983),
    ("chr22",  50818468),
]

INFO_DEFS = [
    ("AC",         "A", "Integer", "Alt allele count"),
    ("AF",         "A", "Float",   "Alt allele frequency"),
    ("AN",         "1", "Integer", "Total alleles called"),
    ("NS",         "1", "Integer", "Number of samples"),
    ("North_AF",   "A", "Float",   "Allele frequency in North Han subgroup"),
    ("North_AN",   "1", "Integer", "Allele number in North Han subgroup"),
    ("Central_AF", "A", "Float",   "Allele frequency in Central Han subgroup"),
    ("Central_AN", "1", "Integer", "Allele number in Central Han subgroup"),
    ("South_AF",   "A", "Float",   "Allele frequency in South Han subgroup"),
    ("South_AN",   "1", "Integer", "Allele number in South Han subgroup"),
    ("Lingnan_AF", "A", "Float",   "Allele frequency in Lingnan Han subgroup"),
    ("Lingnan_AN", "1", "Integer", "Allele number in Lingnan Han subgroup"),
    ("RR",         "1", "String",  "Pipe-separated hom-ref|het|hom-alt counts (RR|RA|AA)"),
    ("DP",         "1", "Integer", "Total depth across samples"),
    ("VQSLOD",     "1", "Float",   "GATK VQSR log-odds score"),
]

written = 0
filtered_ac0 = 0
in_header = True
column_line_written = False

out = sys.stdout
for line in sys.stdin:
    if in_header:
        if line.startswith("#CHROM"):
            # write our INFO and contig lines before the column line
            for chrom, length in HG38_CONTIGS:
                out.write(f"##contig=<ID={chrom},length={length}>\n")
            for info_id, num, vtype, desc in INFO_DEFS:
                out.write(f'##INFO=<ID={info_id},Number={num},Type={vtype},Description="{desc}">\n')
            out.write(line)
            in_header = False
            continue
        if line.startswith("##FORMAT"):
            # drop the leftover FORMAT line from SHAPEIT (sites-only file)
            continue
        out.write(line)
        continue

    # Data line. INFO is column 8 (1-based), so 7 (0-based).
    info_field = line.split("\t", 8)[7]
    if info_field.startswith("AC=0;"):
        filtered_ac0 += 1
        continue
    out.write(line)
    written += 1
    if written % 5000000 == 0:
        sys.stderr.write(f"  written {written:,}\n")

sys.stderr.write(f"written={written} filtered_AC0={filtered_ac0}\n")
