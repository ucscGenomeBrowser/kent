#!/usr/bin/env python3
"""
addPeptideEvidence.py - Extend the GENCODE Phase I ncORF bigGenePred with peptide evidence.

Reads all 7264 rows from Ribo-seq_ORFs.kozak.bb (30 fields, field 9 already carries
Kozak colors), joins five supplemental tables from Deutsch et al. 2026 (PMID:42092140),
and appends 14 extra fields. ORFs with no peptide evidence get "." / 0 defaults and
gray color fields.

Sources in the peptideSupport/ subdirectory of the data directory:
  Table S2  (MOESM4):  Non-HLA tryptic peptide sequences, one row per peptide
  Table S3  (MOESM5):  Non-HLA ORF-level summary (tiers, HPP category)
  Table S6  (MOESM8):  HLA immunopeptidomics peptide sequences, one row per peptide
  Table S7  (MOESM9):  HLA ORF-level summary (tiers, HLA class, HPP category)
  Table S12 (MOESM14): Annotation decisions for 137 ORFs, 121 of them "peptidein"

Non-HLA peptides that the paper excluded (Maps2Core, TooShort, ...) are dropped, and
nonHlaNPeptides counts only the kept peptides, so the count, the sequence list and
the evidence color always agree.

Join key: ORF name in column 4 (e.g. c14riboseqorf80).

Output to stdout: 44-field BED (30 existing + 14 new), all 7264 ORFs.
Diagnostics to stderr: join stats per table.

Usage, from the gencNcOrf data directory:
    addPeptideEvidence.py 2>peptideSupport/addPeptides.log > Ribo-seq_ORFs.kozak.peptides.bed
"""

import argparse
import csv
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

S2_NAME  = "41586_2026_10459_MOESM4_ESM_Table_S2.tsv"
S3_NAME  = "41586_2026_10459_MOESM5_ESM_Table_S3.tsv"
S6_NAME  = "41586_2026_10459_MOESM8_ESM_Table_S6.tsv"
S7_NAME  = "41586_2026_10459_MOESM9_ESM_Table_S7.tsv"
S12_NAME = "41586_2026_10459_MOESM14_ESM_Table_S12.tsv"

# Evidence-type color scheme (colorByEvidence extra field).
COLOR_PEPTIDEIN = "230,168,23"   # gold:         peptidein (Table S12)
COLOR_BOTH      = "230,100,0"    # orange:       HLA + non-HLA, not peptidein
COLOR_HLA       = "70,130,180"   # steel blue:   HLA only, not peptidein
COLOR_NONHLA    = "76,153,0"     # forest green: non-HLA only, not peptidein
COLOR_NONE      = "128,128,128"  # gray:         no peptide evidence

# HLA-class color scheme (colorByHlaClass extra field).
HLA_CLASS_COLOR = {
    "I":    "70,130,180",
    "II":   "200,60,60",
    "Both": "230,100,0",    # orange: both class I and II
}
HLA_CLASS_NONE = "128,128,128"

# Defaults written for ORFs that have no peptide evidence.
EMPTY_PEPTIDE_FIELDS = [
    ".",   # hlaClass
    ".",   # hlaFinalTier
    ".",   # hlaHppCategory
    "0",   # hlaNPeptides
    ".",   # riboseqQuality
    ".",   # hlaIPeptides
    ".",   # hlaIIPeptides
    ".",   # nonHlaFinalTier
    ".",   # nonHlaHppCategory
    "0",   # nonHlaNPeptides
    ".",   # nonHlaPeptides
    "no",  # isPeptidein
    COLOR_NONE,  # colorByEvidence
    HLA_CLASS_NONE,  # colorByHlaClass
]


def err(msg):
    print(msg, file=sys.stderr)


def report_join(label, orf_ids, known_ids, sample_n=5):
    ids = list(orf_ids)
    found   = [x for x in ids if x in known_ids]
    missing = [x for x in ids if x not in known_ids]
    err(f"  {label}: {len(ids)} unique ORF IDs -> "
        f"{len(found)} found in kozak bigBed, {len(missing)} NOT found")
    if missing:
        sample = missing[:sample_n]
        extra = f" ... (+{len(missing)-sample_n} more)" if len(missing) > sample_n else ""
        err(f"    Sample not found: {', '.join(sample)}{extra}")


def load_kozak_bb(filepath):
    """Read all rows from the Phase I kozak bigBed via bigBedToBed.
    Returns ordered list of rows (each a list of strings, 30 fields).
    """
    cmd = ["bigBedToBed", str(filepath), "stdout"]
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    rows = []
    for line in result.stdout.splitlines():
        if line.strip():
            rows.append(line.split("\t"))
    return rows


def load_s2(filepath):
    """Non-HLA peptides, minus those the paper excluded. Returns orf -> list of sequences."""
    seqs = defaultdict(list)
    n_total = n_excluded = 0
    with open(filepath, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            n_total += 1
            if row.get("exclude", "").strip():
                n_excluded += 1
                continue
            orf_id = row["Ribo-Seq_ORF"].strip()
            seq = row["sequence"].strip()
            if seq and seq not in seqs[orf_id]:
                seqs[orf_id].append(seq)
    err(f"Table S2 (non-HLA peptides): {n_total} rows, "
        f"{n_excluded} excluded, {n_total-n_excluded} kept, "
        f"{len(seqs)} ORFs with sequences")
    return dict(seqs)


def load_s3(filepath):
    data = {}
    with open(filepath, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            orf_id = row["Ribo-Seq_ORF"].strip()
            data[orf_id] = {
                "tier":      row.get("Final.tier.for.level.of.evidence", ".").strip() or ".",
                "category":  row.get("HPP.Category", ".").strip() or ".",
            }
    err(f"Table S3 (non-HLA ORF-level): {len(data)} ORFs")
    return data


def load_s6(filepath):
    data = defaultdict(lambda: {"I": [], "II": []})
    n_total = 0
    with open(filepath, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            n_total += 1
            orf_id    = row["Ribo-Seq_ORF"].strip()
            seq       = row["sequence"].strip()
            hla_class = row.get("msrun_hla_classes", "").strip()
            if hla_class in ("I", "I & II") and seq not in data[orf_id]["I"]:
                data[orf_id]["I"].append(seq)
            if hla_class in ("II", "I & II") and seq not in data[orf_id]["II"]:
                data[orf_id]["II"].append(seq)
    err(f"Table S6 (HLA peptides): {n_total} rows, {len(data)} ORFs")
    return dict(data)


def load_s7(filepath):
    data = {}
    with open(filepath, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            orf_id = row["Ribo-Seq_ORF"].strip()
            try:
                n_pep = int(float(row.get("n_peptides_in_entry", "0")))
            except (ValueError, TypeError):
                n_pep = 0
            data[orf_id] = {
                "class_":         row.get("HLA_class", ".").strip() or ".",
                "tier":           row.get("Final.tier.for.level.of.evidence", ".").strip() or ".",
                "category":       row.get("Category", ".").strip() or ".",
                "n_peptides":     n_pep,
                "riboseq_quality": row.get(
                    "Manual.inspection.of.GWIPS-Viz.RiboSeq.Data", ".").strip() or ".",
            }
    err(f"Table S7 (HLA ORF-level): {len(data)} ORFs")
    return data


def load_s12(filepath):
    """Table S12: annotation decisions. Returns the set of ORFs marked 'peptidein'."""
    orfs = set()
    n_total = 0
    with open(filepath, newline="") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            n_total += 1
            if row["annotation decision"].strip().lower() == "peptidein":
                orfs.add(row["Ribo-Seq_ORF"].strip())
    err(f"Table S12 (annotation decisions): {n_total} rows, {len(orfs)} peptidein ORFs")
    return orfs


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    parser.add_argument("--dir", default=".",
                        help="gencNcOrf data directory with Ribo-seq_ORFs.kozak.bb and "
                             "peptideSupport/ (default: current directory)")
    args = parser.parse_args()
    dataDir = Path(args.dir)
    supDir = dataDir / "peptideSupport"

    err("=== Loading Phase I kozak bigBed ===")
    kozak_rows = load_kozak_bb(dataDir / "Ribo-seq_ORFs.kozak.bb")
    kozak_ids  = {r[3] for r in kozak_rows}
    err(f"Phase I kozak bigBed: {len(kozak_rows)} ORFs\n")

    err("=== Loading peptide evidence tables ===")
    s2_seqs   = load_s2(supDir / S2_NAME)
    s3_data   = load_s3(supDir / S3_NAME)
    s6_seqs   = load_s6(supDir / S6_NAME)
    s7_data   = load_s7(supDir / S7_NAME)
    peptideins = load_s12(supDir / S12_NAME)

    err("\n=== Join coverage (ORF IDs vs kozak bigBed) ===")
    report_join("S2", s2_seqs.keys(),  kozak_ids)
    report_join("S3", s3_data.keys(),  kozak_ids)
    report_join("S6", s6_seqs.keys(),  kozak_ids)
    report_join("S7", s7_data.keys(),  kozak_ids)
    report_join("S12 peptidein", peptideins, kozak_ids)

    all_evidence = set(s3_data) | set(s7_data) | peptideins
    err(f"\nORFs with any peptide table entry: {len(all_evidence)} / {len(kozak_rows)}")

    err("\n=== Writing output ===")
    color_counts = defaultdict(int)
    for row in kozak_rows:
        orf_id = row[3]

        if orf_id not in all_evidence:
            # No peptide data - append defaults, preserve existing 30 fields unchanged.
            print("\t".join(row + EMPTY_PEPTIDE_FIELDS))
            color_counts["none"] += 1
            continue

        # --- HLA (S7 + S6) ---
        s7 = s7_data.get(orf_id, {})
        s6 = s6_seqs.get(orf_id, {"I": [], "II": []})
        hla_class      = s7.get("class_", ".")
        hla_tier       = s7.get("tier", ".")
        hla_category   = s7.get("category", ".")
        hla_n          = s7.get("n_peptides", 0)
        riboseq_q      = s7.get("riboseq_quality", ".")
        hla_i_peps     = ",".join(s6.get("I", [])) or "."
        hla_ii_peps    = ",".join(s6.get("II", [])) or "."

        # --- Non-HLA (S3 + S2), count only the peptides kept after exclusion ---
        s3 = s3_data.get(orf_id, {})
        nonhla_tier    = s3.get("tier", ".")
        nonhla_cat     = s3.get("category", ".")
        nonhla_seqs    = s2_seqs.get(orf_id, [])
        nonhla_n       = len(nonhla_seqs)
        nonhla_peps    = ",".join(nonhla_seqs) or "."

        is_peptidein = "yes" if orf_id in peptideins else "no"

        # --- colorByEvidence ---
        has_hla    = hla_n > 0
        has_nonhla = nonhla_n > 0
        if is_peptidein == "yes":
            color_ev, key = COLOR_PEPTIDEIN, "peptidein"
        elif has_hla and has_nonhla:
            color_ev, key = COLOR_BOTH, "both"
        elif has_hla:
            color_ev, key = COLOR_HLA, "hlaOnly"
        elif has_nonhla:
            color_ev, key = COLOR_NONHLA, "nonHlaOnly"
        else:
            color_ev, key = COLOR_NONE, "none"
        color_counts[key] += 1

        # --- colorByHlaClass ---
        color_hla = HLA_CLASS_COLOR.get(hla_class, HLA_CLASS_NONE)

        extra = [
            hla_class,
            hla_tier,
            hla_category,
            str(hla_n),
            riboseq_q,
            hla_i_peps,
            hla_ii_peps,
            nonhla_tier,
            nonhla_cat,
            str(nonhla_n),
            nonhla_peps,
            is_peptidein,
            color_ev,
            color_hla,
        ]
        print("\t".join(row + extra))

    err("Evidence-type color counts:")
    for key in ("peptidein", "both", "hlaOnly", "nonHlaOnly", "none"):
        err(f"  {key}: {color_counts[key]}")
    err(f"Total rows written: {sum(color_counts.values())}")


if __name__ == "__main__":
    main()
