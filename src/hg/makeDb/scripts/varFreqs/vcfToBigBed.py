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

def load_config(scripts_dir, db_file="databases.tsv",
                pop_file="populations.tsv"):
    """Load the database and population config TSVs.

    db_file/pop_file may be bare names (resolved against scripts_dir) or
    absolute paths, so disease/array variants can use their own configs."""
    databases = OrderedDict()

    db_path = db_file if os.path.isabs(db_file) \
        else os.path.join(scripts_dir, db_file)
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
            is_disease = int(parts[5]) if len(parts) > 5 else 0
            disease_role = parts[6].strip() if len(parts) > 6 else ""
            default_an = 0
            if len(parts) > 7 and parts[7].strip():
                try:
                    default_an = int(parts[7].strip())
                except ValueError:
                    print(f"WARNING: bad default_an for {key}: {parts[7]}",
                          file=sys.stderr)
            skip_top_ranking = 0
            if len(parts) > 8 and parts[8].strip():
                try:
                    skip_top_ranking = int(parts[8].strip())
                except ValueError:
                    print(f"WARNING: bad skip_top_ranking for {key}: {parts[8]}",
                          file=sys.stderr)
            databases[key] = {
                "name": name, "vcf": vcf,
                "ac_field": ac_field, "af_field": af_field,
                "is_disease": is_disease,
                "disease_role": disease_role,
                "default_an": default_an,
                "skip_top_ranking": skip_top_ranking,
                "pops": [],
            }

    pop_path = pop_file if os.path.isabs(pop_file) \
        else os.path.join(scripts_dir, pop_file)
    with open(pop_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) < 5:
                continue
            db_key = parts[0]
            phenotype = parts[5].strip() if len(parts) > 5 else ""
            if db_key in databases:
                databases[db_key]["pops"].append({
                    "key": parts[1], "name": parts[2],
                    "ac_field": parts[3], "af_field": parts[4],
                    "phenotype": phenotype,
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


# Short display label per disease-cohort arm key, used in the topAffectedSources
# and topBackgroundSources mouseOver fields so a reader of e.g.
# "SPARK ASD (0.144)" knows which arm of SPARK contributed that AF. Bare
# population-cohort entries use just the db_key.
ARM_DISPLAY = {
    "AUT": "ASD",
    "NON_AUT": "non-ASD",
    "CASE": "case",
    "CTRL": "ctrl",
    "AFF": "affected",
    "UNA": "unaffected",
    "UNK": "unknown",
}


def source_label(db_key, pop_key=None):
    """Display label for a (cohort, arm) contribution to top-N source AFs."""
    if pop_key is None:
        return db_key
    return f"{db_key} {ARM_DISPLAY.get(pop_key, pop_key)}"


def top_n_source_afs(arm_afs, n=3):
    """Format the top-N (source, AF) entries by AF value.

    arm_afs is a list of (source_label, af_value) tuples. Sources appearing
    more than once (shouldn't, but defensive) are deduped to the max AF.
    Ties are broken alphabetically by source label. Returns "" when no
    entry has AF > 0."""
    if not arm_afs:
        return ""
    by_source = {}
    for src, af in arm_afs:
        if af is None or af <= 0:
            continue
        if src not in by_source or af > by_source[src]:
            by_source[src] = af
    if not by_source:
        return ""
    top = sorted(by_source.items(), key=lambda x: (-x[1], x[0]))[:n]
    return ", ".join(f"{src} ({af:.6f})" for src, af in top)


def _pool_arm(ac_val, af_val, default_an):
    """Compute pooled (AC, AN) contribution for one cohort arm.

    Used by the affected and background pooled-AF calculations. Returns
    (0, 0) when we can't determine AN, so the pool denominator never
    includes a cohort's carriers without also including its allele number
    -- the resulting pooled AF stays well-defined and bounded.

    Strategies, in order:
      1. Both AC and AF present with AF > 0: AN = round(AC / AF) (typical case).
      2. AF present but AC empty: synthesize AC = round(AF * default_an)
         and use default_an as AN (e.g. ALFA, ABraOM, which ship only AF).
      3. AC present but AF empty/0: use default_an as AN (e.g. MGRB if it
         had a configured default_an).
      4. None of the above: return (0, 0), arm does not contribute.
    """
    if ac_val is not None and af_val is not None and af_val > 0:
        return ac_val, max(1, round(ac_val / af_val))
    if af_val is not None and default_an > 0:
        return max(0, round(af_val * default_an)), default_an
    if ac_val is not None and default_an > 0:
        return ac_val, default_an
    return 0, 0


def process_chromosome(args):
    """Phase 2: Build the affected and background BEDs for one chromosome from
    the annotated VCF + pre-extracted per-cohort data.

    Two output rows are possible per variant, sharing one schema:
      - affected   BED: variant seen in any affected/case arm of a disease cohort
      - background BED: variant seen in a population cohort or an unaffected/
                        control/unknown arm ("all other variants")
    A variant present in both groups is written to both (overlap is intended, so
    the case-vs-background comparison works).

    With split=False a single BED is written instead (one row per variant,
    score = max of the two summaries); used for tracks with no disease cohorts
    such as the genotyping-array combined track."""
    chrom, annotated_vcf, databases, extract_dir, output_dir, split = args

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

    affected_path = os.path.join(output_dir, f"{chrom}.affected.bed")
    background_path = os.path.join(output_dir, f"{chrom}.background.bed")
    single_path = os.path.join(output_dir, f"{chrom}.bed")
    n_aff = 0
    n_bg = 0
    n_single = 0
    # Length extremes, used to emit data-driven length filter ranges.
    stats = {"max_ref_len": 1, "max_alt_len": 1,
             "min_var_len": 0, "max_var_len": 0}

    out_aff = open(affected_path, "w") if split else None
    out_bg = open(background_path, "w") if split else None
    out_single = None if split else open(single_path, "w")
    try:
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

            # Pooled affected/case summary: sum AC, sum AN, AF = AC/AN.
            # Switched from max-across-cohorts (which was dominated by tiny
            # cohorts like GA4K when they reported high local AF) to a
            # population-weighted ratio so the AF matches the AC scale.
            affected_ac = 0
            affected_an = 0
            affected_cohorts = []
            # Background summary = population cohorts + unaffected/control/unknown
            # arms of disease cohorts ("all other variants"), same pooling.
            background_ac = 0
            background_an = 0
            background_sources = []
            # Per-arm (label, AF) contributions used to compute the top-N
            # source AFs in the mouseOver. Disease-cohort arms get the arm
            # label appended ("SPARK ASD"); population cohorts are bare.
            # Per-population sub-ancestries of population cohorts are
            # excluded so a high sub-pop AF in a reference panel does not
            # crowd out actual project signals.
            affected_arm_afs = []
            background_arm_afs = []
            db_ac_af = []    # per-database AC, AF (raw, for output columns)
            pop_ac_af = []   # per-population AC, AF (raw, for output columns)

            for db_key, db_info in databases.items():
                values = freq_data.get(db_key, {}).get(key, [])

                ac = values[0] if len(values) > 0 and values[0] not in \
                     (".", "") else ""
                af = values[1] if len(values) > 1 and values[1] not in \
                     (".", "") else ""

                is_disease_db = db_info.get("is_disease", 0)
                disease_role = db_info.get("disease_role", "")
                default_an = db_info.get("default_an", 0)
                # When set, the cohort's per-source AF is unreliable for the
                # Top-3 mouseOver ranking (e.g. SGDP and SVatalog encode AC/AN
                # per genotyped individual, so AF defaults near 0.5). The
                # cohort still feeds the pooled AC/AN/AF and appears in the
                # Sources list, just not in the Top-3 ranking.
                skip_top = db_info.get("skip_top_ranking", 0)

                af_val = None
                if af:
                    try:
                        af_val = float(af)
                    except ValueError:
                        af_val = None
                ac_val = None
                if ac:
                    try:
                        ac_val = int(ac)
                    except ValueError:
                        ac_val = None

                ac_add, an_add = _pool_arm(ac_val, af_val, default_an)
                cohort_observes = (ac_val is not None) or (af_val is not None)

                # Track this cohort's appearance in each group's source list.
                # A cohort that observes the variant but lacks a usable AN
                # (e.g. MGRB ships AC only, GREGoR per-arm ships AC only) is
                # still listed but contributes 0 to the pool. Future work:
                # add default_an entries for these cohorts/arms.
                hits_affected = False
                hits_background = False

                if is_disease_db:
                    if disease_role == "affected":
                        affected_ac += ac_add
                        affected_an += an_add
                        if cohort_observes:
                            hits_affected = True
                        if af_val is not None and af_val > 0 and not skip_top:
                            affected_arm_afs.append(
                                (source_label(db_key), af_val))
                    elif disease_role == "unaffected":
                        background_ac += ac_add
                        background_an += an_add
                        if cohort_observes:
                            hits_background = True
                        if af_val is not None and af_val > 0 and not skip_top:
                            background_arm_afs.append(
                                (source_label(db_key), af_val))
                else:
                    background_ac += ac_add
                    background_an += an_add
                    if cohort_observes:
                        hits_background = True
                    if af_val is not None and af_val > 0 and not skip_top:
                        background_arm_afs.append(
                            (source_label(db_key), af_val))

                db_ac_af.extend([ac, af])

                for i, pop in enumerate(db_info["pops"]):
                    idx = 2 + i * 2
                    pop_ac = values[idx] if len(values) > idx and \
                        values[idx] not in (".", "") else ""
                    pop_af = values[idx + 1] if len(values) > idx + 1 and \
                        values[idx + 1] not in (".", "") else ""
                    pop_ac_af.extend([pop_ac, pop_af])

                    pop_af_val = None
                    if pop_af:
                        try:
                            pop_af_val = float(pop_af)
                        except ValueError:
                            pop_af_val = None
                    pop_ac_val = None
                    if pop_ac:
                        try:
                            pop_ac_val = int(pop_ac)
                        except ValueError:
                            pop_ac_val = None
                    # Per-arm default_an would let GREGoR per-arm rows pool
                    # cleanly; for now they fall through with default 0.
                    pop_default_an = pop.get("default_an", 0)
                    pop_ac_add, pop_an_add = _pool_arm(
                        pop_ac_val, pop_af_val, pop_default_an)
                    pop_observes = (pop_ac_val is not None) or \
                                   (pop_af_val is not None)

                    pheno = pop.get("phenotype", "")
                    if is_disease_db and pheno == "affected":
                        affected_ac += pop_ac_add
                        affected_an += pop_an_add
                        if pop_observes:
                            hits_affected = True
                        if (pop_af_val is not None and pop_af_val > 0
                                and not skip_top):
                            affected_arm_afs.append(
                                (source_label(db_key, pop["key"]), pop_af_val))
                    elif is_disease_db and pheno in ("unaffected", "unknown"):
                        # Unaffected relatives, controls, and unknown-phenotype
                        # individuals all feed the background.
                        background_ac += pop_ac_add
                        background_an += pop_an_add
                        if pop_observes:
                            hits_background = True
                        if (pop_af_val is not None and pop_af_val > 0
                                and not skip_top):
                            background_arm_afs.append(
                                (source_label(db_key, pop["key"]), pop_af_val))
                    elif not is_disease_db:
                        # Ancestry breakdown of a population cohort. The
                        # unified row above already pooled the cohort if it
                        # had AC+AF, so we deliberately don't double-count
                        # the per-pop AC/AN here. Per-pop AC and AF still
                        # write to their own bigBed columns above.
                        # We also deliberately exclude these from the
                        # top-N source AFs: sub-populations like
                        # gnomAD-Finnish are not "sources", so they would
                        # crowd out actual project signals.
                        if pop_observes:
                            hits_background = True

                if hits_affected:
                    affected_cohorts.append(db_key)
                if hits_background:
                    background_sources.append(db_key)

            # Compute pooled allele frequencies.
            affected_af = (affected_ac / affected_an) if affected_an > 0 else 0.0
            background_af = (background_ac / background_an) \
                if background_an > 0 else 0.0

            # Top 3 source AFs for the mouseOver. Strings; empty if no source
            # in the relevant group had AF > 0.
            top_affected_sources = top_n_source_afs(affected_arm_afs, 3)
            top_background_sources = top_n_source_afs(background_arm_afs, 3)

            in_affected = 1 if (affected_ac > 0 or affected_af > 0) else 0

            # Track length extremes for data-driven length filter ranges.
            ref_len = len(ref)
            alt_len = len(alt)
            var_len = alt_len - ref_len
            if ref_len > stats["max_ref_len"]:
                stats["max_ref_len"] = ref_len
            if alt_len > stats["max_alt_len"]:
                stats["max_alt_len"] = alt_len
            if var_len < stats["min_var_len"]:
                stats["min_var_len"] = var_len
            if var_len > stats["max_var_len"]:
                stats["max_var_len"] = var_len

            # Shared columns (score at index 4 is filled in per output below).
            base = [
                chrom_name, str(start), str(end), name, "0", "+",
                str(start), str(end), f"{r},{g},{b}",
                ref, alt, str(ref_len), str(alt_len),
                str(var_len), var_type,
                normalize_consequence(consequence),
                gene, transcript, aa_change, dna_change,
                f"{affected_af:.6f}" if affected_af > 0 else "",
                str(affected_ac) if affected_ac > 0 else "",
                str(affected_an) if affected_an > 0 else "",
                ",".join(affected_cohorts),
                top_affected_sources,
                f"{background_af:.6f}" if background_af > 0 else "",
                str(background_ac) if background_ac > 0 else "",
                str(background_an) if background_an > 0 else "",
                ",".join(background_sources),
                top_background_sources,
                str(in_affected),
            ]
            # Database AC/AF first, then population AC/AF — must match autoSql order
            base.extend(db_ac_af)
            base.extend(pop_ac_af)

            has_affected = affected_af > 0 or affected_ac > 0
            has_background = background_af > 0 or background_ac > 0

            if split:
                if has_affected:
                    row = list(base)
                    row[4] = str(min(1000, int(affected_af * 1000)))
                    out_aff.write("\t".join(row) + "\n")
                    n_aff += 1
                if has_background:
                    row = list(base)
                    row[4] = str(min(1000, int(background_af * 1000)))
                    out_bg.write("\t".join(row) + "\n")
                    n_bg += 1
            else:
                if has_affected or has_background:
                    row = list(base)
                    row[4] = str(min(1000, int(
                        max(affected_af, background_af) * 1000)))
                    out_single.write("\t".join(row) + "\n")
                    n_single += 1
    finally:
        for fh in (out_aff, out_bg, out_single):
            if fh is not None:
                fh.close()

    if split:
        print(f"  {chrom}: wrote {n_aff:,} affected + {n_bg:,} background "
              f"BED lines", file=sys.stderr)
        return (affected_path, background_path, stats)
    print(f"  {chrom}: wrote {n_single:,} BED lines", file=sys.stderr)
    return (single_path, None, stats)


# ============================================================================
# AutoSql Generation
# ============================================================================

def generate_autosql(output_path, databases, table_name="varFreqsAll"):
    """Generate autoSql file from database config."""
    with open(output_path, "w") as f:
        f.write(f'table {table_name}\n')
        f.write('"Variant frequencies across population databases"\n')
        f.write('(\n')
        # BED9
        f.write('    string chrom;        "Chromosome"\n')
        f.write('    uint chromStart;     "Start position (0-based)"\n')
        f.write('    uint chromEnd;       "End position"\n')
        f.write('    string name;         "Variant (REF>ALT)"\n')
        f.write('    uint score;          "Score (allele frequency * 1000)"\n')
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
        # Frequency summaries (shared by the affected and background tracks).
        # AF is pooled across contributing arms (sum AC / sum AN), not the
        # max across arms, so the AF matches the AC and AN scale.
        f.write('    string affectedAF;      "Pooled allele frequency in affected/case individuals (sum AC / sum AN)"\n')
        f.write('    string affectedAC;      "Summed allele count in affected/case individuals"\n')
        f.write('    string affectedAN;      "Summed allele number in affected/case individuals (pool denominator)"\n')
        f.write('    string affectedCohorts; "Disease cohorts contributing affected/case carriers"\n')
        f.write('    string topAffectedSources;   "Top 3 affected/case cohorts by per-source AF"\n')
        f.write('    string backgroundAF;    "Pooled allele frequency in population cohorts + unaffected/control individuals (sum AC / sum AN)"\n')
        f.write('    string backgroundAC;    "Summed allele count in population cohorts + unaffected/control individuals"\n')
        f.write('    string backgroundAN;    "Summed allele number in population cohorts + unaffected/control individuals (pool denominator)"\n')
        f.write('    string backgroundSources; "Cohorts contributing to the background (population + unaffected)"\n')
        f.write('    string topBackgroundSources; "Top 3 background cohorts by per-source AF"\n')
        f.write('    uint inAffected;        "1 if seen in an affected/case arm, else 0"\n')
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


def generate_trackdb_fragment(output_path, databases, track_name="varFreqsAll",
                              len_stats=None):
    """Generate trackDb .ra fragment for the combined track filters."""
    if len_stats is None:
        len_stats = {"max_ref_len": 1000, "max_alt_len": 1000,
                     "min_var_len": -1000, "max_var_len": 1000}
    with open(output_path, "w") as f:
        f.write(f"# Auto-generated trackDb fragment for {track_name} filters\n")
        f.write(f"# Paste this into the {track_name} track stanza in varFreqs.ra\n\n")

        # Cohort filters. affectedCohorts = disease cohorts that can contribute
        # an affected/case carrier; backgroundSources = population cohorts plus
        # disease cohorts with an unaffected/control/unknown arm.
        affected_parts = []
        background_parts = []
        for db_key, db_info in databases.items():
            entry = f"{db_key}|{db_info['name']}"
            is_dis = db_info.get("is_disease", 0)
            phenos = {p.get("phenotype", "") for p in db_info["pops"]}
            if is_dis and (db_info.get("disease_role", "") == "affected"
                           or "affected" in phenos):
                affected_parts.append(entry)
            if (not is_dis) or (phenos & {"unaffected", "unknown"}):
                background_parts.append(entry)
        if affected_parts:
            f.write("        filterValues.affectedCohorts "
                    + ",".join(affected_parts) + "\n")
            f.write("        filterType.affectedCohorts multipleListOr\n")
            f.write("        filterLabel.affectedCohorts Affected/case cohort\n")
        if background_parts:
            f.write("        filterValues.backgroundSources "
                    + ",".join(background_parts) + "\n")
            f.write("        filterType.backgroundSources multipleListOr\n")
            f.write("        filterLabel.backgroundSources Background source (population or unaffected)\n")

        # Variant type and consequence (static)
        f.write("        # Variant type and consequence filters\n")
        f.write("        filterValues.varType "
                "SNV|SNV,INS|Insertion,DEL|Deletion,MNV|MNV\n")
        f.write("        filterLabel.varType Variant Type\n")
        f.write("        filterValues.consequence "
                "missense|Missense,synonymous|Synonymous,"
                "stop_gained|Stop Gained,frameshift|Frameshift,"
                "splice_donor|Splice Donor,splice_acceptor|Splice Acceptor,"
                "intron|Intron,3_prime_utr|3' UTR,5_prime_utr|5' UTR,"
                "non_coding|Non-coding,.|Intergenic,others|Other\n")
        f.write("        filterType.consequence multipleListOr\n")
        f.write("        filterLabel.consequence Consequence\n")

        # Length filters. A numeric range filter needs filter.<fld> min:max (the
        # default/limits range) or the slider does not render; ranges are derived
        # from the observed data so nothing is hidden by default.
        f.write("        # Length filters\n")
        len_ranges = {
            "refLen": ("Reference Length", 1, len_stats["max_ref_len"]),
            "altLen": ("Alternate Length", 1, len_stats["max_alt_len"]),
            "varLen": ("Length Change", len_stats["min_var_len"],
                       len_stats["max_var_len"]),
        }
        for fld, (label, lo, hi) in len_ranges.items():
            f.write(f"        filterByRange.{fld} on\n")
            f.write(f"        filterLabel.{fld} {label}\n")
            f.write(f"        filter.{fld} {lo}:{hi}\n")
            f.write(f"        filterLimits.{fld} {lo}:{hi}\n")

        # Affected and background frequency summaries (both tracks carry both,
        # so e.g. the Affected track can be filtered to variants rare in the
        # background).
        #
        # IMPORTANT: hgTrackUi only DISCOVERS a numeric filter from a
        # "filter.<field>" (or "<field>Filter") setting -- see
        # FILTER_NUMBER_WILDCARD in hg/inc/bigBedFilter.h and
        # tdbGetTrackNumFilters() in hg/lib/hui.c. A field that has only
        # filterByRange./filterLimits. is silently dropped. So every range
        # filter needs filter.<field> (the default selected range) to render;
        # filterLimits.<field> only sets the slider bounds shown in the UI.
        # We set filter.<field> to the FULL range so nothing is filtered by
        # default. AC/AN are unbounded counts; the caps below are generous
        # upper bounds (well above the true per-pool maxima) and never hide data.
        AC_CAP_AFFECTED = 500000      # ~130k affected individuals -> AN up to ~260k
        AC_CAP_BACKGROUND = 5000000   # ~1.5M individuals pooled -> AN up to ~3M
        AC_CAP_DB = 2000000           # largest single cohort (FinnGen) AN ~1M

        def range_filter(field, label, lo, hi, enabled=True):
            # enabled=False emits the same four settings commented out, so the
            # filter is documented and trivially re-enabled by removing the "# ".
            pre = "        " if enabled else "        # "
            f.write(f"{pre}filterByRange.{field} on\n")
            f.write(f"{pre}filterLabel.{field} {label}\n")
            f.write(f"{pre}filter.{field} {lo}:{hi}\n")
            f.write(f"{pre}filterLimits.{field} {lo}:{hi}\n")

        f.write("        # Affected/case frequency summary\n")
        range_filter("affectedAF", "Affected/case AF (pooled)", 0, 1)
        range_filter("affectedAC", "Affected/case AC", 0, AC_CAP_AFFECTED)
        range_filter("affectedAN", "Affected/case AN (pool denominator)",
                     0, AC_CAP_AFFECTED)
        f.write("        # Background (population + unaffected) frequency summary\n")
        range_filter("backgroundAF", "Background AF (pooled)", 0, 1)
        range_filter("backgroundAC", "Background AC (population + unaffected)",
                     0, AC_CAP_BACKGROUND)
        range_filter("backgroundAN", "Background AN (pool denominator)",
                     0, AC_CAP_BACKGROUND)
        f.write("        # Affected/case membership flag\n")
        range_filter("inAffected", "Seen in an affected/case arm (1=yes, 0=no)",
                     0, 1)

        # Per-database and per-population AF/AC filters. There are ~130 of these
        # and they overwhelm the track config page, so they are emitted COMMENTED
        # OUT (enabled=False). The data columns are still in the bigBed and show
        # on the details page; only the filter controls are off. Re-enable any of
        # them by deleting the leading "# " on its four lines.
        f.write("        # Per-database AF filters (commented out: re-enable as needed)\n")
        for db_key, db_info in databases.items():
            range_filter(f"{db_key}AF", f"{db_info['name']} AF", 0, 1,
                         enabled=False)

        f.write("        # Per-database AC filters (commented out: re-enable as needed)\n")
        for db_key, db_info in databases.items():
            range_filter(f"{db_key}AC", f"{db_info['name']} AC", 0, AC_CAP_DB,
                         enabled=False)

        # Population-specific filters (commented out: re-enable as needed)
        f.write("        # Population-specific AF/AC filters (commented out: re-enable as needed)\n")
        for db_key, db_info in databases.items():
            if not db_info["pops"]:
                continue
            f.write(f"        # {db_info['name']} populations\n")
            for pop in db_info["pops"]:
                range_filter(f"{db_key}AF_{pop['key']}",
                             f"{db_info['name']} {pop['name']} AF", 0, 1,
                             enabled=False)
            for pop in db_info["pops"]:
                range_filter(f"{db_key}AC_{pop['key']}",
                             f"{db_info['name']} {pop['name']} AC", 0, AC_CAP_DB,
                             enabled=False)

        f.write("        skipEmptyFields on\n")

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
    parser.add_argument("--databases-file", default="databases.tsv",
                        help="DB config TSV (name under scripts-dir or abs path)")
    parser.add_argument("--populations-file", default="populations.tsv",
                        help="Population config TSV (name under scripts-dir or abs path)")
    parser.add_argument("--keep-temp", action="store_true")
    parser.add_argument("--split-affected", action="store_true",
                        help="Emit two tracks: <prefix>Affected and "
                             "<prefix>Background (affected/case vs population+"
                             "unaffected). Without it, a single <prefix> track.")
    args = parser.parse_args()

    databases = load_config(args.scripts_dir, args.databases_file,
                            args.populations_file)
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

    extract_dir = os.path.join(args.work_dir, "extracted")
    bed_dir = os.path.join(args.work_dir, "beds")
    os.makedirs(extract_dir, exist_ok=True)
    os.makedirs(bed_dir, exist_ok=True)

    # Output prefixes: one per track. Split mode yields an Affected and a
    # Background track that share a single autoSql schema.
    if args.split_affected:
        prefixes = [f"{args.output_prefix}Affected",
                    f"{args.output_prefix}Background"]
    else:
        prefixes = [args.output_prefix]

    # Step 1: AutoSql per track (same columns; table name = output prefix).
    for prefix in prefixes:
        generate_autosql(os.path.join(args.work_dir, f"{prefix}.as"),
                         databases, table_name=prefix)

    # Step 2: Phase 1 — pre-extract
    pre_extract_all(databases, args.region, extract_dir, args.threads)

    # Step 3: Phase 2 — process chromosomes
    print(f"\n=== Phase 2: Processing {len(chromosomes)} chromosome(s) ===",
          file=sys.stderr)

    task_args = [(chrom, args.annotated_vcf, databases, extract_dir, bed_dir,
                  args.split_affected) for chrom in chromosomes]

    with Pool(min(args.threads, len(chromosomes))) as pool:
        chrom_results = pool.map(process_chromosome, task_args)
    chrom_results = [r for r in chrom_results if r is not None]

    if not chrom_results:
        print("ERROR: No BED output produced", file=sys.stderr)
        sys.exit(1)

    # Aggregate length extremes across chromosomes for the length filter ranges,
    # then write the trackDb fragment (after processing, so ranges are real).
    len_stats = {"max_ref_len": 1, "max_alt_len": 1,
                 "min_var_len": 0, "max_var_len": 0}
    for r in chrom_results:
        st = r[2]
        len_stats["max_ref_len"] = max(len_stats["max_ref_len"], st["max_ref_len"])
        len_stats["max_alt_len"] = max(len_stats["max_alt_len"], st["max_alt_len"])
        len_stats["min_var_len"] = min(len_stats["min_var_len"], st["min_var_len"])
        len_stats["max_var_len"] = max(len_stats["max_var_len"], st["max_var_len"])
    ra_path = os.path.join(args.work_dir, f"{args.output_prefix}.trackDb.ra")
    generate_trackdb_fragment(ra_path, databases, track_name=args.output_prefix,
                              len_stats=len_stats)

    # Steps 4-5: build one bigBed per track from its per-chromosome BEDs.
    # results column 0 = affected/single BED, column 1 = background BED (or None)
    bed_sets = [[r[0] for r in chrom_results]]
    if args.split_affected:
        bed_sets.append([r[1] for r in chrom_results])

    for prefix, chrom_beds in zip(prefixes, bed_sets):
        as_path = os.path.join(args.work_dir, f"{prefix}.as")
        bb_path = os.path.join(args.work_dir, f"{prefix}.bb")
        bed_path = os.path.join(args.work_dir, f"{prefix}.bed")
        print(f"\n=== {prefix}: concatenating {len(chrom_beds)} chrom files ===",
              file=sys.stderr)
        with open(bed_path, "wb") as out:
            for chrom_bed in chrom_beds:
                subprocess.run(["sort", "-k2,2n", chrom_bed],
                               stdout=out, check=True)
        print(f"=== {prefix}: running bedToBigBed ===", file=sys.stderr)
        cmd = ["bedToBigBed", "-type=bed9+", f"-as={as_path}", "-tab",
               bed_path, args.chrom_sizes, bb_path]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  Error: {result.stderr}", file=sys.stderr)
            sys.exit(1)
        if not args.keep_temp and os.path.exists(bed_path):
            os.remove(bed_path)
        bb_size = os.path.getsize(bb_path) / (1024**3)
        print(f"Done! {bb_path} ({bb_size:.1f} GB)", file=sys.stderr)

    # Cleanup
    if not args.keep_temp:
        shutil.rmtree(extract_dir, ignore_errors=True)
        shutil.rmtree(bed_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
