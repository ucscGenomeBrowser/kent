#!/bin/tcsh
source `which qaConfig.csh`


###############################################
# 
#  03-28-04 & 10-26-2005
#  Checks chain tracks.
#  Written by Bob Kuhn - augmented by Ann Zweig.
#  Runs slow processes (quick processes are in chain.csh).
# 
###############################################

onintr cleanup

set db=""
set split=""
set trackname=""

if ( $#argv != 2 ) then
  # no command line args
  echo
  echo "  runs test suite on chain track (on both regular and Link tables)"
  echo "  expects trackname in chrN_chainOrg format."
  echo "  though it now works for chainOrg format assemblies"
  echo "  this can take a long time."
  echo
  echo "    usage:  database trackname"
  echo "    e.g. chain2.csh mm7 chrN_chainXenTro1 > & mm7.chain2.xenTro1 &"
  echo "      or chain2.csh anoCar1 chainXenTro1  > & anoCar1.chain.xenTro1 &"
  echo
  exit
else
  set db=$argv[1]
  set trackname=$argv[2]
endif

set track=`echo $trackname | sed -e "s/chrN_//"`
set Org=`echo $track | sed -e "s/chain//"`
set split=`getSplit.csh $db chain$Org hgwdev`

echo "using database $db "
echo "trackname: $trackname"
echo "track: $track"
echo "Org: $Org"


# -------------------------------------------------
# get chroms from chromInfo:

getChromlist.csh $db > $db.chromlist$$

if ( $split != "unsplit" ) then
  cat $db.chromlist$$
endif


# ------------------------------------------------
# featureBits

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "run featureBits"
echo

runBits.csh $db $track

# -------------------------------------------------
# check sort
# note:  chain*Link tables do not need to be sorted if they have an index

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check sort:"

positionalTblCheck -verbose=0 $db chain$Org
if ( ! $status ) then
  echo "sort is ok"
endif
echo

# -------------------------------------------------
# check ends for off-end coords:

# slow: 

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "looking for annotations off the end of the chrom"
rm -f $db.$track.offEnd
if ( $split == "unsplit" ) then
  checkOffend.csh $db $trackname
else
  foreach chrom (`cat $db.chromlist$$`)
    # echo " chrom: $chrom"
    # echo " chrom_track:  ${chrom}_$track"
    hgsql -N -e "SELECT chromInfo.chrom, chromInfo.size, \
        chromInfo.size - MAX(${chrom}_$track.tEnd) \
        FROM chromInfo, ${chrom}_$track \
        WHERE chromInfo.chrom = ${chrom}_$track.tName  \
        GROUP BY chromInfo.chrom" $db >> $db.$track.offEnd
  end
  echo "rows from $db that are off the end of the chrom:"
  awk '{if($3<0) {print $3} }' $db.$track.offEnd
  echo "expect blank here - if not, check the file $db.$track.offEnd"
endif


# -------------------------------------------------
# check to see if coords in other assembly are off the end.

set otherDb=`echo $Org | perl -wpe '$_ = lcfirst($_)'`
hgsql -N -e "SELECT size, chrom FROM chromInfo" $otherDb | sort -nr > $otherDb.size 
if ( $split == "unsplit" ) then
  echo "check to see if qSize matches coords in other assembly."
  hgsql -N -e "SELECT DISTINCT qSize, qName FROM chain$Org \
    GROUP by qSize" $db | sort -nr > $db.query.size
  commTrio.csh $db.query.size $otherDb.size 
  echo "expect zero for $db.query.size"
else
# not really needed and too complicated here.
# the chances of this being broken are very small
#   foreach chrom (`cat $db.chromlist$$`)
#     hgsql -N -e "SELECT DISTINCT qSize, qName FROM ${chrom}_chain$Org \
#       GROUP by qSize" $db | sort -nr > ${chrom}.query.size
#     commTrio.csh ${chrom}.query.size $otherDb.size   
#   end
endif


# -------------------------------------------------
# for unsplit: checking that all chroms are used

if ( $split == "unsplit" ) then
  echo
  echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
  echo
  echo "checking that all chroms (scaffolds) have chains in chain table"
  hgsql -N -e "SELECT DISTINCT(tName) FROM $track" $db | sort > $db.$track.chroms
  commTrio.csh $db.chromlist$$ $db.$track.chroms
  if ( `wc -l $db.chromlist$$.Only | awk '{print $1}'` > 0 ) then
    # some chroms have not chains.  display links.
    # get multiz info for link
    set multiz=`hgsql -N -e "SHOW TABLES LIKE 'multiz%'" $db | head -1`
    echo "these have no chains:"
    if ( $multiz != "" ) then
      set multiz="&${multiz}=pack"
    endif
    foreach seq ( `head -3 $db.chromlist$$.Only` )
      echo "http://genome-test.soe.ucsc.edu/cgi-bin/hgTracks?db=$db&position=$seq&chain$Org=pack$multiz"
    end
    echo "get some DNA from these and see how it blats on $otherDb"
  else
    echo "all chroms have chains"
  endif
endif

# -------------------------------------------------
# Link table:  make list of uniq chainIds.  
#  check if they all exist as chainOrg.id
# dropped this.  duplicates joinerCheck

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "running joinerCheck"
joinerCheck -database=$db -keys ~/kent/src/hg/makeDb/schema/all.joiner -identifier=chain${Org}Id

# -------------------------------------------------
#  check $trackname for tStart < tEnd

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check $trackname for tStart < tEnd"
echo "if there is no output here, then it passes."

if ( $split == "unsplit" ) then
  set var3=`hgsql -N -e "SELECT COUNT(*) FROM ${track} \
     WHERE tStart >= tEnd" $db`
  if ($var3 != 0) then
    echo "${track} has $var3 records with tStart >= tEnd"
  endif
else
  foreach chrom (`cat $db.chromlist$$`)
    set var3=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_${track} \
       WHERE tStart >= tEnd" $db`
    if ($var3 != 0) then
      echo "${chrom}_${track} has $var3 records with tStart >= tEnd"
    endif
  end
endif


# -------------------------------------------------
# check qStrand values in $track:
# dropped this section.  done in chain.csh


# -------------------------------------------------

echo
echo "the end."
cleanup:
rm -f $db.chromlist$$
