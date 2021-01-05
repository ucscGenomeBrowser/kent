#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/mapPublic.sh

usage() {
    echo "usage: $0 releaseLabel nextmeta problematic_sites_sarsCov2.vcf epiToPublic"
}

if [ $# != 4 ]; then
  usage
  exit 1
fi

releaseLabel=$1
nextmeta=$2
problematicSitesVcf=$3
epiToPublic=$4

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matToVcf=$usherDir/build/matToVcf
find_parsimonious_assignments=~angie/github/strain_phylogenetics/build/find_parsimonious_assignments

echo "releaseLabel=$releaseLabel problematicSitesVcf=$problematicSitesVcf epiToPublic=$epiToPublic"

# Now extract public-sequence-only subtree and VCF.  Start by making new names with shortened year:
tawk '{if ($4 == "") { print $1, $3 "|" $2;} else { print $1, $3 "|" $2 "|" $4;} }' $epiToPublic \
| sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/; s/, /_/g; s/[, ]/_/g;' \
    > epiToPublicName
wc -l epiToPublicName

join -t$'\t' epi.renaming epiToPublicName | cut -f 2,3 > treeToPublic
wc -l treeToPublic

phyloRenameAndPrune global.reroot.collapsed.nwk treeToPublic global.reroot.public.nwk
sed -re 's/,/,\n/g' global.reroot.public.nwk | wc -l

# Run usher to collapse (having removed GISAID-only variants) and make protobuf for public set too,
# on unmasked VCF, and use matToVcf to get ambig-resolved VCF from that for hgTracks
time vcfRenameAndPrune gisaid-$releaseLabel.unfiltered.vcf.gz treeToPublic stdout \
| gzip -c \
    > public-$releaseLabel.unfiltered.vcf.gz
time $usher -c -u \
    -v public-$releaseLabel.unfiltered.vcf.gz \
    -t global.reroot.public.nwk \
    -o public-$releaseLabel.notMasked.pb
time $matToVcf -i public-$releaseLabel.notMasked.pb \
    -v public-$releaseLabel.vcf
bgzip -f public-$releaseLabel.vcf
tabix -p vcf public-$releaseLabel.vcf.gz

# Run usher on masked public VCF to collapse tree and get protobuf for usher/hgPhyloPlace:
time vcfFilter -excludeVcf=mask.vcf public-$releaseLabel.unfiltered.vcf.gz \
| gzip -c \
    > public-$releaseLabel.unfiltered.masked.vcf.gz
time $usher -c -u \
    -v public-$releaseLabel.unfiltered.masked.vcf.gz \
    -t global.reroot.public.nwk \
    -o public-$releaseLabel.masked.pb
mv uncondensed-final-tree.nh global.reroot.collapsed.public-$releaseLabel.nwk

# Parsimony scores on collapsed tree
time $find_parsimonious_assignments --tree global.reroot.collapsed.public-$releaseLabel.nwk \
    --vcf <(gunzip -c public-$releaseLabel.vcf.gz) \
| tail -n+2 \
| sed -re 's/^[A-Z]([0-9]+)[A-Z,]+.*parsimony_score=([0-9]+).*/\1\t\2/;' \
| tawk '{print "NC_045512v2", $1-1, $1, $2;}' \
| sort -k2n,2n \
    > public-$releaseLabel.parsimony.bg
bedGraphToBigWig public-$releaseLabel.parsimony.bg /hive/data/genomes/wuhCor1/chrom.sizes \
    public-$releaseLabel.parsimony.bw

# Make allele-frequency-filtered versions
# Disregard pipefail just for this pipe because head prevents the cat from completing:
set +o pipefail
sampleCount=$(zcat public-$releaseLabel.vcf.gz | head | grep ^#CHROM | sed -re 's/\t/\n/g' \
              | tail -n+10 | wc -l)
set -o pipefail
minAc001=$(( (($sampleCount + 999) / 1000) ))
vcfFilter -minAc=$minAc001 -rename public-$releaseLabel.vcf.gz \
    > public-$releaseLabel.minAf.001.vcf
wc -l public-$releaseLabel.minAf.001.vcf
bgzip -f public-$releaseLabel.minAf.001.vcf
tabix -p vcf public-$releaseLabel.minAf.001.vcf.gz

minAc01=$(( (($sampleCount + 99) / 100) ))
vcfFilter -minAc=$minAc01 -rename public-$releaseLabel.minAf.001.vcf.gz \
    > public-$releaseLabel.minAf.01.vcf
wc -l public-$releaseLabel.minAf.01.vcf
bgzip -f public-$releaseLabel.minAf.01.vcf
tabix -p vcf public-$releaseLabel.minAf.01.vcf.gz

# And the lineage colors:
# TODO: add Nextstrain clade colors
for source in gisaid lineage nextstrain; do
    Source=${source^}
    zcat ${source}Colors.gz \
    | subColumn -miss=/dev/null 1 stdin treeToPublic stdout \
    | grep -v EPI_ISL > public${Source}Colors
    wc -l public${Source}Colors
    gzip -f public${Source}Colors
done

# Metadata for hgPhyloPlace:
# Header names same as nextmeta (with strain first) so hgPhyloPlace recognizes them:
echo -e "strain\tgenbank_accession\tdate\tcountry\thost\tcompleteness\tlength\tpangolin_lineage" \
    > public-$releaseLabel.metadata.tsv
# NCBI metadata (strip colon-separated location after country if present):
tawk '$8 >= 29000 { print $1, $3, $4, $5, $6, $8; }' \
    /hive/data/outside/otto/sarscov2phylo/ncbi.latest/ncbi_dataset.plusBioSample.tsv \
| sed -re 's/\t([A-Za-z -]+):[A-Za-z0-9 ,()_-]+\t/\t\1\t/;' \
| sed -re 's@SARS-Co[Vv]-2/human/@@; s@SARS-Co[Vv]-2/@@; s@hCo[Vv]-19/@@;' \
| sort > gb.metadata
cut -f 1,2 $epiToPublic | grep -E $'\t''[A-Z]{2}[0-9]{6}\.[0-9]+' > epiToGb
zcat $nextmeta | grep -Fwf <(cut -f 1 epiToGb) | cut -f 3,19 | sort > epiGbToLineage
join -t$'\t' epiToGb epiGbToLineage | cut -f 2,3 | sort > gbToLineage
join -t$'\t' -a 1 gb.metadata gbToLineage \
| tawk '{print $5, $1, $2, $3, $4, "", $6, $7;}' \
    >> public-$releaseLabel.metadata.tsv
# COG-UK metadata:
tail -n+2 /hive/data/outside/otto/sarscov2phylo/cogUk.latest/cog_metadata.csv \
| awk -F, -v 'OFS=\t' '{print $1, "", $4, $2, "", "", "", $6; }' \
    >> public-$releaseLabel.metadata.tsv
# CNCB metadata:
tail -n+2 /hive/data/outside/otto/sarscov2phylo/cncb.latest/cncb.metadata.tsv \
| tawk '$3 != "GISAID" && $3 != "GenBank" && $3 != "Genbank" { print $1, "", $10, $11, $9, $5, $6, ""; }' \
| sed -re 's@\t([A-Za-z -]+)( / [A-Za-z -'"'"']+)+\t@\t\1\t@;  s@BetaCo[Vv]/@@;' \
| sed -re 's@SARS-Co[Vv]-2/human/@@; s@SARS-Co[Vv]-2/@@; s@hCo[Vv]-19/@@;' \
    >> public-$releaseLabel.metadata.tsv
gzip -f public-$releaseLabel.metadata.tsv
