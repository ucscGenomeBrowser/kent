#!/bin/tcsh


###############################################
# 
#  03-28-04 & 10-26-2005
#  Checks chain tracks.
#  Written by Bob Kuhn - augemnted by Ann Zweig.
#  Runs slow processes (quick processes are in chain.csh).
# 
###############################################


set db=""
set trackname=""
set set currDir=$cwd

if ($2 == "") then
  # no command line args
  echo
  echo "  runs test suite on chain track (on both regular and Link tables)"
  echo "  expects trackname in chrN_chainOrg format."
  echo "  this can take a long time."
  echo
  echo "    usage:  database, trackname"
  echo "    e.g. chain2.csh mm7 chrN_chainXenTro1 > & mm7.chain2.xenTro1 &"
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

# -------------------------------------------------
# check ends for off-end coords:


# slow: 

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "looking for largest coords:"
rm -f $db.$track.offEnd
foreach chrom (`cat $db.chromlist`)
  # echo " chrom: $chrom"
  # echo " chrom_track:  ${chrom}_$track"
  hgsql -N -e "SELECT chromInfo.chrom, chromInfo.size, \
      chromInfo.size - MAX(${chrom}_$track.tEnd) \
      FROM chromInfo, ${chrom}_$track \
      WHERE chromInfo.chrom = ${chrom}_$track.tName  \
      GROUP BY chromInfo.chrom" $db >> $db.$track.offEnd
end


echo
# echo "lines from $db.KG.tx.offEnd > 0:"
echo "lines from $db that are off the end of the chrom:"
awk '{if($3<0) {print $3} }' $db.$track.offEnd
echo "expect blank here - if not, check the file $db.$track.offEnd"
echo

# -------------------------------------------------
# $Org Link:  tName value always matches tables name:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Link tables:  tName value always matches tables name:"
echo "this is slow"
echo


foreach chrom (`cat $db.chromlist`)
  set var=`hgsql -N -e 'SELECT COUNT(*) FROM '$chrom'_chain'$Org'Link \
     WHERE tName != "'$chrom'"' $db`
  if ($var != 0) then
    echo "${chrom}_chain${Org}Link has illegal tName"
  else
    echo "${chrom}_chain${Org}Link is ok"
  endif
end

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
foreach chrom (`cat $db.chromlist`)
  set var3=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_${track} \
     WHERE tStart >= tEnd" $db`
  if ($var3 != 0) then
    echo "${chrom}_${track} has $var3 records with tStart >= tEnd"
  endif
end


# -------------------------------------------------
# check qStrand values in $track:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo  "check qStrand values ${track}:"
echo "if there is no output here, then it passes."
foreach chrom (`cat $db.chromlist`)
  set null=`hgsql -N -e 'SELECT COUNT(*) FROM '${chrom}_${track}' \
     WHERE qStrand = ""' $db`
  set plus=`hgsql -N -e 'SELECT COUNT(*) FROM '${chrom}_${track}' \
     WHERE qStrand = "+"' $db`
  set minus=`hgsql -N -e 'SELECT COUNT(*) FROM '${chrom}_${track}' \
     WHERE qStrand = "-"' $db`
  if ($null != 0) then
    echo "${chrom}_${track} has missing qStrand values"
  endif

  if ($plus == 0) then
    echo "${chrom}_${track} has no plus-strand values"
  endif

  if ($minus == 0) then
    echo "${chrom}_${track} has no minus-strand values"
  endif
end
echo

# -------------------------------------------------

echo "the end."
