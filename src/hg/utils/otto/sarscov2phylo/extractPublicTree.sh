#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 date prevDate"
    echo "This assumes that \$date/gisaidAndPublic.\$date.masked.pb.gz has been generated."
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

today=$1
prevDate=$2
ottoDir=/hive/data/outside/otto/sarscov2phylo
cncbDir=$ottoDir/cncb.latest

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

usherDir=~angie/github/usher
matUtils=$usherDir/build/matUtils

cd $ottoDir/$today

# Extract public samples from tree
grep -v \|EPI_ISL_ samples.$today > newPublicNames
# Dunno why, but when I tried using -s together with the filtering params, it ran for 3 hours
# and I killed it -- stuck in a loop?  Run two commands:
$matUtils extract -i gisaidAndPublic.$today.masked.pb.gz \
    -s newPublicNames \
    -o public-$today.all.masked.preTrim.pb.gz
$matUtils extract -i public-$today.all.masked.preTrim.pb.gz \
    --max-parsimony 20 \
    --max-branch-length 60 \
    --max-path-length 225 \
    -O -o public-$today.all.masked.pb.gz

# Add nextclade annotations to protobuf (completely specified by nextstrain.clade-mutations.tsv)
grep -v \|EPI_ISL cladeToName > cladeToPublicName
time $matUtils annotate -T 50 \
    -l \
    -i public-$today.all.masked.pb.gz \
    -P $ottoDir/$prevDate/cladeToPath.public \
    -M $scriptDir/nextstrain.clade-mutations.tsv \
    -D details.nextclade.public \
    -o public-$today.all.masked.nextclade.pb.gz \
    >& annotate.nextclade.public

# Add pangolin lineage annotations to public protobuf
grep -v \|EPI_ISL lineageToName > lineageToPublicName
time $matUtils annotate -T 50 \
    -i public-$today.all.masked.nextclade.pb.gz \
    -P $ottoDir/$prevDate/lineageToPath.public \
    -M $scriptDir/pango.clade-mutations.tsv \
    -c lineageToPublicName \
    -f 0.95 \
    -D details.pango.public \
    -o public-$today.all.masked.nextclade.pangolin.pb.gz \
    >& annotate.pango.public

# Extract Newick and VCF from public-only tree
time $matUtils extract -i public-$today.all.masked.pb.gz \
    -t public-$today.all.nwk \
    -v public-$today.all.masked.vcf
time pigz -p 8 -f public-$today.all.masked.vcf
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| grep -v \|EPI_ISL_ \
| pigz -p 8 \
    > public-$today.metadata.tsv.gz

rm public-$today.all.masked.pb.gz
ln -f public-$today.all.masked.nextclade.pangolin.pb.gz public-$today.all.masked.pb.gz

# Save paths for use tomorrow.
$matUtils extract -i public-$today.all.masked.pb.gz -C clade-paths.public
tail -n+2 clade-paths.public \
| grep -E '^[12]' \
| cut -f 1,3 > cladeToPath.public
tail -n+2 clade-paths.public \
| grep -E '^[A-Za-z]' \
| cut -f 1,3 > lineageToPath.public


echo "sarscov2phylo release 13-11-20; NCBI, COG-UK and CNCB sequences downloaded $today" \
    > version.txt

$matUtils extract -i public-$today.all.masked.pb.gz -u samples.public.$today
sampleCountComma=$(echo $(wc -l < samples.public.$today) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.description.txt

# Make Taxonium V2 .jsonl.gz protobuf for display
usher_to_taxonium --input public-$today.all.masked.pb.gz \
    --metadata public-$today.metadata.tsv.gz \
    --genbank ~angie/github/taxonium/taxoniumtools/test_data/hu1.gb \
    --columns genbank_accession,country,date,pangolin_lineage,pango_lineage_usher \
    --clade_types=nextstrain,pango \
    --name_internal_nodes \
    --title "$today tree with sequences from GISAID, INSDC, COG-UK and CNCB" \
    --output public-$today.all.masked.taxonium.jsonl.gz >& utt.log

# Make a size-limited public tree for ShUShER so it doesn't exceed browser memory limits
$matUtils extract -i public-$today.all.masked.pb.gz --set-size 6000000 \
    -o public-$today.all.masked.ShUShER.pb.gz

# Link to public trees download directory hierarchy
archiveRoot=/hive/users/angie/publicTrees
read y m d < <(echo $today | sed -re 's/-/ /g')
archive=$archiveRoot/$y/$m/$d
mkdir -p $archive
gzip -c public-$today.all.nwk > $archive/public-$today.all.nwk.gz
ln -f `pwd`/public-$today.all.masked.vcf.gz $archive/
ln -f `pwd`/public-$today.all.masked.pb.gz $archive/
ln -f `pwd`/public-$today.metadata.tsv.gz $archive/
gzip -c lineageToPublicName > $archive/lineageToPublicName.tsv.gz
gzip -c cladeToPublicName > $archive/cladeToPublicName.tsv.gz
ln -f `pwd`/hgPhyloPlace.description.txt $archive/public-$today.version.txt
ln -f `pwd`/public-$today.all.masked.taxonium.jsonl.gz $archive/
ln -f `pwd`/public-$today.all.masked.ShUShER.pb.gz $archive/

# Update 'latest' in $archiveRoot
ln -f $archive/public-$today.all.nwk.gz $archiveRoot/public-latest.all.nwk.gz
ln -f $archive/public-$today.all.masked.pb.gz $archiveRoot/public-latest.all.masked.pb.gz
ln -f $archive/public-$today.all.masked.vcf.gz $archiveRoot/public-latest.all.masked.vcf.gz
ln -f $archive/public-$today.metadata.tsv.gz $archiveRoot/public-latest.metadata.tsv.gz
ln -f $archive/public-$today.version.txt $archiveRoot/public-latest.version.txt
ln -f $archive/public-$today.all.masked.taxonium.jsonl.gz \
    $archiveRoot/public-latest.all.masked.taxonium.jsonl.gz
ln -f $archive/public-$today.all.masked.ShUShER.pb.gz \
    $archiveRoot/public-latest.all.masked.ShUShER.pb.gz

# Update hgdownload-test link for archive
mkdir -p /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m
ln -sf $archive /usr/local/apache/htdocs-hgdownload/goldenPath/wuhCor1/UShER_SARS-CoV-2/$y/$m

pigz -p 8 -c samples.public.$today > samples.public.$today.gz

# Update links to latest public protobuf and metadata in /gbdb/wuhCor1/hgPhyloPlaceData/
dir=/gbdb/wuhCor1/hgPhyloPlaceData
ln -sf `pwd`/public-$today.all.masked.pb.gz $dir/public-latest.all.masked.pb.gz
ln -sf `pwd`/public-$today.metadata.tsv.gz $dir/public-latest.metadata.tsv.gz
ln -sf `pwd`/hgPhyloPlace.description.txt $dir/public-latest.version.txt
ln -sf `pwd`/samples.public.$today.gz $dir/public-latest.names.gz

# Update MSA and make tree version with only MSA sequences
awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' samples.public.$today \
| sort > idToName.public
time cat <(faSomeRecords <(xzcat $ottoDir/$prevDate/public-$prevDate.all.aligned.fa.xz) \
                         <(cut -f 1 idToName.public) stdout) \
         <(faSomeRecords new.aligned.fa <(cut -f 1 idToName.public) stdout) \
| xz -T 30 \
    > public-$today.all.aligned.fa.xz
time faRenameRecords <(xzcat public-$today.all.aligned.fa.xz) idToName.public stdout \
| faUniqify stdin stdout \
| xz -T 30 \
    > public-$today.all.msa.fa.xz
fastaNames public-$today.all.msa.fa.xz | sort > msaFaNames
wc -l msaFaNames
$matUtils extract -i public-$today.all.masked.pb.gz -s msaFaNames \
    -o public-$today.all.masked.msa.pb.gz
$matUtils extract -i public-$today.all.masked.msa.pb.gz -u samples.public.msa
cmp msaFaNames <(sort samples.public.msa)
$matUtils extract -i public-$today.all.masked.msa.pb.gz -t public-$today.all.masked.msa.nwk
pigz -f -p 8 public-$today.all.masked.msa.nwk
archiveRoot=/hive/users/angie/publicTrees
read y m d < <(echo $today | sed -re 's/-/ /g')
archive=$archiveRoot/$y/$m/$d
ln -f $(pwd)/public-$today.all.masked.msa.pb.gz $archive/
ln -f $(pwd)/public-$today.all.msa.fa.xz $archive/
ln -f $(pwd)/public-$today.all.masked.msa.nwk.gz $archive/
ln -f $(pwd)/public-$today.all.masked.msa.pb.gz $archiveRoot/public-latest.all.masked.msa.pb.gz
ln -f $(pwd)/public-$today.all.msa.fa.xz $archiveRoot/public-latest.all.msa.fa.xz
ln -f $(pwd)/public-$today.all.masked.msa.nwk.gz $archiveRoot/public-latest.masked.msa.nwk.gz
