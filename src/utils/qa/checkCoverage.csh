#!/bin/tcsh

####################
#  09-20-07 Bob Kuhn
#
#  find all the places where data missing from a track.
#
####################

set db=""
set table=""
set limit=""
set chr=""
set start=""
set send=""

if ($#argv < 2 || $#argv > 3 ) then
  echo
  echo "  find all the places where data missing from a track."
  echo
  echo "      usage:  database table [chrom]"
  echo
  echo '      where "chrom" limits to single chrom'
  echo
  exit
else
  set db=$argv[1]
  set table=$argv[2]
endif

if ($#argv == 3 ) then
  set limit='-chrom='$argv[3]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

# find the correct names for starts and ends
set chr=`getChromFieldName.csh $db $table`
if ( $status ) then
  echo "\n error.  Quitting.\n"
  exit 1
endif
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
hgsql -Ne "SELECT $chr, $start, $end FROM $table" $db > blockBedFile.bed

# make negative of table (block file) as bed file
# and make negative of gap track
# then intersect them
featureBits $db blockBedFile.bed -not $limit -bed=$table.not.bed \
  >& /dev/null
featureBits $db gap    -not $limit -bed=gap.not.bed \
  >& /dev/null
featureBits $db $table.not.bed gap.not.bed $limit \
  -bed=$table.holes.bed >& /dev/null
# save coords and size and sort for biggest
cat $table.holes.bed | awk '{print $1, $2, $3, $3-$2}' | sort -k4,4nr \
  > $table.holes.sort

# report
echo
echo "non-gap holes in track are in this file: "
echo "    $table.holes.bed"
echo
echo "largest missing non-gap regions are here:"

echo
echo "chrom chromStart chromEnd size" \
  | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
echo "----- ---------- -------- ----" \
  | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
head -20 $table.holes.sort \
  | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
echo

echo
# make links for three biggest
set url1="http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=$db&position="
set url2="&$table=pack&gap=dense"
set bigThree=`head -3 $table.holes.sort \
  | awk '{print $1 ":" $2-300 "-" $3+300}'` 
echo "links to the three biggest voids in ${table}:"
echo " (with 300 bp padding on each end)"
foreach item ( $bigThree )
  echo "$url1$item$url2"
end
echo

# clean up
rm -f blockBedFile.bed
rm -f gap.not.bed
rm -f $table.not.bed

exit

