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
set whereLimit=""
set split=""
set chr=""
set chromList=""
set start=""
set end=""

if ( $#argv < 2 || $#argv > 3 ) then
  echo
  echo "  find all non-gap places where data missing from a track."
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

if ( $#argv == 3 ) then
  # limit to a chrom
  set limit='-chrom='$argv[3]
  set whereLimit='WHERE chrom = "'$argv[3]'"'
  set chromList=${argv[3]}_
else
  set chromList=`hgsql -Ne "SELECT chrom FROM chromInfo" $db | sed -r "s/(.*)/\1_/"`
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set split=`getSplit.csh $db $table`
if ( $status ) then
  exit 1
endif
if ( $split != "unsplit" ) then
  set split=${split}_
else 
  set split=""
endif

# echo "split /${split}/"
# echo "splittable $split$table"

# find the correct names for starts and ends
set chr=`getChromFieldName.csh $db $split$table`
if ( $status ) then
  echo "\n error.  not a positional table?  quitting.\n"
  exit 1
endif

# echo "chr $chr"
# echo "split /${split}/"
# echo "split_table ${split}$table"


if ( $chr == "chrom" ) then
  set start=`hgsql -Ne "DESC $split$table" $db | awk '{print $1}' \
    | egrep "txStart|chromStart"`
  set end=`hgsql -Ne "DESC $split$table" $db | awk '{print $1}' \
    | egrep "txEnd|chromEnd"`
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

# make simple BED3 file from track with introns filled in
rm -f blockBedFile.bed
if ( $split != "" ) then
  foreach chrom ( $chromList )
    hgsql -Ne "SELECT $chr, $start, $end FROM $chrom$table" $db >> blockBedFile.bed
  end
else
  hgsql -Ne "SELECT $chr, $start, $end FROM $table $whereLimit" $db > blockBedFile.bed
endif

# echo $db
# echo $table
# echo $chr $start $end

# make negative of track and of gap track
featureBits $db blockBedFile.bed -not $limit -bed=$table.not.bed \
  >& /dev/null
featureBits $db gap    -not $limit -bed=gap.not.bed \
  >& /dev/null

# then intersect them
featureBits $db $table.not.bed gap.not.bed $limit \
  -bed=$table.holes.bed >& /dev/null
# save coords and size and sort for biggest
cat $table.holes.bed | awk '{print $1, $2, $3, $3-$2}' | sort -k4,4nr \
  > $table.holes.sort

if ( -z $table.holes.bed ) then
  echo
  echo "there are no non-gap regions not covered by $table."
  echo "or there may be no gap table."
  echo
else 
  # report
  echo
  echo "largest missing non-gap regions are here:"
  
  echo
  echo "chrom chromStart chromEnd size" \
    | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
  echo "----- ---------- -------- ----" \
    | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
  head -10 $table.holes.sort \
    | awk '{ printf("%13s %14s %14s %12s \n", $1, $2, $3, $4) }'
  echo
  
  echo
  # make links for three biggest
  set url1="http://genome-test.soe.ucsc.edu/cgi-bin/hgTracks?db=$db&position="
  set url2="&$table=pack&gap=dense"
  set bigThree=`head -3 $table.holes.sort \
    | awk '{print $1 ":" $2-300 "-" $3+300}'` 
  echo "links to the three biggest voids in ${table}:"
  echo " (with 300 bp padding on each end)"
  foreach item ( $bigThree )
    echo "$url1$item$url2"
  end
  echo
endif
  
# clean up
rm -f blockBedFile.bed
rm -f gap.not.bed
rm -f $table.not.bed
rm -f $table.holes.bed 
rm -f $table.holes.sort

exit 0
