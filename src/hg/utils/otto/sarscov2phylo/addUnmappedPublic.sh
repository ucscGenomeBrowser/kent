#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/addUnmappedPublic.sh

usage() {
    echo "usage: $0 releaseLabel"
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

releaseLabel=$1

echo "releaseLabel=$releaseLabel"

# Inputs from previous script (extractUnmappedPublic.sh)
alignedFa=unmapped.aligned.fasta
renaming=unmapped.renaming

ref2bit=/hive/data/genomes/wuhCor1/wuhCor1.2bit
usherDir=~angie/github/usher
usher=$usherDir/build/usher
matToVcf=$usherDir/build/matToVcf
find_parsimonious_assignments=~angie/github/strain_phylogenetics/build/find_parsimonious_assignments

scriptDir=$(dirname "${BASH_SOURCE[0]}")

source $scriptDir/util.sh

# Make VCF -- first without masking, for hgTracks
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| gzip -c \
    > unmapped.vcf.gz
# Use usher to add unmapped public sequences to tree of mapped subset and infer missing/ambig bases.
time $usher -u -T 50 \
    -v unmapped.vcf.gz \
    -i public-$releaseLabel.notMasked.pb \
    -o public-$releaseLabel.all.notMasked.pb \
    >& usher.addUnmappedPublic.log
$matToVcf -i public-$releaseLabel.all.notMasked.pb -v public-$releaseLabel.all.vcf
ls -l public-$releaseLabel.all.vcf
wc -l public-$releaseLabel.all.vcf
bgzip -f public-$releaseLabel.all.vcf
tabix -p vcf public-$releaseLabel.all.vcf.gz

# Then with masking to collapse the tree & make protobuf for usher/hgPhyloPlace:
time vcfFilter -excludeVcf=mask.vcf unmapped.vcf.gz \
| gzip -c \
    > unmapped.masked.vcf.gz
time $usher -u -T 50 \
    -v unmapped.masked.vcf.gz \
    -i public-$releaseLabel.masked.pb \
    -o public-$releaseLabel.all.masked.pb \
    >& usher.addUnmappedPublic.log
mv uncondensed-final-tree.nh public-$releaseLabel.all.nh

# Parsimony scores on collapsed tree
time $find_parsimonious_assignments --tree public-$releaseLabel.all.nh \
    --vcf <(gunzip -c public-$releaseLabel.all.vcf.gz) \
| tail -n+2 \
| sed -re 's/^[A-Z]([0-9]+)[A-Z,]+.*parsimony_score=([0-9]+).*/\1\t\2/;' \
| tawk '{print "NC_045512v2", $1-1, $1, $2;}' \
| sort -k2n,2n \
    > public-$releaseLabel.all.parsimony.bg
bedGraphToBigWig public-$releaseLabel.all.parsimony.bg /hive/data/genomes/wuhCor1/chrom.sizes \
    public-$releaseLabel.all.parsimony.bw

# Make allele-frequency-filtered versions
# Disregard pipefail just for this pipe because head prevents the cat from completing:
set +o pipefail
sampleCount=$(zcat public-$releaseLabel.all.vcf.gz | head | grep ^#CHROM | sed -re 's/\t/\n/g' \
              | tail -n+10 | wc -l)
set -o pipefail
minAc001=$(( (($sampleCount + 999) / 1000) ))
vcfFilter -minAc=$minAc001 -rename public-$releaseLabel.all.vcf.gz \
    > public-$releaseLabel.all.minAf.001.vcf
wc -l public-$releaseLabel.all.minAf.001.vcf
bgzip -f public-$releaseLabel.all.minAf.001.vcf
tabix -p vcf public-$releaseLabel.all.minAf.001.vcf.gz

minAc01=$(( (($sampleCount + 99) / 100) ))
vcfFilter -minAc=$minAc01 -rename public-$releaseLabel.all.minAf.001.vcf.gz \
    > public-$releaseLabel.all.minAf.01.vcf
wc -l public-$releaseLabel.all.minAf.01.vcf
bgzip -f public-$releaseLabel.all.minAf.01.vcf
tabix -p vcf public-$releaseLabel.all.minAf.01.vcf.gz

#*** TODO: add colors... run nextclade?  run pangolin??
#*** just use mutations in vcf and/or paths in tree??
