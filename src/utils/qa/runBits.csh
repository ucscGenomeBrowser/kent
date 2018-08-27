#!/bin/tcsh
source `which qaConfig.csh`


###############################################
# 
#  05-11-07 
#  Small module that runs featureBits and checks for overlap with gaps [incl unbridged]
#  Written by Bob Kuhn 
# 
###############################################

onintr cleanup

set checkUnbridged="false"
set db=""
set chrom=""
set split=""
set track=""
set gapUrlFile=( "" "" )
set comment=( "" "" )
set url1=""
set url3=""

if ( $#argv < 2 |  $#argv > 3 |  ) then
  # no command line args
  echo
  echo "  runs featureBits and checks for overlap with gaps."
  echo
  echo "    usage:  database trackname [checkUnbridged]"
  echo "              where overlap with unbridged gaps can be turned on"
  echo
  exit
else
  set db=$argv[1]
  set track=`echo $argv[2] | sed -e "s/chr.*_//"`
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n  error: you must run this script on dev!\n"
 exit 1
endif

if ( $#argv == 3 ) then
  if ( $argv[3] == "checkUnbridged" ) then
    set checkUnbridged="true"
  else
    echo
    echo 'third argument must be "checkUnbridged"'
    echo
    echo $0
    $0
    exit 1
  endif
endif

set split=`getSplit.csh $db gap hgwdev` 

#check to see if track can pack.
if ( 1 == `hgsql -Ne 'SELECT canPack FROM trackDb \
     WHERE tableName = "'$track'"' $db` ) then
  set pack=pack
else
  set pack=full
endif

#set up urls
set url1="http://genome-test.soe.ucsc.edu/cgi-bin/hgTracks?db=$db"
set url3="&$track=$pack&gap=pack"

# ------------------------------------------------
# featureBits

echo
echo "featureBits -countGaps $db $track"
featureBits -countGaps $db $track

if ($status) then
  echo "failed. quitting"
  echo
  exit
endif

echo "featureBits -countGaps $db $track gap"
featureBits -countGaps $db $track gap -bed=$db.gapFile
echo

if ( -z $db.gapFile ) then
  # no overlap to gap.  clean up and quit
  echo
  rm -f $db.gapFile
  exit
else
  set gapUrlFile[1]=$db.gapFile
  set comment[1]="There are gaps overlapping $track (gaps may be bridged or not):"
endif


if ( $checkUnbridged == "true" ) then
  echo "checking for annotations spanning unbridged gaps."
  # note: this means that the annotation need not intersect, 
  #   introns spanning the gap are flagged
  rm -f $db.unbridgedGap.bed
  # create file of unbridged gaps
  if ( $split == "unsplit" ) then
    # gap is not split
    hgsql $db -N -e "SELECT chrom, chromStart, chromEnd FROM gap \
     WHERE bridge = 'no'" > $db.unbridgedGap.bed
  else
    # gap is split.  go thru all chroms
    getChromlist.csh $db > $db.chromlist$$
    foreach chrom (`cat $db.chromlist$$`)
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
  set gapUrlFile[2]=$db.$track.unbridged.gaps
  set comment[2]="total number of unbridged gaps:"
endif

# print three records, either from gap overlap or unbridged gap overlap.
foreach n ( 1 2 )
  if ( ! -e "$gapUrlFile[$n]" ) then
    continue
  else
    # print 3 records as links with 300 bp padding 
    echo $comment[$n]
    wc -l  $gapUrlFile[$n]
    echo
    # set number to print: 3 or less
    set num=`wc -l  $gapUrlFile[$n] | awk '{print $1}'`
    set j=`echo "3 $num" | sed "s/ /\n/g" | sort -n | head -1`
    while ( $j )
      set pos=`sed -n -e "${j}p"  $gapUrlFile[$n] \
        | awk '{print $1":"$2-300"-"$3+300}'`
      echo "$url1&position=$pos$url3"
      set j=`echo $j | awk '{print $1-1}'`
    end
    echo
  endif
end

cleanup:
rm -f $db.unbridgedGap.bed
rm -f $track.bed
rm -f $db.chromlist$$
rm -f $db.gapFile
rm -f $gapUrlFile[1]
rm -f $gapUrlFile[2]

