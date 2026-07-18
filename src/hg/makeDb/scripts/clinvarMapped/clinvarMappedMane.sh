#!/bin/bash
# clinvarMappedMane.sh - build the MANE Select transcript set used as the single
# representative protein-coding transcript per gene.
#
# Inputs:  /gbdb/hg38/mane/mane.bb  (bigGenePred, MANE Select + MANE Plus Clinical)
#          /gbdb/hg38/hg38.2bit
# Outputs (in <outDir>):
#   maneSelect.gp    genePred of MANE Select transcripts (one per gene)
#   maneSelect.faa   translated protein sequence per transcript (id = versioned ENST)
#   maneMeta.tsv     enst  ensg(noVersion)  sym  refSeqTx  refSeqProt  chrom  strand  geneType
#
# We use MANE Select only (not MANE Plus Clinical) so there is exactly one
# transcript per gene; Plus Clinical would add a second transcript for ~74 genes.
# Usage: clinvarMappedMane.sh <outDir>
set -beEu -o pipefail

outDir="${1:?usage: clinvarMappedMane.sh <outDir>}"
maneBb=/gbdb/hg38/mane/mane.bb
twoBit=/gbdb/hg38/hg38.2bit

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Full flat table (25 cols) and full genePred, then restrict to MANE Select.
bigBedToBed "$maneBb" "$tmp/mane.bed"
bigGenePredToGenePred "$maneBb" "$tmp/mane.gp"

# maneStat is the last (25th) column; keep every MANE Select transcript that has
# a CDS (thickStart<thickEnd). A handful (~88) are labelled transcribed_pseudogene
# but still carry a curated ORF, so we keep them and record geneType instead of
# dropping them on the type label.
#   col4 name(ENST)  col18 geneName(ENSG)  col19 geneName2(sym)  col20 geneType
#   col22 ncbiId(RefSeq tx)  col24 ncbiProtAcc(RefSeq prot)  col25 maneStat
awk -F'\t' '$25=="MANE Select" && $7<$8' "$tmp/mane.bed" \
  > "$tmp/maneSelect.bed"

# Metadata table, stripping the version suffix from ENSG (BioMart is unversioned).
awk -F'\t' 'BEGIN{OFS="\t"}
  { ensg=$18; sub(/\.[0-9]+$/,"",ensg);
    print $4, ensg, $19, $22, $24, $1, $6, $20 }' "$tmp/maneSelect.bed" \
  | sort -k1,1 > "$outDir/maneMeta.tsv"

# genePred for just those ENSTs.
cut -f1 "$outDir/maneMeta.tsv" | sort -u > "$tmp/keep.enst"
sort -k1,1 "$tmp/mane.gp" | join -t $'\t' -1 1 -2 1 "$tmp/keep.enst" - \
  > "$outDir/maneSelect.gp"

# Translate CDS -> protein directly from the genome so the protein sequence and
# the genomic<->codon mapping come from the same model.
genePredToProt "$outDir/maneSelect.gp" "$twoBit" "$outDir/maneSelect.faa"

echo "MANE Select transcripts: $(wc -l < "$outDir/maneSelect.gp")" 1>&2
echo "protein records:         $(grep -c '^>' "$outDir/maneSelect.faa")" 1>&2
echo "distinct genes (ENSG):   $(cut -f2 "$outDir/maneMeta.tsv" | sort -u | wc -l)" 1>&2
