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
set split=""
set chrom=""
set trackname=""

if ( $#argv == 0 || $#argv > 2) then
  # no command line args
  echo
  echo "  runs test suite on chain track (on both regular and Link tables)"
  echo "  expects trackname in chrN_chainOrg format"
  echo "  though it now works for chainOrg format assemblies"
  echo "  slow processes are in chain2.csh"
  echo
  echo "    usage:  database trackname"
  echo "    e.g. chain.csh mm7     chrN_chainXenTro1 > & mm7.chain.xenTro1 &"
  echo "      or chain.csh anoCar1 chainXenTro1      > & anoCar1.chain.xenTro1 &"
  echo
  exit
else
  set db=$argv[1]
  set trackname=$argv[2]
endif

set track=`echo $trackname | sed -e "s/chrN_//"`
set Org=`echo $track | sed -e "s/chain//"`
set otherDb=`echo $Org | perl -wpe '$_ = lcfirst($_)'`
set split=`getSplit.csh $db chain$Org hgwdev`

echo "using database $db "
echo "trackname: $trackname"
echo "track: $track"
echo "Org: $Org"
echo

# ------------------------------------------------
# check for priority values for all chains on this assembly:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "all chains/nets for this assembly:"
echo

# make a list of chain and nets to match actual tables
set chainlist=`hgsql -N -e 'SHOW TABLES LIKE "net%"' $db` 
hgsql -N -e 'SHOW TABLES LIKE "net%"' $db \
  | sed -e "s/net/chain/g" > chainlist
echo $chainlist | sed -e "s/ /\n/g" >> chainlist
echo "priority" >> chainlist
echo "----" >> chainlist

hgsql -t -e "SELECT tableName, priority FROM trackDb \
  WHERE tableName LIKE 'chain%' OR tableName LIKE 'net%' \
  ORDER BY priority" $db \
  | grep -f chainlist

# ------------------------------------------------
# check level for html and trackDb entry:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "check level for html and trackDb entry:"
echo
findLevel.csh $db chain$Org

# -------------------------------------------------
# get chroms from chromInfo:

echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo

getChromlist.csh $db > /dev/null
rm -f $db.$Org.pushlist
rm -f $db.$Org.pushlistLink
if ( $split == "unsplit" ) then
   echo "unsplit chain track.  echo of long chromlist suppressed"
   echo $track >> $db.$Org.pushlist
   echo ${track}Link >> $db.$Org.pushlist
  echo
else
  # make push list for split tables
  foreach chrom (`cat $db.chromlist`)
    echo $chrom
    echo ${chrom}_$track >> $db.$Org.pushlist
    echo ${chrom}_${track}Link >> $db.$Org.pushlist
  end
endif

# ------------------------------------------------
# check updateTimes for each table:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo
echo "check updateTimes for each table:"
echo "first: hgwdev"
echo "second: hgwbeta"

if ( $split == "unsplit" ) then
  updateTimes.csh $db chain$Org | grep -v ERROR
else
  updateTimes.csh $db $db.$Org.pushlist | grep -v ERROR
endif

# ------------------------------------------------
# make sure that the tName column matches the table name:
#

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking to see if tName matches table name:"

if ( $split == "unsplit" ) then
  echo "can't actually do this comparison if table is not split."
  echo
else
  echo "if there is no output here, then it passes."
  foreach chrom (`cat $db.chromlist`)
    set numTNames=`hgsql -N -e "SELECT COUNT(DISTINCT(tName)) \
     FROM ${chrom}_chain$Org" $db`
    if ($numTNames != 1) then
      if ($numTNames == 0) then
        echo "${chrom}_chain$Org is empty."
      else
        echo "There are $numTNames tNames in ${chrom}_chain$Org"
        echo "Should be only one"
        echo "(you should check this table by hand)."
      endif
    else
      set tName=`hgsql -N -e "SELECT tName FROM ${chrom}_chain$Org\
        LIMIT 1" $db`
      if ( $tName != $chrom ) then
        echo "tName does not match in $chrom_chain${Org}!"
        echo
      endif
    endif
    set numTNames=`hgsql -N -e "SELECT COUNT(DISTINCT(tName)) \
      FROM ${chrom}_chain${Org}Link" $db`
    if ($numTNames != 1) then
      if ($numTNames == 0) then
        echo "${chrom}_chain${Org}Link is empty."
      else
        echo "There are $numTNames tNames in ${chrom}_chain${Org}Link"
        echo "Should be only one"
        echo "(you should check this table by hand)."
      endif
    else
      set tName=`hgsql -N -e "SELECT tName FROM ${chrom}_chain${Org} \
        LIMIT 1" $db`
      if ( $tName != $chrom ) then
        echo "tName does not match in $chrom_chain${Org}Link!"
        echo
      endif
    endif
  end
endif

# -------------------------------------------------
# check the min and max score values
#  (later: get the size of the largest chrom and set the column width to that)

# find size of longest chrom name for format purposes
if ( $split != "unsplit" ) then
  set length=0
  foreach chrom (`cat $db.chromlist`)
    set len=`echo $chrom | awk '{print length($1)}'`
    if ( $len > $length ) then
      set length=$len
    endif
  end
  set length=`echo $length | awk '{print $1+1}'`
  set longlength=`echo $length | awk '{print $1+12}'`
endif

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "checking min and max score values"
echo

if ( $split == "unsplit" ) then
  set min = `hgsql -N -e "SELECT MIN(score) FROM chain${Org}" $db`
  set max = `hgsql -N -e "SELECT MAX(score) FROM chain${Org}" $db`  
  echo "chrom		min	max"
  echo "-----		---	---"
  echo "$chrom		$min	$max"
else
  echo "look through this list for outliers."
  echo "chrom" "min" "max" \
    | gawk '{ printf("%-'${length}'s %8s %12s \n", $1, $2, $3) }'
  echo "-----" "---" "---" \
    | gawk '{ printf("%-'${length}'s %8s %12s \n", $1, $2, $3) }'
  foreach chrom (`cat $db.chromlist`)
    set min = `hgsql -N -e "SELECT MIN(score) FROM ${chrom}_chain${Org}" $db`
    set max = `hgsql -N -e "SELECT MAX(score) FROM ${chrom}_chain${Org}" $db`  
    echo $chrom	$min $max \
      | gawk '{ printf("%-'${length}'s %8s %12s \n", $1, $2, $3) }'
  end #foreach
endif
echo

# -------------------------------------------------
# check for rowcounts in each table:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "rowcounts"
echo

if ( $split == "unsplit" ) then
  echo $trackname
  hgsql -t -e "SELECT COUNT(*) AS rows FROM chain${Org}" $db
  echo ${trackname}Link
  hgsql -t -e "SELECT COUNT(*) AS rows FROM chain${Org}Link" $db
  echo "too many chroms to do a count per chrom"
else
  echo "check for rowcounts in each table:"
  echo "rowcounts are listed - pay attention to counts of 0"
  echo
  echo "for chrN_chain${Org}:"
  foreach chrom (`cat $db.chromlist`)
    set var1=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_chain${Org}" $db`
    echo ${chrom}_chain${Org} $var1 \
      | gawk '{ printf("%-'${longlength}'s %6s \n", $1, $2) }'
  end
  echo
  echo "for chrN_chain${Org}Link:"
  foreach chrom (`cat $db.chromlist`)
    set var1=`hgsql -N -e "SELECT COUNT(*) FROM ${chrom}_chain${Org}Link" $db`
    set longer=`echo $longlength | awk '{print $1+5}'`
    echo ${chrom}_chain${Org}Link $var1 \
      | gawk '{ printf("%-'${longer}'s %8s \n", $1, $2) }'
  end
  echo
endif
echo

# -------------------------------------------------
# check that qStrand has a valid value

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "count + and - strand alignments"
echo "watch for zeroes"

echo

if ( $split == "unsplit" ) then
  set badStrands=`hgsql -N -e 'SELECT COUNT(*) FROM chain'$Org' \
    WHERE qStrand != "-" AND qStrand != "+"' $db`
  if ( $badStrands > 0 ) then
    echo 'some qStrands are neither "+" nor "-"'
  else
    echo 'all qStrands are either "+" or "-"'
    echo
  endif
  echo "posStrand negStrand" \
    | gawk '{ printf("%8s %8s \n", $1, $2) }'
  echo "--------- ---------" \
    | gawk '{ printf("%8s %8s \n", $1, $2) }'
  set posStrands = `hgsql -N -e "SELECT COUNT(*) \
    FROM chain${Org} WHERE qStrand LIKE '+'" $db`
  set negStrands = `hgsql -N -e "SELECT COUNT(*) \
    FROM chain${Org} WHERE qStrand LIKE '-'" $db`
  echo $chrom $posStrands $negStrands \
    | gawk '{ printf("%8s %8s \n", $1, $2) }'
else
  echo "chrom posStrand negStrand" \
      | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
  echo "------  ---------  ---------" \
      | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
  rm -f badStrands
  foreach chrom (`cat $db.chromlist`)
    set badStrands=`hgsql -N -e 'SELECT COUNT(*) FROM '$chrom'_chain'$Org' \
      WHERE qStrand != "-" AND qStrand != "+"' $db`
    # echo $badStrands
    if ( $badStrands > 0 ) then
      echo $chrom >> badStrands
    endif
    set posStrands = `hgsql -N -e "SELECT COUNT(*) \
      FROM ${chrom}_chain${Org} WHERE qStrand LIKE '+'" $db`
    set negStrands = `hgsql -N -e "SELECT COUNT(*) \
      FROM ${chrom}_chain${Org} WHERE qStrand LIKE '-'" $db`
    echo $chrom $posStrands $negStrands \
      | gawk '{ printf("%-'${length}'s %8s %8s \n", $1, $2, $3) }'
  end #foreach
  echo
  if ( -e badStrands ) then
    echo 'these chroms have some qStrands that are neither "+" nor "-"'
    cat badStrands
  else
    echo 'all qStrands are "+" or "-"'
  endif
  rm -f badStrands
endif

echo

# -------------------------------------------------
# check that qStrand is displayed properly:

echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo "use these three rows to check (manually) that qStrand is \
   displayed properly in the $db browser:"
echo

if ( $split == "unsplit" ) then
  hgsql -t -e "SELECT tName, tStart, tEnd, qName, qStrand \
      FROM $track WHERE tStart > 10000000 LIMIT 3" $db
  echo
else
  # pick a random chrom > 10 million and pull out three records
  set rand=''
  set rand=`hgsql -N -e "SELECT chrom FROM chromInfo \
     WHERE size > 10000000 ORDER BY RAND() \
     LIMIT 1" $db`
  hgsql -t -e "SELECT tName, tStart, tEnd, qName, qStrand \
      FROM ${rand}_$track WHERE tStart > 10000000 LIMIT 3" $db
  echo
endif

# -------------------------------------------------
# check that tables are sorted by tStart:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo  "check that tables are sorted by tStart:"
echo

if ( $split == "unsplit" ) then
  echo "can't check chrom ordering on unsplit chroms right now"
else
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
endif

# -------------------------------------------------
# find the correct paramaters for the matrix variable:


echo
echo "*~*~*~*~*~*~*~*~*~*~*~*~*~*"
echo  "Find the correct parameters for the matrix variable"
echo  "which appears in the chain-OtherOrg download file."
echo  "Compare this to the chain description page."
echo

getMatrixLines.csh $db $otherDb


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
