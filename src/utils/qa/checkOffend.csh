#!/bin/tcsh

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
  echo '    finds the proper column names if "chrom" or "tName".'
  echo
  echo "    usage:  database, table"
  echo
  exit
else
  set db=$1
  set track=$2
endif

set chr=""
set end=""

set chr=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "chrom"`
if ($status) then
  set chr=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "tName"`
  if ($status) then
    set chr=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' \
          | egrep -w "genoName"`
    if ($status) then
      echo '\n  '$db.$track' has no "chrom", "tName" or "genoName" fields.\n'
      echo ${0}:
      $0
      exit 1 
    endif 
endif 


set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "chromEnd"`
if ($status) then
  set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "tEnd"`
  if ($status) then
    set end=`hgsql -N -e "DESC $track" $db | gawk '{print $1}' | egrep -w "txEnd"`
    if ($status) then
      echo '\n  '$db.$track' has no "chromEnd" or "tEnd" or "txEnd" fields.\n'
      exit 1 
    endif 
  endif 
endif 


echo
echo "Looking for off the end of each chrom"

# Will also use the chromInfo table
hgsql -N -e "SELECT chromInfo.chrom, chromInfo.size - MAX($track.$end) \
     AS dist_from_end FROM chromInfo, $track \
     WHERE chromInfo.chrom = $track.$chr \
     GROUP BY chromInfo.chrom" $db > $db.$track.tx.offEnd


awk '{if($2<0) {print $2} }' $db.$track.tx.offEnd
echo "expect blank. any chrom with annotations off the end would be printed."
echo "results are in $db.$track.tx.offEnd.\n"


