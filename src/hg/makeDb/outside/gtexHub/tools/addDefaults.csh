#!/bin/csh -ef
# Add default on subtracks to a trackDb file

set db = $1
set file = $2
if ($db == "" || $file == "") then
    echo "usage: addDefaults.csh database trackDb.*.{male,female}.txt"
    exit 1
endif

set dir = /hive/data/outside/gtex/signal/hub
set indir = $dir/$db
set outdir = "$indir/defaults/"

mkdir -p $outdir
set tempfile = $outdir/$file.lines
set newfile = $outdir/$file
raToLines $indir/$file $tempfile
rm -f $tempfile.new

sed \
-e '/GTEX-ZAB4/s/ off|/ on|/' \
-e '/GTEX-13OW6/s/ off|/ on|/' \
-e '/GTEX-13OW8/s/ off|/ on|/' \
-e '/GTEX-YFC4/s/ off|/ on|/' \
-e '/GTEX-13OVJ/s/ off|/ on|/' \
-e '/GTEX-12WSD/s/ off|/ on|/' \
$tempfile > $tempfile.new
linesToRa $tempfile.new $newfile
echo "Created $newfile. mv $newfile $file"



