#!/bin/tcsh

################################
#  
#  02-22-07
#  Robert Kuhn
#
#  gets info about who wrote the lines is a program
#
################################

set program=""
set location=""

if ( $#argv != 1 ) then
  echo
  echo "  gets info about who wrote the lines is a program."
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

cvs annotate $location | awk '{print $2}' | sed -e "s/^(//" | sort \
  | uniq -c | sort -k2 -nr
echo

