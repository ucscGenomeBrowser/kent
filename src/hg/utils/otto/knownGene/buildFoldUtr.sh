#!/bin/sh -ex
cd $dir
{
# MAKE FOLDUTR TABLES 
# First set up directory structure and extract UTR sequence on hgwdev
cd $dir
rm -rf rnaStruct
mkdir -p rnaStruct

cd rnaStruct
ln -s /cluster/data/$db/$db.2bit $tempDb.2bit
mkdir -p utr3/split utr5/split utr3/fold utr5/fold
# these commands take some significant time
utrFa -nibPath=`pwd` $tempDb knownGene utr3 utr3/utr.fa
utrFa -nibPath=`pwd` $tempDb knownGene utr5 utr5/utr.fa

# Split up files and make files that define job.
faSplit sequence utr3/utr.fa 10000 utr3/split/s
faSplit sequence utr5/utr.fa 10000 utr5/split/s
ls -1 utr3/split > utr3/in.lst
ls -1 utr5/split > utr5/in.lst
cd utr3
cat << _EOF_ > template
#LOOP
rnaFoldBig split/\$(path1) fold
#ENDLOOP
_EOF_
gensub2 in.lst single template jobList
cp template ../utr5
cd ../utr5

gensub2 in.lst single template jobList

# Do cluster runs for UTRs
ssh $cpuFarm "cd $dir/rnaStruct/utr3; para make jobList"
ssh $cpuFarm "cd $dir/rnaStruct/utr5; para make jobList"

ssh $cpuFarm "cd $dir/rnaStruct/utr3; para time"
ssh $cpuFarm "cd $dir/rnaStruct/utr5; para time"

# Load database
    cd $dir/rnaStruct/utr5
    hgLoadRnaFold $tempDb foldUtr5 fold
    cd ../utr3
    hgLoadRnaFold -warnEmpty $tempDb foldUtr3 fold

# Clean up
    rm -r split fold err batch.bak
    cd ../utr5
    rm -r split fold err batch.bak
echo "BuildFoldUtr successfully finished"
} > doFoldUtr.log < /dev/null 2>&1
