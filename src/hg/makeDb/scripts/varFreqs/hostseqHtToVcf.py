#!/usr/bin/env python3
"""Convert the gnomAD-Canada HostSeq "release sites" Hail Table into a
sites-only, bgzipped VCF carrying overall and per-genetic-ancestry AC/AN/AF.

Input is the Hail Table shipped as release_sites_hostseq.tar.gz from
https://www.bcgsc.ca/gnomad/ (10,487 Canadian COVID-19 WGS genomes,
gnomAD-Canada v1.0, GRCh38). The table is already biallelic-split (one row
per alternate allele) and uses UCSC-style chr-prefixed contigs.

Allele frequencies live in a single row array `freq`; the mapping from a
sample-grouping (e.g. overall high-quality, or a genetic-ancestry group) to
its slot in that array is given by the global `freq_meta` list of dicts. We
locate slots by matching those dicts (robust to gnomAD key-string format):
    overall high-quality (adj) : {'group': 'adj'}
    per ancestry group         : {'group': 'adj', 'gen_anc': <pop>}

Output INFO fields:
    AC, AN, AF, nhomalt            overall (adj) high-quality release
    AC_<pop>, AN_<pop>, AF_<pop>   per genetic-ancestry group
    grpmax_AF, grpmax_gen_anc      group with the highest AF (grpmax)
Plus dbSNP rsID (ID column) and the gnomAD variant-QC FILTER status.

Only variants with overall adj AC > 0 are written (this drops AC0-filtered
sites, which carry no frequency signal) and only canonical chr1-22/X/Y/M.

Usage:
    python3 hostseqHtToVcf.py <release_sites.ht> <out.vcf.bgz>

Run inside the dedicated Hail env, e.g.:
    micromamba run -n hail python3 hostseqHtToVcf.py ...
"""

import argparse
import sys
import hail as hl

# gnomAD genetic-ancestry groups present in the HostSeq release, in the order
# used for the per-population INFO fields. Labels are for the VCF header only.
# Note: the release uses the key "oth" for the group the README calls
# "Remaining individuals (formerly Other)".
POPS = [
    ("afr", "African/African-American"),
    ("amr", "Latino/Admixed-American"),
    ("asj", "Ashkenazi Jewish"),
    ("eas", "East Asian"),
    ("fin", "European (Finnish)"),
    ("mid", "Middle Eastern"),
    ("nfe", "European (non-Finnish)"),
    ("oth", "Remaining/Other individuals"),
    ("sas", "South Asian"),
]

CANON = ["chr%d" % i for i in range(1, 23)] + ["chrX", "chrY", "chrM"]


def find_meta_index(freq_meta, want):
    """Return the index i where freq_meta[i] == want, else None."""
    for i, d in enumerate(freq_meta):
        if dict(d) == want:
            return i
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ht_path")
    ap.add_argument("out_vcf")  # should end in .bgz
    ap.add_argument("--tmp", default="/hive/data/genomes/hg38/bed/varFreqs/hostseq/tmp")
    ap.add_argument("--local-tmp", default="/data/tmp")
    # The gnomAD-Canada release table is split into ~121k tiny partitions.
    # naive_coalesce merges adjacent partitions (no shuffle) so the Spark
    # driver isn't overwhelmed scheduling that many tasks and the single-file
    # export doesn't have to concatenate 121k shards.
    ap.add_argument("--partitions", type=int, default=2000)
    args = ap.parse_args()

    hl.init(
        quiet=True,
        tmp_dir=args.tmp,
        local_tmpdir=args.local_tmp,
        spark_conf={"spark.local.dir": args.local_tmp},
    )

    ht = hl.read_table(args.ht_path)
    n_part = ht.n_partitions()
    print("source partitions:", n_part, file=sys.stderr)
    if args.partitions and n_part > args.partitions:
        ht = ht.naive_coalesce(args.partitions)
        print("coalesced to", args.partitions, "partitions", file=sys.stderr)
    total_input = ht.count()
    print("total input rows:", total_input, file=sys.stderr)

    freq_meta = [dict(d) for d in hl.eval(ht.freq_meta)]
    overall_idx = find_meta_index(freq_meta, {"group": "adj"})
    if overall_idx is None:
        sys.exit("ERROR: could not find overall adj freq index in freq_meta")
    print("overall adj freq index:", overall_idx, file=sys.stderr)

    pop_idx = {}
    for pop, _label in POPS:
        idx = find_meta_index(freq_meta, {"group": "adj", "gen_anc": pop})
        pop_idx[pop] = idx
        print("pop %s -> index %s" % (pop, idx), file=sys.stderr)

    freq = ht.freq

    # Build the INFO struct. AC/AN/nhomalt forced to int32 for VCF export.
    info = hl.struct(
        AC=hl.int32(freq[overall_idx].AC),
        AN=hl.int32(freq[overall_idx].AN),
        AF=freq[overall_idx].AF,
        nhomalt=hl.int32(freq[overall_idx].homozygote_count),
    )
    for pop, _label in POPS:
        idx = pop_idx[pop]
        if idx is None:
            continue
        info = info.annotate(**{
            "AC_%s" % pop: hl.int32(freq[idx].AC),
            "AN_%s" % pop: hl.int32(freq[idx].AN),
            "AF_%s" % pop: freq[idx].AF,
        })

    # grpmax (population with the maximum AF), if present.
    if "grpmax" in ht.row:
        info = info.annotate(
            grpmax_AF=ht.grpmax.AF,
            grpmax_gen_anc=ht.grpmax.gen_anc,
        )

    out = ht.select(rsid=ht.rsid, filters=ht.filters, info=info)

    # Keep only sites with an observed allele in the release, on canonical chroms.
    out = out.filter(
        hl.is_defined(out.info.AC) & (out.info.AC > 0)
        & hl.set(CANON).contains(out.locus.contig)
    )

    # VCF header descriptions.
    info_meta = {
        "AC": {"Description": "Alternate allele count, overall high-quality release samples"},
        "AN": {"Description": "Total number of alleles, overall high-quality release samples"},
        "AF": {"Description": "Alternate allele frequency, overall high-quality release samples"},
        "nhomalt": {"Description": "Count of homozygous alternate individuals, overall"},
        "grpmax_AF": {"Description": "Maximum allele frequency across non-bottlenecked genetic ancestry groups (grpmax)"},
        "grpmax_gen_anc": {"Description": "Genetic ancestry group with the maximum allele frequency (grpmax)"},
    }
    for pop, label in POPS:
        info_meta["AC_%s" % pop] = {"Description": "Alternate allele count, %s" % label}
        info_meta["AN_%s" % pop] = {"Description": "Total alleles, %s" % label}
        info_meta["AF_%s" % pop] = {"Description": "Alternate allele frequency, %s" % label}
    filter_meta = {
        "AC0": {"Description": "Allele count is zero after filtering out low-confidence genotypes"},
        "RF": {"Description": "Failed the gnomAD random-forest variant-quality filter"},
        "InbreedingCoeff": {"Description": "GATK InbreedingCoeff below the release cutoff"},
        "PASS": {"Description": "Passed all variant filters"},
    }
    metadata = {"info": info_meta, "filter": filter_meta}

    hl.export_vcf(out, args.out_vcf, metadata=metadata, tabix=False)
    print("wrote", args.out_vcf, file=sys.stderr)


if __name__ == "__main__":
    main()
