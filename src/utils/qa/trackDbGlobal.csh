#!/bin/tcsh

#######################
#
#  01-23-05
#  checks all databases on beta and compares trackDbs.
#  
#  Robert Kuhn
#
#######################

set machine="hgw1"
set today=""
set diffs=0
set outfile=""
set summaryFile=""
set mode="fast"

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  checks all databases on RR and compares trackDbs."
  echo
  echo "    usage:  RRmachine, "
  echo "            [mode] (realTime|fast)"
  echo "       - defaults to fast which uses mysql-genome instead of WGET"
  echo
  exit 1
else
  set machine=$argv[1]
endif

if ( $#argv ==  2 ) then
  set mode="$argv[2]"
  if ( $mode == "realTime" ) then
    set mode = ""
  else
    if ( $mode != "fast" ) then
      echo '\n  mode not "realTime" or "fast"\n'
      echo "${0}:"
      $0
      exit 1
    endif
  endif
endif

checkMachineName.csh $machine
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

set dbs=`getAssemblies.csh trackDb hgwbeta \
  | egrep -v "checking|get all|found|^qa_|^zoo" | egrep "."`
 
# set file paths and URLs
set today=`date +%Y-%m-%d`
# set today="2005-01-23"
set dirPath="/usr/local/apache/htdocs/qa/test-results/trackDb"
set urlPath="http://hgwdev.cse.ucsc.edu/qa/test-results/trackDb"
mkdir -p $dirPath/$today
set summaryFile=$dirPath/$today/$today.trackDb.html
set summaryUrl=$urlPath/$today/$today.trackDb.html
rm -f $summaryFile
echo "<HTML><HEAD><TITLE>trackDb Diffs $today</TITLLE></HEAD><BODY>\n<PRE> \
      \n" >! $summaryFile

echo "db      diffs"
echo "-------------"
foreach db ( $dbs )
  set todaysPath="$dirPath/$today"
  set summaryOut="$db.$machine.trackDbAll"
  set htmlOut="$db.$machine.htmlOut"
  set outfile="$todaysPath/$summaryOut"
  set htmlfile="$todaysPath/$htmlOut"
  compareTrackDbAll.csh $db hgwbeta $machine $mode >& $outfile
  if ( $status ) then
    echo "$db err" | gawk '{printf "%7s %3s", $1, $2}'
    echo 
    echo '<A HREF ="'$summaryOut'">'$db'</A> error \n\n' \
        >> $summaryFile
    # echo "${db}: error detected from compareTrackDbAll.csh"  >> $summaryFile
  else
    set diffs=`cat $outfile | egrep "Diff|The diff" | wc -l` 
    echo "$db $diffs" | gawk '{printf "%7s %2d", $1, $2}'
    echo
    if ( $diffs == 0 ) then
      echo "$db number of diffs:" >> $summaryFile
      # rm -f $outfile
    else
      echo '<A HREF ="'$summaryOut'">'$db'</A> number of diffs ' \
       '\n<A HREF ="'$htmlOut'">html</A> field' >> $summaryFile
    endif
    echo "    $diffs\n<P>" >> $summaryFile
  endif
  compareTrackDbFast.csh hgwbeta $machine $db html >& $htmlfile
end

echo "\n</PRE></BODY></HTML>" >> $summaryFile
echo $summaryUrl
exit 0

