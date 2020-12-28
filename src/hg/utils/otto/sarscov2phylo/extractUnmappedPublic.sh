#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/extractUnmappedPublic.sh

usage() {
    echo "usage: $0 epiToPublicNameAndDate genbank.fa cogUk.fa cncb.fa"
}

if [ $# != 4 ]; then
  usage
  exit 1
fi

epiToPublic=$1
genbankFa=$2
cogUkFa=$3
cncbFa=$4

minSize=29000

echo "epiToPublic=$epiToPublic"
echo "genbank=$genbankFa"
echo "cogUk=$cogUkFa"
echo "cncb=$cncbFa"

ottoDir=/hive/data/outside/otto/sarscov2phylo
gbToDate=$ottoDir/ncbi.latest/gbToDate
cogUkToDate=$ottoDir/cogUk.latest/cogUkToDate
cncbToDate=$ottoDir/cncb.latest/cncbToDate

# Outputs for next script (addUnmappedPublic.sh):
alignedFa=unmapped.aligned.fasta
renaming=unmapped.renaming

scriptDir=$(dirname "${BASH_SOURCE[0]}")

source $scriptDir/util.sh

# faFilter discards fasta header info after the first word, so before running it,
# make files that associate IDs and strain names.
fastaNames $genbankFa | cleanGenbank | sort > gbAccName
# COG-UK has only names, so no need to associate anything.
# CNCB puts name first, then ID, so swap the order:
fastaNames $cncbFa | cleanCncb \
| tawk '{print $2, $1;}' | sort > cncbAccName

# Filter minSize and exclude sequences that were mapped to GISAID IDs
join -t$'\t' epi.renaming $epiToPublic | cut -f 3 > mappedIds
xcat $genbankFa \
| faFilter -minSize=$minSize stdin stdout \
| faSomeRecords -exclude stdin mappedIds genbank.unmapped.fa
fastaSeqCount genbank.unmapped.fa
#*** TODO: also exclude COG-UK sequences that are in GenBank (some with incomplete names)
xcat $cogUkFa \
faFilter -minSize=$minSize stdin stdout \
| faSomeRecords -exclude stdin mappedIds cogUk.unmapped.fa
fastaSeqCount cogUk.unmapped.fa
#*** TODO: also exclude CNCB sequences that are in GenBank (some with incomplete names)
# Tweak CNCB's fasta headers around to use acc not name:
xcat $cncbFa \
| sed -re 's/^>.*\| */>/;' \
| faFilter -minSize=$minSize stdin stdout \
| faSomeRecords -exclude stdin mappedIds cncb.unmapped.fa
fastaSeqCount cncb.unmapped.fa

cat genbank.unmapped.fa cogUk.unmapped.fa cncb.unmapped.fa > unmapped.fa
fastaSeqCount unmapped.fa

# Make a file for renaming sequences from just the ID to isolateName|ID|date
join -t$'\t' <(fastaNames genbank.unmapped.fa | sort) gbAccName > gbAccNameUnmapped
join -t$'\t' <(fastaNames cncb.unmapped.fa | sort) cncbAccName > cncbAccNameUnmapped
join -t$'\t' <(sort cncbAccNameUnmapped gbAccNameUnmapped) <(sort $cncbToDate $gbToDate) \
| grep -vF NC_045512.2 \
| tawk '{ if ($2 == "") { $2 = "?";} if ($3 == "") { $3 = "?"; } print $1, $2 "|" $1 "|" $3; }' \
| sed -re 's/, /_/g; s/[, ]/_/g;' \
    > $renaming
join -t$'\t' <(fastaNames cogUk.unmapped.fa | sort) <(sort $cogUkToDate) \
| tawk '{print $1, $1 "|" $2;}' \
    >> $renaming

#*** TODO: extract submitter credits from metadata, make acknowledgements file

# Use Rob's script that aligns each sequence to NC_045512.2 and concatenates the results
# as a multifasta alignment (from which we can extract VCF with SNVs):
#conda install -c bioconda mafft
rm -f $alignedFa
export TMPDIR=/dev/shm
time bash ~angie/github/sarscov2phylo/scripts/global_profile_alignment.sh \
  -i unmapped.fa \
  -o $alignedFa \
  -t 50
faSize $alignedFa

