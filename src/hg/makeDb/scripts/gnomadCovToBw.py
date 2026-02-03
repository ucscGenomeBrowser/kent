#!/usr/bin/env python3
"""Convert gnomAD coverage TSV to bigWig files - single pass, direct to bigWig."""

import gzip
import pyBigWig
import sys

#INPUT = "gnomad.genomes.r3.0.1.coverage.summary.tsv.bgz"
INPUT = sys.argv[1]
CHROMSIZES = "/hive/data/genomes/hg38/chrom.sizes"

# Column definitions: (name, column_index 0-based)
# locus(0) mean(1) median_approx(2) total_DP(3) over_1(4) over_5(5) over_10(6)
# over_15(7) over_20(8) over_25(9) over_30(10) over_50(11) over_100(12)
COLUMNS = [
    ("mean", 1),
    ("median", 2),
    ("over_1", 4),
    ("over_5", 5),
    ("over_10", 6),
    ("over_15", 7),
    ("over_20", 8),
    ("over_25", 9),
    ("over_30", 10),
    ("over_50", 11),
    ("over_100", 12),
]

def load_chrom_sizes(path):
    """Load chromosome sizes as dict for lookup."""
    sizes = {}
    with open(path) as f:
        for line in f:
            parts = line.strip().split('\t')
            sizes[parts[0]] = int(parts[1])
    return sizes

# Standard gnomAD chromosome order (numeric, then X, Y, M)
GNOMAD_CHROM_ORDER = [
    "chr1", "chr2", "chr3", "chr4", "chr5", "chr6", "chr7", "chr8", "chr9", "chr10",
    "chr11", "chr12", "chr13", "chr14", "chr15", "chr16", "chr17", "chr18", "chr19", "chr20",
    "chr21", "chr22", "chrX", "chrY", "chrM"
]

def main():
    print("Loading chromosome sizes...")
    chrom_size_dict = load_chrom_sizes(CHROMSIZES)

    # Build header with chromosomes in gnomAD order (not chrom.sizes order)
    chrom_sizes = [(c, chrom_size_dict[c]) for c in GNOMAD_CHROM_ORDER if c in chrom_size_dict]
    print(f"  Using {len(chrom_sizes)} chromosomes in gnomAD order")

    print(f"Opening {len(COLUMNS)} bigWig files for writing...")
    bw_files = {}
    for name, _ in COLUMNS:
        outfile = f"gnomad.coverage.{name}.bw"
        bw = pyBigWig.open(outfile, "w")
        bw.addHeader(chrom_sizes)
        bw_files[name] = bw
        print(f"  Opened {outfile}")

    print(f"Processing {INPUT}...")

    # Buffer entries per chromosome - only flush at chromosome boundaries
    buffers = {name: {"chroms": [], "starts": [], "ends": [], "values": []}
               for name, _ in COLUMNS}
    current_chrom = None
    last_pos = -1
    line_count = 0
    dup_count = 0

    with gzip.open(INPUT, 'rt') as f:
        next(f)  # Skip header

        for line in f:
            line_count += 1
            if line_count % 10000000 == 0:
                print(f"  Processed {line_count:,} lines...", flush=True)

            parts = line.strip().split('\t')
            locus = parts[0]
            chrom, pos = locus.rsplit(':', 1)
            pos = int(pos)
            start = pos - 1  # Convert to 0-based

            # Skip duplicate positions (same chrom and pos as previous line)
            if chrom == current_chrom and start == last_pos:
                dup_count += 1
                continue

            # When chromosome changes, flush buffers for previous chromosome
            if chrom != current_chrom and current_chrom is not None:
                for name, _ in COLUMNS:
                    buf = buffers[name]
                    if buf["chroms"]:
                        bw_files[name].addEntries(
                            buf["chroms"], buf["starts"], ends=buf["ends"], values=buf["values"]
                        )
                        buffers[name] = {"chroms": [], "starts": [], "ends": [], "values": []}
                print(f"  Finished {current_chrom}", flush=True)
                last_pos = -1  # Reset for new chromosome

            current_chrom = chrom

            # Add to buffers
            for name, col_idx in COLUMNS:
                buf = buffers[name]
                buf["chroms"].append(chrom)
                buf["starts"].append(start)
                buf["ends"].append(start + 1)
                buf["values"].append(float(parts[col_idx]))

            last_pos = start

    # Flush remaining data for last chromosome
    for name, _ in COLUMNS:
        buf = buffers[name]
        if buf["chroms"]:
            bw_files[name].addEntries(
                buf["chroms"], buf["starts"], ends=buf["ends"], values=buf["values"]
            )

    print(f"  Finished {current_chrom}")
    print(f"Total lines processed: {line_count:,}")
    if dup_count > 0:
        print(f"Skipped {dup_count:,} duplicate positions")

    # Close all files
    print("Closing files...")
    for name, _ in COLUMNS:
        bw_files[name].close()

    print("Done!")

if __name__ == "__main__":
    main()
