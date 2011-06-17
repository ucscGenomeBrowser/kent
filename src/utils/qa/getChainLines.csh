#!/bin/tcsh
source `which qaConfig.csh`


###############################################
# 
#  09-23-2009
#  Gets the two variables needed for chain blocks in trackDb.ra files.
#  Written by Ann Zweig
# 
###############################################


set db1=""
set db2=""
set out1=""
set out2=""
set chainMinScore=""
set chainLinearGap=""
set fullLine=""

if ( $#argv != 2 ) then
  echo
  echo '  Searches the README.txt files to find the correct parameters for the '
  echo '  $chainMinScore and $chainLinearGap variables. ' 
  echo
  echo '    usage:  fromDb toDb (these can be in either order)'
  echo
  exit
else
  set db1=$argv[1]
  set db2=$argv[2]
endif

if ( "$HOST" != "hgwdev" ) then
  echo "\n ERROR: you must run this script on dev!\n"
  exit 1
endif

set Db1=`echo $db1 | perl -wpe '$_ = ucfirst($_)'`
set Db2=`echo $db2 | perl -wpe '$_ = ucfirst($_)'`

set out1=`grep -s "Chain minimum score" /data/apache/htdocs-hgdownload/goldenPath/$db1/vs$Db2/README.txt`
set out2=`grep -s "Chain minimum score" /data/apache/htdocs-hgdownload/goldenPath/$db2/vs$Db1/README.txt`

if ( "$out1" != "") then
  set fullLine="$out1"
else 
  if ( "$out2" != "") then
    set fullLine="$out2"
  else
    echo ' \nERROR: cannot find either $chain variable for this pair\n'
    exit 1
  endif
endif

# strip any white space from start and end of line
set fullLine=`echo $fullLine | sed -e "s/^\s+//"`
set fullLine=`echo $fullLine | sed -e "s/\z//"`

if ( "$fullLine" != "" ) then
  # extract the chainMinScore value
  set minScore=`echo $fullLine | sed -e "s/Chain minimum score: //" \
  | awk '{print $1}' | sed -e "s/,//"`
  echo "chainMinScore $minScore"
  
  # extract the chainLinearGap method
  set linearGap=`echo $fullLine | awk '{print $9}' | sed "s/://" \
  | sed "s/(//" | sed "s/)//"`
  echo "chainLinearGap $linearGap"

exit
