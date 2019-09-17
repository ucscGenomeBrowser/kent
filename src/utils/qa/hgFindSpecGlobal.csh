#!/bin/tcsh
source `which qaConfig.csh`

#######################
#
#  checks all databases on beta against an RR machine and compares hgFindSpecs.
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
  echo "  checks all databases on RR and compares hgFindSpecs."
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

set dbs=`getAssemblies.csh hgFindSpec hgwbeta \
  | egrep -v "checking|get all|found|^qa_|^zoo" | egrep "."`
 
# set file paths and URLs
set today=`date +%Y-%m-%d`
# set today="2005-01-23"
set dirPath="/usr/local/apache/htdocs-genecats/qa/test-results/hgFindSpec"
set urlPath="http://genecats.soe.ucsc.edu/qa/test-results/hgFindSpec"
mkdir -p $dirPath/$today
set summaryFile=$dirPath/$today/$today.hgFindSpec.html
set summaryUrl=$urlPath/$today/$today.hgFindSpec.html
rm -f $summaryFile
echo "<HTML><HEAD><TITLE>hgFindSpec Diffs $today</TITLE></HEAD><BODY>\n<PRE> \
      \n" >! $summaryFile

echo "db      diffs"
echo "-------------"
foreach db ( $dbs )
  set todaysPath="$dirPath/$today"
  set summaryOut="$db.$machine.hgFindSpec"
  set outfile="$todaysPath/$summaryOut"
  compareHgFindSpecAll.csh $db hgwbeta $machine $mode >& $outfile
  if ( $status ) then
    echo "$db err" | gawk '{printf "%7s %3s", $1, $2}'
    echo 
    echo '<A HREF ="'$summaryOut'">'$db'</A> error \n\n' \
        >> $summaryFile
    # echo "${db}: error detected from compareHgFindSpecAll.csh"  >> $summaryFile
  else
    set diffs=`cat $outfile | egrep "Diff|The diff" | wc -l` 
    echo "$db $diffs" | gawk '{printf "%7s %2d", $1, $2}'
    echo
    if ( $diffs == 0 ) then
      echo "$db number of diffs:" >> $summaryFile
      # rm -f $outfile
    else
      echo '<A HREF ="'$summaryOut'">'$db'</A> number of diffs '  >> $summaryFile
    endif
    echo "    $diffs\n<P>" >> $summaryFile
  endif
end

echo "\n</PRE></BODY></HTML>" >> $summaryFile
echo $summaryUrl
exit 0

