#/bin/bash
set -euo pipefail
# script to create a crispr track for an assembly
# has only two arguments: the genome db and the gene track to use

# steps:
# - get 200bp-regions around exons
# - get the 20bp-GG sequences in them
# - take only the unique ones and split these into chunks

# path to a git clone of the crispor github repo, must be under /hive
CRISPOR=/hive/data/outside/crisprTrack/crispor

# name of parasol cluster to ssh into
CLUSTER=ku

if [ ! -f $CRISPOR/crispor.py ]; then
    echo error: cannot find $CRISPOR/crispor.py
    exit 1
fi

# path to the crispr track pipeline scripts
# look at ~/kent/src/hg/makeDb/crisprTrack/README.txt for
# more info about these scripts in there
crisprTrack=/hive/data/outside/crisprTrack/scripts

set +eu
if [[ -z "$1" || -z "$2" ]]; then
    echo to build a crispr track, you need to specify two arguments:
    echo 1 - genome db
    echo 2 - name of a gene track
    exit 1
fi
set -eu

db=$1
geneTrackName=$2

chromSizesPath=/hive/data/genomes/$db/chrom.sizes

genomeFname=$CRISPOR/genomes/$db/$db.fa.bwt

if [ ! -f $genomeFname ]; then
    echo error: $genomeFname not found - make sure that you have bwa indexed this genome in crispor
    exit 1
fi

mkdir /hive/data/genomes/$db/bed/crispr
cd /hive/data/genomes/$db/bed/crispr

echo `date` $USER >> doCrispr.log
echo gene track: $geneTrackName >> doCrispr.log

mkdir -p ranges
# select fields explicitly: some dbs have bin fields, others don't
echo getting genes
hgsql $db -NB -e 'select name,chrom,strand,txStart,txEnd,cdsStart,cdsEnd,exonCount,exonStarts,exonEnds from '$geneTrackName' where chrom not like "%_alt" and chrom not like "%hap%"; '  > ranges/genes.gp
echo Number of transcripts: >> doCrispr.log
echo `wc -l ranges/genes.gp` >> doCrispr.log

echo break genes into exons and add 200 bp on each side
genePredToBed ranges/genes.gp stdout | grep -v hap | grep -v chrUn | bedToExons stdin stdout | awk '{$2=$2-200; $3=$3+200; $6="+"; print}'| bedClip stdin /hive/data/genomes/$db/chrom.sizes stdout | grep -v _alt | grep -i -v hap > ranges/ranges.bed

echo Get sequence, removing gaps. This can take 10-15 minutes 
echo featureBits of target ranges >> doCrispr.log
featureBits $db -not gap -bed=ranges/notGap.bed
featureBits $db ranges/ranges.bed ranges/notGap.bed -faMerge -fa=ranges/ranges.fa -minSize=20 -bed=stdout | cut -f-3 2>> doCrispr.log > crisprRanges.bed 

echo split the sequence file into pieces for the cluster
mkdir -p ranges/inFa/ ranges/outGuides
# split sequences into smaller parts
faSplit sequence ranges/ranges.fa 100 ranges/inFa/

echo find all guides in the pieces
# now pull out all potential guide sequences within these ranges in .bed and .fa
# don't know how to do $var expansion in heredoc, so using simple loop, not template/gensub2
for i in ranges/inFa/*.fa; do echo python $crisprTrack/findGuides.py {check in exists inFa/`basename $i`} {check in exists $chromSizesPath} outGuides/`basename $i .fa`.bed {check out exists outGuides/`basename $i`}; done > ranges/jobList

# actually just a few minutes on the cluster
ssh $CLUSTER "cd `pwd`/ranges; para freeBatch; para make jobList"

echo concat all guides that were found
cat ranges/outGuides/*.fa | grep -v \> > allGuides.txt
cat ranges/outGuides/*.bed > allGuides.bed

echo preparing the biggest cluster job, specificity alignment:
echo filtering and splitting guide sequences into small pieces
mkdir -p specScores/jobs/inFa specScores/jobs/outGuides specScores/jobs/outOffs
# the next step will take 30 mins or so
# It will create 1 million tiny files for the cluster jobs.
# splitGuidesSpecScore.py will remove all non-unique sequences from allGuides.txt
# it will also re-format the guide sequences into the weird format required by the CRISPOR
# script: separate them by NNNN and append the artificial AGG PAM, so CRISPOR accepts 
# these sequences and finds off-targets for them. The NNNN are needed so CRISPOR won't find 
# any guides in overlaps of two guide sequences, as CRISPOR ignores all guides with Ns.
# however, CRISPOR will still find guides in all sequences that start with CC, which
# will waste a bit of CPU.
python $crisprTrack/splitGuidesSpecScore.py allGuides.txt specScores/jobs/inFa specScores/jobNames.txt

echo creating a jobList file from the pieces
# make the big jobList file, I used python, but not really necessary
python $crisprTrack/makeSpecJoblist.py specScores/jobNames.txt $CRISPOR $db specScores/jobList
ssh $CLUSTER "cd `pwd`/specScores; para freeBatch; para make jobList"

# also calc the efficiency scores
python $crisprTrack/splitGuidesEffScore.py $chromSizesPath allGuides.bed effScores/jobs effScores/jobNames.txt

# cannot get $db substitution to work with a bash heredoc
for i in `find effScores/jobs/bed -type f`; do echo /cluster/software/bin/python $crisprTrack/jobCalcEffScores.py $CRISPOR $db {check in exists jobs/bed/`basename $i`} {check out exists jobs/out/`basename $i .bed`.tab}; done > effScores/jobList

ssh $CLUSTER "cd `pwd`/effScores; para freeBatch; para make jobList"

# now concat the cluster job output back into two files
# around 10 mins each
find specScores/jobs/outGuides -type f | xargs cut -f3-6 > specScores.tab
find effScores/jobs/out/ -type f | xargs cat > effScores.tab

echo Number of guides >> doCrispr.log
echo `wc -l specScores.tab` >> doCrispr.log
echo Number of guides >> doCrispr.log
echo `wc -l specScores.tab` >> doCrispr.log

echo converting off-targets
mkdir specScores/catJobs/inFnames specScores/catJobs/out/ -p
echo specScores/jobs/outOffs/*.tab | tr ' ' '\n' | sed -e s/specScores/../g > specScores/otFnames.txt
splitFile specScores/otFnames.txt 20 specScores/catJobs/inFnames/otJob
for i in specScores/catJobs/inFnames/otJob*; do fname=`basename $i`; echo python $crisprTrack/convOffs.py $db {check in exists inFnames/$fname} {check out exists out/$fname};  done > specScores/catJobs/jobList

ssh $CLUSTER "cd `pwd`/specScores/catJobs; para freeBatch; para make jobList"

echo concating and indexing the off-targets 
$crisprTrack/catAndIndex specScores/catJobs/out crisprDetails.tab offtargets.offsets.tab --headers=_mismatchCounts,_crisprOfftargets

# create the bigBed file
# approx 30 mins on human
echo creating a bigBed file
time python $crisprTrack/createBigBed.py $db allGuides.bed specScores.tab effScores.tab offtargets.offsets.tab

# now link it into gbdb
mkdir -p /gbdb/$db/crispr/
ln -sf `pwd`/crispr.bb /gbdb/$db/crispr/crispr.bb
ln -sf `pwd`/crisprDetails.tab /gbdb/$db/crispr/crisprDetails.tab
hgBbiDbLink $db crisprTargets /gbdb/$db/crispr/crispr.bb
hgLoadBed $db crisprRanges crisprRanges.bed
echo You can add locus names to each off-target in the CRISPR hgc page by running
echo '"doLocusName"' now, if this assembly does not yet have a locusName table.
