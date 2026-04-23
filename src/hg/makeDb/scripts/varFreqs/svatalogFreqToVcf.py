#!/usr/bin/env python3
"""Convert SVatalog per-chromosome allele-frequency TSVs to a single VCF.

Input columns per line:
    CHROM POS ID REF ALT AF gnomAD_genome31_AF_nfe dbsnp

The source only ships AF; AC and AN are synthesized from AF assuming the
full 101-sample / 202-allele denominator (AN=202, AC=round(AF*202)). This
is an approximation but gives the varFreqs detail pages usable AC/AN
fields. The gnomAD v3.1 non-Finnish European AF and the dbSNP rsID are
passed through as additional INFO fields.

Usage:
    svatalogFreqToVcf.py out.vcf chr1_allele_freq.txt [chr2_allele_freq.txt ...]

Dataset: Chirmade et al. 2026, Heredity, PMID 41203876 / GWAS SVatalog
data release (Zenodo 13367574). SNPs called with GATK HaplotypeCaller
v4.0.0.0 on 10X Genomics linked short-read WGS of the 101 GWAS SVatalog
samples; phased with SHAPEIT v4.2.0.
"""

import sys

N_SAMPLES = 101
AN_FIXED = N_SAMPLES * 2


VCF_HEADER_TOP = """##fileformat=VCFv4.2
##source=GWAS_SVatalog_allele_freq
##reference=GRCh38
##INFO=<ID=AF,Number=A,Type=Float,Description="Alternate allele frequency in the 101 GWAS SVatalog samples">
##INFO=<ID=AC,Number=A,Type=Integer,Description="Alternate allele count (synthesized: round(AF * AN))">
##INFO=<ID=AN,Number=1,Type=Integer,Description="Total number of alleles (fixed at 2*101 = 202; source file does not ship AN)">
##INFO=<ID=GNOMAD_NFE_AF,Number=A,Type=Float,Description="Alternate allele frequency in gnomAD v3.1 non-Finnish European samples (from source file column gnomAD_genome31_AF_nfe)">
##INFO=<ID=RSID,Number=1,Type=String,Description="dbSNP rsID (from source file column dbsnp)">
"""

CHROM_SIZES = "/hive/data/genomes/hg38/chrom.sizes"


def buildContigHeader():
    lines = []
    with open(CHROM_SIZES) as f:
        for line in f:
            name, size = line.rstrip("\n").split("\t")
            lines.append(f"##contig=<ID={name},length={size}>")
    return "\n".join(lines) + "\n"


def toFloat(s):
    if s is None:
        return None
    s = s.strip()
    if s == "" or s == "NA" or s == "nan":
        return None
    try:
        return float(s)
    except ValueError:
        return None


def processFile(path, fOut):
    n = 0
    with open(path) as fIn:
        header = fIn.readline().rstrip("\n").split("\t")
        cols = {name: i for i, name in enumerate(header)}
        needed = ["CHROM", "POS", "ID", "REF", "ALT", "AF"]
        for col in needed:
            if col not in cols:
                raise SystemExit(f"{path}: missing required column {col}")

        for line in fIn:
            fields = line.rstrip("\n").split("\t")
            if len(fields) < len(header):
                continue
            chrom = fields[cols["CHROM"]]
            pos = fields[cols["POS"]]
            vid = fields[cols["ID"]]
            ref = fields[cols["REF"]]
            alt = fields[cols["ALT"]]
            af = toFloat(fields[cols["AF"]])
            if af is None:
                continue

            nfe = None
            if "gnomAD_genome31_AF_nfe" in cols:
                nfe = toFloat(fields[cols["gnomAD_genome31_AF_nfe"]])

            rsid = ""
            if "dbsnp" in cols:
                raw = fields[cols["dbsnp"]].strip()
                if raw and raw != "NA":
                    rsid = raw

            ac = max(0, min(AN_FIXED, round(af * AN_FIXED)))
            infoParts = [
                f"AF={af:.6f}",
                f"AC={ac}",
                f"AN={AN_FIXED}",
            ]
            if nfe is not None:
                infoParts.append(f"GNOMAD_NFE_AF={nfe:.6f}")
            if rsid:
                infoParts.append(f"RSID={rsid}")

            displayId = rsid if rsid else vid
            fOut.write(
                f"{chrom}\t{pos}\t{displayId}\t{ref}\t{alt}\t.\tPASS\t"
                + ";".join(infoParts)
                + "\n"
            )
            n += 1
    return n


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    outPath = sys.argv[1]
    inPaths = sys.argv[2:]

    total = 0
    with open(outPath, "w") as fOut:
        fOut.write(VCF_HEADER_TOP)
        fOut.write(buildContigHeader())
        fOut.write("#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n")
        for p in inPaths:
            n = processFile(p, fOut)
            print(f"{p}: {n} records", file=sys.stderr)
            total += n
    print(f"total {total} records written to {outPath}", file=sys.stderr)


if __name__ == "__main__":
    main()
