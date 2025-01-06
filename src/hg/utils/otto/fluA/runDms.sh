#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

# Select H5N1 HA sequences from 2020 or newer, align to Bloom Lab's Deep Mutational Scanning
# reference sequence, tally DMS scores for each sequence.

# https://www.biorxiv.org/content/10.1101/2024.05.23.595634v1.full.pdf
# reference = A/American Wigeon/South Carolina/USDA-000345-001/2021(H5N1)
# Or with its undoctored name, in GenBank:
# OQ958044.1 A/American Wigeon/South Carolina/22-000345-001/2021 (2021-12-30)
dmsRefAcc=OQ958044.1

fluADir=/hive/data/outside/otto/fluA
fluANcbiDir=$fluADir/ncbi/ncbi.latest
fluAScriptDir=$(dirname "${BASH_SOURCE[0]}")

# Look in metadata.tsv for year >= 2020 and segment == 4 (HA) and type is H5N1.
tawk '$16 == "H5N1" && ($17 == 4 || $17 == "HA") && $5 ~ /^202[0-9]/ {print $1;}' tweakedMetadata.tsv \
| faSomeRecords <(xzcat $fluANcbiDir/genbank.fa.xz) stdin stdout \
| faRenameRecords stdin renaming.tsv recentH5N1HA.fa

# Combine those with the HA segment sequences from Andersen Lab's SRA assemblies, run nextclade:
cat recentH5N1HA.fa \
    <(fastaNames $fluADir/andersen_lab.srrNotGb.renamed.fa | grep _HA/ \
      | faSomeRecords $fluADir/andersen_lab.srrNotGb.renamed.fa stdin stdout) \
| nextclade run -j 32 \
    --input-ref $fluADir/OQ958044.1.fa --input-annotation $fluADir/OQ958044.1.gff3 \
    --output-columns-selection seqName,clade,totalSubstitutions,totalDeletions,totalInsertions,totalMissing,totalNonACGTNs,alignmentStart,alignmentEnd,substitutions,deletions,insertions,aaSubstitutions,aaDeletions,aaInsertions,missing,unknownAaRanges,nonACGTNs \
    --output-tsv recentH5N1HA.nextclade.tsv \
    >& tmp.log

$fluAScriptDir/dms_annotate_wigeon.py \
    ~/github/Flu_H5_American-Wigeon_South-Carolina_2021-H5N1_DMS/results/summaries/all_sera_escape.csv \
    recentH5N1HA.nextclade.tsv \
    H5N1_HA_DMS_metadata.tsv \
    H5N1_HA_DMS_colorings.json

