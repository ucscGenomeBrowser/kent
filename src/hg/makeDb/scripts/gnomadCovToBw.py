#!/usr/bin/env python3
"""Convert gnomAD coverage TSV to bigWig files - single pass, direct to bigWig."""

import gzip
import pyBigWig
import sys

INPUT = "gnomad.genomes.r3.0.1.coverage.summary.tsv.bgz"
CHROMSIZES = "/hive/data/genomes/hg38/bed/gnomad/coverage/gnomad.chrom.sizes"

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
    """Load chromosome sizes as list of tuples (preserves order)."""
    sizes = []
    with open(path) as f:
        for line in f:
            parts = line.strip().split('\t')
            sizes.append((parts[0], int(parts[1])))
    return sizes

def main():
    print("Loading chromosome sizes...")
    chrom_sizes = load_chrom_sizes(CHROMSIZES)

    print(f"Opening {len(COLUMNS)} bigWig files for writing...")
    bw_files = {}
    for name, _ in COLUMNS:
        outfile = f"gnomad.coverage.{name}.bw"
        bw = pyBigWig.open(outfile, "w")
        bw.addHeader(chrom_sizes)
        bw_files[name] = bw
        print(f"  Opened {outfile}")

    print(f"Processing {INPUT}...")

    # Buffer entries by chromosome for batch writing
    buffers = {name: {"chroms": [], "starts": [], "ends": [], "values": []}
               for name, _ in COLUMNS}
    current_chrom = None
    line_count = 0
    BUFFER_SIZE = 1000000  # Write in batches of 1M entries

    with gzip.open(INPUT, 'rt') as f:
        next(f)  # Skip header

        for line in f:
            line_count += 1
            if line_count % 10000000 == 0:
                print(f"  Processed {line_count:,} lines...")

            parts = line.strip().split('\t')
            locus = parts[0]
            chrom, pos = locus.rsplit(':', 1)
            pos = int(pos)
            start = pos - 1  # Convert to 0-based

            # When chromosome changes, flush buffers
            if chrom != current_chrom and current_chrom is not None:
                for name, _ in COLUMNS:
                    buf = buffers[name]
                    if buf["chroms"]:
                        bw_files[name].addEntries(
                            buf["chroms"], buf["starts"], ends=buf["ends"], values=buf["values"]
                        )
                        buffers[name] = {"chroms": [], "starts": [], "ends": [], "values": []}
                print(f"  Finished {current_chrom}")

            current_chrom = chrom

            # Add to buffers
            for name, col_idx in COLUMNS:
                buf = buffers[name]
                buf["chroms"].append(chrom)
                buf["starts"].append(start)
                buf["ends"].append(start + 1)
                buf["values"].append(float(parts[col_idx]))

            # Flush if buffer is full
            if len(buffers[COLUMNS[0][0]]["chroms"]) >= BUFFER_SIZE:
                for name, _ in COLUMNS:
                    buf = buffers[name]
                    bw_files[name].addEntries(
                        buf["chroms"], buf["starts"], ends=buf["ends"], values=buf["values"]
                    )
                    buffers[name] = {"chroms": [], "starts": [], "ends": [], "values": []}

    # Flush remaining data
    for name, _ in COLUMNS:
        buf = buffers[name]
        if buf["chroms"]:
            bw_files[name].addEntries(
                buf["chroms"], buf["starts"], ends=buf["ends"], values=buf["values"]
            )

    print(f"  Finished {current_chrom}")
    print(f"Total lines processed: {line_count:,}")

    # Close all files
    print("Closing files...")
    for name, _ in COLUMNS:
        bw_files[name].close()

    print("Done!")

if __name__ == "__main__":
    main()
