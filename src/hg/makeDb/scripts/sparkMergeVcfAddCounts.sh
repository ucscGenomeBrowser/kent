#!/bin/bash
# Merge all per-chromosome VCFs into a single sites-only VCF
# Uses bcftools fill-tags plugin to calculate AC/AN/AF from genotypes
# Output INFO fields: AC, AF, AN, AQ

set -euo pipefail

#INBASE="vcf/SPARK.iWES_v3.2024_08.deepvariant"
INBASE=$1
OUTFILE=`basename $INBASE.sites.vcf.gz`
TMPDIR="tmp_merge_$$"
BCFTOOLS=~/software/bcftools/bin/bcftools
export BCFTOOLS_PLUGINS=~/software/bcftools/libexec/bcftools
export INBASE

# Number of parallel jobs (adjust based on available CPUs/memory)
NJOBS=${2:-4}

mkdir -p "$TMPDIR"

echo "Processing chromosomes with fill-tags plugin (parallel=$NJOBS)..."
echo "Started at $(date)"

# Function to process one chromosome
process_chr() {
    local chr=$1
    local infile="${INBASE}.${chr}.vcf.gz"
    local outfile="$TMPDIR/${chr}.sites.vcf.gz"

    if [[ ! -f "$infile" ]]; then
        echo "WARNING: $infile not found, skipping" >&2
        return
    fi

    echo "Processing $chr..."

    # Fill AC/AN/AF tags, then drop samples
    $BCFTOOLS +fill-tags "$infile" -- -t AC,AN,AF 2>/dev/null | \
        $BCFTOOLS view -G -Oz -o "$outfile" - 2>/dev/null
    $BCFTOOLS index -t "$outfile"

    echo "  Done: $chr"
}

export -f process_chr
export TMPDIR BCFTOOLS BCFTOOLS_PLUGINS

# Run chromosomes in parallel
printf '%s\n' chr{1..22} chrX chrY | xargs -P "$NJOBS" -I {} bash -c 'process_chr "$@"' _ {}

echo ""
echo "Concatenating all chromosomes..."

# Create file list in chromosome order
for chr in chr{1..22} chrX chrY; do
    echo "$TMPDIR/${chr}.sites.vcf.gz"
done > "$TMPDIR/files.txt"

# Concatenate
$BCFTOOLS concat -f "$TMPDIR/files.txt" -Oz -o "$OUTFILE"
$BCFTOOLS index -t "$OUTFILE"

echo "Cleaning up..."
rm -rf "$TMPDIR"

echo ""
echo "Done! Output: $OUTFILE"
echo "Finished at $(date)"
echo ""
echo "Verifying output..."
$BCFTOOLS view -h "$OUTFILE" | grep "^##INFO"
echo ""
echo "Sample variants:"
$BCFTOOLS view "$OUTFILE" 2>/dev/null | grep -v "^#" | head -5
