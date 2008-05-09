#!/bin/tcsh


###############################################
# 
#  03-28-04 & 10-27-2005
#  Checks net tracks.
#  Written by Bob Kuhn - augmented by Ann Zweig
# 
###############################################


set db=""
set trackname=""
set currDir=$cwd

if ($2 == "") then
  # no command line args
  echo
  echo "  runs test suite on net track."
  echo "  expects trackname in netOrg format"
  echo "  e.g. net.csh mm7 netXenTro1 > & mm7.net.xenTro1 & "
  echo
  echo "    usage:  database trackname"
  echo
  exit
else
  set db=$1
  set trackname=$2
endif

set Org=`echo $trackname | sed -e "s/net//"`
set track=$trackname

echo "using database $db "
echo "track: $track"
echo "trackname: $trackname"
echo "Org: $Org"

# ------------------------------------------------
# check level for html and trackDb entry:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check level for html and trackDb entry:"
echo
findLevel.csh $db net$Org

# -------------------------------------------------
# check updateTimes:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Update Times (hgwdev vs. hgwbeta):"
echo
updateTimes.csh $db $trackname


# -------------------------------------------------
# rowcounts:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "rowcounts:"
hgsql -t -e "SELECT COUNT(*) AS rows FROM $trackname" $db



# ------------------------------------------------
# featureBits

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "run featureBits"
echo

runBits.csh $db $track

# -------------------------------------------------
# get two records:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "view these two randomly-chosen records in the browser:"
echo

hgsql -t -e "SELECT * FROM $trackname LIMIT 2" $db
echo


# -------------------------------------------------
# get chroms from chromInfo:

getChromlist.csh $db > /dev/null

# -------------------------------------------------
# check for each chrom having data:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check that each chrom has data:"
echo "if there is no output, then it passes."
echo 'if this list is long (as in scaffold assemblies), grep for "Look" \
      to get past the list'
set var=""

foreach chrom (`cat $db.chromlist`)
  set var=` hgsql -N -e 'SELECT COUNT(*) from 'net$Org' \
     WHERE tName = "'$chrom'"' $db`
  if ($var == 0) then
    echo "$chrom is empty"
  else
    # echo "$chrom is ok"   # debug
  endif
end



# -------------------------------------------------
# check ends for off-end coords:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"

checkOffend.csh $db net$Org


# -------------------------------------------------
# check that all levels fall between 1-12 inclusive:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Here is a list of the levels in the $track file:"
echo "Expect 1-12 inclusive:"
echo
hgsql -N -e "SELECT DISTINCT(level) FROM  $trackname" $db
echo


# -------------------------------------------------
# check that all of the types are of (top, gap, inv, syn, or nonSyn):

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Here is a list of the types in the $track file:"
echo "Expect these types: top, gap, inv, syn, nonSyn:"
echo
hgsql -N -e "SELECT DISTINCT(type) FROM $trackname" $db
echo


# -------------------------------------------------
# check to ensure that if level=1 then type=top (and vice versa):

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking to make sure that if level=1 then type=top and vice versa:"
echo "if there is no output, then it passes."
echo

#  set var=`hgsql -N -e 'SELECT COUNT(*) FROM '$chrom'_chain'$Org'Link \
#     WHERE tName != "'$chrom'"' $db`

set var1=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE type = "top" AND level != 1' $db`
set var2=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE level = 1 AND type != "top"' $db`

if ($var1 != 0) then
  echo "there is at least one instance where type = top and level != 1 (check this by hand in the database table)."
endif

if ($var2 != 0) then
  echo "there is at least one instance where level = 1 and type != top (check this by hand in the database table)."
endif



# -------------------------------------------------
# check to ensure that if type=gap then level is an even number (and vice versa):

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking to make sure that if type=gap, then level is an even number:"
echo "expect to see 2, 4, 6, 8, 10, 12 in the following list:"
echo 

hgsql -N -e 'SELECT DISTINCT(level) FROM '$trackname' WHERE type = "gap" ORDER BY level' $db



# -------------------------------------------------
# check to ensure that types are all on the correct levels:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking to make sure that types are on the correct levels:"
echo "expect to see types of inv, syn and nonSyn on levels 3, 5, 7, 9, 11, 13"
echo "(these won't necessarily go all the way to 13):"
echo
echo "type = inv:"
hgsql -N -e 'SELECT DISTINCT(level) FROM '$trackname' WHERE type = "inv" ORDER BY level' $db

echo
echo "type = syn:"
hgsql -N -e 'SELECT DISTINCT(level) FROM '$trackname' WHERE type = "syn" ORDER BY level' $db

echo
echo "type = nonSyn:"
hgsql -N -e 'SELECT DISTINCT(level) FROM '$trackname' WHERE type = "nonSyn" ORDER BY level' $db
echo


# -------------------------------------------------
# generate counts by type:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "here's a list of counts by type:"
echo
echo "type	count"
echo "____	_____"
hgsql -N -e 'SELECT DISTINCT(type) AS types, COUNT(*) AS number \
   FROM '$trackname' GROUP BY types ORDER BY number DESC' $db


# -------------------------------------------------
# generate counts by level:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "here's a list of counts by level:"
echo
echo "level	count"
echo "_____	_____"
hgsql -N -e 'SELECT DISTINCT(level) AS levels, COUNT(*) AS number \
    FROM '$trackname' GROUP BY levels ORDER BY level' $db



# -------------------------------------------------
# check that strand has a valid value and is displayed correctly:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "use these three rows to check (manually) that qStrand is displayed properly in the browser:"
echo

hgsql -t -e "SELECT tName, tStart, tEnd, level, type, qName, strand FROM $trackname WHERE tStart > 10000000 LIMIT 3" $db
echo

echo "not gap"
hgsql -t -e "SELECT tName, tStart, tEnd, level, type, qName, strand FROM $trackname WHERE tStart > 10000000 AND type != 'gap' LIMIT 3" $db
echo



# -------------------------------------------------
# check that strand has a valid value


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check that strand has valid values"
echo "expect '+ -'"
echo

hgsql -N -e "SELECT DISTINCT(strand) FROM ${trackname}" $db

set numBlank = `hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE strand != "+" AND strand != "-"' $db`

if ($numBlank > 0) then
  echo "In addition to the above strand values, there are $numBlank blank strand values.  These should be checked by hand."
endif
echo


# -------------------------------------------------
# check that chainId, ali and score are 0 for gaps
# and >0 for all other types

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check that chainId, ali and score have valid values"
echo

echo "chainId value(s) for type = gap (expect 0):"
hgsql -N -e 'SELECT DISTINCT(chainId) FROM '$trackname' WHERE type = "gap"' $db
echo

echo "ali value(s) for type = gap (expect 0):"
hgsql -N -e 'SELECT DISTINCT(ali) FROM '$trackname' WHERE type = "gap"' $db
echo

echo "score value(s) for type = gap (expect 0):"
hgsql -N -e 'SELECT DISTINCT(score) FROM '$trackname' WHERE type = "gap"' $db
echo

echo "count of chainId values that are '0' for all other types (expect 0):"
hgsql -N -e 'SELECT COUNT(chainId) FROM '$trackname' WHERE type != "gap" AND chainId = 0' $db
echo

echo "count of ali values that are '0' for all other types (expect 0):"
hgsql -N -e 'SELECT COUNT(ali) FROM '$trackname' WHERE type != "gap" AND ali = 0' $db
echo

echo "count of score values that are '0' for all other types (expect 0):"
hgsql -N -e 'SELECT COUNT(score) FROM '$trackname' WHERE type != "gap" AND score = 0' $db
echo



# -------------------------------------------------
# check min and max values for ali and score (for nonGap types)

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking min and max values for ali and score (for type != gap):"
echo

set types=`hgsql -N -e 'SELECT DISTINCT(type) FROM '$trackname' WHERE type != "gap"' $db`;

foreach control('ali' 'score')

foreach type ($types)

  echo type=$type
  echo column=$control

  hgsql -e 'SELECT MIN('$control') as minimum, MAX('$control') as maximum FROM '$trackname' WHERE type LIKE "'$type'"' $db

echo
end #foreach
end #foreach



# -------------------------------------------------
# check max values for score by type (for nonGap types)

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking max values for score by type (for type != gap):"
echo

  hgsql -e 'SELECT MAX(score) AS scores, type FROM '$trackname' GROUP BY type ORDER BY type DESC' $db



# -------------------------------------------------
# check that chrN_chainOrg.id  is uniq:
# superceded by joinerCheck
# check that chainIds are all found in chainOrg.id:
# superceded by joinerCheck

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "Note: joinerCheck is run in chain2.csh - it covers the net track too."
echo



# -------------------------------------------------
# qOver, qFar, and qDup should be -1 for type=gap

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking that qOver, qFar, and qDup = -1 for type=gap:"
echo

set count=''
foreach variable ('qOver' 'qFar' 'qDup')
  set count=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE type LIKE "'gap'" AND '$variable' != -1' $db`

  if ($count != 0) then
    echo "$variable has $count rows where value != -1 and type = gap"
    echo "you should investigate this by hand."
  else
    echo "$variable passes (has $count rows where value != 1 and type = gap)"
  endif

set count=0
end #foreach



# -------------------------------------------------
# qDup should be 0 or greater for nonGaps

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking that qDup >= 0 for type != gap:"
echo

#set types=`hgsql -N -e 'SELECT DISTINCT(type) FROM '$trackname' WHERE type != "gap"' $db`;

foreach type ($types)

  echo type=$type

  set count=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE qDup < 0 AND type LIKE "'$type'"' $db`

  if ($count != 0) then
    ech  "There are $count rows where type = $type and qDup < 0"
    echo "You should investigate these by hand."
    echo
  else
    echo "passed."
    echo
  endif
end #foreach
echo


# -------------------------------------------------
# for nonGaps, if qOver is -1, then qFar must be -1 and vice versa

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking that if qOver = -1 then qFar = -1 (and vice versa):"
echo

set pass1='F'
set pass2='F'

set var1=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE qOver = -1 AND qFar != -1' $db`
set var2=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE qOver != -1 AND qFar = -1' $db`

if ($var1 != 0) then
  echo "there is at least one instance where qOver = -1 and qFar != 1 (check this by hand in the database table)."
  set pass1='F'
else
  set pass1='T'
endif

if ($var2 != 0) then
  echo "there is at least one instance where qOver != 1 and qFar = 1 (check this by hand in the database table)."
  set pass2='F'
else
  set pass2='T'
endif

if ($pass1 == 'T' && $pass2 == 'T') then
    echo passed both tests.
  endif
endif
echo


# -------------------------------------------------
# for nonGaps, if qOver > 0, then qFar = 0

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking that for type != gap, if qOver > 0, then qFar = 0:"
echo


#set types=`hgsql -N -e 'SELECT DISTINCT(type) FROM '$trackname' WHERE type != "gap"' $db`;

foreach type ($types)

  echo type=$type

  set count=`hgsql -N -e 'SELECT COUNT(*) FROM '$trackname' WHERE qOver > 0 AND qFar != 0' $db`

  if ($count != 0) then
    echo "There are $count rows where qOver > 0 and qFar != 0"
    echo "You should investigate these by hand."
    echo
  else
    echo "passed."
    echo
  endif
end #foreach
echo



# -------------------------------------------------
# note MIN and MAX values for chainId (for nonGaps)

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking MIN and MAX values for chainId (for type != gap):"
echo


#set types=`hgsql -N -e 'SELECT DISTINCT(type) FROM '$trackname' WHERE type != "gap"' $db`;

foreach type ($types)

  echo type=$type

  hgsql -e 'SELECT MIN(chainId) as minimum, MAX(chainId) as maximum FROM '$trackname' WHERE type LIKE "'$type'"' $db

echo
end #foreach
echo 

# -------------------------------------------------
# add track to list of files to push and find size of entire push:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"

echo $trackname >> $db.$Org.pushlist
sort -u $db.$Org.pushlist > pushlist2

getTableSize.csh $db pushlist2 hgwdev
rm -f pushlist2

echo "the end."
