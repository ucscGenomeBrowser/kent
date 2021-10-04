#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 date"
    echo "This assumes that \$date/gisaidAndPublic.\$date.masked.pb has been generated."
}

if [ $# != 1 ]; then
  usage
  exit 1
fi

today=$1
ottoDir=/hive/data/outside/otto/sarscov2phylo
cncbDir=$ottoDir/cncb.latest

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

cd $ottoDir/$today

# Extract public samples from tree
grep -v EPI_ISL_ samples.$today > newPublicNames
# Dunno why, but when I tried using -s together with the filtering params, it ran for 3 hours
# and I killed it -- stuck in a loop?  Run two commands:
$matUtils extract -i gisaidAndPublic.$today.masked.pb \
    -s newPublicNames \
    -o public-$today.all.masked.preTrim.pb
$matUtils extract -i public-$today.all.masked.preTrim.pb \
    --max-parsimony 20 \
    --max-branch-length 30 \
    -O -o public-$today.all.masked.pb

# Add nextclade annotations to public protobuf
if [ -s cladeToName ]; then
    # Use combined tree's clade assignments to annotate clades on public tree
    grep -v EPI_ISL cladeToName > cladeToPublicName
    time $matUtils annotate -T 50 \
        -l \
        -i public-$today.all.masked.pb \
        -c cladeToPublicName \
        -f 0.95 \
        -D details.nextclade.public \
        -o public-$today.all.masked.nextclade.pb \
        >& annotate.nextclade.public
else
    time $matUtils annotate -T 50 \
        -l \
        -i public-$today.all.masked.pb \
        -P ../nextstrain.clade-paths.public.tsv \
        -o public-$today.all.masked.nextclade.pb
fi

# Add pangolin lineage annotations to public protobuf
if [ -s lineageToName ]; then
    grep -v EPI_ISL lineageToName > lineageToPublicName
    time $matUtils annotate -T 50 \
        -i public-$today.all.masked.nextclade.pb \
        -c lineageToPublicName \
        -f 0.95 \
        -D details.pango.public \
        -o public-$today.all.masked.nextclade.pangolin.pb \
        >& annotate.pango.public
else
    time $matUtils annotate -T 50 \
        -i public-$today.all.masked.nextclade.pb \
        -P ../pango.clade-paths.public.tsv \
        -o public-$today.all.masked.nextclade.pangolin.pb
fi

# Extract Newick and VCF from public-only tree
time $matUtils extract -i public-$today.all.masked.pb \
    -t public-$today.all.nwk \
    -v public-$today.all.masked.vcf
time gzip -f public-$today.all.masked.vcf
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| grep -v EPI_ISL_ \
| gzip -c \
    > public-$today.metadata.tsv.gz

rm public-$today.all.masked.pb
ln -f public-$today.all.masked.nextclade.pangolin.pb public-$today.all.masked.pb

cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; NCBI and COG-UK sequences downloaded $today; CNCB sequences downloaded $cncbDate" \
    > version.txt

$matUtils extract -i public-$today.all.masked.pb -u samples.public.$today
sampleCountComma=$(echo $(wc -l < samples.public.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.description.txt

# Make Taxodium-formatted protobuf for display
zcat /hive/data/genomes/wuhCor1/goldenPath/bigZips/genes/ncbiGenes.gtf.gz > ncbiGenes.gtf
zcat /hive/data/genomes/wuhCor1/wuhCor1.fa.gz > wuhCor1.fa
zcat public-$today.metadata.tsv.gz > metadata.tmp.tsv
time $matUtils extract -i public-$today.all.masked.pb \
    -f wuhCor1.fa \
    -g ncbiGenes.gtf \
    -M metadata.tmp.tsv \
    --write-taxodium public-$today.all.masked.taxodium.pb
rm metadata.tmp.tsv wuhCor1.fa
gzip -f public-$today.all.masked.taxodium.pb

# Link to public trees download directory hierarchy
archiveRoot=/hive/users/angie/publicTrees
read y m d < <(echo $today | sed -re 's/-/ /g')
archive=$archiveRoot/$y/$m/$d
mkdir -p $archive
gzip -c public-$today.all.nwk > $archive/public-$today.all.nwk.gz
ln -f `pwd`/public-$today.all.masked.{pb,vcf.gz} $archive/
gzip -c public-$today.all.masked.pb > $archive/public-$today.all.masked.pb.gz
ln -f `pwd`/public-$today.metadata.tsv.gz $archive/
gzip -c public-$today.all.masked.nextclade.pangolin.pb \
    > $archive/public-$today.all.masked.nextclade.pangolin.pb.gz
if [ -s lineageToPublicName ]; then
    gzip -c lineageToPublicName > $archive/lineageToPublicName.tsv.gz
else
    gzip -c ../nextstrain.clade-paths.public.tsv > $archive/nextstrain.clade-paths.public.tsv.gz
fi
if [ -s cladeToPublicName ]; then
    gzip -c cladeToPublicName > $archive/cladeToPublicName.tsv.gz
else
    gzip -c ../pango.clade-paths.public.tsv > $archive/pango.clade-paths.public.tsv.gz
fi
ln -f `pwd`/hgPhyloPlace.description.txt $archive/public-$today.version.txt
ln -f `pwd`/public-$today.all.masked.taxodium.pb.gz $archive/

# Update 'latest' in $archiveRoot
ln -f $archive/public-$today.all.nwk.gz $archiveRoot/public-latest.all.nwk.gz
ln -f $archive/public-$today.all.masked.pb $archiveRoot/public-latest.all.masked.pb
ln -f $archive/public-$today.all.masked.pb.gz $archiveRoot/public-latest.all.masked.pb.gz
ln -f $archive/public-$today.all.masked.vcf.gz $archiveRoot/public-latest.all.masked.vcf.gz
ln -f $archive/public-$today.metadata.tsv.gz $archiveRoot/public-latest.metadata.tsv.gz
ln -f $archive/public-$today.version.txt $archiveRoot/public-latest.version.txt
ln -f $archive/public-$today.all.masked.taxodium.pb.gz \
    $archiveRoot/public-latest.all.masked.taxodium.pb.gz

# Update hgdownload-test link for archive
mkdir -p /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m
ln -sf $archive /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m

# Update links to latest public protobuf and metadata in hgwdev cgi-bin directories
for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/public-$today.all.masked.pb $dir/public-latest.all.masked.pb
    ln -sf `pwd`/public-$today.metadata.tsv.gz $dir/public-latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.description.txt $dir/public-latest.version.txt
done
