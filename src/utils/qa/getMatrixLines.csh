#!/bin/tcsh
source `which qaConfig.csh`


###############################################
# 
#  10-23-2007
#  Gets the two lines needed for chain blocks in trackDb.ra files.
#  Written by Ann Zweig
# 
###############################################


set db1=""
set db2=""
set out1=""
set out2=""
set matrix=""

if ( $#argv != 2 ) then
  echo
  echo '  Searches the README.txt files to find the correct parameters for the '
  echo '  $matrix variable.  This is the q-parameter from the blastz run. '
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

set out1=`grep -sA7 "The blastz scoring" /data/apache/htdocs-hgdownload/goldenPath/$db1/vs$Db2/README.txt \
  | tail -6`
set out2=`grep -sA7 "The blastz scoring" /data/apache/htdocs-hgdownload/goldenPath/$db2/vs$Db1/README.txt \
  | tail -6`

if ( "$out1" != "") then
  set matrix="$out1"
else 
  if ( "$out2" != "") then
    set matrix="$out2"
  else
    echo ' \nERROR: cannot find a $matrix variable for this pair\n'
    exit 1
  endif
endif

# strip any white space from start and end of line
set matrix=`echo $matrix | sed -e "s/^\s+//"`
set matrix=`echo $matrix | sed -e "s/\z//"`

if ( "$matrix" != "" ) then
  # line1 is for the 'matrixHeader' line
  
  # remove everything after the "A C G T"
  set line1=`echo $matrix | sed -e "s/.A.*//"`
  
  # Add the word 'matrixHeader' to the start of the line, and commas after that
  set line1=`echo $line1 | sed -e "s/ /, /g" | sed -e "s/^/matrixHeader /"`
  echo "\n$line1"

  # line2 is for the 'matrix 16' line
  # remove the "A C G T A"
  set line2=`echo $matrix | sed -e "s/A C G T A //"`

  # remove all of the interspersed letters
  set line2=`echo $line2 | sed -e "s/[CGT]//g"`

  # add the words 'matrix 16' to the start of the line, and commas after that
  set line2=`echo $line2 | sed -e "s/ /,/g" | sed -e "s/^/matrix 16 /"`
  echo "$line2\n"
endif
exit
