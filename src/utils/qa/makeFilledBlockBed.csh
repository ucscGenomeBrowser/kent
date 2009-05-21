#!/bin/tcsh
source `which qaConfig.csh`

####################
#  10-03-07 Bob Kuhn and Brooke Rhead
#
#  make bed file of large blocks, ignoring intron/exons
#
####################

onintr cleanup

set split=""
set db=""
set table=""
set chr=""
set start=""
set send=""

if ($#argv != 3 ) then
  echo
  echo "  make bed file of large blocks, fusing introns/exons"
  echo "  will take split tables without the chrN_ prefix."
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

# get the chrom name for this table
set split=`getSplit.csh $db $table hgwdev`
if ( $split == "unsplit" ) then
  set split=""
else
  set split=${split}_
endif
set chr=`getChromFieldName.csh $db ${split}$table`

# echo "chr = $chr"
# echo "split = $split"

# find the correct names for starts and ends
if ( $chr == "chrom" ) then
  set start=`hgsql -Ne "DESC $split$table" $db | awk '{print $1}' \
    | egrep "txStart|chromStart" | head -1 | awk '{print $1}'`
  set end=`hgsql -Ne "DESC $split$table" $db | awk '{print $1}' \
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
  else
    echo "\nThere is no chrom field called chrom , tName or genoName.\n"
    exit 1
  endif
endif

# echo $db
# echo $table
# echo $chr $start $end

# make bed file of large blocks, ignoring intron/exons
rm -f $outfile
if ( $split == "" ) then
    hgsql -Ne "SELECT $chr, $start, $end FROM $table" $db > $outfile
else
  getChromlist.csh $db > $db.chromlist$$
  foreach chrom (`cat $db.chromlist$$`)
    hgsql -Ne "SELECT $chr, $start, $end FROM ${chrom}_$table" $db >> $outfile
  end
endif

cleanup:
rm -f $db.chromlist$$
exit
