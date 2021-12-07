#!/bin/bash
# create mapping from UniProt protein sequences to genome
# input is protein sequence file and transcript table, and optional a mapping proteinId <-> transcriptId

# originally inspired/copied from Markd's script LS-SNP pipeline
# original: snpProtein/build/Makefile

# for human, this is now using RefSeq, see #27836
# The reason is that for all proteins that are broken in the reference, the positions will be wrong 
# for Gencode, but not with RefSeq (for which we have a proper PSL alignment).

# parameters:
# $1 = the fasta file with uniprot sequences
# $2 = the transcript fasta file
# $3 = the transcript->genome PSL file
# $4 = MINALI, the minimum percent ID, e.g. 0.93
# $5 = the parasol cluster
# $6 = temporary workdir
# $7 = OUTPUT: the nucleotide PSL output file to create
# $8 = optional: tsv table with mapping from uniprot to transcript

# Will always rm -rf the work directory, before and after a run

if [ "$1" == "" ]; then
        echo Please specify a uniprotToTab fasta input file and a db, a gene table, a cluster name and the output file name
        exit 0
fi

# ideally, we have a table of uniprotID <-> gene model ID, so can decide where to map protein sequences that match identically twice.
# but often we don't have known genes tables.
# to get an idea of the impact, I compared hg38 with and without known gene tables
#
# 2385 out of 38931 PSLs are multi-mapping

# most of them are mapping twice, but some 35 times
#   2 ************************************************************ 1477
#   3 ********** 238
#   4 ***** 126
#   5 **** 109
#   6 *** 84
#   7 ****** 142
#   8 *** 82
#   9  10
#  10 *** 66
#  11  1
#  12  3
#  13  6
#  14  6
#  15  6
#  16  6
#  17  7
#  18  3
#  19  3
#  20  3
# <minVal or >= 21  7

# worst ones:
#A6NER0  20      0.042%
#Q99706-6        20      0.042%
#P43629-2        20      0.042%
#P43630-2        28      0.059%
#P43630-1        28      0.059%
#Q99706-3        30      0.063%
#Q99706-4        30      0.063%
#Q99706-2        30      0.063%
#Q99706-1        30      0.063%
#Q8N743  32      0.067%

# the input fasta file has to be created by the uniprotToTab parser (originally from the publications track code)
UNIPROTFAGZ=$1
TRANSCRIPTFA=$2
TRANSCRIPTPSL=$3
MINALI=$4
CLUSTER=$5
WORKDIR=$6
OUTFNAME=$7
PAIRNAME=$8

#if [[ "$DB" == "ci3" ]]; then
   #MINALI=0.85
#fi

# if you change BLASTDIR, also must change the cluster script mapUniprot_doBlast
BLASTDIR=/cluster/bin/blast/x86_64/blast-2.2.16/bin

# stop on errors
set -e
# show commands
set -x 


#WORKDIR=makeUniProtPsl-$2-$3-$4.tmp

#rm -rf $WORKDIR

mkdir -p $WORKDIR

cp ${UNIPROTFAGZ} $WORKDIR/uniProt.fa
cp $TRANSCRIPTFA $WORKDIR/transcripts.fa
cp $TRANSCRIPTPSL $WORKDIR/transcripts.psl
if [ "$PAIRNAME" != "" ] ; then
   cp $PAIRNAME $WORKDIR/upToTrans.pairs
fi

# setup blast uniprot -> transcript cDNAs
if [ -f $WORKDIR/bestAln.psl ] ; then
        echo WARNING: re-using existing protein-transcript alignments to save time! see $WORKDIR/bestAln.psl
else
        mkdir $WORKDIR/queries 
        mkdir $WORKDIR/aligns 
        faSplit about $WORKDIR/uniProt.fa 2500 $WORKDIR/queries/
        ${BLASTDIR}/formatdb -i $WORKDIR/transcripts.fa -p F

        # create joblist and run
        set +x # quiet for now
        >$WORKDIR/jobList
        for i in $WORKDIR/queries/*.fa; do
                echo "mapUniprot_doBlast transcripts.fa queries/`basename $i` {check out exists aligns/`basename $i .fa`.psl}" >> $WORKDIR/jobList
        done; 
        set -x
        cp mapUniprot_doBlast $WORKDIR/
        ssh $CLUSTER "cd `pwd`/$WORKDIR && para make jobList"
        echo Concatenating and filtering protein/transcript alignments
        # sort and pick the best alignments for each protein
        find $WORKDIR/aligns -name '*.psl' | xargs cat | pslReps -noIntrons -nohead -nearTop=0.01 -minAli=$MINALI stdin stdout /dev/null > $WORKDIR/bestAln.psl
fi

# when we have a list of pairs to filter on, then use it
if [ -f $WORKDIR/upToTrans.pairs ]; then
    # these are very special pslSelect options:
    # - pass through all PSLs for queries that do not appear in our tables (=all Trembl and a few swissprot sequences missed by our tables)
    # - only compare the part before the dot
    # - use pslSwap as a workaround, as pslSelect doesn't have -tDelim yet and the query IDs don't have dots in them anyways
    #   too lazy to patch pslSelect now
    cat $WORKDIR/upToTrans.pairs | gawk '{OFS="\t"; print $2, $1; }' > $WORKDIR/transToUp.pairs
    pslSwap $WORKDIR/bestAln.psl stdout | pslSelect -qPass -qDelim=. -qtPairs=$WORKDIR/transToUp.pairs stdin stdout | pslSwap stdin $WORKDIR/uniProtVsTranscripts.psl
    # when we have no known gene table, blat the proteins directly and
    # take the top 1% alignments with 93% query coverage. Hopefully that gives similar results...
    # inspired by kent/src/hg/makeDb/doc/ucscGenes/hg19.ucscGenes13.csh
    # as requested by Alejo: lower to 93%, which are the pslReps defaults
else
    cp $WORKDIR/bestAln.psl $WORKDIR/uniProtVsTranscripts.psl
fi

# now combine the two alignments with pslMap
pslMap $WORKDIR/uniProtVsTranscripts.psl $WORKDIR/transcripts.psl $WORKDIR/uniProtVsGenome.psl -mapInfo=$WORKDIR/mapInfo.tab
# 2016: lowering to 95% identity due to hg38 alt loci sucking up our main (and more important) alignments from the
# 2021: using MINALI is more consistent
# normal chromosomes
sort -k10,10 $WORKDIR/uniProtVsGenome.psl | pslCDnaFilter stdin -globalNearBest=$MINALI  -bestOverlap -filterWeirdOverlapped stdout | sort | uniq > $OUTFNAME
cp $WORKDIR/mapInfo.tab ${OUTFNAME}.mapInfo
#rm -rf $WORKDIR
