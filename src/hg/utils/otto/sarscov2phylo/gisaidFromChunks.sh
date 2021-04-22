#!/bin/bash
source ~/.bashrc
set -beEu -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/gisaidFromChunks.sh

# Make nextfasta and nextmeta substitute files from chunks of downloaded GISAID sequences

lastRealNextmeta=metadata_2020-12-08_20-35.tsv.gz

today=$(date +%F)

# Run pangolin and nextclade on any chunks that need it
cd /hive/users/angie/gisaid/chunks
make

cd /hive/users/angie/gisaid
# Glom all the chunks together.
# Remove initial "hCoV-19/" and remove spaces a la nextmeta (e.g. "Hong Kong" -> "HongKong").
# Strip single quotes (e.g. "Cote d'Ivoire" --> "CotedIvoire").
# Also remove a stray comma in a name that caused Newick parsing error ("Hungary/US-32533w,/2020").
# Keep the strain|epiId|date "full names".
time xzcat chunks/gisaid_epi_isl_*.fa.xz \
| sed -re 's@^>hCo[Vv]-19/+@>@;  s/[ '"'"',()]//g;  s/\r$//;' \
| xz -T 50 \
    > gisaid_fullNames_$today.fa.xz

# Make tmp files with a fullName key and various columns that we'll join together.
fastaNames gisaid_fullNames_$today.fa.xz \
| awk -F\| -vOFS="\t" '{print $0, $1, $2, $3;}' \
| sort \
    > tmp.first3
# Sequence length
faSize -detailed  <(xzcat gisaid_fullNames_$today.fa.xz) | sort > tmp.lengths
# Lineage & clade assignments
sort chunks/pangolin.tsv \
    > tmp.lineage
sort chunks/nextclade.tsv \
    > tmp.clade
# Join locally computed fields and sort by EPI ID for joining with latest real nextmeta
join -t$'\t' -a 1 tmp.first3 tmp.lengths \
| join -t$'\t' -a 1 - tmp.clade \
| join -t$'\t' -a 1 - tmp.lineage \
| tawk '{print $3, $2, $4, $5, $6, $7;}' \
| sort \
    > tmp.epiToLocalMeta
# Join with latest real nextmeta and put locally computed fields in nextmeta column positions.
# Last real nextmeta has 27 columns.  These are the columns we can fill in:
#1       strain
#3       gisaid_epi_isl
#4       genbank_accession # fold in later, after updating mapping
#5       date
#14      length
#18      Nextstrain_clade
#19      pangolin_lineage
# Fill in other columns from nextmeta when possible (join on EPI ID since names change over time)
set +o pipefail
zcat $lastRealNextmeta | head -1 \
    > metadata_batch_$today.tsv
set -o pipefail
zcat $lastRealNextmeta \
| tail -n+2 \
| sort -k 3,3 \
| join -t$'\t' -a 1 -2 3 \
    -o 1.2,2.2,1.1,2.4,1.3,2.6,2.7,2.8,2.9,2.10,2.11,2.12,2.13,1.4,2.15,2.16,2.17,1.5,1.6,2.20,2.21,2.22,2.23,2.24,2.25,2.26,2.27 \
    tmp.epiToLocalMeta - \
| sort \
    >> metadata_batch_$today.tsv
wc -l metadata_batch_$today.tsv
gzip -f metadata_batch_$today.tsv

# Make fasta with strain-name headers a la nextfasta.
xzcat gisaid_fullNames_$today.fa.xz \
| sed -re 's/\|.*//' \
| xz -T 50 \
    > sequences_batch_$today.fa.xz

# Clean up
rm tmp.*
