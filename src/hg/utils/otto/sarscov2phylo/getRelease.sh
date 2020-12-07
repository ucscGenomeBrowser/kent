#!/bin/bash
set -beEu -x -o pipefail

#	Do not modify this script, modify the source tree copy:
#	kent/src/hg/utils/otto/sarscov2phylo/getRelease.sh

usage() {
    echo "usage: $0 releaseLabel metadata_date.tsv.gz sequences_date.fasta.gz"
}

if [ $# != 3 ]; then
  usage
  exit 1
fi

releaseLabel=$1
nextmeta=$2
nextfasta=$3

alignmentScript=~angie/github/sarscov2phylo/scripts/global_profile_alignment.sh

# Outputs for next script (processRelease.sh):
metadata=gisaid_metadata.tsv
fasta=gisaid_sequences.fasta
alignedFasta=gisaid_aligned.fasta

# Download tree from sarscov2phylo release
rm -f ft_SH.tree
wget --quiet https://github.com/roblanf/sarscov2phylo/releases/download/$releaseLabel/ft_SH.tree

# Get IDs used in tree
sed -re 's/\)[0-9.]+:/\):/g; s/:[0-9e.:-]+//g; s/[\(\);]//g; s/,/\n/g;'" s/'//g;" ft_SH.tree \
| sort > tree.ids.sorted

# How many samples?
wc -l tree.ids.sorted

scriptDir=$(dirname "${BASH_SOURCE[0]}")
source $scriptDir/util.sh

expectedHeader="strain	virus	gisaid_epi_isl	genbank_accession	date	region	country	division	location	region_exposure	country_exposure	division_exposure	segment	length	host	age	sex	Nextstrain_clade	pangolin_lineage	GISAID_clade	originating_lab	submitting_lab	authors	url	title	paper_url	date_submitted"

# Disregard pipefail just for this pipe because head prevents the cat from completing:
set +o pipefail
header=`xcat $nextmeta | head -1`
set -o pipefail

if [ "$header" != "$expectedHeader" ]; then
    echo ""
    echo "*** ERROR: nextmeta header format changed ***"
    echo ""
    echo "Expected first line of $nextmeta to be this:"
    echo ""
    echo "$expectedHeader"
    echo ""
    echo "But got this:"
    echo "$header"
    echo ""
    exit 1
fi

xcat $nextmeta | tail -n +2 > $metadata

# Prune fasta to keep only the sequences that are in both metadata and tree.
# fasta names are strain names, tree IDs are EPI IDs; use metadata to join them up.
awk -F"\t" '{print $3 "\t" $1;}' $metadata | sort > metadata.epiToName
join -o 2.2 tree.ids.sorted metadata.epiToName > namesInTreeAndMetadata
xcat $nextfasta \
| faSomeRecords stdin namesInTreeAndMetadata $fasta

# Use Rob's script that aligns each sequence to NC_045512.2 and concatenates the results
# as a multifasta alignment (from which we can extract VCF with SNVs):
#conda install -c bioconda mafft
rm -f $alignedFasta
export TMPDIR=/dev/shm
time bash $alignmentScript \
  -i $fasta \
  -o $alignedFasta \
  -t 50
grep ^\> $alignedFasta | wc -l
