#!/bin/bash
# clinvarMappedParalogPairs.sh - fetch genome-wide within-species paralog pairs from
# Ensembl BioMart (release chosen by www.ensembl.org, currently 116).
#
# A single genome-wide martservice request does not close its socket cleanly and
# curl always hits --max-time, so we cannot tell a complete payload from a
# truncated one. Instead we query one chromosome at a time (chromosome_name
# filter): each chunk completes and closes normally, and the concatenation is
# provably complete.
#
# Output columns (TSV):
#   geneId  geneSym  paralogId  paralogSym  percIdQ  percIdT
#   percIdQ = % of the query gene's protein identical to the paralog
#   percIdT = % of the paralog's protein identical to the query gene
# Usage: clinvarMappedParalogPairs.sh <outDir>
set -beEu -o pipefail

outDir="${1:?usage: clinvarMappedParalogPairs.sh <outDir>}"
out="$outDir/paralogPairs.tsv"
chunkDir="$outDir/paralogPairsChunks"
mkdir -p "$chunkDir"

chroms=$(echo {1..22} X Y MT)

fetchChrom() {
    local chrom="$1" dest="$2"
    local query
    query='<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE Query>
<Query virtualSchemaName="default" formatter="TSV" header="0" uniqueRows="1" count="" datasetConfigVersion="0.6">
  <Dataset name="hsapiens_gene_ensembl" interface="default">
    <Filter name="chromosome_name" value="'"$chrom"'"/>
    <Attribute name="ensembl_gene_id"/>
    <Attribute name="external_gene_name"/>
    <Attribute name="hsapiens_paralog_ensembl_gene"/>
    <Attribute name="hsapiens_paralog_associated_gene_name"/>
    <Attribute name="hsapiens_paralog_perc_id"/>
    <Attribute name="hsapiens_paralog_perc_id_r1"/>
  </Dataset>
</Query>'
    curl -sS --retry 3 --retry-delay 10 --max-time 600 \
        -o "$dest" --data-urlencode "query=$query" \
        'https://www.ensembl.org/biomart/martservice'
    if grep -q "Query ERROR\|Exception" "$dest"; then
        echo "ERROR: BioMart error for chrom $chrom:" 1>&2; head -3 "$dest" 1>&2; return 1
    fi
}

for chrom in $chroms; do
    dest="$chunkDir/chr${chrom}.tsv"
    echo "$(date '+%F %T') fetching chromosome $chrom ..." 1>&2
    fetchChrom "$chrom" "$dest"
    echo "  chr${chrom}: $(wc -l < "$dest") rows" 1>&2
done

# Concatenate, drop rows with no paralog (empty paralog id).
cat "$chunkDir"/chr*.tsv | awk -F'\t' 'NF>=3 && $3!=""' | sort -u > "$out"
echo "$(date '+%F %T') wrote $(wc -l < "$out") paralog pair rows to $out" 1>&2
