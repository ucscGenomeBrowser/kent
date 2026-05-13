#!/bin/bash
# Build the DRACH (m6A consensus) motif track for hg38.
#
# Pipeline:
#   1. fetch MANE Ensembl rna fasta + genomic GTF (release 1.5)
#   2. scan transcripts for [AGT][AG]AC[ACT] -> transcript-coord BED
#   3. lift via bedToPsl + pslMap through the MANE transcript->genome PSL
#   4. decorate to bigBed 12+5 with motif/gene/region metadata
#
# Run from the data directory (default /hive/data/genomes/hg38/bed/rnaMod/drach).

set -euo pipefail

DATADIR="${DATADIR:-/hive/data/genomes/hg38/bed/rnaMod/drach}"
SCRIPTS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHROMSIZES="/hive/data/genomes/hg38/chrom.sizes"
MANE_BASE="https://ftp.ncbi.nlm.nih.gov/refseq/MANE/MANE_human/release_1.5"
RNA_FA="MANE.GRCh38.v1.5.ensembl_rna.fna.gz"
GTF="MANE.GRCh38.v1.5.ensembl_genomic.gtf.gz"

mkdir -p "$DATADIR"
cd "$DATADIR"

echo "[$(date +%T)] working in $DATADIR"

# 1. download
if [ ! -s "$RNA_FA" ]; then
    echo "[$(date +%T)] curl $RNA_FA"
    curl -sSf -o "$RNA_FA" "$MANE_BASE/$RNA_FA"
fi
if [ ! -s "$GTF" ]; then
    echo "[$(date +%T)] curl $GTF"
    curl -sSf -o "$GTF" "$MANE_BASE/$GTF"
fi

# 2. scan for DRACH in transcript coords
echo "[$(date +%T)] scan DRACH motifs"
python3 "$SCRIPTS/drachFromFasta.py" "$RNA_FA" \
    drach.tx.bed tx.sizes tx2gene.tsv

# 3a. transcripts -> genePred -> transcript->genome PSL
echo "[$(date +%T)] gtfToGenePred"
gunzip -c "$GTF" > MANE.gtf
gtfToGenePred -genePredExt MANE.gtf MANE.gp
echo "[$(date +%T)] genePredToPsl"
genePredToPsl "$CHROMSIZES" MANE.gp MANE.tx2genome.psl

# 3b. motif BED -> motif PSL (in transcript space)
echo "[$(date +%T)] bedToPsl (motifs in transcript space)"
bedToPsl tx.sizes drach.tx.bed drach.tx.psl

# 3c. project motifs onto the genome
echo "[$(date +%T)] pslMap"
pslMap -mapInfo=drach.mapInfo drach.tx.psl MANE.tx2genome.psl drach.genome.psl

# 4a. PSL -> BED12 (preserves multi-block features at splice junctions)
echo "[$(date +%T)] pslToBed"
pslToBed drach.genome.psl drach.bed12.tmp

# 4b. decorate to bed12+5
echo "[$(date +%T)] decorate to bed12+5"
python3 "$SCRIPTS/drachBedToBigBed.py" \
    drach.bed12.tmp MANE.gp tx2gene.tsv > drach.bed

# 4c. sort + bigBed
echo "[$(date +%T)] sort + bedToBigBed"
sort -k1,1 -k2,2n drach.bed > drach.sorted.bed
bedToBigBed -tab -type=bed12+5 -as="$SCRIPTS/drach.as" \
    drach.sorted.bed "$CHROMSIZES" drach.bb

# 5. summary report
echo
echo "===== DRACH track build summary ====="
n_tx=$(wc -l < tx.sizes)
n_motif_in=$(wc -l < drach.tx.bed)
n_motif_mapped=$(wc -l < drach.bed12.tmp)
n_final=$(wc -l < drach.sorted.bed)
n_multi=$(awk -F'\t' '$10>1' drach.sorted.bed | wc -l)
echo "MANE transcripts processed:           $n_tx"
echo "DRACH motifs found (transcript):      $n_motif_in"
echo "Motifs mapped to genome (pslToBed):   $n_motif_mapped"
echo "Final features in drach.bb:           $n_final"
echo "Multi-block (splice junction):        $n_multi"
if [ "$n_motif_in" != "$n_motif_mapped" ]; then
    dropped=$((n_motif_in - n_motif_mapped))
    echo "WARN: $dropped motifs were not mapped to the genome." >&2
    echo "      See drach.mapInfo for details." >&2
fi
echo

echo "[$(date +%T)] done. Output: $DATADIR/drach.bb"
