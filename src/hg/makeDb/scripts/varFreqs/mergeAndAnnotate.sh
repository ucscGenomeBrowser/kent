#!/bin/bash
set -eu
# Note: not using pipefail due to bcftools pipeline exit codes
#
# bcftools csq's --unify-chr-names was added in 1.22; conda installs 1.14 by
# default. Pin the 1.22 build at the front of PATH so every bcftools call in
# this script (including the parallel-spawned ones) finds the right binary.
export PATH=/cluster/software/src/bcftools-1.22:$PATH

# Merge multiple VCF files and annotate with protein consequences
# Uses bcftools for merging and consequence annotation
# Step 4 (strip+normalize) runs in parallel for speed.
#
# Usage:
#   ./mergeAndAnnotate.sh              # full genome
#   ./mergeAndAnnotate.sh --region chrM # quick test on chrM

WORKDIR="/hive/data/genomes/hg38/bed/varFreqs/all"
# Source of truth for which VCFs go into the combined track. The same TSV
# drives vcfToBigBed.py; reading both pipeline stages off it keeps the two
# lists from drifting (the old files.txt could miss new entries silently)
# and keeps stale entries in normalized/ out of the merge.
DBTSV="$HOME/kent/src/hg/makeDb/scripts/varFreqs/databases.tsv"
REF2BIT="/hive/data/genomes/hg38/hg38.2bit"
REFFASTA="$WORKDIR/hg38.fa"
GFF3="$WORKDIR/Homo_sapiens.GRCh38.115.chr.gff3.gz"
THREADS=8
PARALLEL_JOBS=8

# Parse flags
#   --region chrM        process a single region (quick test)
#   --databases <tsv>    DB config (default databases.tsv); subsets reuse the
#                        shared normalized/ cache built by the full run
#   --tag <name>         suffix for merged/annotated outputs so disease/array
#                        builds (e.g. --tag .disease) don't clobber the all run
REGION=""
TAG=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --region) REGION="$2"; shift 2 ;;
        --databases) DBTSV="$2"; shift 2 ;;
        --tag) TAG="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

REGION_ARGS=""
SUFFIX=""
VIEW_REGION=""
if [[ -n "$REGION" ]]; then
    REGION_ARGS="-r $REGION"
    SUFFIX=".${REGION%%:*}"  # e.g. ".chrM"
    VIEW_REGION="$REGION"
    echo "*** REGION MODE: $REGION ***"
else
    VIEW_REGION="chr1,chr2,chr3,chr4,chr5,chr6,chr7,chr8,chr9,chr10,chr11,chr12,chr13,chr14,chr15,chr16,chr17,chr18,chr19,chr20,chr21,chr22,chrX,chrY,chrM"
fi

# Output files (TAG keeps disease/array merges separate from the all build)
MERGED="$WORKDIR/merged${TAG}${SUFFIX}.vcf.gz"
ANNOTATED="$WORKDIR/merged${TAG}${SUFFIX}.annotated.vcf.gz"

echo "=== VCF Merge and Annotate Pipeline ==="
echo "Working directory: $WORKDIR"
echo ""

# Step 1: Check GFF3 annotation file
if [[ ! -f "$GFF3" ]]; then
    echo "Step 1: ERROR: GFF3 file not found: $GFF3"
    echo "  Download from: https://ftp.ensembl.org/pub/release-115/gff3/homo_sapiens/Homo_sapiens.GRCh38.115.chr.gff3.gz"
    exit 1
fi
if [[ ! -f "${GFF3}.tbi" ]]; then
    echo "Step 1: Indexing GFF3..."
    tabix -p gff "$GFF3"
else
    echo "Step 1: GFF3 OK"
fi

# Step 2: Create FASTA from 2bit if not present
if [[ ! -f "$REFFASTA" ]]; then
    echo "Step 2: Converting 2bit to FASTA..."
    twoBitToFa "$REF2BIT" "$REFFASTA"
    samtools faidx "$REFFASTA"
else
    echo "Step 2: FASTA already exists, skipping"
fi

# Step 3: Create chromosome rename map
CHRMAP="$WORKDIR/chr_rename.txt"
if [[ ! -f "$CHRMAP" ]]; then
    echo "Step 3: Creating chromosome rename map..."
    for i in {1..22} X Y M; do echo "$i chr$i"; done > "$CHRMAP"
    echo "MT chrM" >> "$CHRMAP"
    echo "23 chrX" >> "$CHRMAP"
    echo "24 chrY" >> "$CHRMAP"
else
    echo "Step 3: Chromosome rename map already exists, skipping"
fi

# ============================================================================
# Step 4: Strip and normalize VCFs (PARALLEL)
# ============================================================================

# Export variables needed by the worker function
export WORKDIR SUFFIX VIEW_REGION REFFASTA CHRMAP

# Worker function: process one VCF (strip + normalize)
process_one_vcf() {
    local vcf="$1"
    local bname=$(basename "$vcf" .vcf.gz)
    local stripped="$WORKDIR/stripped/${bname}${SUFFIX}.stripped.vcf.gz"
    local outfile="$WORKDIR/normalized/${bname}${SUFFIX}.norm.vcf.gz"

    if [[ -f "$outfile" ]]; then
        echo "  $bname: already done, skipping"
        return 0
    fi

    echo "  Processing: $bname"

    # Check chromosome naming
    local first_chr
    first_chr=$(bcftools view -H "$vcf" 2>/dev/null | head -1 | cut -f1)
    if [[ -z "$first_chr" ]]; then
        echo "    $bname: WARNING: Empty VCF or cannot read, skipping"
        return 0
    fi

    # Check if header uses chr prefix
    local header_chr
    header_chr=$(bcftools view -h "$vcf" 2>/dev/null | grep '##contig' | head -1 | grep -o 'ID=chr' || true)

    # Strip fields and fix chromosome naming
    if [[ "$first_chr" =~ ^chr ]]; then
        # Data already has chr prefix
        bcftools view -r "$VIEW_REGION" "$vcf" 2>/dev/null | \
            bcftools annotate --force -x ID,QUAL,FILTER,INFO -Oz -o "$stripped" 2>/dev/null || {
            echo "    $bname: ERROR: strip failed"; return 1
        }
    elif [[ -n "$header_chr" ]]; then
        # Header has chr but data doesn't (e.g. AllOfUs)
        echo "    $bname: fixing mismatched chr prefix"
        bcftools annotate --force -x ID,QUAL,FILTER,INFO "$vcf" 2>/dev/null | \
            awk 'BEGIN{OFS="\t"} /^#/{print;next} {$1="chr"$1; print}' | \
            bgzip -c > "${stripped}.tmp.vcf.gz" 2>/dev/null && \
        bcftools index -t "${stripped}.tmp.vcf.gz" 2>/dev/null && \
        bcftools view -r "$VIEW_REGION" "${stripped}.tmp.vcf.gz" -Oz -o "$stripped" 2>/dev/null && \
        rm -f "${stripped}.tmp.vcf.gz" "${stripped}.tmp.vcf.gz.tbi" || {
            echo "    $bname: ERROR: strip/fix failed"; rm -f "${stripped}.tmp.vcf.gz"*; return 1
        }
    else
        # No chr prefix anywhere
        echo "    $bname: adding chr prefix"
        bcftools annotate --force -x ID,QUAL,FILTER,INFO "$vcf" 2>/dev/null | \
            awk 'BEGIN{OFS="\t";
                       r["23"]="chrX"; r["24"]="chrY"; r["MT"]="chrM"
                     }
                 /^##contig=<ID=23>/{sub(/ID=23/,"ID=chrX"); print; next}
                 /^##contig=<ID=24>/{sub(/ID=24/,"ID=chrY"); print; next}
                 /^##contig=<ID=MT>/{sub(/ID=MT/,"ID=chrM"); print; next}
                 /^##contig=<ID=([0-9]|[XYM])/{sub(/ID=/,"ID=chr")}
                 /^#/{print;next}
                 {c=$1; if(c in r) $1=r[c]; else $1="chr"c; print}' | \
            bgzip -c > "${stripped}.tmp.vcf.gz" 2>/dev/null && \
        bcftools index -t "${stripped}.tmp.vcf.gz" 2>/dev/null && \
        bcftools view -r "$VIEW_REGION" "${stripped}.tmp.vcf.gz" -Oz -o "$stripped" 2>/dev/null && \
        rm -f "${stripped}.tmp.vcf.gz" "${stripped}.tmp.vcf.gz.tbi" || {
            echo "    $bname: ERROR: strip/rename failed"; rm -f "${stripped}.tmp.vcf.gz"*; return 1
        }
    fi

    bcftools index -t "$stripped" 2>/dev/null

    # Normalize (split multi-allelic sites)
    bcftools norm -m -any --check-ref x -f "$REFFASTA" "$stripped" -Oz -o "$outfile" 2>/dev/null || {
        echo "    $bname: ERROR: normalization failed"
        rm -f "$stripped" "$stripped.tbi"
        return 1
    }

    bcftools index -t "$outfile" 2>/dev/null
    rm -f "$stripped" "$stripped.tbi"

    local nvars
    nvars=$(bcftools index -n "$outfile" 2>/dev/null || echo "?")
    echo "    $bname: Done: $nvars variants"
}
export -f process_one_vcf

echo ""
echo "Step 4: Processing VCFs in parallel ($PARALLEL_JOBS jobs)..."
mkdir -p "$WORKDIR/normalized" "$WORKDIR/stripped"

# Pull the VCF column out of databases.tsv (skip comments + blank lines).
awk -F'\t' '!/^#/ && NF >= 3 && $3 != "" {print $3}' "$DBTSV" | \
    parallel -j "$PARALLEL_JOBS" --line-buffer process_one_vcf {}

echo "Step 4: Done."

# ============================================================================
# Step 5: Merge all normalized VCFs
# ============================================================================
echo ""
echo "Step 5: Merging all VCFs..."

# Build the merge list from databases.tsv so it tracks exactly what Step 4
# processed. Previously we used `find normalized/ -name '*.norm.vcf.gz'`,
# which silently re-merged stale entries from prior builds (e.g.
# IndiGenomes_Variants.norm.vcf.gz after IndiGen was dropped from the
# config). Basename rule mirrors process_one_vcf: strip .vcf.gz only
# (.vcf.bgz like SG10K_Health is left intact, matching the cache layout).
NORM_LIST="$WORKDIR/normalized_files${TAG}${SUFFIX}.txt"
awk -F'\t' '!/^#/ && NF >= 3 && $3 != "" {print $3}' "$DBTSV" | \
while read -r vcf; do
    bn=$(basename "$vcf" .vcf.gz)
    echo "$WORKDIR/normalized/${bn}${SUFFIX}.norm.vcf.gz"
done | sort > "$NORM_LIST"
nfiles=$(wc -l < "$NORM_LIST")
echo "  Found $nfiles normalized VCF files"

if [[ ! -f "$MERGED" ]]; then
    bcftools merge \
        --threads "$THREADS" \
        -l "$NORM_LIST" \
        -m none \
        --force-samples \
        -Oz -o "$MERGED"
    bcftools index -t "$MERGED"
    echo "  Merged VCF created: $MERGED"
else
    echo "  Merged VCF already exists, skipping"
fi

# ============================================================================
# Step 6: Annotate with consequences
# ============================================================================
echo ""
echo "Step 6: Annotating with protein consequences..."

if [[ ! -f "$ANNOTATED" ]]; then
    echo "  Running bcftools csq..."
    bcftools csq \
        --threads "$THREADS" \
        $REGION_ARGS \
        -p a \
        -l \
        -n 64 \
        --unify-chr-names chr,-,chr \
        --force \
        -f "$REFFASTA" \
        -g "$GFF3" \
        "$MERGED" \
        -Oz -o "$ANNOTATED" 2>"$WORKDIR/csq.log" || {
            echo "  WARNING: bcftools csq had errors. Check csq.log"
            cat "$WORKDIR/csq.log"
        }

    if [[ -f "$ANNOTATED" ]]; then
        bcftools index -t "$ANNOTATED"
        echo "  Annotated VCF created: $ANNOTATED"
    fi
else
    echo "  Annotated VCF already exists, skipping"
fi

# ============================================================================
# Step 7: Summary
# ============================================================================
echo ""
echo "=== Summary ==="
if [[ -f "$MERGED" ]]; then
    echo "Merged variants: $(bcftools view -H "$MERGED" | wc -l)"
fi

if [[ -f "$ANNOTATED" ]]; then
    echo ""
    echo "Consequence summary (top 20):"
    bcftools query -f '%INFO/BCSQ\n' "$ANNOTATED" 2>/dev/null | \
        cut -d'|' -f1 | sort | uniq -c | sort -rn | head -20
fi

echo ""
echo "Done! Output files:"
echo "  - Merged VCF: $MERGED"
echo "  - Annotated VCF: $ANNOTATED"
