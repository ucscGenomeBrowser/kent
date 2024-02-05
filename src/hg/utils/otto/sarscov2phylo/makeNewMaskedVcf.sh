#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/makeNewMaskedVcf.sh

usage() {
    echo "usage: $0 prevDate today problematicSitesVcf [baseProtobuf]"
    echo "This assumes that ncbi.latest and cogUk.latest links/directories have been updated."
}

if (( $# != 3 && $# != 4 )); then
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

lineageProposalsRecombinants=https://raw.githubusercontent.com/sars-cov-2-variants/lineage-proposals/main/recombinants.tsv

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

# First, in order to catch & remove duplicates and update names that have changed, extract the
# accession from every name in the tree.
awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' prevNames \
| subColumn 1 -miss=/dev/null stdin <(cut -f 1,2 $epiToPublic) stdout \
| sort \
    > prevIdToName
# Arbitrarily pick one of each duplicated ID to keep in the tree
sort -k1,1 -u prevIdToName > prevIdToNameDeDup
# Remove whichever duplicated items were not arbitrarily chosen to remain
comm -23 prevIdToName prevIdToNameDeDup | cut -f 2 > prevNameToRemove
# Also remove items for which we have no metadata (i.e. they were removed from repository,
# or .1 was replaced by .2 etc.)
cut -f 1 prevIdToNameDeDup | grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]+' > gb.acc
comm -13 <(cut -f 1 $ncbiDir/ncbi_dataset.plusBioSample.tsv) gb.acc > gb.removed.acc
if [ -s gb.removed.acc ]; then
    grep -Fwf gb.removed.acc prevIdToName | cut -f 2 >> prevNameToRemove
fi
cut -f 1 prevIdToNameDeDup | grep -E '^EPI_ISL_' > epi.acc
comm -13 <(zcat $gisaidDir/metadata_batch_$today.tsv.gz | cut -f 3 | sort) epi.acc > epi.removed.acc
if [ -s epi.removed.acc ]; then
    grep -Fwf epi.removed.acc prevIdToName | cut -f 2 >> prevNameToRemove
fi
cut -f 1 prevIdToNameDeDup | grep -E '^(England|Northern|Scotland|Wales)' > cog.acc
comm -13 <(zcat $cogUkDir/cog_metadata.csv.gz | cut -d, -f 1 | sort) cog.acc > cog.removed.acc
if [ -s cog.removed.acc ]; then
    grep -Fwf cog.removed.acc prevIdToName \
    | grep -vE '^([A-Z]{2}[0-9]{6}\.[0-9]+|EPI_ISL_)' \
    | cut -f 2 >> prevNameToRemove
fi
cut -f 1 prevIdToNameDeDup \
| grep -vE '^([A-Z]{2}[0-9]{6}\.[0-9]+|EPI_ISL_|England|Northern|Scotland|Wales)' \
    > cncb.acc
comm -13 <(cut -f 2 $cncbDir/cncb.metadata.tsv | sort) cncb.acc > cncb.removed.acc
if [ -s cncb.removed.acc ]; then
    grep -Fwf cncb.removed.acc prevIdToName | cut -f 2 >> prevNameToRemove
fi

# If a sequence is in both INSDC and COG-UK, keep the INSDC and remove the COG-UK as dup.
set +o pipefail
cut -f 6 $ncbiDir/ncbi_dataset.plusBioSample.tsv \
| grep ^COG-UK/ \
| sed -re 's@COG-UK/@@' \
| grep -Fwf - prevNames \
| grep -vE '\|[A-Z]{2}[0-9]{6}\.[0-9]+\|' \
| grep -vE '\|EPI_ISL_[0-9]+\|' \
| cat \
    >> prevNameToRemove
set -o pipefail

# Remove duplicates and withdrawn sequences
if [ -s prevNameToRemove ]; then
    rm -f prevDedup.pb
    $matUtils extract -i $baseProtobuf \
        -p -s <(sort -u prevNameToRemove) \
        -u prevDedupNames \
        -o prevDedup.pb
else
    ln -sf $baseProtobuf prevDedup.pb
    $matUtils extract -i prevDedup.pb -u prevDedupNames
fi

function gbAccCogRenaming {
    # pipeline: one INSDC accession per line of stdin, acc to full name if COG-UK on stdout
    grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | grep COG-UK/ \
    | tawk '{ if ($4 != "") { print $1, $4 "/" $6 "/" $3 "|" $1 "|" $3; } else { if ($3 != "") { print $1, $6  "/" $3 "|" $1 "|" $3; } else { print $1, $6 "|" $1 "|?"; } } }' \
    | sed -re 's@COG-UK/@@g; s/United Kingdom://; s/(\/[0-9]{4})(-[0-9]+)*/\1/; s/ //g;'
}

function gbAccNonCogRenaming {
    # pipeline: one INSDC accession per line of stdin, acc to full name if non-COG-UK on stdout
    grep -Fwf - $ncbiDir/ncbi_dataset.plusBioSample.tsv \
    | grep -v COG-UK/ \
    | cleanGenbank \
    | tawk '{ if ($3 == "") { $3 = "?"; }
              if ($6 != "") { print $1 "\t" $6 "|" $1 "|" $3; }
              else { print $1 "\t" $1 "|" $3; } }' \
    | sed -re 's/ /_/g'
}

# To update names that have changed and simplify detection of new sequences to add, relate to acc.
# Strip country and year from COG-UK names to get COG acc.
awk -F\| '{ if ($3 == "") { print $1 "\t" $0; } else { print $2 "\t" $0; } }' prevDedupNames \
| subColumn 1 -miss=/dev/null stdin <(cut -f 1,2 $epiToPublic) stdout \
| sed -re 's@^(England|Northern_?Ireland|Scotland|Wales)/([A-Z]+[_-]?[A-Za-z0-9]+)/[0-9]+@COG:\2@;' \
| sort \
    > accToPrevDedupName

# Break down accs by source -- we will need those both for renaming acc to full name and for
# figuring out which seqs the tree already has vs. which are new.
cut -f 1 accToPrevDedupName | grep -E '^[A-Z]{2}[0-9]{6}\.[0-9]+' > prevGbAcc
cut -f 1 accToPrevDedupName | grep -E '^COG:' | sed -re 's/^COG://;' > prevCogUk
cut -f 1 accToPrevDedupName | grep -E '^EPI_ISL_' > prevGisaid
cut -f 1 accToPrevDedupName | grep -vE '^([A-Z]{2}[0-9]{6}\.[0-9]+|COG:|EPI_ISL_)' > prevCncb

# GenBank renaming has both COG-UK and non-COG-UK versions:
gbAccCogRenaming < prevGbAcc > accToNewName
gbAccNonCogRenaming < prevGbAcc >> accToNewName
# Restore the COG:isolate format for non-GenBank COG-UK sequences:
zcat $cogUkDir/cog_metadata.csv.gz \
| grep -Fwf prevCogUk \
| awk -F, '{print $1 "\t" $1 "|" $5;}' \
| sed -re 's@^(England|Northern_?Ireland|Scotland|Wales)/([A-Z]+[_-]?[A-Za-z0-9]+)/[0-9]+@COG:\2@;' \
    >> accToNewName
# GISAID:
zcat $gisaidDir/metadata_batch_$today.tsv.gz \
| grep -Fwf prevGisaid \
| tawk '$3 != "" {print $3 "\t" $1 "|" $3 "|" $5;}' \
    >> accToNewName
# CNCB:
grep -Fwf prevCncb $cncbDir/cncb.metadata.tsv \
| cleanCncb \
| sed -re 's/ /_/g;' \
| tawk '{print $2 "\t" $1 "|" $2 "|" $10;}' \
    >> accToNewName
join -t$'\t' accToPrevDedupName <(sort accToNewName) \
| tawk '$2 != $3 {print $2, $3;}' \
    > prevDedupNameToNewName

$matUtils mask -i prevDedup.pb -r prevDedupNameToNewName -o prevRenamed.pb \
    >& renaming.out
rm renaming.out

# OK, now that the tree names are updated, figure out which seqs are already in there and
# which need to be added.
# Add GenBank COG-UK sequences to prevCogUk so we don't add dups.
cut -f 6 $ncbiDir/ncbi_dataset.plusBioSample.tsv | grep COG-UK | sed -re 's@^COG-UK/@@;' >> prevCogUk
# Add public sequences that have been mapped to GISAID sequences to prevGisaid.
cut -f 1 $epiToPublic >> prevGisaid
wc -l prev{GbAcc,CogUk,Gisaid,Cncb}

# Exclude some sequences based on nextclade counts of reversions and other-clade mutations.
zcat $gisaidDir/chunks/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > gisaid.dropoutContam
zcat $ncbiDir/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > gb.dropoutContam
zcat $cogUkDir/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > cog.dropoutContam
zcat $cncbDir/nextclade.full.tsv.gz \
| $scriptDir/findDropoutContam.pl > cncb.dropoutContam
cut -f 1 *.dropoutContam \
| awk -F\| '{ if ($3 == "") { print $1; } else { print $2; } }' \
    > dropoutContam.ids
# Also exclude sequences with unbelievably low numbers of mutations given sampling dates.
zcat $gisaidDir/chunks/nextclade.full.tsv.gz | cut -f 2,11 \
| awk -F\| '{ if ($3 == "") { print $1 "\t" $2; } else { print $2 "\t" $3; } }' \
| $scriptDir/findRefBackfill.pl > gisaid.refBackfill
zcat $ncbiDir/nextclade.full.tsv.gz | cut -f 2,11 | sort \
| join -t $'\t' <(cut -f 1,3 $ncbiDir/ncbi_dataset.plusBioSample.tsv | sort) - \
| $scriptDir/findRefBackfill.pl > gb.refBackfill
zcat $cogUkDir/nextclade.full.tsv.gz | cut -f 2,11 | sort \
| join -t $'\t' <(zcat $cogUkDir/cog_metadata.csv.gz | cut -d, -f 1,5 | tr , $'\t' | sort) - \
| $scriptDir/findRefBackfill.pl > cog.refBackfill
zcat $cncbDir/nextclade.full.tsv.gz | cut -f 2,11 | sort \
| join -t$'\t' <(cut -f 2,10 $cncbDir/cncb.metadata.tsv | sort) - \
| $scriptDir/findRefBackfill.pl > cncb.refBackfill
cut -f 1 *.refBackfill > refBackfill.ids
curl -sS $lineageProposalsRecombinants  | tail -n+2 | cut -f 1 \
| sed -re 's@(England|Northern[ _]?Ireland|Scotland|Wales)/([A-Z0-9_-]+).*@\2@;
           s/.*(EPI_ISL_[0-9]+|[A-Z]{2}[0-9]+{6}(\.[0-9]+)?).*/\1/;' \
    > tmp
grep -Fwf tmp $epiToPublic | cut -f 2 | grep -E '^[A-Z]{2}[0-9]{6}' > tmp2
sort -u tmp tmp2 > lpRecombinantIds
rm tmp tmp2
sort -u ../tooManyEpps.ids ../badBranchSeed.ids dropoutContam.ids refBackfill.ids \
| grep -vFwf <(tail -n+2 $scriptDir/includeRecombinants.tsv | cut -f 1) \
| grep -vFwf lpRecombinantIds \
    > exclude.ids

# Get new GenBank sequences with at least $minReal non-N bases.
xzcat $ncbiDir/genbank.fa.xz \
| faSomeRecords -exclude stdin <(cat prevGbAcc exclude.ids) newGenBank.fa
faSize -veryDetailed newGenBank.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > gbTooSmall
# NCBI also includes NC_045512 in the download, but that's our reference, so... exclude that too.
echo NC_045512.2 >> gbTooSmall
faSomeRecords -exclude newGenBank.fa gbTooSmall newGenBank.filtered.fa
faSize newGenBank.filtered.fa

# Get new COG-UK sequences with at least $minReal non-N bases.
# Also exclude cog_all.fasta sequences not found in cog_metadata.csv.
fastaNames $cogUkDir/cog_all.fasta.xz \
| grep -vFwf prevCogUk \
| grep -vFwf exclude.ids \
| grep -Fwf <(zcat $cogUkDir/cog_metadata.csv.gz | cut -d, -f 1) \
    > newCogUk.accs
xzcat $cogUkDir/cog_all.fasta.xz \
| faSomeRecords stdin newCogUk.accs newCogUk.fa
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

# Get new CNCB sequences with at least $minReal non-N bases.
xzcat $cncbDir/cncb.nonGenBank.acc.fasta.xz \
| faSomeRecords -exclude stdin <(cat prevCncb exclude.ids) newCncb.fa
faSize -veryDetailed newCncb.fa \
| tawk '$4 < '$minReal' {print $1;}' \
    > cncbTooSmall
faSomeRecords -exclude newCncb.fa cncbTooSmall newCncb.filtered.fa
faSize newCncb.filtered.fa

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
  -t 30
faSize $alignedFa

# Now make a renaming that converts accessions back to full name|acc|year names.
cp /dev/null $renaming
if [ -s newCogUk.filtered.fa ]; then
    set +o pipefail
    fastaNames newCogUk.filtered.fa \
    | grep -Fwf - <(zcat $cogUkDir/cog_metadata.csv.gz) \
    | awk -F, '{print $1 "\t" $1 "|" $5;}' \
        >> $renaming
    set -o pipefail
fi
if [ -s newGenBank.filtered.fa ]; then
    # Special renaming for COG-UK sequences: strip COG-UK/, add back country and year
    set +o pipefail
    fastaNames newGenBank.filtered.fa \
    | sed -re 's/[ |].*//' \
    | gbAccCogRenaming \
        >> $renaming
    fastaNames newGenBank.filtered.fa \
    | sed -re 's/[ |].*//' \
    | gbAccNonCogRenaming \
        >> $renaming
    set -o pipefail
fi
if [ -s newGisaid.filtered.fa ]; then
    zcat $gisaidDir/metadata_batch_$today.tsv.gz \
    | grep -Fwf <(fastaNames newGisaid.filtered.fa) \
    | tawk '$3 != "" {print $3 "\t" $1 "|" $3 "|" $5;}' \
        >> $renaming
fi
if [ -s newCncb.filtered.fa ]; then
    cleanCncb < $cncbDir/cncb.metadata.tsv \
    | sed -re 's/ /_/g;' \
    | grep -Fwf <(fastaNames newCncb.filtered.fa) \
    | tawk '{print $2 "\t" $1 "|" $2 "|" $10;}' \
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

