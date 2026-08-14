#!/usr/bin/env python3
"""
Convert the Neale Lab UK Biobank Round 2 variants.tsv.bgz manifest to a
sites-only VCF (GRCh37 / hg19 coordinates, with chr prefix). The source
file ships post-QC allele counts and imputation INFO scores from 361,194
imputed UK Biobank samples, restricted to white British ancestry, and is
documented at https://www.nealelab.is/uk-biobank.

Rows with AC=0 are dropped (variant not seen in the cohort).
AN is computed as 2 * n_called (diploid). For chrX this slightly
over-estimates the number of alleles in males but matches the convention
the rest of the Neale Lab pipeline uses.
"""
import sys, gzip

INPUT = "variants.tsv.bgz"
OUTPUT = "ukbb.hg19.vcf"

# (column-name-in-source, vcf-info-id, Number, Type, Description)
INFO_FIELDS = [
    ("AC",              "AC",     "A", "Integer", "Allele count (alt)"),
    ("AN",              "AN",     "1", "Integer", "Total alleles called (2 * n_called)"),
    ("AF",              "AF",     "A", "Float",   "Alt allele frequency"),
    ("info",            "INFO_S", "A", "Float",   "Imputation INFO score"),
    ("p_hwe",           "HWE",    "A", "Float",   "Hardy-Weinberg equilibrium p-value"),
    ("n_called",        "NS",     "1", "Integer", "Number of samples called"),
    ("n_hom_ref",       "n_hom_ref", "1", "Integer", "Number of hom-ref samples"),
    ("n_het",           "n_het",     "1", "Integer", "Number of heterozygous samples"),
    ("n_hom_var",       "n_hom_var", "1", "Integer", "Number of hom-alt samples"),
    ("consequence",     "CSQ",    "1", "String",  "Most severe VEP consequence (Neale Lab annotation)"),
    ("consequence_category", "CSQC", "1", "String", "VEP consequence category (non_coding/missense/synonymous/ptv)"),
]

# hg19 contig sizes (used in the VCF header so downstream tools accept the
# file). CrossMap rewrites them on lift.
HG19_CONTIGS = [
    ("chr1",  249250621), ("chr2",  243199373), ("chr3",  198022430),
    ("chr4",  191154276), ("chr5",  180915260), ("chr6",  171115067),
    ("chr7",  159138663), ("chr8",  146364022), ("chr9",  141213431),
    ("chr10", 135534747), ("chr11", 135006516), ("chr12", 133851895),
    ("chr13", 115169878), ("chr14", 107349540), ("chr15", 102531392),
    ("chr16",  90354753), ("chr17",  81195210), ("chr18",  78077248),
    ("chr19",  59128983), ("chr20",  63025520), ("chr21",  48129895),
    ("chr22",  51304566), ("chrX",  155270560),
]


def is_missing(v):
    return v == "" or v == "NA" or v == "nan" or v == "."


def main():
    inf  = gzip.open(INPUT, "rt")
    out  = open(OUTPUT, "w")

    header = inf.readline().rstrip("\n").split("\t")
    idx = {c: i for i, c in enumerate(header)}

    out.write("##fileformat=VCFv4.2\n")
    out.write("##source=NealeLab_UKB_Round2_variants\n")
    out.write("##reference=GRCh37\n")
    for name, length in HG19_CONTIGS:
        out.write(f"##contig=<ID={name},length={length}>\n")
    for _, info_id, num, vtype, desc in INFO_FIELDS:
        out.write(f'##INFO=<ID={info_id},Number={num},Type={vtype},Description="{desc}">\n')
    out.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")

    total = 0
    skipped_ac0 = 0
    skipped_bad = 0
    written = 0
    for line in inf:
        total += 1
        f = line.rstrip("\n").split("\t")
        if len(f) < len(header):
            skipped_bad += 1
            continue

        chrom = f[idx["chr"]]
        pos   = f[idx["pos"]]
        ref   = f[idx["ref"]]
        alt   = f[idx["alt"]]
        rsid  = f[idx["rsid"]]
        ac_str = f[idx["AC"]]
        n_called_str = f[idx["n_called"]]

        try:
            ac = int(ac_str)
        except ValueError:
            skipped_bad += 1
            continue
        if ac == 0:
            skipped_ac0 += 1
            continue

        try:
            n_called = int(n_called_str)
        except ValueError:
            skipped_bad += 1
            continue
        an = 2 * n_called

        if not chrom.startswith("chr"):
            chrom = "chr" + chrom

        if is_missing(rsid):
            rsid = "."

        info_parts = []
        for src_name, info_id, _, vtype, _ in INFO_FIELDS:
            if src_name == "AN":
                info_parts.append(f"AN={an}")
                continue
            v = f[idx[src_name]] if src_name in idx else ""
            if is_missing(v):
                continue
            if vtype == "Integer":
                try:
                    info_parts.append(f"{info_id}={int(float(v))}")
                except ValueError:
                    pass
            elif vtype == "Float":
                try:
                    info_parts.append(f"{info_id}={float(v):.6g}")
                except ValueError:
                    pass
            else:
                vv = v.replace(";", "%3B").replace("=", "%3D").replace(" ", "_")
                info_parts.append(f"{info_id}={vv}")

        info_str = ";".join(info_parts) if info_parts else "."
        out.write(f"{chrom}\t{pos}\t{rsid}\t{ref}\t{alt}\t.\tPASS\t{info_str}\n")
        written += 1

        if written % 1000000 == 0:
            sys.stderr.write(f"  written {written:,}\n")

    inf.close()
    out.close()
    sys.stderr.write(
        f"total={total} written={written} skipped_AC0={skipped_ac0} skipped_bad={skipped_bad}\n"
    )


if __name__ == "__main__":
    main()
