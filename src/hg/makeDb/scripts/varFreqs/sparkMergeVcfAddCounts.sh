#!/bin/bash
# Merge all per-chromosome VCFs into a single sites-only VCF
# Uses bcftools fill-tags plugin to calculate AC/AN/AF from genotypes
# Output INFO fields: AC, AF, AN, AQ
#
# Optional 3rd arg: a SPARK individuals_registration TSV. When given, samples
# are split by the "asd" column (TRUE -> AUT, FALSE -> NON_AUT) and fill-tags
# also emits per-group AC_AUT/AN_AUT/AF_AUT and AC_NON_AUT/AN_NON_AUT/AF_NON_AUT.
# Samples missing from the registration file (or with a blank asd value) still
# count toward the overall AC/AN/AF but contribute to neither group.

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

# Optional ASD phenotype registration TSV (column 1 = subject_sp_id, column 8 = asd)
REGFILE=${3:-}
GROUPFILE=""

mkdir -p "$TMPDIR"

# Build a fill-tags sample-group file (sample<TAB>group) if a registration
# TSV was supplied. Restrict to samples actually present in the VCF so the
# plugin never sees an unknown sample ID.
if [[ -n "$REGFILE" ]]; then
    echo "Building ASD/non-ASD sample groups from $REGFILE ..."
    # Find the first per-chromosome VCF that exists to read the sample list.
    FIRSTVCF=""
    for chr in chr{1..22} chrX chrY; do
        if [[ -f "${INBASE}.${chr}.vcf.gz" ]]; then FIRSTVCF="${INBASE}.${chr}.vcf.gz"; break; fi
    done
    if [[ -z "$FIRSTVCF" ]]; then echo "ERROR: no per-chromosome VCF found for $INBASE" >&2; exit 1; fi

    GROUPFILE="$TMPDIR/asd_groups.txt"
    $BCFTOOLS query -l "$FIRSTVCF" > "$TMPDIR/vcf_samples.txt"
    # asd column: TRUE -> AUT, FALSE -> NON_AUT, anything else -> unassigned
    awk -F'\t' 'NR==FNR{a[$1]=$8; next}
                {st=a[$1];
                 if(st=="TRUE") print $1"\tAUT";
                 else if(st=="FALSE") print $1"\tNON_AUT"}' \
        "$REGFILE" "$TMPDIR/vcf_samples.txt" > "$GROUPFILE"
    nAut=$(awk -F'\t' '$2=="AUT"' "$GROUPFILE" | wc -l)
    nNon=$(awk -F'\t' '$2=="NON_AUT"' "$GROUPFILE" | wc -l)
    nTot=$(wc -l < "$TMPDIR/vcf_samples.txt")
    echo "  $nTot VCF samples: $nAut ASD (AUT), $nNon non-ASD (NON_AUT), $((nTot-nAut-nNon)) unassigned"
fi
export GROUPFILE

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

    # Fill AC/AN/AF tags (per-group too if a group file was built), then drop samples
    if [[ -n "$GROUPFILE" ]]; then
        $BCFTOOLS +fill-tags "$infile" -- -t AC,AN,AF -S "$GROUPFILE" 2>/dev/null | \
            $BCFTOOLS view -G -Oz -o "$outfile" - 2>/dev/null
    else
        $BCFTOOLS +fill-tags "$infile" -- -t AC,AN,AF 2>/dev/null | \
            $BCFTOOLS view -G -Oz -o "$outfile" - 2>/dev/null
    fi
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
$BCFTOOLS view -h "$OUTFILE" | grep "^##INFO" || true
echo ""
echo "Sample variants:"
# `| head -5` closes the pipe early, so the upstream bcftools exits via
# SIGPIPE (141). With set -o pipefail that would fail the whole script
# after a successful build, breaking any && chain in the caller.
{ $BCFTOOLS view "$OUTFILE" 2>/dev/null || true; } | grep -v "^#" | head -5 || true
