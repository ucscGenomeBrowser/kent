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

if ( $#argv != 1 ) then
  echo
  echo "  checks all databases on RR and compares trackDbs."
  echo
  echo "    usage:  RRmachine."
  echo
  exit 1
else
  set machine=$argv[1]
endif

checkMachineName.csh $machine
if ( $status ) then
  echo "${0}:"
  $0
  exit 1
endif

set dbs=`getAssemblies.csh trackDb hgwbeta \
  | egrep -v "checking|get all|found|^qa_|^zoo" | egrep "."`
 
set today=`date +%Y-%m-%d`
# set today="2005-01-23"
set dirPath="/usr/local/apache/htdocs/qa/test-results/trackDb"
set urlPath="http://hgwdev.cse.ucsc.edu/qa/test-results/trackDb"
if ( ! -d $dirPath/$today ) then
  mkdir $dirPath/$today
endif

set summaryFile=$dirPath/$today/$today.trackDb.html
set summaryUrl=$urlPath/$today/$today.trackDb.html
rm -f $summaryFile
echo "<HTML><BODY>\n<PRE>\n" >> $summaryFile

foreach db ( $dbs )
  compareTrackDbAll.csh $db hgwbeta $machine \
     >& $dirPath/$today/$db.$machine.trackDbAll
  if ( $status ) then
    echo "error detected from compareTrackDbAll.csh"  >> $summaryFile
  endif
  set outfile="$dirPath/$today/$db.$machine.trackDbAll"

  set diffs=`cat $outfile | egrep "Diff|The diff" | wc -l` 
  echo "db   diffs"
  echo "$db   $diffs"
  if ( $diffs == 0 ) then
    echo "$db number of diffs:" >> $summaryFile
    # rm -f $outfile
  else
    echo '<A HREF ="'$db.$machine.trackDbAll'">'$db'</A> number of diffs:' \
      >> $summaryFile
  endif
  echo "    $diffs\n<P>" >> $summaryFile
  sleep 30
end

echo "\n</PRE></BODY></HTML>" >> $summaryFile
echo $summaryUrl
exit 1

