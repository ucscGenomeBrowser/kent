#!/bin/csh -f
# split trackDb files into 3 age groups (20-49, 50-59, 60-79)

set db = $1
set tissue = $2
set sex = $3
if ($db == "" || $tissue == "" || $sex == "") then
    echo "usage: splitTracks.csh database tissue sex"
    exit 1
endif

set dir = /hive/data/outside/gtex/signal/hub
set indir = $dir/$db
set outdir = "$indir/split/"

mkdir -p $outdir
set file = trackDb.$tissue.$sex.txt
set tempfile = $outdir/$file.lines
set newfile = $outdir/$file
raToLines $indir/$file $tempfile
rm -f $tempfile.new
grep -E "age=a20 | age=a30 | age=a40" $tempfile | \
    sed -e 's/gtexRnaSignalMale/gtexRnaSignalMaleYoung/' \
        -e 's/gtexRnaSignalFemale/gtexRnaSignalFemaleYoung/' >> $tempfile.new
grep -E "age=a50" $tempfile | \
    sed -e 's/gtexRnaSignalMale/gtexRnaSignalMaleMiddle/' \
        -e 's/gtexRnaSignalFemale/gtexRnaSignalFemaleMiddle/' >> $tempfile.new
grep -E "age=a60 | age=a70" $tempfile | \
    sed -e 's/gtexRnaSignalMale/gtexRnaSignalMaleOld/' \
        -e 's/gtexRnaSignalFemale/gtexRnaSignalFemaleOld/' >> $tempfile.new
linesToRa $tempfile.new  $newfile
echo "Created $newfile. mv $newfile $indir/$file"
#rm $tempfile $tempfile.new
