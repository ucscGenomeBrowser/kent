#!/bin/tcsh

####################
#  05-03-04 Bob Kuhn
#
#  Script to process use featureBits to get yield and enrichment
#
####################


set track=""
set refTrack=""
set db=""
set current_dir = $cwd
set uniqFlag=0

if ($2 == "") then
  # not enough command line args
  echo
  echo "  uses featureBits to get yield and enrichment."
  echo
  echo "    usage:  database, trackname, [reference track] defaults to refGene"
  echo
  exit
else
  set db=$1
  set track=$2
  set refTrack="refGene"
endif

if ($3 != "") then
  set refTrack=$3
endif
echo
echo "database: $db"
echo "track: $track"
echo "reference track: $refTrack"


# -------------------------------------------------
# check featureBits:

echo
echo "$track"
featureBits $db $track > thisTrack
cat thisTrack
echo
echo "${refTrack}:cds"
featureBits $db ${refTrack}:cds > $refTrack 
cat $refTrack
echo
echo "intersection of $track with ${refTrack}:cds"
featureBits $db ${refTrack}:cds $track > union
cat union
echo


set thisTrack=`cat thisTrack | gawk '{print $1}'`
set genome=`cat thisTrack | gawk '{print $4}'`
set ref=`cat $refTrack | gawk '{print $1}'`
set union=`cat union | gawk '{print $1}'`


set yield=`echo $union $ref \
     | gawk '{printf "%.1f\n", $1/$2*100}'`
set enrichment=`echo $union $thisTrack $ref $genome \
     | gawk '{printf "%.1f\n", ($1/$2)/($3/$4)}'`

echo "thisTrack = $thisTrack"
echo "refTrack  = $ref"
echo "union     = $union"
echo "genome  = $genome"
echo
echo "yield       = ${yield}% (union / $refTrack)"
echo "enrichment  = ${enrichment}x ((union / $track) / ($refTrack / genome))"
echo

rm union
rm thisTrack
rm $refTrack



