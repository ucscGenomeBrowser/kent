#!/bin/tcsh


###############################################
# 
#  03-28-04 & 10-26-05
#  Checks chain tracks.
#  Written by Bob Kuhn - augmented by Ann Zweig
#  Slow processes are in chain2.csh
# 
###############################################


set db=""
set trackname=""
set currDir=$cwd

if ($2 == "") then
  # no command line args
  echo
  echo "  runs test suite on chain track (on both regular and Link tables)"
  echo "  expects trackname in chrN_chainOrg format"
  echo "  slow processes are in chain2.csh"
  echo
  echo "    usage:  database, trackname"
  echo "    e.g. chain.csh mm7 chrN_chainXenTro1 > & mm7.chain.xenTro1 &"
  echo
  exit
else
  set db=$1
  set trackname=$2
endif

set track=`echo $trackname | sed -e "s/chrN_//"`
set Org=`echo $track | sed -e "s/chain//"`

echo "using database $db "
echo "trackname: $trackname"
echo "track: $track"
echo "Org: $Org"


# -------------------------------------------------
# get chroms from chromInfo:

~kuhn/bin/getChromlist.csh $db

# ------------------------------------------------
# check updateTimes for each table:

echo "check updateTimes for each table:"
echo "first: hgwdev"
echo "second: hgwbeta"
echo

rm -f $db.$Org.pushlist
foreach chrom (`cat $db.chromlist`)
  echo ${chrom}_$track >> $db.$Org.pushlist
end

updateTimes.csh $db $db.$Org.pushlist


# ------------------------------------------------
# make sure that the tName column matches the table name:
#
# actually, all this does is check to see if there is only
# one type of tName; it doesn't actually check to see if
# that tName value matches the table name!

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking to see if tName matches table name:"
echo "if there is no output here, then it passes."
echo

foreach chrom (`cat $db.chromlist`)
  set numTNames=`hgsql -N -e "SELECT COUNT(DISTINCT(tName)) FROM ${chrom}_chain${Org}" $db`
  if ($numTNames != 1) then
  echo num = $numTNames    
  echo "Not all of the tNames in ${chrom}_chain${Org} match the table name (you should check this table by hand)."
  else
    #echo $chrom  # debug
  endif
end

echo
foreach chrom (`cat $db.chromlist`)
  set numTNames=`hgsql -N -e "SELECT COUNT(DISTINCT(tName)) FROM ${chrom}_chain${Org}Link" $db`
  if ($numTNames != 1) then
  echo num = $numTNames
  echo "Not all of the tNames in ${chrom}_chain${Org}Link match the table name (you should check this table by hand)."
  else
    #echo $chrom  # debug
  endif
end
echo

# -------------------------------------------------
# check that all chainIds in chrN_chainOrgLink are 
# used in the corresponding chrN_chainOrg table (as id).

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check that all chainIds in Link table are used in the other table"
echo "if there is no output here, then it passes."

foreach chrom (`cat $db.chromlist`)
  set numChainIdList = `hgsql -N -e "SELECT COUNT(DISTINCT(chainId)) FROM ${chrom}_chain${Org}Link" $db`
  set numIdList = `hgsql -N -e "SELECT COUNT(DISTINCT(id)) FROM ${chrom}_chain${Org}" $db`

  if ($numChainIdList != $numIdList) then
    echo "Not all of the chainId names in the ${chrom}_chain${Org}Link table appear in the ${chrom}_chain${Org} table"
    echo "You should check the ${chrom}_chain${Org}Link table by hand to determine the problem."
  endif
end #foreach
echo


# ----------------------------------------------
# check the min and max score values


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking min and max score values"
echo "look through this list for outliers."
echo
echo "chrom		min	max"
echo "_____		___	___"
foreach chrom (`cat $db.chromlist`)
  set min = `hgsql -N -e "SELECT MIN(score) FROM ${chrom}_chain${Org}" $db`
  set max = `hgsql -N -e "SELECT MAX(score) FROM ${chrom}_chain${Org}" $db`  
  echo "$chrom		$min	$max"
end #foreach
echo


# -------------------------------------------------
# check for rowcounts in each table:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check for rowcounts in each table:"
echo "only blank tables are reported"
echo

echo "for chrN_chain${Org}:"
foreach chrom (`cat $db.chromlist`)
  set var1=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_chain${Org}" $db`
  if ($var1 == 0) then
    echo "${chrom}_chain${Org} is empty"
  else
    # echo $chrom  # debug
  endif
end
echo

echo "for chrN_chain${Org}Link:"
foreach chrom (`cat $db.chromlist`)
  set var2=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_chain${Org}Link" $db`
  if ($var2 == 0) then
    echo "${chrom}_chain${Org} is empty"
  else
    # echo $chrom  # debug
  endif
end

echo


# -------------------------------------------------
# check that qStrand has a valid value


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check that qStrand has valid values"
echo "expect '+ -'"
echo

echo "chrom		qStrand Values"
echo "_____		______________"
foreach chrom (`cat $db.chromlist`)
  set qStrands = `hgsql -N -e "SELECT DISTINCT(qStrand) FROM ${chrom}_chain${Org} ORDER BY qStrand" $db`
  echo "$chrom		$qStrands"
end #foreach

echo


# -------------------------------------------------
# check that qStrand is displayed properly:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "use these three rows to check (manually) that qStrand is displayed properly in the browser:"
echo

hgsql -t -e "SELECT tName, tStart, tEnd, qName, qStrand \
    FROM chr3_$track WHERE tStart > 10000000 LIMIT 3" $db
echo


# -------------------------------------------------
# check that tables are sorted by tStart:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo  "check that tables are sorted by tStart:"
echo
echo  "tStart:"

foreach chrom (`cat $db.chromlist`)
  # echo $chrom
  hgsql -N -e "SELECT tStart FROM ${chrom}_${track}" $db \
    > $db.$track.tStart
  sort -n $db.$track.tStart > $db.$track.tStart.sort
  set sortCheck=`comm -23 $db.$track.tStart $db.$track.tStart.sort | wc -l`
  # echo $sortCheck
  if ($sortCheck != 0) then
    echo "${chrom}_${track} is not sorted by tStart"
  endif
end
rm $db.$track.tStart $db.$track.tStart.sort
echo "only prints if there is a problem"
echo
echo

# -------------------------------------------------
# make push list for split tables

rm -f $db.$Org.pushlist
rm -f $db.$Org.pushlistLink
foreach chrom (`cat $db.chromlist`)
  echo ${chrom}_$track >> $db.$Org.pushlist
  echo ${chrom}_${track}Link >> $db.$Org.pushlistLink
end

# -------------------------------------------------
# to push to beta:

echo
echo "to push to beta:"
echo
echo  "-------------------------------------------------"
echo  "     bigPush.csh $db $db.$Org.pushlistLink       "
echo  "     bigPush.csh $db $db.$Org.pushlist           "
echo  "-------------------------------------------------"
echo

echo "the end."
