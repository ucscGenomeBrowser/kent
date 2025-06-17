import pandas as pd
import os
import subprocess

# Inputs
tsv_file = "mcap_v1_4.txt.gz"
chrom_sizes_file = "hg19.chrom.sizes"  # You must download this file from UCSC if not available

# Output
output_dir = "bw"
os.makedirs(output_dir, exist_ok=True)

# Load TSV
#df = pd.read_csv(tsv_file, sep="\t", comment="#")

# Ensure column names are stripped

df = pd.read_csv(tsv_file, sep="\t", comment=None)
df.columns = df.columns.str.strip().str.lstrip("#")  # remove leading '#' from column names

# Normalize chromosome names (e.g., add "chr" prefix if needed)
df['grch37_chrom'] = df['grch37_chrom'].astype(str)
df['chrom'] = df['grch37_chrom'].apply(lambda x: f"chr{x}" if not x.startswith("chr") else x)

# Add start and end positions (bedGraph format: zero-based start, one-based end)
df['start'] = df['pos'] - 1
df['end'] = df['pos']

# Write bedGraph and convert to bigWig for each alt nucleotide
for alt_base in ['A', 'C', 'G', 'T']:
    subset = df[df['alt'] == alt_base]
    if subset.empty:
        continue

    bedgraph_file = os.path.join(output_dir, f"{alt_base}.bedGraph")
    bigwig_file = os.path.join(output_dir, f"{alt_base}.bw")

    subset[['chrom', 'start', 'end', 'mcapv1.4']].to_csv(
        bedgraph_file, sep='\t', header=False, index=False
    )

    # Convert to bigWig
    subprocess.run([
        "bedGraphToBigWig",
        bedgraph_file,
        chrom_sizes_file,
        bigwig_file
    ], check=True)

    print(f"Created: {bigwig_file}")

