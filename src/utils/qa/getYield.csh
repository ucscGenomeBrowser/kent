#!/bin/tcsh
source `which qaConfig.csh`

####################
#  05-03-04 Bob Kuhn
#
#  Script to use featureBits to get yield and enrichment
#
####################
# note:  output words "union" changed to "intersection" 
#   but internal variable not changed

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif


set track=""
set refTrack=""
set db=""
set current_dir = $cwd
set uniqFlag=0

if ($#argv < 2 || $#argv > 3) then
  # not enough command line args
  echo
  echo "  uses featureBits to get yield and enrichment."
  echo
  echo "    usage:  database trackname [reference track]"
  echo "              refTrack defaults to refGene"
  echo
  exit
else
  set db=$argv[1]
  set track=$argv[2]
  set refTrack="refGene"
endif

if ($#argv == 3) then
  set refTrack=$argv[3]
endif

echo
echo "database: $db"
echo "track: $track"
echo "reference track: $refTrack"

hgsql -N -e "SHOW TABLES" $db | grep -w $track > /dev/null
if ($status) then
  echo "\n  error. table $track not found in $db\n"
  exit
endif

hgsql -N -e "SHOW TABLES" $db | grep -w $refTrack > /dev/null
if ($status) then
  echo "\n  error. table $refTrack not found in $db\n" 
  exit
endif


# -------------------------------------------------
# check featureBits:

echo
echo "$track"
featureBits $db $track >& thisTrack
cat thisTrack
echo
echo "${refTrack}"
featureBits $db ${refTrack} >& $refTrack 
cat $refTrack
echo
echo "intersection of $track with ${refTrack}"
featureBits $db ${refTrack} $track >& union
cat union
echo

set thisTrack=`cat thisTrack | gawk '{print $1}'`
set genome=`cat thisTrack | gawk '{print $4}'`
set ref=`cat $refTrack | gawk '{print $1}'`
cat $refTrack
set union=`cat union | gawk '{print $1}'`

set yield=`echo $union $ref \
     | gawk '{printf "%.1f\n", $1/$2*100}'`
set enrichment=`echo $union $thisTrack $ref $genome \
     | gawk '{printf "%.1f\n", ($1/$2)/($3/$4)}'`

# yield is what portion of the reference track was captured in the
#    other track
# enrichment is how many times better this track overlaps the reference
#    track than that track's proportion of the genome (I think)

echo "thisTrack = $thisTrack"
echo "refTrack  = $ref"
echo "intersection = $union"
echo "genome  = $genome"
echo
echo "yield       = ${yield}% (intersection / $refTrack)"
echo "enrichment  = ${enrichment}x ((intersection / $track) / ($refTrack / genome))"
echo

rm union
rm thisTrack
rm $refTrack



