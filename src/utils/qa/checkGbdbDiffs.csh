#!/bin/tcsh

####################
#  06-20-05 Bob Kuhn
#
#  Script to check /gbdb files that might have been lost after reboot.
#
####################

set filePath=""
set db=""
set errFlag=0
set debug="true"
set debug="false"

if ($#argv != 1) then
  echo
  echo "  script to check for /gbdb files missing after reboot."
  echo "    checks for missing files on hgnfs1."
  echo "    uses existing list of /gbdb files - in dev:qa/test-results."
  echo "    ignores scaffolds description genbank axtNetDp1 ci1 zoo."
  echo "    ignores /sacCer1/sam."
  echo
  echo '      usage:  go'
  echo
  exit
endif

set go=$argv[1]

if ($go != "go") then
  echo 'the only way to run this is to use "go" on the command line.'
  exit 0
endif

set todayDate=`date +%Y%m%d`
set outpath="/usr/local/apache/htdocs/qa/test-results/gbdb"
set urlpath="http://hgwdev.cse.ucsc.edu//qa/test-results/gbdb"
set output=$outpath/$todayDate.gbdb.diff

rm -f $output

# get the two files to compare or create if none for today
if (! -e $outpath/gbdb.all.$todayDate ) then
  getGbdbBeta.csh all > /dev/null
endif

set twoFiles=`ls -ltr $outpath | grep all | tail -2 | awk '{print $NF}'`
set file1=`echo $twoFiles | awk '{print $1}'`
set file2=`echo $twoFiles | awk '{print $2}'`
echo "files: $file1 $file2\n" > $outpath/gbdbDiff.$todayDate
diff $outpath/$file1 $outpath/$file2 >> $outpath/gbdbDiff.$todayDate
if ( $status ) then
  set lost=`grep "<" $outpath/gbdbDiff.$todayDate | wc -l`
  set  new=`grep ">" $outpath/gbdbDiff.$todayDate | wc -l`
  echo
  echo "  $lost files lost"
  echo "  $new new files"
  echo " see: $urlpath/gbdbDiff.$todayDate"
  echo
else
  rm -f $outpath/gbdbDiff.$todayDate
  echo "\n  files match: $file1 $file2\n"
endif

exit
