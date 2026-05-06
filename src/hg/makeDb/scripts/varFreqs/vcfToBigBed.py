#!/usr/bin/env python3
"""
Convert merged/annotated VCF to bigBed with variant frequencies.

Reads database config from TSV files in the kent source tree.
Two-phase parallel processing:
  Phase 1: Pre-extract frequency data from each VCF (parallel by database)
  Phase 2: Build BED from annotated VCF + pre-extracted data (parallel by chromosome)

Usage:
  python3 vcfToBigBed.py                          # full genome
  python3 vcfToBigBed.py --region chrM             # quick test on chrM
  python3 vcfToBigBed.py --region chr22 --threads 4
"""

import sys
import os
import subprocess
import argparse
import shutil
from collections import OrderedDict
from typing import List, Tuple, Dict, Optional
from multiprocessing import Pool

# ============================================================================
# Constants
# ============================================================================

SCRIPTS_DIR = os.path.join(os.path.expanduser("~"), "kent", "src", "hg",
                           "makeDb", "scripts", "varFreqs")

ALL_CHROMOSOMES = [f"chr{i}" for i in range(1, 23)] + ["chrX", "chrY", "chrM"]

CHR_RENAME = {str(i): f"chr{i}" for i in range(1, 23)}
CHR_RENAME.update({"X": "chrX", "Y": "chrY", "M": "chrM", "MT": "chrM",
                    "23": "chrX", "24": "chrY"})

# ============================================================================
# Consequence Handling
# ============================================================================

LOF_CSQ = {"stop_gained", "stop_lost", "frameshift", "splice_donor",
           "splice_acceptor", "start_lost", "transcript_ablation",
           "feature_truncation"}
MISSENSE_CSQ = {"missense", "inframe_insertion", "inframe_deletion",
                "protein_altering", "coding_sequence"}
SYN_CSQ = {"synonymous", "stop_retained", "start_retained"}


def get_color(bcsq):
    """Map BCSQ to RGB color based on most severe consequence."""
    if not bcsq or bcsq == ".":
        return (128, 128, 128)
    best_priority = -1
    best_color = (128, 128, 128)
    for entry in bcsq.split(","):
        csq = entry.split("|")[0].lower()
        for part in csq.split("&"):
            if part in LOF_CSQ and best_priority < 3:
                best_priority, best_color = 3, (255, 0, 0)
            elif part in MISSENSE_CSQ and best_priority < 2:
                best_priority, best_color = 2, (31, 119, 180)
            elif part in SYN_CSQ and best_priority < 1:
                best_priority, best_color = 1, (0, 128, 0)
    return best_color


def parse_bcsq(bcsq):
    """Parse BCSQ → (consequence, gene, transcript, aa_change, dna_change).
    aa_change/dna_change return "" rather than "." for non-coding so the
    mouseOver and detail page render a clean blank line."""
    if not bcsq or bcsq == ".":
        return (".", ".", ".", "", "")
    best, best_pri = None, -1
    for entry in bcsq.split(","):
        parts = entry.split("|")
        if len(parts) < 4:
            continue
        csq = parts[0].lower().split("&")[0]
        pri = 3 if csq in LOF_CSQ else 2 if csq in MISSENSE_CSQ \
              else 1 if csq in SYN_CSQ else 0
        if pri > best_pri:
            best_pri, best = pri, parts
    if best is None:
        best = bcsq.split(",")[0].split("|")
    return (
        best[0] if len(best) > 0 else ".",
        best[1] if len(best) > 1 else ".",
        best[2] if len(best) > 2 else ".",
        best[5] if len(best) > 5 else "",
        best[6] if len(best) > 6 else "",
    )


# Filter buckets exposed in the trackdb consequence multipleListOr filter.
# Anything that produces no token in this set gets ",others" appended so it
# is still selectable via the "Other" bucket.
NAMED_CONSEQUENCES = {
    "missense", "synonymous", "stop_gained", "frameshift",
    "splice_donor", "splice_acceptor", "intron", ".",
    "3_prime_utr", "5_prime_utr", "non_coding",
}


def normalize_consequence(raw):
    """Convert bcftools csq consequence (& or , joined) to a comma-joined
    token list. Append "others" if no token matches a named filter bucket."""
    if not raw or raw == ".":
        return "."
    tokens = raw.replace("&", ",").split(",")
    if not any(tok in NAMED_CONSEQUENCES for tok in tokens):
        tokens.append("others")
    return ",".join(tokens)


def get_vartype(ref, alt):
    if len(ref) == 1 and len(alt) == 1:
        return "SNV"
    elif len(ref) == 1:
        return "INS"
    elif len(alt) == 1:
        return "DEL"
    return "MNV"


# ============================================================================
# Configuration Loading
# ============================================================================

def load_config(scripts_dir):
    """Load databases.tsv and populations.tsv."""
    databases = OrderedDict()

    db_path = os.path.join(scripts_dir, "databases.tsv")
    with open(db_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) < 5:
                print(f"WARNING: skipping malformed line: {line}",
                      file=sys.stderr)
                continue
            key, name, vcf, ac_field, af_field = (
                parts[0], parts[1], parts[2], parts[3], parts[4])
            databases[key] = {
                "name": name, "vcf": vcf,
                "ac_field": ac_field, "af_field": af_field,
                "pops": [],
            }

    pop_path = os.path.join(scripts_dir, "populations.tsv")
    with open(pop_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) < 5:
                continue
            db_key = parts[0]
            if db_key in databases:
                databases[db_key]["pops"].append({
                    "key": parts[1], "name": parts[2],
                    "ac_field": parts[3], "af_field": parts[4],
                })
            else:
                print(f"WARNING: population references unknown db '{db_key}'",
                      file=sys.stderr)

    return databases


# ============================================================================
# Phase 1: Pre-extract frequency data (parallel by database)
# ============================================================================

def pre_extract_one(args):
    """Extract frequency data from one VCF, split output by chromosome."""
    db_key, db_info, region, extract_dir = args

    vcf = db_info["vcf"]
    if not os.path.exists(vcf):
        print(f"  {db_key}: VCF not found ({vcf}), skipping", file=sys.stderr)
        return db_key, 0

    out_dir = os.path.join(extract_dir, db_key)
    os.makedirs(out_dir, exist_ok=True)

    # Build bcftools query format: CHROM POS REF ALT [AC AF] [pop_ac pop_af ...]
    fmt_parts = ["%CHROM", "%POS", "%REF", "%ALT"]
    if db_info["ac_field"] != ".":
        fmt_parts.append(f"%INFO/{db_info['ac_field']}")
    else:
        fmt_parts.append(".")
    if db_info["af_field"] != ".":
        fmt_parts.append(f"%INFO/{db_info['af_field']}")
    else:
        fmt_parts.append(".")
    for pop in db_info["pops"]:
        if pop["ac_field"] != ".":
            fmt_parts.append(f"%INFO/{pop['ac_field']}")
        else:
            fmt_parts.append(".")
        if pop["af_field"] != ".":
            fmt_parts.append(f"%INFO/{pop['af_field']}")
        else:
            fmt_parts.append(".")
    fmt = "\t".join(fmt_parts) + "\n"

    cmd = ["bcftools", "query", "-f", fmt]
    if region:
        cmd.extend(["-r", region])
    cmd.append(vcf)

    chrom_files = {}
    total = 0

    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        for line in proc.stdout:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 4:
                continue
            chrom = parts[0]
            if chrom in CHR_RENAME:
                chrom = CHR_RENAME[chrom]
            if chrom not in chrom_files:
                chrom_files[chrom] = open(
                    os.path.join(out_dir, f"{chrom}.tsv"), "w")
            # Write: POS\tREF\tALT\tAC\tAF\t[pop fields...]
            chrom_files[chrom].write("\t".join(parts[1:]) + "\n")
            total += 1
        proc.wait()
    except Exception as e:
        print(f"  {db_key}: ERROR: {e}", file=sys.stderr)
    finally:
        for fh in chrom_files.values():
            fh.close()

    print(f"  {db_key}: {total:,} variants extracted", file=sys.stderr)
    return db_key, total


def pre_extract_all(databases, region, extract_dir, threads):
    """Phase 1: Pre-extract all VCFs in parallel."""
    print(f"\n=== Phase 1: Pre-extracting frequency data "
          f"({threads} parallel) ===", file=sys.stderr)

    tasks = [(db_key, db_info, region, extract_dir)
             for db_key, db_info in databases.items()]

    with Pool(min(threads, len(tasks))) as pool:
        pool.map(pre_extract_one, tasks)


# ============================================================================
# Phase 2: Process chromosomes (parallel by chromosome)
# ============================================================================

def load_extracted(db_key, chrom, extract_dir):
    """Load pre-extracted TSV into dict keyed by pos:ref:alt."""
    path = os.path.join(extract_dir, db_key, f"{chrom}.tsv")
    data = {}
    if not os.path.exists(path):
        return data
    with open(path) as f:
        for line in f:
            parts = line.rstrip('\n').split('\t')
            if len(parts) < 3:
                continue
            key = f"{parts[0]}:{parts[1]}:{parts[2]}"
            data[key] = parts[3:]
    return data


def process_chromosome(args):
    """Phase 2: Build BED for one chromosome from annotated VCF + extracted data."""
    chrom, annotated_vcf, databases, extract_dir, output_dir = args

    # Read annotated variants (try BCSQ first, fall back to plain variants)
    cmd = ["bcftools", "query", "-r", chrom, "-f",
           "%CHROM\t%POS\t%REF\t%ALT\t%INFO/BCSQ\n", annotated_vcf]
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0 or not result.stdout.strip():
        # Fall back: read without BCSQ (use "." as consequence)
        cmd = ["bcftools", "query", "-r", chrom, "-f",
               "%CHROM\t%POS\t%REF\t%ALT\t.\n", annotated_vcf]
        result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0 or not result.stdout.strip():
        print(f"  {chrom}: no variants", file=sys.stderr)
        return None

    ann_lines = result.stdout.strip().split("\n")
    print(f"  {chrom}: {len(ann_lines):,} variants, "
          f"loading frequencies...", file=sys.stderr)

    # Load pre-extracted frequency data
    freq_data = {}
    for db_key in databases:
        freq_data[db_key] = load_extracted(db_key, chrom, extract_dir)

    output_path = os.path.join(output_dir, f"{chrom}.bed")
    count = 0

    with open(output_path, "w") as out:
        for line in ann_lines:
            parts = line.split("\t")
            if len(parts) < 5:
                continue

            chrom_name, pos, ref, alt, bcsq = parts[0:5]
            key = f"{pos}:{ref}:{alt}"

            consequence, gene, transcript, aa_change, dna_change = \
                parse_bcsq(bcsq)
            r, g, b = get_color(bcsq)

            pos_int = int(pos)
            start = pos_int - 1
            end = start + len(ref)

            ref_d = ref[:17] + "..." if len(ref) > 20 else ref
            alt_d = alt[:17] + "..." if len(alt) > 20 else alt
            name = f"{ref_d}>{alt_d}"
            var_type = get_vartype(ref, alt)

            max_af = 0.0
            sources = []
            db_ac_af = []    # per-database AC, AF
            pop_ac_af = []   # per-population AC, AF (written AFTER all db fields)
            total_ac = 0

            for db_key, db_info in databases.items():
                values = freq_data.get(db_key, {}).get(key, [])

                ac = values[0] if len(values) > 0 and values[0] not in \
                     (".", "") else ""
                af = values[1] if len(values) > 1 and values[1] not in \
                     (".", "") else ""

                # A source contributes if the unified AC/AF slot has data OR
                # any per-population slot has data. AllOfUs ships only
                # per-population fields ("." in the unified slot) so without
                # this fallback its 67M+ variants get no source attribution.
                has_data = bool(ac) or bool(af)
                if not has_data:
                    for i in range(len(db_info["pops"])):
                        idx = 2 + i * 2
                        pop_ac = values[idx] if len(values) > idx and \
                            values[idx] not in (".", "") else ""
                        pop_af = values[idx + 1] if len(values) > idx + 1 \
                            and values[idx + 1] not in (".", "") else ""
                        if pop_ac or pop_af:
                            has_data = True
                            break

                if has_data:
                    sources.append(db_key)

                if ac:
                    try:
                        total_ac += int(ac)
                    except ValueError:
                        pass

                db_ac_af.extend([ac, af])

                if af:
                    try:
                        max_af = max(max_af, float(af))
                    except ValueError:
                        pass

                for i, pop in enumerate(db_info["pops"]):
                    idx = 2 + i * 2
                    pop_ac = values[idx] if len(values) > idx and \
                        values[idx] not in (".", "") else ""
                    pop_af = values[idx + 1] if len(values) > idx + 1 and \
                        values[idx + 1] not in (".", "") else ""
                    pop_ac_af.extend([pop_ac, pop_af])

                    if pop_af:
                        try:
                            max_af = max(max_af, float(pop_af))
                        except ValueError:
                            pass

            score = min(1000, int(max_af * 1000))

            fields = [
                chrom_name, str(start), str(end), name, str(score), "+",
                str(start), str(end), f"{r},{g},{b}",
                ref, alt, str(len(ref)), str(len(alt)),
                str(len(alt) - len(ref)), var_type,
                normalize_consequence(consequence),
                gene, transcript, aa_change, dna_change,
                f"{max_af:.6f}" if max_af > 0 else "0",
                str(total_ac) if total_ac > 0 else "0",
                ",".join(sources),
            ]
            # Database AC/AF first, then population AC/AF — must match autoSql order
            fields.extend(db_ac_af)
            fields.extend(pop_ac_af)

            out.write("\t".join(fields) + "\n")
            count += 1

    print(f"  {chrom}: wrote {count:,} BED lines", file=sys.stderr)
    return output_path


# ============================================================================
# AutoSql Generation
# ============================================================================

def generate_autosql(output_path, databases):
    """Generate autoSql file from database config."""
    with open(output_path, "w") as f:
        f.write('table varFreqsAll\n')
        f.write('"Variant frequencies across population databases"\n')
        f.write('(\n')
        # BED9
        f.write('    string chrom;        "Chromosome"\n')
        f.write('    uint chromStart;     "Start position (0-based)"\n')
        f.write('    uint chromEnd;       "End position"\n')
        f.write('    string name;         "Variant (REF>ALT)"\n')
        f.write('    uint score;          "Score (maxAF * 1000)"\n')
        f.write('    char[1] strand;      "Strand"\n')
        f.write('    uint thickStart;     "Thick start"\n')
        f.write('    uint thickEnd;       "Thick end"\n')
        f.write('    uint reserved;       "Color by consequence"\n')
        # Variant info
        f.write('    lstring ref;         "Reference allele"\n')
        f.write('    lstring alt;         "Alternate allele"\n')
        f.write('    int refLen;          "Reference length"\n')
        f.write('    int altLen;          "Alternate length"\n')
        f.write('    int varLen;          "Length change (alt-ref)"\n')
        f.write('    string varType;      "Type (SNV/INS/DEL/MNV)"\n')
        # Consequence
        f.write('    string consequence;  "Consequence"\n')
        f.write('    string gene;         "Gene"\n')
        f.write('    string transcript;   "Transcript"\n')
        f.write('    lstring aaChange;    "AA change"\n')
        f.write('    lstring dnaChange;   "DNA change"\n')
        # Frequencies
        f.write('    float maxAF;         "Max allele frequency"\n')
        f.write('    uint totalAC;        "Total allele count across all databases"\n')
        f.write('    string sources;      "Source databases"\n')
        # Per-database AC/AF
        for db_key, db_info in databases.items():
            f.write(f'    string {db_key}AC;'
                    f'      "{db_info["name"]} AC"\n')
            f.write(f'    string {db_key}AF;'
                    f'      "{db_info["name"]} AF"\n')
        # Per-population AC/AF
        for db_key, db_info in databases.items():
            for pop in db_info["pops"]:
                f.write(f'    string {db_key}AC_{pop["key"]};'
                        f'  "{db_info["name"]} {pop["name"]} AC"\n')
                f.write(f'    string {db_key}AF_{pop["key"]};'
                        f'  "{db_info["name"]} {pop["name"]} AF"\n')
        f.write(')\n')
    print(f"Wrote autoSql: {output_path}", file=sys.stderr)


def generate_trackdb_fragment(output_path, databases):
    """Generate trackDb .ra fragment for the varFreqsAll track filters."""
    with open(output_path, "w") as f:
        f.write("# Auto-generated trackDb fragment for varFreqsAll filters\n")
        f.write("# Paste this into the varFreqsAll track stanza in varFreqs.ra\n\n")

        # Source database filter
        src_parts = []
        for db_key, db_info in databases.items():
            src_parts.append(f"{db_key}|{db_info['name']}")
        f.write("        filterValues.sources "
                + ",".join(src_parts) + "\n")
        f.write("        filterType.sources multipleListOr\n")
        f.write("        filterLabel.sources Source Database\n")

        # Variant type and consequence (static)
        f.write("        # Variant type and consequence filters\n")
        f.write("        filterValues.varType "
                "SNV|SNV,INS|Insertion,DEL|Deletion,MNV|MNV\n")
        f.write("        filterLabel.varType Variant Type\n")
        f.write("        filterValues.consequence "
                "missense|Missense,synonymous|Synonymous,"
                "stop_gained|Stop Gained,frameshift|Frameshift,"
                "splice_donor|Splice Donor,splice_acceptor|Splice Acceptor,"
                "intron|Intron,.|Intergenic\n")
        f.write("        filterLabel.consequence Consequence\n")

        # Length filters
        f.write("        # Length filters\n")
        for fld in ["refLen", "altLen", "varLen"]:
            label = {"refLen": "Reference Length", "altLen": "Alternate Length",
                     "varLen": "Length Change"}[fld]
            f.write(f"        filterByRange.{fld} on\n")
            f.write(f"        filterLabel.{fld} {label}\n")

        # Max AF filter
        f.write("        # Max AF filter\n")
        f.write("        filterByRange.maxAF on\n")
        f.write("        filterLabel.maxAF Max Allele Frequency\n")
        f.write("        filterLimits.maxAF 0:1\n")

        # Total AC filter
        f.write("        # Total AC filter\n")
        f.write("        filterByRange.totalAC on\n")
        f.write("        filterLabel.totalAC Total Allele Count (all databases)\n")

        # Per-database AF and AC filters
        f.write("        # Per-database AF filters\n")
        for db_key, db_info in databases.items():
            f.write(f"        filterByRange.{db_key}AF on\n")
            f.write(f"        filterLabel.{db_key}AF "
                    f"{db_info['name']} AF\n")

        f.write("        # Per-database AC filters\n")
        for db_key, db_info in databases.items():
            f.write(f"        filterByRange.{db_key}AC on\n")
            f.write(f"        filterLabel.{db_key}AC "
                    f"{db_info['name']} AC\n")

        # Population-specific filters
        f.write("        # Population-specific AF filters\n")
        for db_key, db_info in databases.items():
            if not db_info["pops"]:
                continue
            f.write(f"        # {db_info['name']} populations\n")
            for pop in db_info["pops"]:
                f.write(f"        filterByRange.{db_key}AF_{pop['key']} on\n")
                f.write(f"        filterLabel.{db_key}AF_{pop['key']} "
                        f"{db_info['name']} {pop['name']} AF\n")
            for pop in db_info["pops"]:
                f.write(f"        filterByRange.{db_key}AC_{pop['key']} on\n")
                f.write(f"        filterLabel.{db_key}AC_{pop['key']} "
                        f"{db_info['name']} {pop['name']} AC\n")

    print(f"Wrote trackDb fragment: {output_path}", file=sys.stderr)


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert annotated VCF to bigBed (two-phase parallel)")
    parser.add_argument("--annotated-vcf", default="merged.annotated.vcf.gz")
    parser.add_argument("--output-prefix", default="varFreqs")
    parser.add_argument("--chrom-sizes",
                        default="/hive/data/genomes/hg38/chrom.sizes")
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--work-dir", default=".")
    parser.add_argument("--region", default=None,
                        help="Region to process, e.g. chrM or chr22")
    parser.add_argument("--scripts-dir", default=SCRIPTS_DIR)
    parser.add_argument("--keep-temp", action="store_true")
    args = parser.parse_args()

    databases = load_config(args.scripts_dir)
    print(f"Loaded {len(databases)} databases:", file=sys.stderr)
    for key, info in databases.items():
        pops = ", ".join(p["key"] for p in info["pops"])
        extra = f" [{pops}]" if pops else ""
        exists = "OK" if os.path.exists(info["vcf"]) else "MISSING"
        print(f"  {key}: {info['name']}{extra} ({exists})", file=sys.stderr)

    if args.region:
        chromosomes = [args.region.split(":")[0]]
    else:
        chromosomes = ALL_CHROMOSOMES

    as_path = os.path.join(args.work_dir, f"{args.output_prefix}.as")
    bb_path = os.path.join(args.work_dir, f"{args.output_prefix}.bb")
    key_path = os.path.join(args.work_dir, f"{args.output_prefix}.keys.txt")
    extract_dir = os.path.join(args.work_dir, "extracted")
    bed_dir = os.path.join(args.work_dir, "beds")
    os.makedirs(extract_dir, exist_ok=True)
    os.makedirs(bed_dir, exist_ok=True)

    # Step 1: AutoSql and trackDb fragment
    generate_autosql(as_path, databases)
    ra_path = os.path.join(args.work_dir, f"{args.output_prefix}.trackDb.ra")
    generate_trackdb_fragment(ra_path, databases)

    # Step 2: Phase 1 — pre-extract
    pre_extract_all(databases, args.region, extract_dir, args.threads)

    # Step 3: Phase 2 — process chromosomes
    print(f"\n=== Phase 2: Processing {len(chromosomes)} chromosome(s) ===",
          file=sys.stderr)

    task_args = [(chrom, args.annotated_vcf, databases, extract_dir, bed_dir)
                 for chrom in chromosomes]

    with Pool(min(args.threads, len(chromosomes))) as pool:
        chrom_beds = pool.map(process_chromosome, task_args)
    chrom_beds = [b for b in chrom_beds if b is not None]

    if not chrom_beds:
        print("ERROR: No BED output produced", file=sys.stderr)
        sys.exit(1)

    # Step 4: Concatenate and sort
    print(f"\n=== Concatenating {len(chrom_beds)} chromosome files ===",
          file=sys.stderr)
    bed_path = os.path.join(args.work_dir, f"{args.output_prefix}.bed")
    with open(bed_path, "w") as out:
        for chrom_bed in chrom_beds:
            result = subprocess.run(
                ["sort", "-k2,2n", chrom_bed],
                capture_output=True, text=True)
            out.write(result.stdout)

    # Step 5: bedToBigBed
    print(f"\n=== Running bedToBigBed ===", file=sys.stderr)
    cmd = ["bedToBigBed", "-type=bed9+", f"-as={as_path}", "-tab",
           bed_path, args.chrom_sizes, bb_path]
    print(f"  {' '.join(cmd)}", file=sys.stderr)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  Error: {result.stderr}", file=sys.stderr)
        sys.exit(1)

    # Step 6: Key mapping
    with open(key_path, "w") as f:
        for key, info in databases.items():
            f.write(f"{key}|{info['name']}\n")

    # Cleanup
    if not args.keep_temp:
        for chrom_bed in chrom_beds:
            if os.path.exists(chrom_bed):
                os.remove(chrom_bed)
        if os.path.exists(bed_path):
            os.remove(bed_path)
        shutil.rmtree(extract_dir, ignore_errors=True)
        shutil.rmtree(bed_dir, ignore_errors=True)

    bb_size = os.path.getsize(bb_path) / (1024**3)
    print(f"\nDone! {bb_path} ({bb_size:.1f} GB)", file=sys.stderr)


if __name__ == "__main__":
    main()
