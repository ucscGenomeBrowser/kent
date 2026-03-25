#!/bin/bash
# Filter a large tabix-indexed VCF in parallel by splitting the genome into chunks.
# Strips genotypes (-G) and keeps only specified INFO fields.
# Uses GNU parallel to process chunks concurrently, then concatenates results.
#
# Usage:
#   sh vcfFilterParallel.sh <input.vcf.gz> <output.vcf.gz> <keep_fields> [jobs] [chunk_mb]
#
# Example:
#   KEEP="INFO/AC,INFO/AF,INFO/AN,INFO/nhomalt"
#   sh vcfFilterParallel.sh input.vcf.gz output.vcf.gz "$KEEP" 12 50
#
# Arguments:
#   input.vcf.gz  - tabix-indexed input VCF
#   output.vcf.gz - output path for filtered VCF
#   keep_fields   - comma-separated INFO fields to keep (passed to bcftools annotate -x "^...")
#   jobs          - number of parallel jobs (default: 12)
#   chunk_mb      - chunk size in Mb of genomic coordinates (default: 50)

set -euo pipefail

if [ $# -lt 3 ]; then
    echo "Usage: $0 <input.vcf.gz> <output.vcf.gz> <keep_fields> [jobs] [chunk_mb]" >&2
    exit 1
fi

INPUT="$1"
OUTPUT="$2"
KEEP="$3"
JOBS="${4:-12}"
CHUNK_MB="${5:-50}"
CHUNK_SIZE=$((CHUNK_MB * 1000000))

CHROMSIZES="/hive/data/genomes/hg38/chrom.sizes"
TMPDIR=$(mktemp -d "${OUTPUT%.vcf.gz}.chunks.XXXXXX")

echo "Input:      $INPUT"
echo "Output:     $OUTPUT"
echo "Jobs:       $JOBS"
echo "Chunk size: ${CHUNK_MB} Mb"
echo "Temp dir:   $TMPDIR"
echo ""

# Generate regions: split each standard chromosome into chunks
REGIONS_FILE="$TMPDIR/regions.txt"
grep -E '^chr[0-9]+\b|^chrX\b' "$CHROMSIZES" | sort -V | while read chr size; do
    start=1
    chunk=0
    while [ "$start" -le "$size" ]; do
        end=$((start + CHUNK_SIZE - 1))
        if [ "$end" -gt "$size" ]; then
            end="$size"
        fi
        printf "%s\t%s:%d-%d\t%s/%s_%04d.vcf.gz\n" "$chr" "$chr" "$start" "$end" "$TMPDIR" "$chr" "$chunk"
        start=$((end + 1))
        chunk=$((chunk + 1))
    done
done > "$REGIONS_FILE"

NCHUNKS=$(wc -l < "$REGIONS_FILE")
echo "Generated $NCHUNKS chunks across $(cut -f1 "$REGIONS_FILE" | sort -u | wc -l) chromosomes"
echo "Starting parallel processing with $JOBS jobs..."
echo ""

# Process each chunk: strip genotypes, keep only specified INFO fields
process_chunk() {
    local region="$1"
    local outfile="$2"
    bcftools view -G -r "$region" "$INPUT" \
        | bcftools annotate -x "^${KEEP}" -Oz -o "$outfile"
    bcftools index -t "$outfile"
}
export -f process_chunk
export INPUT KEEP

# Run in parallel
cut -f2,3 "$REGIONS_FILE" | parallel -j "$JOBS" --colsep '\t' process_chunk {1} {2}

echo ""
echo "All chunks done. Concatenating..."

# Build file list in correct order (regions file is already sorted)
FILE_LIST="$TMPDIR/filelist.txt"
cut -f3 "$REGIONS_FILE" > "$FILE_LIST"

# Naive concat - chunks are sorted and non-overlapping
bcftools concat -n -f "$FILE_LIST" -Oz -o "$OUTPUT"
tabix -p vcf "$OUTPUT"

echo "Cleaning up chunks..."
rm -rf "$TMPDIR"

echo ""
echo "Done: $(ls -lh "$OUTPUT" | awk '{print $5}') $OUTPUT"
