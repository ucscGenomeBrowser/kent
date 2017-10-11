#!/bin/bash
# create mapping from uniProt protein positions to genome positions

# originally inspired/copied from Markd's script LS-SNP pipeline
# original: snpProtein/build/Makefile

# for hg38, this should probably use RefSeq, not Ensembl (=Gencode=knownGene)
# The reason is that for all proteins that are broken in the reference, the positions will be wrong 
# for Gencode, but not with RefSeq (for which we have a proper alignment file).
# This also means that for RefSeq, I need to find the real Refseq sequences file, not the our
# reconstructed one with getRnaPred
# pslSelect then probably has some table that it can use... or maybe use refSeq's refSeqId -> uniProt ID mapping?
# or could use UniProt's refseq mapping...

# parameters:
# $1 = the fasta file with uniprot sequences
# $2 = the UCSC database name, e.g. hg19 or hg38 or else
# $3 = the gene table
# $4 = the cluster
# $5 = the nucleotide output file

# Currently: does not work with refGene, finds different query lengths at the pslMap step, when refseq differ from faked Psls

# Will always rm -rf the work directory, before and after a run

if [ "$1" == "" ]; then
        echo Please specify a uniprotToTab fasta input file and a db, a gene table, a cluster name and the output file name
        exit 0
fi

MINALI=0.93
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
DB=$2
GENETRACK=$3
CLUSTER=$4
OUTFNAME=$5

if [[ "$DB" == "ci3" ]]; then
   MINALI=0.85
fi

# if you change BLASTDIR, also must change the cluster script mapUniprot_doBlast
BLASTDIR=/cluster/bin/blast/x86_64/blast-2.2.16/bin

# stop on errors
set -e
# show commands
set -x 


WORKDIR=makeUniProtPsl-$2-$3-$4.tmp

#rm -rf $WORKDIR

mkdir -p $WORKDIR

# get transcript sequences and a (pseudo) alignment from transcript to genome using the exon information
cp ${UNIPROTFAGZ} $WORKDIR/uniProt.fa
if [ ! -f $WORKDIR/transcripts.fa ] ; then
        hgsql -Ne 'select name, chrom, strand, txStart, txEnd, cdsStart, cdsEnd, exonCount, exonStarts, exonEnds from '$GENETRACK' where cdsStart < cdsEnd' $DB  > $WORKDIR/transcripts.gp
        genePredToFakePsl $DB $WORKDIR/transcripts.gp $WORKDIR/transcripts.psl $WORKDIR/transcripts.cds
        getRnaPred $DB $WORKDIR/transcripts.gp all $WORKDIR/transcripts.fa
fi

if [ "$GENETRACK" == "knownGene" ]; then
    # our kg system sometimes keeps the variant number, sometimes it doesn't (e.g. mm10 vs hg19)
    #hgsql -Ne 'select spID, kgID from kgXref where spID!=""' $DB | awk '{OFS="\t"; print;} /-/ {split($1,a,"-"); print a[1], $2}' > $WORKDIR/ucscUniProt.pairs
    hgsql -Ne 'select spID, kgID from kgXref where spID!=""' $DB > $WORKDIR/ucscUniProt.pairs
fi

# setup blast uniprot -> transcript cDNAs
if [ ! -d $WORKDIR/aligns ] ; then
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
fi

# sort and pick the best alignments for each protein
# when we have a knownGene table, just use our existing uniProt mapping from there
if [ -f $WORKDIR/ucscUniProt.pairs ]; then
    # this are very special pslSelect options:
    # - pass through all PSLs for queries that do not appear in our tables (=all Trembl and a few swissprot sequences missed by our tables)
    # - only compare the part before the dot
    find $WORKDIR/aligns -name '*.psl' | xargs cat | pslSelect -qPass -qDelim=. -qtPairs=$WORKDIR/ucscUniProt.pairs stdin stdout | pslReps -noIntrons -nohead -nearTop=0.01 -minAli=$MINALI stdin stdout /dev/null > $WORKDIR/uniProtVsTranscripts.psl
#when we have no known gene table, take the top 1% alignments with 93% query coverage. Hopefully that gives similar results.
else
    # inspired by kent/src/hg/makeDb/doc/ucscGenes/hg19.ucscGenes13.csh
    # as requested by Alejo: lower to 93%, which are the pslReps defaults
    find $WORKDIR/aligns -name '*.psl' | xargs cat | pslReps -noIntrons -nohead -nearTop=0.01 -minAli=$MINALI stdin stdout /dev/null > $WORKDIR/uniProtVsTranscripts.psl
fi

# now combine the two alignments with pslMap
pslMap $WORKDIR/uniProtVsTranscripts.psl $WORKDIR/transcripts.psl $WORKDIR/uniProtVsGenome.psl
# 2016: lowering to 99% identity due to hg38 alt loci sucking up our main (and more important) alignments from the
# normal chromosomes
sort -k10,10 $WORKDIR/uniProtVsGenome.psl | pslCDnaFilter stdin -globalNearBest=0.99  -bestOverlap -filterWeirdOverlapped stdout | sort | uniq > $OUTFNAME
rm -rf $WORKDIR
