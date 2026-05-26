#!/usr/bin/env python3
"""
barChartReorder.py: Reorder the bars (expScores columns) in a bigBarChart .bb
file so they appear in a custom display order on the UCSC Genome Browser
chromosome track.

A .bb stores expScores values per row in a fixed column order set at build
time. The .categories file labels each column by index. To change the
chromosome-track bar order, both files have to be rebuilt with the columns
in the desired order.

This script reads:
  - input .bb            (values in original column order)
  - input .categories    (one row per category: label TAB color)
  - .facets file         (whose row order, after the header, is the DESIRED
                          display order; column 1 of each row is the label)
  - chrom.sizes          for the assembly
  - barChartBed.as       schema file for bedToBigBed

It writes:
  - output .bb           (expScores columns reordered to match the .facets)
  - output .categories   (rows reordered to match)

Example:
  ./barChartReorder.py \\
    --bb            in.bb \\
    --categories    in.categories \\
    --facets        in.facets \\
    --chromSizes    mm10.chrom.sizes \\
    --outBb         out.bb \\
    --outCategories out.categories
"""

import argparse
import os
import subprocess
import sys


def read_categories(path):
    """Return a list of (label, color) tuples in file order."""
    cats = []
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                sys.exit("unexpected .categories row: " + repr(line))
            cats.append((parts[0], parts[1]))
    return cats


def read_facets_order(path):
    """Return a list of labels (column 1) in row order, skipping the header."""
    order = []
    with open(path) as f:
        f.readline()  # header
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split("\t")
            order.append(parts[0])
    return order


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bb", required=True, help="input .bb file")
    ap.add_argument("--categories", required=True, help="input .categories file")
    ap.add_argument("--facets", required=True,
                    help=".facets file whose row order = desired display order")
    ap.add_argument("--chromSizes", required=True,
                    help="chrom.sizes file for the assembly")
    ap.add_argument("--outBb", required=True, help="output .bb file")
    ap.add_argument("--outCategories", required=True,
                    help="output .categories file")
    args = ap.parse_args()

    orig_cats = read_categories(args.categories)
    desired_labels = read_facets_order(args.facets)

    label_to_orig_idx = {}
    for i in range(len(orig_cats)):
        label = orig_cats[i][0]
        label_to_orig_idx[label] = i

    # Sanity check: facets and categories must contain the same labels
    facets_set = set(desired_labels)
    cats_set = set(label_to_orig_idx.keys())
    if facets_set != cats_set:
        only_in_facets = sorted(facets_set - cats_set)
        only_in_cats = sorted(cats_set - facets_set)
        sys.exit(
            "label mismatch between .facets and .categories.\n"
            "  only in .facets: " + str(only_in_facets) + "\n"
            "  only in .categories: " + str(only_in_cats))
    if len(desired_labels) != len(orig_cats):
        sys.exit("length mismatch: facets=" + str(len(desired_labels))
                 + " cats=" + str(len(orig_cats)))

    # Map new column position -> original column position
    new_to_orig = []
    for label in desired_labels:
        new_to_orig.append(label_to_orig_idx[label])

    # Dump input .bb to bed, reorder expScores per row, write a temp .bed
    tmp_bed = args.outBb + ".tmp.bed"
    n_fields_seen = None
    n_rows = 0
    try:
        proc = subprocess.Popen(
            ["bigBedToBed", args.bb, "stdout"],
            stdout=subprocess.PIPE, text=True)
        with open(tmp_bed, "w") as out_bed:
            for line in proc.stdout:
                line = line.rstrip("\n")
                if not line:
                    continue
                fields = line.split("\t")
                if n_fields_seen is None:
                    n_fields_seen = len(fields)
                if len(fields) < 9:
                    sys.exit("row has too few fields (" + str(len(fields))
                             + "): " + repr(line))
                # bed6 + name2 + expCount + expScores ... expScores is field 8
                exp_str = fields[8].rstrip(",")
                exp_scores = exp_str.split(",")
                if len(exp_scores) != len(orig_cats):
                    sys.exit("row expScores has " + str(len(exp_scores))
                             + " values, expected " + str(len(orig_cats)))
                reordered = []
                for i in range(len(desired_labels)):
                    reordered.append(exp_scores[new_to_orig[i]])
                fields[8] = ",".join(reordered) + ","
                out_bed.write("\t".join(fields) + "\n")
                n_rows += 1
        proc.wait()
        if proc.returncode != 0:
            sys.exit("bigBedToBed failed with return code "
                     + str(proc.returncode))
        sys.stderr.write("reordered " + str(n_rows)
                         + " rows; building output .bb...\n")

        # Output keeps the same field count as the input. Generate a matching
        # bigBarChart schema (bed6+3 if input had 9 fields, bed6+5 if 11).
        n_extra = n_fields_seen - 6
        bed_type = "bed6+" + str(n_extra)
        tmp_as = args.outBb + ".tmp.as"
        with open(tmp_as, "w") as f:
            f.write('table barChartBed\n')
            f.write('"bigBarChart bar graph"\n')
            f.write('    (\n')
            f.write('    string chrom;       "Reference sequence chromosome or scaffold"\n')
            f.write('    uint   chromStart;  "Start position in chromosome"\n')
            f.write('    uint   chromEnd;    "End position in chromosome"\n')
            f.write('    string name;        "Name or ID of item"\n')
            f.write('    uint   score;       "Score (0-1000)"\n')
            f.write('    char[1] strand;     "+, -, or ."\n')
            f.write('    string name2;       "Alternate name for item"\n')
            f.write('    uint expCount;      "Number of bar graphs"\n')
            f.write('    float[expCount] expScores;  "Comma separated list of category values"\n')
            if n_extra >= 5:
                f.write('    bigint _dataOffset; "Offset of sample data in matrix file"\n')
                f.write('    int    _dataLen;    "Length of sample data row in matrix file"\n')
            f.write('    )\n')
        result = subprocess.run(
            ["bedToBigBed", "-type=" + bed_type, "-as=" + tmp_as,
             tmp_bed, args.chromSizes, args.outBb],
            capture_output=True, text=True)
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        if result.returncode != 0:
            sys.exit("bedToBigBed failed with return code "
                     + str(result.returncode))
    finally:
        if os.path.exists(tmp_bed):
            os.remove(tmp_bed)
        tmp_as = args.outBb + ".tmp.as"
        if os.path.exists(tmp_as):
            os.remove(tmp_as)

    # Only write the new .categories after the .bb build succeeds
    with open(args.outCategories, "w") as f:
        for label in desired_labels:
            color = orig_cats[label_to_orig_idx[label]][1]
            f.write(label + "\t" + color + "\n")

    sys.stderr.write("wrote: " + args.outBb + "\n")
    sys.stderr.write("wrote: " + args.outCategories + "\n")


if __name__ == "__main__":
    main()
