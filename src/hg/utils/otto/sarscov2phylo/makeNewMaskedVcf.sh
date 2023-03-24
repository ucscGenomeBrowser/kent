#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/makeNewMaskedVcf.sh

usage() {
    echo "usage: $0 prevDate today problematicSitesVcf [baseProtobuf]"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if [ $# != 3 && $# != 4 ]; then
  usage
  exit 1
fi

prevDate=$1
today=$2
problematicSitesVcf=$3
if [ $# == 4 ]; then
    baseProtobuf=$4
else
    baseProtobuf=
fi

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

mkdir -p $ottoDir/$today
cd $ottoDir/$today

# If there's a version that I didn't want to push out to the main site, but wanted to be used
# as the basis for the next day's build (for example with some extra pruning), use that:
if [ -e $ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.useMe.pb ]; then
    prevProtobufMasked=$ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.useMe.pb
else
    prevProtobufMasked=$ottoDir/$prevDate/gisaidAndPublic.$prevDate.masked.pb
fi

usherDir=~angie/github/usher
usher=$usherDir/build/usher
matUtils=$usherDir/build/matUtils

renaming=oldAndNewNames

if [ "$baseProtobuf" == "" ]; then
    baseProtobuf=$prevProtobufMasked
fi

# Make lists of sequences already in the tree.
$matUtils extract -i $baseProtobuf -u prevNames

# Before updating the tree with new sequences, update the names used in the tree:
# * Sequences that are already in the tree with EPI_ IDs, but that have been mapped to public IDs
# * COG-UK sequences that are in GenBank.  Per Sam Nicholls the isolate alone is enough to identify
#   them -- no need to match country & year which are sometimes incorrect at first.
grep COG-UK/ $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| tawk '{print $6, $4 "/" $6 "/" $3 "|" $1 "|" $3;}' \
| sed -re 's@COG-UK/@@g; s/United Kingdom://; s/(\/[0-9]{4})(-[0-9]+)*/\1/; s/ //g;' \
| sort \
    > cogUkInGenBankIsolateToNewName
fastaNames $cogUkDir/cog_all.fasta.xz > cogUk.names
grep -Fwf <(cut -f 1 cogUkInGenBankIsolateToNewName) cogUk.names \
    > cogUkInGenBank
set +o pipefail
# From 2021-05-04 on there should be no more unrenamed COG-UK/ in prevNames, but doesn't hurt
# to check.
grep COG-UK/ prevNames \
| awk -F\| '{print $1 "\t" $0;}' \
| sed -re 's@COG-UK/@@;' \
| sort \
    > cogUkInGenBankIsolateToPrevName
set -o pipefail
join -t$'\t' cogUkInGenBankIsolateToPrevName cogUkInGenBankIsolateToNewName \
| cut -f 2,3 \
    > prevTree.renaming
# Look for COG-UK isolates in prevNames that have just been added to GenBank and need to be renamed.
# Unfortunately for now we are not getting those from $epiToPublic.
grep -Fwf <(cut -f 1 cogUkInGenBankIsolateToNewName) prevNames \
| awk -F\| '$3 == ""' \
| awk -F/ '{print $2 "\t" $0;}' \
| sort \
    > cogUkInGenBankIsolateToPrevName
join -t$'\t' cogUkInGenBankIsolateToPrevName cogUkInGenBankIsolateToNewName \
| cut -f 2,3 \
    >> prevTree.renaming
# Look for names with EPI_IDs that have just today been mapped to public sequences.
grep EPI_ISL_ prevNames \
| awk -F\| '{print $2 "\t" $0;}' \
| sort \
    > epiToPrevName
set +o pipefail
grep -Fwf <(cut -f 1 epiToPrevName) $epiToPublic \
| grep -v COG-UK/ \
| tawk '{if ($4 != "" && $3 == $2) { print $1, $2 "|" $4; } else if ($4 != "") { print $1, $3 "|" $2 "|" $4; }}' \
| sort \
    > epiToNewName
set -o pipefail
# Argh, missing sequences in COG-UK metadata can mean that a sequence may have been added to the
# tree both with and without EPI ID... so renaming makes a name conflict.
# If there are any of those then prune the sequences with EPI_ID or longer dates, so renaming doesn't
# cause conflict.
comm -12 <(cut -f 2 epiToNewName | sort) <(sort prevNames) > epiToNewNameAlreadyInTree
join -t$'\t' epiToPrevName <(grep -vFwf epiToNewNameAlreadyInTree epiToNewName) \
| cut -f 2,3 \
    >> prevTree.renaming
cp /dev/null dupsToRemove
if [ -s epiToNewNameAlreadyInTree ]; then
    set +o pipefail
    grep -Fwf <(cut -d\| -f 1 epiToNewNameAlreadyInTree) prevNames \
    | grep EPI_ISL \
    | cat \
        >> dupsToRemove
    set -o pipefail
fi
# Don't rename if the final name is already in the tree; remove the dup with the old name.
set +o pipefail
cut -f 2 prevTree.renaming \
| grep -Fwf - prevNames | cat \
    > alreadyThere
grep -Fwf alreadyThere prevTree.renaming | cut -f 1 \
    >> dupsToRemove
# And finally, sometimes there are duplicates due to country or date being corrected in COG-UK
# metadata.
grep -vFwf dupsToRemove prevTree.renaming \
| cut -f 2 | sort | uniq -c | awk '$1 > 1 {print $2;}' \
| cut -d/ -f 2 \
    > cogUkIsolateStillDup
grep -Fwf cogUkIsolateStillDup $cogUkDir/cog_metadata.csv \
| awk -F, '{print $1 "|" $5;}' \
    > cogDupsLatestMetadata
grep -Fwf cogUkIsolateStillDup prevNames \
| grep -vFwf dupsToRemove \
| grep -vFwf cogDupsLatestMetadata \
| cat \
    >> dupsToRemove
set -o pipefail
startingProtobuf=$baseProtobuf
if [ -s dupsToRemove ]; then
    startingProtobuf=prevDupTrimmed.pb
    $matUtils extract -i $baseProtobuf -p -s dupsToRemove -o $startingProtobuf
fi

if [ -s prevTree.renaming ]; then
    $matUtils mask -r prevTree.renaming -i $startingProtobuf -o prevRenamed.pb >& rename.out
    $matUtils extract -i prevRenamed.pb -u prevNames
else
    ln -sf $startingProtobuf prevRenamed.pb
fi

# OK, now that the tree names are updated, figure out which seqs are already in there and
# which need to be added.
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]' > prevGbAcc
grep -E '^(England|Northern|Scotland|Wales)' prevNames \
| cut -d\| -f1 > prevCogUk
awk -F\| '{if ($3 == "") { print $1; } else { print $2; } }' prevNames \
| grep -E '^EPI_ISL_' > prevGisaid
# Add public sequences that have been mapped to GISAID sequences to prevGisaid.
grep -Fwf prevGbAcc $epiToPublic | cut -f 1 >> prevGisaid
grep -Fwf prevCogUk $epiToPublic | cut -f 1 >> prevGisaid
wc -l prev*

# Exclude some sequences based on nextclade counts of reversions and other-clade mutations.
zcat $gisaidDir/chunks/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > gisaid.dropoutContam
zcat $ncbiDir/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > gb.dropoutContam
zcat $cogUkDir/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > cog.dropoutContam
cut -f 1 *.dropoutContam \
| awk -F\| '{ if ($3 == "") { print $1; } else { print $2; } }' \
    > dropoutContam.ids
# Also exclude sequences with unbelievably low numbers of mutations given sampling dates.
zcat $gisaidDir/chunks/nextclade.full.tsv.gz | cut -f 1,10 \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $2; } else { print $2 "\t" $3; } }' \
| $scriptDir/findRefBackfill.pl > gisaid.refBackfill
zcat $ncbiDir/nextclade.full.tsv.gz | cut -f 1,10 | sort \
| join -t $'\t' <(cut -f 1,3 $ncbiDir/ncbi_dataset.plusBioSample.tsv | sort) - \
| $scriptDir/findRefBackfill.pl > gb.refBackfill
zcat $cogUkDir/nextclade.full.tsv.gz | cut -f 1,10 | sort \
| join -t $'\t' <(cut -d, -f 1,5 $cogUkDir/cog_metadata.csv | tr , $'\t' | sort) - \
| $scriptDir/findRefBackfill.pl > cog.refBackfill
cut -f 1 *.refBackfill > refBackfill.ids
sort -u ../tooManyEpps.ids ../badBranchSeed.ids dropoutContam.ids refBackfill.ids \
| grep -vFwf <(tail -n+2 $scriptDir/includeRecombinants.tsv | cut -f 1) \
    > exclude.ids

# Get new GenBank sequences with at least $minReal non-N bases.
# Exclude seqs in the tree with EPI IDs that that have been mapped in the very latest $epiToPublic.
set +o pipefail
egrep $'\t''[A-Z][A-Z][0-9]{6}\.[0-9]+' $epiToPublic \
| grep -Fwf prevGisaid - \
| grep -vFwf prevGbAcc \
| cat \
    >> prevGbAcc
set -o pipefail
xzcat $ncbiDir/genbank.fa.xz \
| faSomeRecords -exclude stdin <(cat prevGbAcc exclude.ids) newGenBank.fa
faSize -veryDetailed newGenBank.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gbTooSmall
# NCBI also includes NC_045512 in the download, but that's our reference, so... exclude that too.
set +o pipefail
fastaNames newGenBank.fa | grep NC_045512 >> gbTooSmall
set -o pipefail
faSomeRecords -exclude newGenBank.fa gbTooSmall newGenBank.filtered.fa
faSize newGenBank.filtered.fa

# Get new COG-UK sequences with at least $minReal non-N bases.
# Also exclude cog_all.fasta sequences not found in cog_metadata.csv.
comm -23 <(fastaNames $cogUkDir/cog_all.fasta.xz | sort) \
    <(cut -d, -f1 $cogUkDir/cog_metadata.csv | sort) \
    > cogFaNotMeta
# Also exclude COG-UK sequences that have been added to GenBank (cogUkInGenBank, see above).
xzcat $cogUkDir/cog_all.fasta.xz \
| faSomeRecords -exclude stdin \
    <(cat prevCogUk cogFaNotMeta cogUkInGenBank exclude.ids) newCogUk.fa
faSize -veryDetailed newCogUk.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > cogUkTooSmall
faSomeRecords -exclude newCogUk.fa cogUkTooSmall newCogUk.filtered.fa
faSize newCogUk.filtered.fa

# Get new GISAID sequences with at least $minReal non-N bases.
xzcat $gisaidDir/gisaid_fullNames_$today.fa.xz \
| sed -re 's/^>.*\|(EPI_ISL_[0-9]+)\|.*/>\1/' \
| faSomeRecords -exclude stdin <(cat prevGisaid exclude.ids) newGisaid.fa
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

# Now make a renaming that keeps all the prevNames and adds full names for the new seqs.
tawk '{print $1, $1;}' prevNames > $renaming
if [ -s newCogUk.filtered.fa ]; then
    # Sometimes all of the new COG-UK sequences are missing from cog_metadata.csv -- complained.
    set +o pipefail
    fastaNames newCogUk.filtered.fa \
    | grep -Fwf - $cogUkDir/cog_metadata.csv \
    | awk -F, '{print $1 "\t" $1 "|" $5;}' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGenBank.filtered.fa ]; then
    # Special renaming for COG-UK sequences: strip COG-UK/, add back country and year
    set +o pipefail
    fastaNames newGenBank.filtered.fa \
    | grep COG-UK/ \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{print $1, $4 "/" $6 "/" $3 "|" $1 "|" $3;}' \
    | sed -re 's@COG-UK/@@g; s/United Kingdom://; s/(\/[0-9]{4})(-[0-9]+)*/\1/; s/ //g;' \
        >> $renaming
    fastaNames newGenBank.filtered.fa \
    | grep -v COG-UK/ \
    | sed -re 's/[ |].*//' \
    | grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | tawk '{ if ($3 == "") { $3 = "?"; }
              if ($6 != "") { print $1 "\t" $6 "|" $1 "|" $3; }
              else { print $1 "\t" $1 "|" $3; } }' \
    | cleanGenbank \
    | sed -re 's/ /_/g' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGisaid.filtered.fa ]; then
    zcat $gisaidDir/metadata_batch_$today.tsv.gz \
    | grep -Fwf <(fastaNames newGisaid.filtered.fa) \
    | tawk '$3 != "" {print $3 "\t" $1 "|" $3 "|" $5;}' \
        >> $renaming
fi
wc -l $renaming

# Make masked VCF
tawk '{ if ($1 ~ /^#/) { print; } else if ($7 == "mask") { $1 = "NC_045512v2"; print; } }' \
    $problematicSitesVcf > mask.vcf
time cat <(twoBitToFa $ref2bit stdout) $alignedFa \
| faToVcf -maxDiff=200 \
    -excludeFile=exclude.ids \
    -verbose=2 stdin stdout \
| vcfRenameAndPrune stdin $renaming stdout \
| vcfFilter -excludeVcf=mask.vcf stdin \
| gzip -c \
    > new.masked.vcf.gz

