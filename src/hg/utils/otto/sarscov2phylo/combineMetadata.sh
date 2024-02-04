#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 prevDate today"
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

prevDate=$1
today=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo
ncbiDir=$ottoDir/ncbi.latest
cogUkDir=$ottoDir/cogUk.latest
cncbDir=$ottoDir/cncb.latest
gisaidDir=/hive/users/angie/gisaid
minReal=20000
scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

mkdir -p $ottoDir/$today
cd $ottoDir/$today

prevMeta=$ottoDir/$prevDate/public-$prevDate.metadata.tsv.gz

# Metadata for hgPhyloPlace:
# Header names same as nextmeta (with strain first) so hgPhyloPlace recognizes them:
echo -e "strain\tgenbank_accession\tdate\tcountry\thost\tcompleteness\tlength\tNextstrain_clade\tpangolin_lineage" \
    > gisaidAndPublic.$today.metadata.tsv
# It's not always possible to recreate both old and new names correctly from metadata,
# so make a file to translate accession or COG-UK to the name used in VCF, tree and protobufs.
awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' samples.$today \
| sort \
    > idToName
# NCBI metadata for COG-UK: strip COG-UK/ & United Kingdom:, add country & year to name
grep COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '$8 >= '$minReal' {print $1, $3, $4, $5, $4 "/" $6 "/" $3 "|" $1 "|" $3, $8;}' \
| sed -re 's@COG-UK/@@g; s/United Kingdom://g;  s/(\/[0-9]{4})(-[0-9]+)*/\1/;
           s@Northern Ireland/@NorthernIreland/@;' \
    > tmp
# NCBI metadata for non-COG-UK (strip colon-separated location after country if present):
grep -v COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '$8 >= '$minReal' { print $1, $3, $4, $5, $6, $8; }' \
| sed -re 's@\t([A-Za-z -]+):[^\t]+\t@\t\1\t@;' \
| perl -wpe '@w = split("\t"); $w[4] =~ s/ /_/g; $_ = join("\t", @w);' \
| cleanGenbank \
| sort tmp - > gb.metadata
if [ -e $ncbiDir/lineage_report.csv ]; then
    echo Getting GenBank Pangolin lineages from $ncbiDir/lineage_report.csv
    tail -n+2  $ncbiDir/lineage_report.csv \
    | sed -re 's/^([A-Z][A-Z][0-9]{6}\.[0-9]+)[^,]*/\1/;' \
    | awk -F, '$2 != "" && $2 != "None" {print $1 "\t" $2;}' \
    | sort -u \
        > gbToLineage
else
    echo Getting GenBank Pangolin lineages from $prevMeta
    zcat $prevMeta \
    | tail -n+2 \
    | tawk '$2 != "" && $8 != "" { print $2, $8; }' \
    | sort -u \
        > gbToLineage
fi
wc -l gbToLineage
if [ -e $ncbiDir/nextclade.full.tsv.gz ]; then
    zcat $ncbiDir/nextclade.full.tsv.gz | cut -f 2,8 | sed -re 's/"//g;' | sort -u > gbToNextclade
else
    touch gbToNextclade
fi
wc -l gbToNextclade
join -t$'\t' -a 1 gb.metadata gbToNextclade \
| join -t$'\t' -a 1 - gbToLineage \
| tawk '{ if ($2 == "") { $2 = "?"; }
          print $1, $1, $2, $3, $4, "", $6, $7, $8; }' \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
| uniq \
    >> gisaidAndPublic.$today.metadata.tsv
# COG-UK metadata:
if [ -e $cogUkDir/nextclade.full.tsv.gz ]; then
    zcat $cogUkDir/nextclade.full.tsv.gz | cut -f 2,8 | sed -re 's/"//g' | sort -u > cogUkToNextclade
else
    touch cogUkToNextclade
fi
#*** Could also add sequence length to metadata from faSizes output...
zcat $cogUkDir/cog_metadata.csv.gz \
| tail -n+2 \
| awk -F, -v 'OFS=\t' '{print $1, "", $5, $3, "", "", "", $7; }' \
| sed -re 's/UK-ENG/England/; s/UK-NIR/Northern Ireland/; s/UK-SCT/Scotland/; s/UK-WLS/Wales/;' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2,1.8 - cogUkToNextclade \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> gisaidAndPublic.$today.metadata.tsv
# CNCB metadata:
tail -n+2 $cncbDir/cncb.metadata.tsv \
| tawk '{ if ($3 != "GISAID" && $3 != "GenBank" && $3 != "Genbank") {
            print $2, "", $10, $11, $9, $5, $6} }' \
| sed -re 's@\t([A-Za-z -]+)( / [A-Za-z -'"'"']+)+\t@\t\1\t@;  s/Sapiens/sapiens/;' \
| sort \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,2.2 - $cncbDir/nextclade.tsv \
| join -t$'\t' -a 1 -o 1.1,1.2,1.3,1.4,1.5,1.6,1.7,1.8,2.2 - $cncbDir/pangolin.tsv \
| join -t$'\t' -o 1.2,2.2,2.3,2.4,2.5,2.6,2.7,2.8,2.9 idToName - \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
zcat $gisaidDir/metadata_batch_$today.tsv.gz \
| grep -Fwf <(grep EPI_ISL samples.$today | cut -d\| -f 2) \
| tawk '{print $1 "|" $3 "|" $5, "", $5, $7, $15, $13, $14, $18, $19;}' \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
pigz -p 8 -f gisaidAndPublic.$today.metadata.tsv
