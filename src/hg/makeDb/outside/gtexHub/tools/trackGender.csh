#!/bin/csh -ef
# Create trackDb files in <db> directory for a GTEx RNA-seq signal run
# Output files to hub directory trackDb.run.{male|female}.ra

set db = $1
set run = $2
set gender = $3
if ($db == "" || $run == "" || $gender == "") then
    echo "usage: trackGender.csh database runId male|female"
    exit 1
endif
set tissue = $run

if ($gender == "male" && ($tissue == "ovary" || $tissue == "fallopianTube" || $tissue == "uterus" \
        || $tissue == "vagina" || $tissue == "encodocervix" || $tissue == "ectocervix")) then
    echo "$tissue is not a male tissue"
    exit 1
endif
if ($gender == "female" && ($tissue == "testis" || $tissue == "prostate")) then
    echo "$tissue is not a female tissue"
    exit 1
endif

set bindir = /hive/data/outside/gtex/signal/bin
set outdir = /hive/data/outside/gtex/signal/out/$run
set hubdir = /hive/data/outside/gtex/signal/hub
set sampleFile = /hive/data/outside/gtex/signal/metadata/sraToSample.txt
#debug
#set hubdir = .
#mkdir $hubdir/$db
echo "Creating trackDb.$run.$gender.txt files in $outdir/$db"

set genderUp = `echo $gender | sed 's/./\U&/'`

set temp = gtexTrack.$$
echo "select description from gtexTissue where name='$tissue'" | hgsql -N hgFixed > $temp
set desc = `cat $temp`
rm $temp

# create trackDb file
set outfile = $hubdir/$db/trackDb.$run.$gender.txt
if (-e $outfile) then
    echo "Found file $outfile, aborting"
    exit
endif
#add subtracks
set files = `ls $outdir/$db/*.bigWig`
foreach file ($files)
    set id = $file:t:r:r
    echo "select description from gtexTissue where name='$tissue'" | hgsql -N hgFixed > $temp
    set sampleGender = `grep $id $sampleFile | awk '{print $3}'`
    if ($sampleGender == $gender) then
    csh $bindir/trackOneForAll.csh $id $run $db | \
        sed -e 's/subGroups sex=. /subGroups /' \
                -e "s/parent gtexRnaSignal/parent gtexRnaSignal$genderUp/" >> $outfile
    endif
end
rm $temp

echo "include trackDb.$run.$gender.txt" >> $hubdir/$db/trackDb.$gender.txt

echo "DONE"



