#!/bin/tcsh

################################
#  
#  02-22-07
#  Robert Kuhn
#
#  gets info about who wrote the lines in a program
#
################################

set program=""
set location=""
set size=""

if ( $#argv != 1 ) then
  echo
  echo "  gets info about who wrote the lines in a program."
  echo
  echo "    usage:  program"
  echo
  exit
else
  set program=$argv[1]
endif

if ( "$HOST" != "hgwdev" ) then
 echo "\n error: you must run this script on dev!\n"
 exit 1
endif

set cwd=`pwd`
cd ~/kent
set location=`find . -name $program`


foreach file ( $location )
  cvs annotate $file| awk '{print $2}' | sed -e "s/^(//" | sort \
    | uniq -c | sort -k2 -nr >& xxOutFilexx
  set size=`cat xxOutFilexx | awk '{total+=$1} END {print total}'`
  cat xxOutFilexx
  if ( `wc -l xxOutFilexx | awk '{print $1}'` > 1 ) then
    echo "-----" "-----" | awk '{printf("%7s %-10s\n", $1, $2)}'
    echo $size "total" | awk '{printf("%7s %-10s\n", $1, $2)}'
    echo
  endif 
  rm xxOutFilexx
end



echo



