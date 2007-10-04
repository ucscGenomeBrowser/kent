#!/bin/tcsh

####################
#  10-03-07 Bob Kuhn and Brooke Rhead
#
#  make bed file of large blocks, ignoring intron/exons
#
####################

set db=""
set table=""
set chr=""
set start=""
set send=""

if ($#argv != 3 ) then
  echo
  echo "  make bed file of large blocks, ignoring intron/exons"
  echo
  echo "      usage:  database table outfile.bed"
  echo
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
  set outfile=$argv[3]
endif

# find the correct names for starts and ends
set chr=`getChromFieldName.csh $db $table`
if ( $chr == "chrom" ) then
  set start=`hgsql -Ne "DESC $table" $db | awk '{print $1}' \
    | egrep "txStart|chromStart" | head -1 | awk '{print $1}'`
  set end=`hgsql -Ne "DESC $table" $db | awk '{print $1}' \
    | egrep "txEnd|chromEnd" | head -1 | awk '{print $1}'`
else 
  if ( $chr == "tName" ) then
    set start="tStart"
    set end="tEnd"
  else 
    if ( $chr == "genoName" ) then
      set start="genoStart"
      set end="genoEnd"
    endif
  endif
endif

# echo $db
# echo $table
# echo $chr $start $end

# make bed file of large blocks, ignoring intron/exons
hgsql -Ne "SELECT $chr, $start, $end FROM $table" $db > $outfile

exit
