#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Select PB2 sequences  <??constraints??> , align to Bloom Lab's Deep Mutational Scanning
# reference sequence, tally DMS scores for each sequence.

# https://elifesciences.org/articles/45079
# reference = A/Green-winged Teal/Ohio/175/1986
# In GenBank:
# CY018884.1  A/green-winged teal/Ohio/175/1986
dmsRefAcc=CY018884.1

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest
fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

threads=32

# Look in metadata.tsv  segment == 1 (PB2):
tawk '$17 == 1 || $17 == "PB2" {print $1;}' tweakedMetadata.tsv \
| faSomeRecords <(xzcat $fluANcbiDir/genbank.fa.xz) stdin stdout \
| faRenameRecords stdin renaming.tsv PB2.fa

# Combine those with the HA segment sequences from Andersen Lab's SRA assemblies, run nextclade:
cat PB2.fa \
    <(fastaNames $fluADir/andersen_lab.srrNotGb.renamed.fa | grep _PB2/ \
      | faSomeRecords $fluADir/andersen_lab.srrNotGb.renamed.fa stdin stdout) \
| nextclade run -j $threads \
    --input-ref $fluADir/$dmsRefAcc.fa --input-annotation $fluADir/$dmsRefAcc.gff3 \
    --output-columns-selection seqName,clade,totalSubstitutions,totalDeletions,totalInsertions,totalMissing,totalNonACGTNs,alignmentStart,alignmentEnd,substitutions,deletions,insertions,aaSubstitutions,aaDeletions,aaInsertions,missing,unknownAaRanges,nonACGTNs \
    --output-tsv PB2.nextclade.tsv \
    >& tmp.log

$fluAScriptDir/dms_annotate_pb2.py \
    ~/github/PB2-DMS/results/diffsel/summary_prefs_effects_diffsel.csv \
    PB2.nextclade.tsv \
    PB2_DMS_metadata.tsv \
    PB2_DMS_colorings.json
