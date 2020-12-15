#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/processGisaid.sh

usage() {
    echo "usage: $0 treeDate treeDir msaFile nextmeta problematic_sites_sarsCov2.vcf"
}

if [ $# != 5 ]; then
  usage
  exit 1
fi

releaseLabel=$1
treeDir=$2
msaFile=$3
nextmeta=$4
problematicSitesVcf=$5

echo "releaseLabel=$releaseLabel treeDir=$treeDir msaFile=$msaFile nextmeta=$nextmeta problematicSitesVcf=$problematicSitesVcf"

scriptDir=$(dirname "${BASH_SOURCE[0]}")

# UShER (and matToVcf)
usherDir=~angie/github/usher
usher=$usherDir/build/usher
matToVcf=$usherDir/build/matToVcf

# strain_phylogenetics / find_parsimonious_assignments for parsimony scores
find_parsimonious_assignments=~angie/github/strain_phylogenetics/build/find_parsimonious_assignments

metadata=$treeDir/metadata.csv
treeFile=$treeDir/global.tree

sed -re 's/,/,\n/g' $treeFile | sed -re 's/:.*//; s/[()]//g;' | sort > tree.ids.sorted
grep ^\> $msaFile | sed -re 's/^>//' | sort > msa.ids.sorted

treeNotMsa=$(comm -23 tree.ids.sorted msa.ids.sorted | wc -l)
if (( $treeNotMsa > 0 )); then
    echo ""
    echo "Error: $treeFile has $treeNotMsa sequences that are not in $msaFile."
    echo "Try using an msa download from a later date."
    exit
fi

# Make renaming files for tree (EPI_ IDs) and VCF (isolate names): add strain name and
# date with shortened year.
tail -n+2 $metadata \
| csvToTab \
| awk -F"\t" '{print $1 "\t" $2 "|" $1 "|" $3;}' \
| sed -re 's@hCo[Vv]-19/@@;  s/\|20/\|/;  s/ //g;  s/,//g;' \
| sort > epi.renaming
wc -l epi.renaming

# Rename tree IDs from just EPI IDs to strain|epi|date ids:
phyloRenameAndPrune $treeFile epi.renaming global.renamed.nwk
sed -re 's/,/,\n/g' global.renamed.nwk | sed -re 's/:.*//; s/[()]//g;' \
| sort > tree.renamed.ids.sorted
wc -l tree.renamed.ids.sorted

# Set the root of the tree to the Wuhan/Hu-1 (NC_045512.2) reference not WIV04:
~angie/github/newick_utils/src/nw_reroot global.renamed.nwk \
    'Wuhan/Hu-1/2019|EPI_ISL_402125|19-12-31' > global.reroot.nwk

# Run faToVcf without any filtering, masking or conversion of ambiguous alleles:
time faToVcf $msaFile stdout -includeRef \
  -ref=EPI_ISL_402125 \
  -vcfChrom=NC_045512v2 -verbose=2 \
| vcfRenameAndPrune stdin epi.renaming gisaid-$releaseLabel.unfiltered.vcf
ls -l gisaid-$releaseLabel.unfiltered.vcf
wc -l gisaid-$releaseLabel.unfiltered.vcf
gzip -f gisaid-$releaseLabel.unfiltered.vcf

# Run usher and matToVcf to make ambig-resolved VCF for hgTracks
time $usher -c \
  --vcf gisaid-$releaseLabel.unfiltered.vcf.gz \
  --tree global.reroot.nwk \
  --save-mutation-annotated-tree sarscov2phylo-$releaseLabel.notMasked.pb \
  --write-uncondensed-final-tree
mv uncondensed-final-tree.nh global.reroot.collapsed.notMasked.nwk
$matToVcf -i sarscov2phylo-$releaseLabel.notMasked.pb \
    -v gisaid-$releaseLabel.vcf
bgzip -f gisaid-$releaseLabel.vcf
tabix -p vcf gisaid-$releaseLabel.vcf.gz

# Remove problematic sites recommended for masking for usher/phyloPlace
tawk '{ if ($1 ~ /^#/) { print; } else if ($7 == "mask") { $1 = "NC_045512v2"; print; } }' \
    $problematicSitesVcf > mask.vcf
time vcfFilter -excludeVcf=mask.vcf gisaid-$releaseLabel.unfiltered.vcf.gz \
| gzip -c \
    > gisaid-$releaseLabel.unfiltered.masked.vcf.gz

# Full find_parsimonious_assignments output on uncollapsed tree for collaborators
time $find_parsimonious_assignments --tree global.reroot.nwk \
    --vcf gisaid-$releaseLabel.unfiltered.masked.vcf.gz \
| gzip -c \
    > find_parsimonious_assignments.$releaseLabel.out.gz

# Run usher to collapse tree and make masked protobuf for hgPhyloPlace
time $usher -c \
  --vcf gisaid-$releaseLabel.unfiltered.masked.vcf.gz \
  --tree global.reroot.nwk \
  --save-mutation-annotated-tree sarscov2phylo-$releaseLabel.masked.pb \
  --write-uncondensed-final-tree
mv uncondensed-final-tree.nh global.reroot.collapsed.nwk

# Parsimony scores on collapsed tree
time $find_parsimonious_assignments --tree global.reroot.collapsed.nwk \
    --vcf <(gunzip -c gisaid-$releaseLabel.vcf.gz) \
| tail -n+2 \
| sed -re 's/^[A-Z]([0-9]+)[A-Z,]+.*parsimony_score=([0-9]+).*/\1\t\2/;' \
| tawk '{print "NC_045512v2", $1-1, $1, $2;}' \
| sort -k2n,2n \
    > gisaid-$releaseLabel.parsimony.bg
bedGraphToBigWig gisaid-$releaseLabel.parsimony.bg /hive/data/genomes/wuhCor1/chrom.sizes \
    gisaid-$releaseLabel.parsimony.bw

# Full find_parsimonious_assignments output on collapsed tree for collaborators
time $find_parsimonious_assignments --tree global.reroot.collapsed.nwk \
    --vcf gisaid-$releaseLabel.unfiltered.masked.vcf.gz \
    > find_parsimonious_assignments.$releaseLabel.collapsed.out

# Now filter by frequency for faster track display: alt al freq >= 0.001
sampleCount=$(wc -l < tree.renamed.ids.sorted)
minAc001=$(( (($sampleCount + 999) / 1000) ))
time vcfFilter -minAc=$minAc001 -rename gisaid-$releaseLabel.vcf.gz \
    > gisaid-$releaseLabel.minAf.001.vcf
ls -l gisaid-$releaseLabel.minAf.001.vcf
wc -l gisaid-$releaseLabel.minAf.001.vcf
bgzip -f gisaid-$releaseLabel.minAf.001.vcf
tabix -p vcf gisaid-$releaseLabel.minAf.001.vcf.gz

# Alt al freq >= 0.01
minAc01=$(( (($sampleCount + 99) / 100) ))
time vcfFilter -minAc=$minAc01 -rename gisaid-$releaseLabel.minAf.001.vcf.gz \
    > gisaid-$releaseLabel.minAf.01.vcf
ls -l gisaid-$releaseLabel.minAf.01.vcf
wc -l gisaid-$releaseLabel.minAf.01.vcf
bgzip -f gisaid-$releaseLabel.minAf.01.vcf
tabix -p vcf gisaid-$releaseLabel.minAf.01.vcf.gz

# Make sample color files
gunzip -c $nextmeta | cut -f3,18-20 \
| sort > metadata.epiToLineageAndClades
# Join on EPI ID to associate tree sample names with lineages.
join -t$'\t' epi.renaming metadata.epiToLineageAndClades -o 1.2,2.2,2.3,2.4 > sampleToLineage
$scriptDir/cladeLineageColors.pl sampleToLineage
gzip -f *Colors
