#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/mapPublic.sh

usage() {
    echo "usage: $0 releaseLabel problematic_sites_sarsCov2.vcf epiToPublic"
}

if [ $# != 3 ]; then
  usage
  exit 1
fi

releaseLabel=$1
problematicSitesVcf=$2
epiToPublic=$3

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matToVcf=$usherDir/build/matToVcf

echo "releaseLabel=$releaseLabel problematicSitesVcf=$problematicSitesVcf epiToPublic=$epiToPublic"

# Now extract public-sequence-only subtree and VCF.  Start by making new names with shortened year:
tawk '{if ($4 == "") { print $1, $3 "|" $2;} else { print $1, $3 "|" $2 "|" $4;} }' $epiToPublic \
| sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/; s/, /_/g; s/[, ]/_/g;' \
    > epiToPublicName
wc -l epiToPublicName

join -t$'\t' tree.renaming epiToPublicName | cut -f 2,3 > treeToPublic
wc -l treeToPublic

phyloRenameAndPrune ft_SH.reroot.collapsed.nh treeToPublic ft_SH.reroot.public.nh
sed -re 's/,/,\n/g' ft_SH.reroot.public.nh | wc -l

# Run usher to collapse (having removed GISAID-only variants) and make protobuf for public set too,
# on unmasked VCF, and use matToVcf to get ambig-resolved VCF from that for hgTracks
time vcfRenameAndPrune gisaid-$releaseLabel.unfiltered.vcf.gz treeToPublic stdout \
| gzip -c \
    > public-$releaseLabel.unfiltered.vcf.gz
time $usher -c -u \
    -v public-$releaseLabel.unfiltered.vcf.gz \
    -t ft_SH.reroot.public.nh \
    -o public-$releaseLabel.notMasked.pb
time $matToVcf -i public-$releaseLabel.notMasked.pb \
    -v public-$releaseLabel.vcf
bgzip -f public-$releaseLabel.vcf
tabix -p vcf public-$releaseLabel.vcf.gz

# Run usher on masked public VCF to get protobuf for usher/hgPhyloPlace:
time vcfFilter -excludeVcf=mask.vcf public-$releaseLabel.unfiltered.vcf.gz \
| gzip -c \
    > public-$releaseLabel.unfiltered.masked.vcf.gz
time $usher -c -u \
    -v public-$releaseLabel.unfiltered.masked.vcf.gz \
    -t ft_SH.reroot.public.nh \
    -o public-$releaseLabel.masked.pb
mv uncondensed-final-tree.nh ft_SH.reroot.collapsed.public-$releaseLabel.nwk

#*** Should we get rid of ambigToN... or keep it around as an option?  Maybe inferring missing
#*** info is too ambitious/optimistic for some purposes?

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
