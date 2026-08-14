#!/usr/bin/env python3
"""
Convert m6A-Atlas v2 hg38_CL_Tech.txt.gz (high-confidence) to bed9+ for the
UCSC Genome Browser.

The input is a TSV with 1-based coordinates (start == end, width == 1) and
the following columns:
  1  seqnames
  2  start            1-based position of the modified A
  3  end              1-based position (== start)
  4  width            always 1
  5  strand           + / -
  6  GSE_tech_species_cellLine_treatment_C_count_Coverage   per-study evidence
                     pipe-separated; each entry has 5 fields
  7  SeqCont          101-base sequence context (we drop this)
  8  annotation       region (Exon, 3'UTR/Exon, 5'UTR/Exon, Intron,
                              Intergenic, 5'UTR/3'UTR/Exon)
  9  Gene_Name
 10  Gene_Source
 11  Gene_Biotype
 12  Ensembl_ID
 13  ID               m6A_hg38_NNN, used in detail URL
 14  RBP_Num
 15  miRNA_Num
 16  SplicingSite_Num
 17  SNP_Num
 18  Cell_Line        semicolon-separated list
 19  Cell_Line_Num
 20  Technique        semicolon-separated list
 21  Technique_Num

Output BED columns (bed9+):
  chrom chromStart chromEnd name score strand thickStart thickEnd reserved
  region geneName geneBiotype ensemblId
  nTechniques techniques nCellLines cellLines
  rbpNum miRNANum splicingSiteNum snpNum
  evidence geneSource detailUrl
"""
import gzip
import sys

# region -> RGB; categorical, flat colors per CLAUDE skill guidance.
REGION_COLOR = {
    "5'UTR/Exon":           "0,128,0",     # green
    "Exon":                 "31,119,180",  # dark blue (CDS-like)
    "3'UTR/Exon":           "214,39,40",   # red
    "5'UTR/3'UTR/Exon":     "148,103,189", # purple (mixed)
    "Intron":               "127,127,127", # gray
    "Intergenic":           "189,189,189", # light gray
}
DEFAULT_COLOR = "0,0,0"

DETAIL_URL = "http://rnamd.org/m6a/detail.php?species=hg38&atlas_id={}"


def score_from_counts(n_tech, n_cells):
    # Scale: each technique ~150, each extra cell line ~30, capped at 1000.
    s = 150 * n_tech + 30 * max(0, n_cells - 1)
    if s > 1000:
        s = 1000
    if s < 0:
        s = 0
    return s


def main(in_path, out_path):
    chrom_seen = set()
    n_in = 0
    n_out = 0
    n_skip_chrom = 0
    n_skip_bounds = 0

    # Load chrom sizes so we can sanity-check coords.
    chrom_sizes = {}
    with open("/hive/data/genomes/hg38/chrom.sizes") as f:
        for line in f:
            chrom, size = line.rstrip().split("\t")
            chrom_sizes[chrom] = int(size)

    opener = gzip.open if in_path.endswith(".gz") else open
    with opener(in_path, "rt") as fin, open(out_path, "w") as fout:
        header = fin.readline()
        if not header.startswith("seqnames"):
            sys.exit("unexpected header: " + header)
        for line in fin:
            n_in += 1
            f = line.rstrip("\n").split("\t")
            if len(f) < 21:
                sys.exit("short line {}: {}".format(n_in, line[:200]))
            chrom = f[0]
            start1 = int(f[1])
            end1 = int(f[2])
            width = int(f[3])
            strand = f[4]
            evidence = f[5]
            # f[6] is SeqCont (sequence context) -- dropped.
            region = f[7]
            gene_name = f[8] or ""
            gene_source = f[9] or ""
            gene_biotype = f[10] or ""
            ensembl_id = f[11] or ""
            site_id = f[12]
            rbp_num = int(f[13]) if f[13] else 0
            mirna_num = int(f[14]) if f[14] else 0
            splice_num = int(f[15]) if f[15] else 0
            snp_num = int(f[16]) if f[16] else 0
            cell_lines = f[17].replace(";", ", ")
            n_cells = int(f[18]) if f[18] else 0
            techniques = f[19].replace(";", ", ").strip()
            n_tech = int(f[20]) if f[20] else 0

            if chrom not in chrom_sizes:
                n_skip_chrom += 1
                continue
            if width != 1 or start1 != end1:
                sys.exit("unexpected width/coords on line {}: {}".format(n_in, line[:200]))

            # 1-based inclusive -> 0-based half-open
            chrom_start = start1 - 1
            chrom_end = end1
            if chrom_start < 0 or chrom_end > chrom_sizes[chrom]:
                n_skip_bounds += 1
                sys.stderr.write(
                    "WARN out-of-bounds {} {}-{} (chromSize {}); skipped\n".format(
                        chrom, chrom_start, chrom_end, chrom_sizes[chrom]))
                continue

            color = REGION_COLOR.get(region, DEFAULT_COLOR)
            score = score_from_counts(n_tech, n_cells)
            detail_url = DETAIL_URL.format(site_id)

            chrom_seen.add(chrom)

            fout.write("\t".join([
                chrom, str(chrom_start), str(chrom_end),
                site_id, str(score), strand,
                str(chrom_start), str(chrom_end), color,
                region, gene_name, gene_biotype, ensembl_id,
                str(n_tech), techniques,
                str(n_cells), cell_lines,
                str(rbp_num), str(mirna_num), str(splice_num), str(snp_num),
                evidence, gene_source, detail_url,
            ]) + "\n")
            n_out += 1

    sys.stderr.write(
        "input rows: {}\noutput rows: {}\nskipped (unknown chrom): {}\n"
        "skipped (out-of-bounds): {}\nchroms with output: {}\n".format(
            n_in, n_out, n_skip_chrom, n_skip_bounds, len(chrom_seen)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: m6aAtlasToBed.py input.txt.gz output.bed")
    main(sys.argv[1], sys.argv[2])
