#!/bin/csh -ef
# Create trackDb entry for a GTEx RNA-seq signal file

set id = $1
set run = $2
set tissue = $run
set db = $3

if ($id == "" || $db == "" || $run == "") then
    echo "usage: trackOneForAll.csh sampleId runId hg38|hg19"
    exit 1
endif
set outdir = /hive/data/outside/gtex/signal/out/$run/$db
set sampleMeta = /hive/data/outside/gtex/signal/metadata/sraToSample.tab

# NOTE: using V4 colors for now
set tissueColors = /hive/data/outside/gtex/V4/metadata/tissueColors.rgb.txt

set track = "gtexRnaSignal"
set temp = gtexSignal.$$
#set descr = `echo "select description from gtexTissue where name='$run'" | hgsql -N hgFixed`
# fails trying to open .hgsql.cnf temp file -- punting for now with temp file
echo "select description from gtexTissue where name='$tissue'" | hgsql -N hgFixed > $temp
set descr = `cat $temp`
rm $temp

# get sampleId for this file
set sample = `grep "$id" $sampleMeta | awk '{print $2}'`

# get donor metadata, using second component of sample to identify donor
set donorShort = `echo $sample | sed -e 's/GTEX-//' -e 's/-.*//'`
echo "select gender, age, deathClass from gtexDonor where name='GTEX-$donorShort'" | \
    hgsql -N hgFixed > $temp
set sex = `awk '{print $1}' $temp`
set age = `awk '{print $2}' $temp`
set hardyScore = `awk '{print $3}' $temp`
rm $temp
set sexLong = "female"
if ($sex == "M") set sexLong = "male"
set tissueShort = `echo $tissue | cut -c1-9`
set color = `grep $tissue $tissueColors | awk '{print $2}'`

# output track settings
cat << EOF
    track ${track}${id}
    parent ${track} off
    type bigWig
    bigDataUrl $run/${track}${id}.$db.bigWig
    shortLabel $tissueShort $sex $donorShort
    longLabel GTEx RNA signal from $sexLong $descr ($sample)
    subGroups sex=$sex age=a$age tissue=$tissue donor=GTEX$donorShort
    color $color
EOF

echo "select count(*) from gtexSample where sampleId='$sample'" | hgsql -N hgFixed > $temp
set sampleExists = `cat $temp`
rm $temp

if ($sampleExists == 1) then
# If sample is in sample table (apparently not all are) then use entry to create metadata line
    echo "select autolysisScore, rin, batchId, collectionSites, isolationType, isolationDate from gtexSample where sampleId='$sample'" | \
        hgsql -N hgFixed > $temp
    set autolysis = `awk '{print $1}' $temp`
    set rin = `awk '{print $2}' $temp`
    set batch = `awk '{print $3}' $temp`
    set center = `awk '{print $4}' $temp`
    set method = `awk '{print $5}' $temp`
    set date = `awk '{print $6}' $temp`
    rm $temp

    cat << EOF
    metadata donor=GTEX-$donorShort age=$age sex=$sex hardyScore=$hardyScore sample=$sample \
            tissue=$tissue autolysisScore=$autolysis rnaIntegrity=$rin batch=$batch center=$center \
            isolationMethod=$method isolationDate=$date
EOF
endif

echo ""

