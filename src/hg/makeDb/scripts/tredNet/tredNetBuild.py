#!/usr/bin/env python3
"""
Build tredNet enhancer and silencer bigBed tracks.

Reads all .enhancer.bed and .silencer.bed files from the input directory,
merges overlapping regions, annotates each with the contributing cell types/
tissues/cell lines, and writes BED9+ files for conversion to bigBed.
"""

import os
import sys
import subprocess
import argparse

# ── Ontology ID → readable name ──────────────────────────────────────────────

ID_MAP = {
    # CL – Cell Ontology (primary cell types)
    "CL-0000047": "neural stem cell",
    "CL-0000062": "osteoblast",
    "CL-0000084": "T cell",
    "CL-0000127": "astrocyte",
    "CL-0000134": "mesenchymal stem cell",
    "CL-0000182": "hepatocyte",
    "CL-0000223": "endodermal cell",
    "CL-0000236": "B cell",
    "CL-0000312": "keratinocyte",
    "CL-0000351": "trophoblast cell",
    "CL-0000515": "skeletal muscle myoblast",
    "CL-0000623": "natural killer cell",
    "CL-0000624": "CD4-positive, alpha-beta T cell",
    "CL-0000625": "CD8-positive, alpha-beta T cell",
    "CL-0000746": "cardiac muscle cell",
    "CL-0000792": "CD4-positive, CD25-positive, alpha-beta regulatory T cell",
    "CL-0000895": "naive thymus-derived CD4-positive, alpha-beta T cell",
    "CL-0000897": "CD4-positive, alpha-beta memory T cell",
    "CL-0000905": "effector memory CD4-positive, alpha-beta T cell",
    "CL-0000909": "CD8-positive, alpha-beta memory T cell",
    "CL-0001054": "CD14-positive monocyte",
    "CL-0001059": "common myeloid progenitor, CD34-positive",
    "CL-0002327": "mammary gland epithelial cell",
    "CL-0002372": "myotube",
    "CL-0002551": "fibroblast of dermis",
    "CL-0002553": "fibroblast of lung",
    "CL-0002618": "endothelial cell of umbilical vein",
    "CL-1001606": "foreskin keratinocyte",
    "CL-1001608": "foreskin fibroblast",
    "CL-2000044": "brain microvascular endothelial cell",
    "CL-2000045": "foreskin melanocyte",
    # EFO – Experimental Factor Ontology (cell lines)
    "EFO-0001187": "HepG2",
    "EFO-0001196": "IMR-90",
    "EFO-0001203": "MCF-7",
    "EFO-0001221": "NCI-H929",
    "EFO-0002067": "K562",
    "EFO-0002074": "PC-3",
    "EFO-0002106": "A673",
    "EFO-0002330": "SJSA1",
    "EFO-0002713": "Panc1",
    "EFO-0002784": "GM12878",
    "EFO-0002791": "HeLa-S3",
    "EFO-0002793": "HL-60",
    "EFO-0002824": "HCT116",
    "EFO-0002847": "PC-9",
    "EFO-0003042": "H1-hESC",
    "EFO-0003045": "H9",
    "EFO-0003072": "SK-N-SH",
    "EFO-0005719": "Karpas 422",
    "EFO-0005722": "SJRH30",
    "EFO-0005723": "GM23248",
    "EFO-0005724": "MM.1S",
    "EFO-0006270": "AG04450",
    "EFO-0007074": "DND-41",
    "EFO-0007096": "iPS DF 19.11",
    "EFO-0007098": "iPS DF 6.9",
    "EFO-0007598": "HAP-1",
    "EFO-0007610": "RWPE2",
    "EFO-0007950": "GM23338",
    # NTR – New Term Request (ENCODE-specific)
    "NTR-0000493": "left ventricle myocardium inferior",
    "NTR-0000856": "mesendoderm",
    # UBERON – Uberon multi-species anatomy (tissues/organs)
    "UBERON-0000059": "large intestine",
    "UBERON-0000317": "colonic mucosa",
    "UBERON-0000945": "stomach",
    "UBERON-0000996": "vagina",
    "UBERON-0001157": "transverse colon",
    "UBERON-0001159": "sigmoid colon",
    "UBERON-0001264": "pancreas",
    "UBERON-0001383": "muscle of leg",
    "UBERON-0001774": "skeletal muscle of trunk",
    "UBERON-0001873": "caudate nucleus",
    "UBERON-0001987": "placenta",
    "UBERON-0002048": "lung",
    "UBERON-0002080": "heart right ventricle",
    "UBERON-0002084": "heart left ventricle",
    "UBERON-0002106": "spleen",
    "UBERON-0002107": "liver",
    "UBERON-0002108": "small intestine",
    "UBERON-0002113": "kidney",
    "UBERON-0002168": "left lung",
    "UBERON-0002170": "upper lobe of right lung",
    "UBERON-0002171": "lower lobe of right lung",
    "UBERON-0002240": "spinal cord",
    "UBERON-0002367": "prostate gland",
    "UBERON-0002369": "adrenal gland",
    "UBERON-0002370": "thymus",
    "UBERON-0003124": "chorion membrane",
    "UBERON-0004648": "esophagus muscularis mucosa",
    "UBERON-0006483": "Brodmann area 46",
    "UBERON-0006631": "right atrium",
    "UBERON-0006920": "esophagus squamous epithelium",
    "UBERON-0007610": "tibial artery",
    "UBERON-0008367": "breast epithelium",
    "UBERON-0008450": "psoas muscle",
    "UBERON-0008953": "lower lobe of left lung",
    "UBERON-0011907": "gastrocnemius medialis",
    "UBERON-0036149": "suprapubic skin",
}

# NTR-0000493 is a tissue; NTR-0000856 is a cell type (in vitro differentiated)
TISSUE_IDS = {"NTR-0000493"}
CELL_TYPE_IDS = {"NTR-0000856"}


def get_category(sample_id):
    prefix = sample_id.split("-")[0]
    if prefix == "UBERON" or sample_id in TISSUE_IDS:
        return "tissue"
    elif prefix == "CL" or sample_id in CELL_TYPE_IDS:
        return "cell_type"
    elif prefix == "EFO":
        return "cell_line"
    return "other"


# Okabe-Ito palette (colorblind-safe)
COLORS = {
    "tissue":    (0, 158, 115),   # bluish green
    "cell_type": (0, 114, 178),   # blue
    "cell_line": (213, 94, 0),    # vermillion
    "mixed":     (230, 159, 0),   # orange
}

MOUSEOVER_THRESHOLD = 10  # show full list up to this count


def category_counts(sample_ids):
    counts = {"tissue": 0, "cell_type": 0, "cell_line": 0, "other": 0}
    for sid in sample_ids:
        counts[get_category(sid)] += 1
    return counts


def dominant_category(sample_ids):
    counts = category_counts(sample_ids)
    total = len(sample_ids)
    for cat in ("tissue", "cell_type", "cell_line"):
        if counts[cat] / total > 0.5:
            return cat
    return "mixed"


def make_name(sample_ids):
    n = len(sample_ids)
    counts = category_counts(sample_ids)
    # Use a specific label only when all samples are the same type
    if counts["tissue"] == n:
        label = "tissue"
    elif counts["cell_type"] == n:
        label = "cell type"
    elif counts["cell_line"] == n:
        label = "cell line"
    else:
        label = "sample"
    return f"{n} {label}{'s' if n != 1 else ''}"


def make_mouseover(names, sample_ids):
    n = len(names)
    counts = category_counts(sample_ids)
    if n <= MOUSEOVER_THRESHOLD:
        # Determine label for header
        if counts["tissue"] == n:
            header = f"{n} tissue{'s' if n != 1 else ''}"
        elif counts["cell_type"] == n:
            header = f"{n} cell type{'s' if n != 1 else ''}"
        elif counts["cell_line"] == n:
            header = f"{n} cell line{'s' if n != 1 else ''}"
        else:
            header = f"{n} sample{'s' if n != 1 else ''}"
        items = "<br>".join(f"&bull; {nm}" for nm in sorted(names))
        return f"{header}:<br>{items}"
    else:
        parts = []
        if counts["tissue"]:
            parts.append(f"{counts['tissue']} tissue{'s' if counts['tissue']!=1 else ''}")
        if counts["cell_type"]:
            parts.append(f"{counts['cell_type']} cell type{'s' if counts['cell_type']!=1 else ''}")
        if counts["cell_line"]:
            parts.append(f"{counts['cell_line']} cell line{'s' if counts['cell_line']!=1 else ''}")
        return f"{n} samples: " + ", ".join(parts)


def process_type(indir, etype, outbed, chrom_sizes):
    """Merge all .{etype}.bed files and annotate with sample info."""

    # Collect input files
    files = sorted(f for f in os.listdir(indir) if f.endswith(f".{etype}.bed"))
    if not files:
        print(f"No {etype} files found", file=sys.stderr)
        return

    print(f"Processing {len(files)} {etype} files...", file=sys.stderr)

    # Build tagged BED4 (chrom, start, end, sample_id) and pipe into bedtools merge
    # Use bedtools merge -c 4 -o distinct to collapse sample IDs per merged region
    # Sort chromosomes in natural genome order using chrom.sizes

    tagged_cmd = ["cat"]  # placeholder; we'll use subprocess.Popen chains

    # Step 1: cat all files with sample tag, sort, merge
    sort_proc = subprocess.Popen(
        ["sort", "-k1,1", "-k2,2n"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
    )

    merge_proc = subprocess.Popen(
        ["bedtools", "merge", "-i", "stdin", "-c", "4", "-o", "distinct"],
        stdin=sort_proc.stdout,
        stdout=subprocess.PIPE,
    )
    sort_proc.stdout.close()

    # Feed data to sort_proc
    total_in = 0
    for fname in files:
        sample_id = fname.replace(f".{etype}.bed", "").replace("_", "-")
        # Normalize: filenames use hyphens matching our ID_MAP keys
        fpath = os.path.join(indir, fname)
        with open(fpath) as fh:
            for line in fh:
                line = line.rstrip()
                if not line:
                    continue
                parts = line.split("\t")
                chrom, start, end = parts[0], parts[1], parts[2]
                sort_proc.stdin.write(f"{chrom}\t{start}\t{end}\t{sample_id}\n".encode())
                total_in += 1
    sort_proc.stdin.close()

    print(f"  Input regions: {total_in:,}", file=sys.stderr)

    # Step 2: process merge output → BED9+ rows
    total_out = 0
    missing_ids = set()
    rows = []

    for line in merge_proc.stdout:
        parts = line.decode().rstrip().split("\t")
        chrom, start, end = parts[0], parts[1], parts[2]
        sample_ids = parts[3].split(",")

        # Resolve names
        names = []
        for sid in sample_ids:
            nm = ID_MAP.get(sid)
            if nm is None:
                missing_ids.add(sid)
                nm = sid
            names.append(nm)

        cat = dominant_category(sample_ids)
        color = COLORS[cat]
        rgb = f"{color[0]},{color[1]},{color[2]}"

        name = make_name(sample_ids)
        score = 0
        strand = "."
        sample_list = ",".join(sorted(names))
        mouseover = make_mouseover(names, sample_ids)
        n_samples = len(sample_ids)

        rows.append(
            f"{chrom}\t{start}\t{end}\t{name}\t{score}\t{strand}\t"
            f"{start}\t{end}\t{rgb}\t{n_samples}\t{sample_list}\t"
            f"{mouseover}\t{cat}\n"
        )
        total_out += 1

    merge_proc.wait()

    if missing_ids:
        print(f"  WARNING: unresolved IDs: {missing_ids}", file=sys.stderr)

    print(f"  Merged output regions: {total_out:,}", file=sys.stderr)

    # Write BED (sorted by chrom/start for bigBedToBed)
    with open(outbed, "w") as fh:
        fh.writelines(rows)

    # Sort for bigBed conversion (bedtools merge output is already sorted,
    # but re-sort to be safe with chrom ordering matching chrom.sizes)
    sorted_bed = outbed + ".sorted"
    subprocess.run(
        ["sort", "-k1,1", "-k2,2n", outbed],
        stdout=open(sorted_bed, "w"),
        check=True,
    )
    os.rename(sorted_bed, outbed)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--indir", default="in", help="Input directory with bed files")
    ap.add_argument("--outdir", default=".", help="Output directory")
    ap.add_argument("--chrom-sizes", default="/hive/data/genomes/hg38/chrom.sizes")
    args = ap.parse_args()

    for etype in ("enhancer", "silencer"):
        outbed = os.path.join(args.outdir, f"tredNet{etype.capitalize()}.bed")
        process_type(args.indir, etype, outbed, args.chrom_sizes)
        print(f"Wrote {outbed}", file=sys.stderr)


if __name__ == "__main__":
    main()
