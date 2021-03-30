#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/updateCombinedTree.sh

usage() {
    echo "usage: $0 prevDate problematicSitesVcf"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if [ $# != 2 ]; then
  usage
  exit 1
fi

prevDate=$1
problematicSitesVcf=$2

ottoDir=/hive/data/outside/otto/sarscov2phylo
ncbiDir=$ottoDir/ncbi.latest
cogUkDir=$ottoDir/cogUk.latest
cncbDir=$ottoDir/cncb.latest
gisaidDir=/hive/users/angie/gisaid
minReal=20000
ref2bit=/hive/data/genomes/wuhCor1/wuhCor1.2bit
epiToPublic=$gisaidDir/epiToPublicAndDate.latest
scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

today=$(date +%F)
cd $ottoDir/$today

prevProtobufMasked=$ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.pb

usherDir=~angie/github/yatish_usher
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils

# Make lists of sequences already in the tree.
# matUtils creates full paths for output files, defaulting to current directory, so can't
# output to named pipe because it becomes $cwd//dev/fd/63 ...
$matUtils summary -i $prevProtobufMasked -s prevSamples
tail -n+2 prevSamples | cut -f 1 > prevNames
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]' > prevGbAcc
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^(England|Northern|Scotland|Wales)' > prevCogUk
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^EPI_ISL_' > prevGisaid
# Add public sequences that have been mapped to GISAID sequences to prevGisaid.
grep -Fwf prevGbAcc $epiToPublic | cut -f 1 >> prevGisaid
grep -Fwf prevCogUk $epiToPublic | cut -f 1 >> prevGisaid
wc -l prev*

# Get new GenBank sequences with at least $minReal non-N bases.
#*** TODO: exclude seqs already in tree in their GISAID forms
xzcat $ncbiDir/genbank.fa.xz \
| faSomeRecords -exclude stdin prevGbAcc newGenBank.fa
faSize -veryDetailed newGenBank.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gbTooSmall
# NCBI also includes NC_045512 in the download, but that's our reference, so... exclude that too.
fastaNames newGenBank.fa | grep NC_045512 >> gbTooSmall
faSomeRecords -exclude newGenBank.fa gbTooSmall newGenBank.filtered.fa
faSize newGenBank.filtered.fa

# Get new COG-UK sequences with at least $minReal non-N bases.
xzcat $cogUkDir/cog_all.fasta.xz \
| faSomeRecords -exclude stdin prevCogUk newCogUk.fa
faSize -veryDetailed newCogUk.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > cogUkTooSmall
faSomeRecords -exclude newCogUk.fa cogUkTooSmall newCogUk.filtered.fa
faSize newCogUk.filtered.fa

# Get new GISAID sequences with at least $minReal non-N bases.
xzcat $gisaidDir/gisaid_fullNames_$today.fa.xz \
| sed -re 's/^>.*\|(EPI_ISL_[0-9]+)\|.*/>\1/' \
| faSomeRecords -exclude stdin prevGisaid newGisaid.fa
faSize -veryDetailed newGisaid.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gisaidTooSmall
faSomeRecords -exclude newGisaid.fa gisaidTooSmall newGisaid.filtered.fa
faSize newGisaid.filtered.fa

# Exclude public-mapped sequences from newGisaid:
set +o pipefail
fastaNames newGisaid.filtered.fa \
| grep -Fwf - $epiToPublic \
| cut -f 1 \
    > newGisaid.public.names
set -o pipefail
if [ -s newGisaid.public.names ]; then
    faSomeRecords -exclude newGisaid.filtered.fa newGisaid.public.names tmp
    mv tmp newGisaid.filtered.fa
    faSize newGisaid.filtered.fa
fi
cat new*.filtered.fa > new.fa
faSize new.fa

# Use Rob's script that aligns each sequence to NC_045512.2 and concatenates the results
# as a multifasta alignment (from which we can extract VCF with SNVs):
#conda install -c bioconda mafft
alignedFa=new.aligned.fa
rm -f $alignedFa
export TMPDIR=/dev/shm
time bash ~angie/github/sarscov2phylo/scripts/global_profile_alignment.sh \
  -i new.fa \
  -o $alignedFa \
  -t 50
faSize $alignedFa

#*** TODO: if EPIs have recently been mapped to public sequences, we should convert their
#*** tree names from EPI to public.

# Now make a renaming that keeps all the prevNames and adds full names (with shortened dates)
# for the new seqs.
renaming=oldAndNewNames
tawk '{print $1, $1;}' prevNames > $renaming
if [ -s newCogUk.filtered.fa ]; then
    # Sometimes all of the new COG-UK sequences are missing from cog_metadata.csv -- complained.
    set +o pipefail
    fastaNames newCogUk.filtered.fa \
    | grep -Fwf - $cogUkDir/cog_metadata.csv \
    | awk -F, '{print $1 "\t" $1 "|" $5;}' \
    | sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/;' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGenBank.filtered.fa ]; then
    fastaNames newGenBank.filtered.fa \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{ if ($3 == "") { $3 = "?"; }
              if ($6 != "") { print $1 "\t" $6 "|" $1 "|" $3; }
              else { print $1 "\t" $1 "|" $3; } }' \
    | cleanGenbank \
    | sed -re 's/ /_/g' \
    | sed -re 's/20([0-9][0-9])(-[0-9-]+)?$/\1\2/;' \
        >> $renaming
fi
if [ -s newGisaid.filtered.fa ]; then
    zcat $gisaidDir/metadata_batch_$today.tsv.gz \
    | grep -Fwf <(fastaNames newGisaid.filtered.fa) \
    | tawk '{print $3 "\t" $1 "|" $3 "|" $5;}' \
        >> $renaming
fi
wc -l $renaming

# Remove old tree's GISAID items that have been mapped to public sequences so we don't have dups
set +o pipefail
cut -f 1 $epiToPublic \
| grep -Fwf - prevNames \
| cat \
    > mappedGisaidToRemove
set -o pipefail
wc -l mappedGisaidToRemove

if [ -s mappedGisaidToRemove ]; then
    $matUtils extract -i $prevProtobufMasked \
        -s mappedGisaidToRemove \
        -p \
        -o prevTrimmed.pb
else
    cp -p $prevProtobufMasked prevTrimmed.pb
fi

# Make masked VCF
tawk '{ if ($1 ~ /^#/) { print; } else if ($7 == "mask") { $1 = "NC_045512v2"; print; } }' \
    $problematicSitesVcf > mask.vcf
# Add masked VCF to previous protobuf
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| vcfFilter -excludeVcf=mask.vcf stdin \
| gzip -c \
    > new.masked.vcf.gz
time $usher -u -T 50 \
    -v new.masked.vcf.gz \
    -i prevTrimmed.pb \
    -o gisaidAndPublic.$today.masked.pb \
    >& usher.addNew.log
mv uncondensed-final-tree.nh gisaidAndPublic.$today.nwk

# Metadata for hgPhyloPlace: for now, start with already-built public metadata.
zcat public-$today.metadata.tsv.gz > gisaidAndPublic.$today.metadata.tsv
zcat $gisaidDir/metadata_batch_$today.tsv.gz \
| grep -Fwf <(cut -f 2 $renaming | grep EPI_ISL | cut -d\| -f 2) \
| tawk '{print $1 "|" $3 "|" $5, "", $5, $7, $15, $13, $14, $18, $19;}' \
    >> gisaidAndPublic.$today.metadata.tsv
wc -l gisaidAndPublic.$today.metadata.tsv
gzip gisaidAndPublic.$today.metadata.tsv

# version/description files
cncbDate=$(ls -l $cncbDir | sed -re 's/.*cncb\.([0-9]{4}-[0-9][0-9]-[0-9][0-9]).*/\1/')
echo "sarscov2phylo release 13-11-20; GISAID, NCBI and COG-UK sequences downloaded $today; CNCB sequences downloaded $cncbDate" \
    > version.plusGisaid.txt
sampleCountComma=$(echo $(wc -l < $renaming) \
                   | sed -re 's/([0-9]+)([0-9]{3})$/\1,\2/; s/([0-9]+)([0-9]{3},[0-9]{3})$/\1,\2/;')
echo "$sampleCountComma genomes from GISAID, GenBank, COG-UK and CNCB ($today); sarscov2phylo 13-11-20 tree with newer sequences added by UShER" \
    > hgPhyloPlace.plusGisaid.description.txt

# Add nextclade annotations to protobuf
zcat gisaidAndPublic.$today.metadata.tsv.gz \
| tail -n+2 | tawk '$8 != "" {print $8, $1;}' \
| sed -re 's/^20E \(EU1\)/20E.EU1/;' \
    > cladeToName
time $matUtils annotate -T 50 \
    -l \
    -i gisaidAndPublic.$today.masked.pb \
    -c cladeToName \
    -o gisaidAndPublic.$today.masked.nextclade.pb \
    >& annotate.nextclade.out

# Add pangolin lineage annotations to protobuf.  Use pangoLEARN training samples;
# convert EPI IDs to public to match tree IDs.
tail -n+2 ~angie/github/pangoLEARN/pangoLEARN/data/lineages.metadata.csv \
| awk -F, '{print $7 "\t" $2;}' \
| sort > epiExemplarToLineage
subColumn -miss=/dev/null 1 epiExemplarToLineage \
    <(cut -f 1,2 $gisaidDir/epiToPublicAndDate.$today) stdout \
| sort > idExemplarToLineage
cut -f 2 $renaming | grep -Fwf <(cut -f 1 idExemplarToLineage) \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $0 } else { print $2 "\t" $0; } }' \
| sort > idExemplarToName
join -t$'\t' idExemplarToName idExemplarToLineage \
| tawk '{print $3, $2;}' \
| sort > lineageToName
linFile=lineageToName

# Until pangoLEARN training is done, use pango-designation file for now.
#linFile=/hive/users/angie/matLineages/linToPublicPlusGisaidName.2021-03-16

#***
##***time $matUtils annotate -T 50 \
time ~angie/github/usher/build/matUtils annotate -T 50 \
    -i gisaidAndPublic.$today.masked.nextclade.pb \
    -c $linFile \
    -o gisaidAndPublic.$today.masked.nextclade.pangolin.pb \
    >& annotate.pangolin.out

mv gisaidAndPublic.$today.masked{,.unannotated}.pb
ln gisaidAndPublic.$today.masked.nextclade.pangolin.pb gisaidAndPublic.$today.masked.pb

# Extract public samples from tree
$matUtils summary -i gisaidAndPublic.$today.masked.pb -s newSamples
tail -n+2 newSamples | cut -f 1 > newNames
grep -v EPI_ISL_ newNames > newPublicNames
$matUtils extract -i gisaidAndPublic.$today.masked.pb \
    -s newPublicNames \
    -O -o public.$today.masked.pb

for dir in /usr/local/apache/cgi-bin{-angie,-beta,}/hgPhyloPlaceData/wuhCor1; do
    ln -sf `pwd`/gisaidAndPublic.$today.masked.pb $dir/public.plusGisaid.latest.masked.pb
    ln -sf `pwd`/gisaidAndPublic.$today.metadata.tsv.gz \
        $dir/public.plusGisaid.latest.metadata.tsv.gz
    ln -sf `pwd`/hgPhyloPlace.plusGisaid.description.txt $dir/public.plusGisaid.latest.version.txt
done


