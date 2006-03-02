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
set mode=""
set debug="true"
set debug="false"

if ($#argv != 1) then
  echo
  echo "  script to check for most /gbdb files missing after reboot."
  echo "    checks assemblies and hgFixed."
  echo "    checks for missing files on hgnfs1."
  echo "    uses existing list of /gbdb files - in dev:qa/test-results."
  echo "    ignores scaffolds, axtNetDp1, ci1, zoo, /sacCer1/sam."
  echo
  echo '      usage:  go|override'
  echo '        where "override" regenerates the file for today'
  echo
  exit
endif

set mode=$argv[1]

if ($mode != "go" && $mode != "override") then
  echo '\n  the only way to run this is to use "go" or "override" \
       on the command line.'
  echo "\n${0}:"
  $0
  exit 0
endif

set todayDate=`date +%Y%m%d`
set outpath="/usr/local/apache/htdocs/qa/test-results/gbdb"
set urlpath="http://hgwdev.cse.ucsc.edu/qa/test-results/gbdb"

# get the two files to compare or create if none for today
if ($mode == "override" ) then
  rm -f $outpath/gbdb.all.$todayDate
endif

if (! -e $outpath/gbdb.all.$todayDate) then
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
