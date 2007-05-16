#!/bin/tcsh


###############################################
# 
#  05-11-07 
#  Small module that runs featureBits and checks for overlap with unbridged gaps
#  Written by Bob Kuhn 
# 
###############################################


set db=""
set chrom=""
set split=""
set trackname=""

if ( $#argv == 0 || $#argv > 2) then
  # no command line args
  echo
  echo "  runs featureBits and checks for overlap with unbridged gaps"
  echo
  echo "    usage:  database trackname"
  echo
  exit
else
  set db=$argv[1]
  set trackname=$argv[2]
endif

set track=`echo $trackname | sed -e "s/chrN_//"`
set split=`getSplit.csh $db $track hgwdev` 


# ------------------------------------------------
# featureBits

echo
echo "counting gaps in denominator throughout"
echo
echo "featureBits -countGaps $db $track"
featureBits -countGaps $db $track
if ($status) then
  echo "quitting"
  echo
  exit
endif

echo
echo "featureBits -countGaps $db $track gap"
rm -f file
featureBits -countGaps $db $track gap >& file 
cat file

set isZero=`head -1 file | awk '{print $1}'`
rm file

if ( $isZero == "0" ) then
  echo
  exit
endif

echo "check for overlap to unbridged gaps:"
rm -f $db.unbridgedGap.bed
if ( $split == "unsplit" ) then
  hgsql $db -N -e "SELECT chrom, chromStart, chromEnd FROM gap \
   WHERE bridge = 'no'" > $db.unbridgedGap.bed
else
  if (! -e $db.chromlist ) then
    getChromlist.csh $db > /dev/null
  endif
  foreach chrom (`cat $db.chromlist`)
    hgsql $db -N -e "SELECT chrom, chromStart, chromEnd FROM ${chrom}_gap \
     WHERE bridge = 'no'" >> $db.unbridgedGap.bed
  end
endif

rm -f $db.$track.unbridged.gaps
featureBits $db $track $db.unbridgedGap.bed -bed=$db.$track.unbridged.gaps
echo
if ( -e $db.$track.unbridged.gaps ) then
  # print 3 records, both as text and as links
  echo "total number:"
  wc -l $db.$track.unbridged.gaps
  head -3 $db.$track.unbridged.gaps
  echo
  set url1="http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=$db"
  set url3="&$track=pack&gap=pack"
  set n=3
  while ( $n )
    set pos=`sed -n -e "${n}p" $db.unbridged.gaps \
      | awk '{print $1":"$2"-"$3}'`
    echo "$url1&position=$pos$url3"
    set n=`echo $n | awk '{print $1-1}'`
  end
else
  echo "no overlap with unbridged gaps"
endif
echo

rm -f db.unbridgedGap.bed

