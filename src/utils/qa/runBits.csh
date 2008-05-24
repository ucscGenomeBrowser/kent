#!/bin/tcsh


###############################################
# 
#  05-11-07 
#  Small module that runs featureBits and checks for overlap with gaps [incl unbridged]
#  Written by Bob Kuhn 
# 
###############################################


set checkUnbridged="false"
set db=""
set chrom=""
set split=""
set track=""
set gapUrlFile=""
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

#set up urls
set url1="http://genome-test.cse.ucsc.edu/cgi-bin/hgTracks?db=$db"
set url3="&$track=pack&gap=pack"

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
  # no overlap to gap.  clean up and quit
  echo
  rm -f $db.gapFile
  rm -f $db.chromlist
  exit
endif

if ( $checkUnbridged == "false" ) then
  set gapUrlFile=$db.gapFile
else
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
  set gapUrlFile=$db.$track.unbridged.gaps
endif

# print three records, either from gap overlap or unbridged gap overlap.
if ( -z $gapUrlFile ) then
  exit
else
  # print 3 records, both as text and as links
  # links have 300 bp padding 
  echo "total number of unbridged gaps:"
  wc -l  $gapUrlFile
  head -3  $gapUrlFile
  echo
  set number=`wc -l  $gapUrlFile | awk '{print $1}'`
  if ( $number < 3 ) then
    set n=$number
  else
    set n=3
  endif
  while ( $n )
    set pos=`sed -n -e "${n}p"  $gapUrlFile \
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

