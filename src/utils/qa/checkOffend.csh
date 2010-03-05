#!/bin/tcsh
source `which qaConfig.csh`

###############################################
#  05-10-04
# 
#  Checks for table values off the end of chrom
# 
###############################################

set db=""
set chr=""
set track=""
set end=""

if ($#argv != 2) then
  echo
  echo "  checks for entries beyond the end of the chromsome."
  echo '    finds the proper column names if "chrom", "tName" or "genoName".'
  echo
  echo "    usage:  database table"
  echo
  exit
else
  set db=$1
  set track=$2
endif

set chr=""
set end=""

set chr=`getChromFieldName.csh $db $track`
if ($status) then
  echo "\nERROR: not a chromosome-based table.\n"
  exit 1
endif

set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "chromEnd"`
if ($status) then
  set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "tEnd"`
  if ($status) then
    set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "txEnd"`
    if ($status) then
      set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "genoEnd"`
      if ($status) then
        echo '\n  '$db.$track' has no "chromEnd", "tEnd", "txEnd" or "genoEnd" fields.\n'
         exit 1 
      endif
    endif 
  endif 
endif 


echo
echo "Looking for off the end of each chrom"

# Will also use the chromInfo table
hgsql -e "SELECT chromInfo.chrom, chromInfo.size, MAX($track.$end) \
     FROM chromInfo, $track \
     WHERE chromInfo.chrom = $track.$chr \
     GROUP BY chromInfo.chrom" $db > $db.$track.tx.offEnd

head -1 $db.$track.tx.offEnd
awk '{if($2<$3) {print $1, $2, $3} }' $db.$track.tx.offEnd
echo "expect blank. any chrom with annotations off the end would be printed."
echo

rm $db.$track.tx.offEnd

