#!/bin/bash
# Make a small version of the gnomAD HGDP+1000G genotype VCF (3.5 TB) for the
# phasedVars hgdp1k track, so haplotype clustering works at higher zoom levels.
# Keeps only PASS SNVs with INFO/AC > 5, the GT genotype field and INFO/AC,AN,AF.
# Runs 10 Mbp chunks in parallel, then concatenates them.
#
# Usage:
#   sh hgdp1kCommonSnvs.sh <input.vcf.gz> <output.vcf.gz> [jobs] [chunk_mb]

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <input.vcf.gz> <output.vcf.gz> [jobs] [chunk_mb]" >&2
    exit 1
fi

INPUT="$1"
OUTPUT="$2"
JOBS="${3:-60}"
CHUNK_MB="${4:-10}"
CHUNK_SIZE=$((CHUNK_MB * 1000000))

CHROMSIZES="/hive/data/genomes/hg38/chrom.sizes"
TMPDIR=$(mktemp -d "${OUTPUT%.vcf.gz}.chunks.XXXXXX")
echo "Temp dir: $TMPDIR"

# chunks for every chromosome that is in the tabix index
REGIONS_FILE="$TMPDIR/regions.txt"
tabix -l "$INPUT" | while read chr; do
    size=$(awk -v c="$chr" '$1==c {print $2}' "$CHROMSIZES")
    start=1
    chunk=0
    while [ "$start" -le "$size" ]; do
        end=$((start + CHUNK_SIZE - 1))
        if [ "$end" -gt "$size" ]; then
            end="$size"
        fi
        printf "%s:%d-%d\t%s/%s_%04d.vcf.gz\n" "$chr" "$start" "$end" "$TMPDIR" "$chr" "$chunk"
        start=$((end + 1))
        chunk=$((chunk + 1))
    done
done > "$REGIONS_FILE"
echo "$(wc -l < "$REGIONS_FILE") chunks, running $JOBS jobs"

# by default -r returns every record that overlaps the region, so a record could
# end up in two chunks; --regions-overlap pos only uses the start position
process_chunk() {
    bcftools view --regions-overlap pos -r "$1" -f PASS -v snps -i 'INFO/AC>5' "$INPUT" -Ou \
        | bcftools annotate -x '^INFO/AC,INFO/AN,INFO/AF,^FORMAT/GT' -Oz -o "$2.tmp"
    mv "$2.tmp" "$2"
}
export -f process_chunk
export INPUT

cut -f1,2 "$REGIONS_FILE" | parallel --halt now,fail=1 -j "$JOBS" --colsep '\t' process_chunk {1} {2}

cut -f2 "$REGIONS_FILE" > "$TMPDIR/filelist.txt"
bcftools concat -n -f "$TMPDIR/filelist.txt" -Oz -o "$OUTPUT"
tabix -p vcf "$OUTPUT"

rm -rf "$TMPDIR"
echo "Done: $(ls -lh "$OUTPUT" | awk '{print $5}') $OUTPUT"
