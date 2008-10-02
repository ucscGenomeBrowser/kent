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
set dropdir=""
set dirs=""
set diffs=0
set outfile=""
set summaryFile=""
set comment=0
set archived=0
set mode="fast"

if ( $#argv < 1 || $#argv > 2 ) then
  echo
  echo "  checks all databases on RR and compares trackDbs."
  echo
  echo "    usage:  RRmachine  [realTime | fast]"
  echo "       - defaults to fast which uses genome-mysql instead of wget"
  echo "       - uses fast for settings and html fields, even in realTime"
  echo "       - overwrites file in dev/qa/test-results/trackDb "
  echo "           if used twice the same day."
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

set dbs=`getAssemblies.csh chromInfo hgwbeta \
  | egrep -v "getting|found" | egrep "."`
if ( $status ) then
  echo $dbs
  exit 1
endif
 
# set file paths and URLs
set today=`date +%Y-%m-%d`
# set today="2005-01-23"
set dirPath="/usr/local/apache/htdocs/qa/test-results/trackDb"
set urlPath="http://hgwdev.cse.ucsc.edu/qa/test-results/trackDb"

# remove dirs from same month last year
set dropdir=`getMonthLastYear.csh go`
set dirs=`ls -l $dirPath | grep $dropdir | grep ^d | awk '{print $9}'`
foreach dir ( $dirs )
  rm -r $dirPath/$dir
end

# make new dir for today's output
mkdir -p $dirPath/$today
set summaryFile=$dirPath/$today/index.html
set summaryUrl=$urlPath/$today/index.html
rm -f $summaryFile
echo "<HTML><HEAD><TITLE>trackDb Diffs $today</TITLE></HEAD><BODY>\n<PRE> \
      \n" >! $summaryFile

echo "db      diffs"
echo "-------------"
foreach db ( $dbs )
  set todaysPath="$dirPath/$today"
  set dbSummaryOut="$db.$machine.trackDbAll"
  set dbHtmlOut="$db.$machine.htmlOut"
  set outfile="$todaysPath/$dbSummaryOut"
  set htmlfile="$todaysPath/$dbHtmlOut"
  set active=`hgsql -h genome-centdb -N -e 'SELECT active FROM dbDb \
     WHERE name = "'$db'"' hgcentral`
  if ( 0 == $active ) then
    set comment="staged"
    set archived=`hgsql -h genome-centdb -N -e 'SELECT active FROM dbDbArch \
       WHERE name = "'$db'"' hgcentral`
    if ( 1 == $archived ) then
      set comment="archived"
    endif
    echo $db $comment | gawk '{printf "%7s %-10s", $1, $2}'
    echo
    echo $db $comment | gawk '{printf "%7s %-10s", $1, $2}' >> $summaryFile
    echo "\n\n\n" >> $summaryFile
    continue
  endif
  compareTrackDbAll.csh $db hgwbeta $machine $mode >& $outfile
  if ( $status ) then
    echo "$db dbDbErr" | gawk '{printf "%7s %3s", $1, $2}'
    echo 
    echo '<A HREF ="'$dbSummaryOut'">'$db'</A> dbDbErr \n\n' \
        >> $summaryFile
  else
    set diffs=`cat $outfile | egrep " Diff|The diff" | wc -l` 
    echo "$db $diffs" | gawk '{printf "%7s %2d", $1, $2}'
    echo
    if ( $diffs == 0 ) then
      echo "$db number of diffs:" >> $summaryFile
      # rm -f $outfile
    else
      echo '<A HREF ="'$dbSummaryOut'">'$db'</A> number of diffs:' \
        >> $summaryFile
      tail $outfile | grep "Differences exist in html field" > /dev/null
      if (! $status ) then
        echo '<A HREF ="'$dbHtmlOut'">html</A> field' >> $summaryFile
      endif
    endif
    echo "    $diffs\n<P>" >> $summaryFile
  endif
  # get output again for html field for display purposes
  compareTrackDbFast.csh hgwbeta $machine $db html >& $htmlfile
end

echo "\n</PRE></BODY></HTML>" >> $summaryFile
echo $summaryUrl
exit 0

