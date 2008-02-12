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
set track=""

if ( $#argv != 2 ) then
  # no command line args
  echo
  echo "  runs featureBits and checks for overlap with unbridged gaps."
  echo
  echo "    usage:  database trackname"
  echo
  exit
else
  set db=$argv[1]
  set track=`echo $argv[2] | sed -e "s/chr.*_//"`
endif

set split=`getSplit.csh $db gap hgwdev` 

# ------------------------------------------------
# featureBits

echo
echo "featureBits -countGaps $db $track"
featureBits -countGaps $db $track
if ($status) then
  echo "quitting"
  echo
  exit
endif

echo "featureBits -countGaps $db $track gap"
rm -f file
featureBits -countGaps $db $track gap -bed=$db.gapFile

if ( -z $db.gapFile ) then
  echo
  rm -f $db.gapFile
  rm -f $db.chromlist
  exit
endif

echo "check for overlap (including introns) to unbridged gaps:"
rm -f $db.unbridgedGap.bed
# create file of unbridged gaps
if ( $split == "unsplit" ) then
  # gap is not split
  hgsql $db -N -e "SELECT chrom, chromStart, chromEnd FROM gap \
   WHERE bridge = 'no'" > $db.unbridgedGap.bed
else
  # gap is split.  go thru all chroms
  if (! -e $db.chromlist ) then
    getChromlist.csh $db > /dev/null
  endif
  foreach chrom (`cat $db.chromlist`)
    hgsql $db -N -e "SELECT chrom, chromStart, chromEnd FROM ${chrom}_gap \
     WHERE bridge = 'no'" >> $db.unbridgedGap.bed
  end
endif

# check for intersection of track and gap
# make a file with introns filled in
makeFilledBlockBed.csh $db $track $track.bed
rm -f $db.$track.unbridged.gaps
featureBits $db $track.bed $db.unbridgedGap.bed -bed=$db.$track.unbridged.gaps
echo

if ( -z $db.$track.unbridged.gaps ) then
  exit
else
  # print 3 records, both as text and as links
  # links have 300 bp padding 
  echo "total number of unbridged gaps:"
  wc -l $db.$track.unbridged.gaps
  head -3 $db.$track.unbridged.gaps
  echo
  set url1="http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=$db"
  set url3="&$track=pack&gap=pack"
  set number=`wc -l $db.$track.unbridged.gaps | awk '{print $1}'`
  if ( $number < 3 ) then
    set n=$number
  else
    set n=3
  endif
  while ( $n )
    set pos=`sed -n -e "${n}p" $db.$track.unbridged.gaps \
      | awk '{print $1":"$2-300"-"$3+300}'`
    echo "$url1&position=$pos$url3"
    set n=`echo $n | awk '{print $1-1}'`
  end
endif
echo

rm -f $db.unbridgedGap.bed
rm -f $track.bed
rm -f $db.chromlist
rm -f $db.gapFile


