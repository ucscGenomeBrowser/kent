#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  05-12-2004  Robert Kuhn
# 
#  checks all the static links in htdocs tree.
# 
###############################################

set pathfile=""
set excludeList=""
set errdirs=0
set errors=0
set outfile=`date +%Y-%m-%d`
set genecats = "/usr/local/apache/htdocs-genecats/qa/test-results/staticLinks"

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv != 1 ) then
  # wrong number of command line args
  echo
  echo "  checks all the static links in htdocs tree."
  echo "  uses directory on beta."
  echo "  excludes files listed in /cluster/bin/scripts/linkCheckExclude"
  echo "  writes local file named yyyy-mm-dd"
  echo "     then moves it to htdocs-genecats/qa/test-results/staticLinks"
  echo
  echo "    usage: `basename $0` <fileOfPaths | all>"
  echo '       "all" uses /cluster/bin/scripts/staticpaths'
  echo
  exit
endif

if ($argv[1] == "all") then
  # use default list of paths
  set pathfile="/cluster/bin/scripts/staticpaths"
else
  set pathfile=$argv[1]
  file $pathfile | grep -q "ASCII text"
  if ( $status ) then
    echo "\n file of paths $pathfile does not exist\n"
    exit 1
  endif
endif

set excludeList=/cluster/bin/scripts/linkCheckExclude
if ( $status ) then
  echo "\n exclude file does not exist\n"
  exit 1
endif

foreach dir (`cat $pathfile`)
  echo "checking $dir"
  checkStaticLinks.csh $dir $excludeList
end

# consolidate results
set dir=""
rm -f $outfile
# foreach dir ( 0 goldenPath.help test ) 
foreach dir (`cat $pathfile`)
  set dir=`echo $dir | sed "s@/@.@g"`
  if ( $dir == 0 )  then
    set dir=htdocs
  endif
  cat $dir.err                                        >> $outfile
  echo "==========================================\n" >> $outfile
  tail -20 $dir.err | grep -q "found no files with errors"
  if ( $status ) then
    @ errors = $errors + 1
  endif
  rm -f $dir.err
  @ errdirs = $errdirs + 1
end

if ( $errdirs == 1 ) then
  echo " checked $errdirs directory"                  >> $outfile
else
  echo " checked $errdirs directories"                >> $outfile
endif
  echo " found $errors with errors"                   >> $outfile

# allow two levels of backup

if ( -e $genecats/$outfile ) then
  if ( -e $genecats/${outfile}.bak ) then
    # echo "there's a bak file"
    # echo "making a bak2 file"
    mv $genecats/${outfile}.bak $genecats/${outfile}.bak2
  endif
    # echo "making a bak file"
    mv $genecats/$outfile $genecats/${outfile}.bak
endif
# echo "moving file to genecats dir"
mv $outfile $genecats
